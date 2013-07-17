#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2013 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_cc.c
 * \brief 数据库模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2013-03-10: create the document
 ***************************************************************************/
#define AM_DEBUG_LEVEL 1

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "am_debug.h"
#include "am_time.h"
#include "am_userdata.h"
#include "am_misc.h"
#include "am_cc.h"
#include "am_cc_internal.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
 
#define CC_POLL_TIMEOUT 1000
#define CC_FLASH_PERIOD 1000

#define AMSTREAM_USERDATA_PATH "/dev/amstream_userdata"
#define VIDEO_WIDTH_FILE "/sys/class/video/frame_width"
#define VIDEO_HEIGHT_FILE "/sys/class/video/frame_height"


#define SAFE_TITLE_AREA_WIDTH (672) /* 16 * 42 */
#define SAFE_TITLE_AREA_HEIGHT (390) /* 26 * 15 */
#define ROW_W (SAFE_TITLE_AREA_WIDTH/42)
#define ROW_H (SAFE_TITLE_AREA_HEIGHT/15)

 
/****************************************************************************
 * Static data
 ***************************************************************************/
 static const vbi_opacity opacity_map[AM_CC_OPACITY_MAX] = 
{
	VBI_OPAQUE,             /*not used, just take a position*/
	VBI_TRANSPARENT_SPACE,  /*AM_CC_OPACITY_TRANSPARENT*/
	VBI_SEMI_TRANSPARENT,   /*AM_CC_OPACITY_TRANSLUCENT*/
	VBI_OPAQUE,             /*AM_CC_OPACITY_SOLID*/
	VBI_OPAQUE,             /*AM_CC_OPACITY_FLASH*/
};

static const vbi_color color_map[AM_CC_COLOR_MAX] = 
{
	VBI_BLACK, /*not used, just take a position*/
	VBI_WHITE,
	VBI_BLACK,
	VBI_RED,
	VBI_GREEN,
	VBI_BLUE,
	VBI_YELLOW,
	VBI_MAGENTA,
	VBI_CYAN,
};

static AM_ErrorCode_t am_cc_calc_caption_size(int *w, int *h)
{
	int rows, vw, vh;	
	char wbuf[32];
	char hbuf[32];
	AM_ErrorCode_t ret;

#if 1
	vw = 0;
	vh = 0;
	ret  = AM_FileRead(VIDEO_WIDTH_FILE, wbuf, sizeof(wbuf));
	ret |= AM_FileRead(VIDEO_HEIGHT_FILE, hbuf, sizeof(hbuf));
	if (ret != AM_SUCCESS ||
		sscanf(wbuf, "%d", &vw) != 1 ||
		sscanf(hbuf, "%d", &vh) != 1)
	{
		AM_DEBUG(1, "Get video size failed, default set to 16:9");
		vw = 1920;
		vh = 1080;
	}
#else
	vw = 1920;
	vh = 1080;
#endif
	rows = (vw * 3 * 32) / (vh * 4);
	if (rows < 32)
		rows = 32;
	else if (rows > 42)
		rows = 42;

	AM_DEBUG(2, "Video size: %d X %d, rows %d", vw, vh, rows);

	*w = rows * ROW_W;
	*h = SAFE_TITLE_AREA_HEIGHT;

	return AM_SUCCESS;
}


static uint8_t *am_cc_get_page_canvas(AM_CC_Decoder_t *cc, struct vbi_page *pg)
{
	if (pg->pgno <= 8)
	{
		return cc->cpara.bmp_buffer;
	}
	else
	{
		int x, y, r, b;
		struct dtvcc_service *ds = &cc->decoder.dtvcc.service[pg->pgno- 1 - 8];
		struct dtvcc_window *dw = &ds->window[pg->subno];

		if (dw->anchor_relative)
		{
			x = dw->anchor_horizontal * SAFE_TITLE_AREA_WIDTH / 100;
			y = dw->anchor_vertical * SAFE_TITLE_AREA_HEIGHT/ 100;
		}
		else
		{
			x = dw->anchor_horizontal * SAFE_TITLE_AREA_WIDTH / 210;
			y = dw->anchor_vertical * SAFE_TITLE_AREA_HEIGHT/ 75;
		}

		switch (dw->anchor_point)
		{
			case 0:
			default:
				break;
			case 1:
				x -= ROW_W*dw->column_count/2;
				break;
			case 2:
				x -= ROW_W*dw->column_count;
				break;
			case 3:
				y -= ROW_H*dw->row_count/2;
				break;
			case 4:
				x -= ROW_W*dw->column_count/2;
				y -= ROW_H*dw->row_count/2;
				break;
			case 5:
				x -= ROW_W*dw->column_count;
				y -= ROW_H*dw->row_count/2;
				break;
			case 6:
				y -= ROW_H*dw->row_count;
				break;
			case 7:
				x -= ROW_W*dw->column_count/2;
				y -= ROW_H*dw->row_count;
				break;
			case 8:
				x -= ROW_W*dw->column_count;
				y -= ROW_H*dw->row_count;
				break;
		}

		r = x + dw->column_count * ROW_W;
		b = y + dw->row_count * ROW_H;
		
		AM_DEBUG(2, "x %d, y %d, r %d, b %d", x, y, r, b);
		
		if (x < 0)
			x = 0;
		if (y < 0)
			y = 0;
		if (r > SAFE_TITLE_AREA_WIDTH)
			r = SAFE_TITLE_AREA_WIDTH;
		if (b > SAFE_TITLE_AREA_HEIGHT)
			b = SAFE_TITLE_AREA_HEIGHT;

		/*calc the real displayed rows & cols*/
		pg->columns = (r - x) / ROW_W;
		pg->rows = (b - y) / ROW_H;
				
		AM_DEBUG(2, "window prio(%d), row %d, cols %d ar %d, ah %d, av %d, "
			"ap %d, screen position(%d, %d), displayed rows/cols(%d, %d)", 
			dw->priority, dw->row_count, dw->column_count, dw->anchor_relative,
			dw->anchor_horizontal, dw->anchor_vertical, dw->anchor_point, 
			x, y, pg->rows, pg->columns);

		return cc->cpara.bmp_buffer + y*cc->cpara.pitch + x*4;
	}
}

static void am_cc_clear_safe_title(AM_CC_Decoder_t *cc)
{
	int i;
	uint8_t *p = cc->cpara.bmp_buffer;
	
	for (i=0; i<SAFE_TITLE_AREA_HEIGHT; i++)
	{
		memset(p, 0x00, SAFE_TITLE_AREA_WIDTH * 4);
		p += cc->cpara.pitch;
	}
}
	
static void am_cc_override_by_user_options(AM_CC_Decoder_t *cc, struct vbi_page *pg)
{
	int i, j, opacity;
	vbi_char *ac;

#define OVERRIDE_ATTR(_uo, _uon, _text, _attr, _map)\
	AM_MACRO_BEGIN\
		if (_uo > _uon##_DEFAULT && _uo < _uon##_MAX){\
			_text->_attr = _map[_uo];\
		}\
	AM_MACRO_END

	for (i=0; i<pg->rows; i++)
	{
		for (j=0; j<pg->columns; j++)
		{
			ac = &pg->text[i*pg->columns + j];
			if (pg->pgno <= 8)
			{
				/*NTSC CC style*/
				if (ac->opacity == VBI_OPAQUE)
					ac->opacity = (VBI_OPAQUE<<4) | VBI_OPAQUE;
				else if (ac->opacity == VBI_SEMI_TRANSPARENT)
					ac->opacity = (VBI_OPAQUE<<4) | VBI_SEMI_TRANSPARENT;
				else if (ac->opacity == VBI_TRANSPARENT_FULL)
					ac->opacity = (VBI_OPAQUE<<4) | VBI_TRANSPARENT_SPACE;
			}
			else if (ac->unicode == 0x20 && ac->opacity == VBI_TRANSPARENT_SPACE)
			{
				ac->opacity = (VBI_TRANSPARENT_SPACE<<4) | VBI_TRANSPARENT_SPACE;
			}
			else
			{
				/*DTV CC style, override by user options*/
				OVERRIDE_ATTR(cc->spara.user_options.fg_color, AM_CC_COLOR, \
					ac, foreground, color_map);
				OVERRIDE_ATTR(cc->spara.user_options.bg_color, AM_CC_COLOR, \
					ac, background, color_map);
				OVERRIDE_ATTR(cc->spara.user_options.fg_opacity, AM_CC_OPACITY, \
					ac, opacity, opacity_map);
				opacity = ac->opacity;
				OVERRIDE_ATTR(cc->spara.user_options.bg_opacity, AM_CC_OPACITY, \
					ac, opacity, opacity_map);
				ac->opacity = ((opacity&0x0F) << 4) | (ac->opacity&0x0F);

				/*flash control*/
				if (cc->spara.user_options.bg_opacity == AM_CC_OPACITY_FLASH &&
					cc->spara.user_options.fg_opacity != AM_CC_OPACITY_FLASH)
				{
					/*only bg flashing*/
					if (cc->flash_stat == FLASH_SHOW)
					{
						ac->opacity &= 0xF0;
						ac->opacity |= VBI_OPAQUE;
					}
					else if (cc->flash_stat == FLASH_HIDE)
					{
						ac->opacity &= 0xF0;
						ac->opacity |= VBI_TRANSPARENT_SPACE;
					}
				}
				else if (cc->spara.user_options.bg_opacity != AM_CC_OPACITY_FLASH &&
					cc->spara.user_options.fg_opacity == AM_CC_OPACITY_FLASH)
				{
					/*only fg flashing*/
					if (cc->flash_stat == FLASH_SHOW)
					{
						ac->opacity &= 0x0F;
						ac->opacity |= (VBI_OPAQUE<<4);
					}
					else if (cc->flash_stat == FLASH_HIDE)
					{
						ac->opacity &= 0x0F;
						ac->opacity |= (ac->opacity<<4);
						ac->foreground = ac->background;
					}
				}
				else if (cc->spara.user_options.bg_opacity == AM_CC_OPACITY_FLASH &&
					cc->spara.user_options.fg_opacity == AM_CC_OPACITY_FLASH)
				{
					/*bg & fg both flashing*/
					if (cc->flash_stat == FLASH_SHOW)
					{
						ac->opacity = (VBI_OPAQUE<<4) | VBI_OPAQUE;
					}
					else if (cc->flash_stat == FLASH_HIDE)
					{
						ac->opacity = (VBI_TRANSPARENT_SPACE<<4) | VBI_TRANSPARENT_SPACE;
					}
				}
			}
		}
	}
}

static void am_cc_vbi_event_handler(vbi_event *ev, void *user_data)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)user_data;
	int pgno, subno;

	if(ev->type != VBI_EVENT_CAPTION)
		return;
	pthread_mutex_lock(&cc->lock);
	
	AM_DEBUG(2, "VBI Caption event: pgno %d, cur_pgno %d", 
			ev->ev.caption.pgno, cc->vbi_pgno);
	
	if (cc->vbi_pgno == ev->ev.caption.pgno && 
		(cc->vbi_pgno <= 8 || cc->flash_stat == FLASH_NONE))
	{
		cc->render_flag= AM_TRUE;
		pthread_cond_signal(&cc->cond);
	}
	
	pthread_mutex_unlock(&cc->lock);
}

static void dump_cc_data(uint8_t *buff, int size)
{
	int i;
	char buf[4096];

	if (size > 1024)
		size = 1024;
	for (i=0; i<size; i++)
	{
		sprintf(buf+i*3, "%02x ", buff[i]);
	}

	AM_DEBUG(2, "%s", buf);
}

static void am_cc_check_flash(AM_CC_Decoder_t *cc)
{
	if (cc->spara.user_options.bg_opacity == AM_CC_OPACITY_FLASH ||
		cc->spara.user_options.fg_opacity == AM_CC_OPACITY_FLASH)
	{
		if (cc->flash_stat == FLASH_NONE)
			cc->flash_stat = FLASH_SHOW;
		else if (cc->flash_stat == FLASH_SHOW)
			cc->flash_stat = FLASH_HIDE;
		else if (cc->flash_stat == FLASH_HIDE)
			cc->flash_stat = FLASH_SHOW;
		cc->timeout = CC_FLASH_PERIOD;
		cc->render_flag = AM_TRUE;
	}
	else if (cc->flash_stat != FLASH_NONE)
	{
		cc->flash_stat = FLASH_NONE;
		cc->timeout = 0;
	}
}

static void am_cc_render(AM_CC_Decoder_t *cc)
{
	AM_CC_DrawPara_t draw_para;
	struct vbi_page sub_pages[8];
	int sub_pg_cnt, i;
	
	AM_DEBUG(2, "CC Rendering...");

	/*Flashing?*/
	am_cc_check_flash(cc);

	if (am_cc_calc_caption_size(&draw_para.caption_width, 
		&draw_para.caption_height) != AM_SUCCESS)
		return;
	
	if (cc->cpara.draw_begin)
		cc->cpara.draw_begin(cc, &draw_para);

	/*clear safe title*/
	if (cc->vbi_pgno > 8)
		am_cc_clear_safe_title(cc);
	
	/*fetch cc pages from libzvbi*/
	sub_pg_cnt = AM_ARRAY_SIZE(sub_pages);
	tvcc_fetch_page(&cc->decoder, cc->vbi_pgno, &sub_pg_cnt, sub_pages);
	
	/*draw each*/
	for (i=0; i<sub_pg_cnt; i++)
	{
		/*Override by user options*/
		am_cc_override_by_user_options(cc, &sub_pages[i]);
		
		vbi_draw_cc_page_region(
			&sub_pages[i], VBI_PIXFMT_RGBA32_LE, 
			am_cc_get_page_canvas(cc, &sub_pages[i]),
			cc->cpara.pitch, 0, 0, 
			sub_pages[i].columns, sub_pages[i].rows);
	}
	
	if (cc->cpara.draw_end)
		cc->cpara.draw_end(cc, &draw_para);
}

static void am_cc_handle_event(AM_CC_Decoder_t *cc, int evt)
{
	switch (evt)
	{
		case AM_CC_EVT_SET_USER_OPTIONS:
			/*force redraw*/
			am_cc_clear_safe_title(cc);
			cc->render_flag = AM_TRUE;
			break;
		default:
			break;
	}
}

/**\brief CC data thread*/
static void *am_cc_data_thread(void *arg)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)arg;
	uint8_t buf[5*1024];
	uint8_t cc_data[256];/*In fact, 99 is enough*/
	int cnt, left, fd, cc_data_cnt;
	struct pollfd fds;
	int last_pkg_idx = -1, pkg_idx;
	int ud_dev_no = 0;
	AM_USERDATA_OpenPara_t para;
	
    AM_DEBUG(1, "CC data thread start.");

	memset(&para, 0, sizeof(para));
	/* Start the cc data */
	if (AM_USERDATA_Open(ud_dev_no, &para) != AM_SUCCESS)
	{
		AM_DEBUG(1, "Cannot open userdata device %d", ud_dev_no);
		return NULL;
	}

	while (cc->running)
	{
		cc_data_cnt = AM_USERDATA_Read(ud_dev_no, cc_data, sizeof(cc_data), CC_POLL_TIMEOUT);
		if (cc_data_cnt > 4 && 
			cc_data[0] == 0x47 &&
			cc_data[1] == 0x41 && 
			cc_data[2] == 0x39 &&
			cc_data[3] == 0x34)
		{
			dump_cc_data(cc_data+4, cc_data_cnt-4);
			
			if (cc_data[4] != 0x03 /* 0x03 indicates cc_data */)
			{
				AM_DEBUG(1, "Unprocessed user_data_type_code 0x%02x, we only expect 0x03", cc_data[4]);	
				continue;
			}

			/*decode this cc data*/
			tvcc_decode_data(&cc->decoder, 0, cc_data+4, cc_data_cnt-4);
		}
	}
	
	/*Stop the cc data*/
	AM_USERDATA_Close(ud_dev_no);
		
	AM_DEBUG(1, "CC data thread exit now");
	return NULL;
}

/**\brief CC rendering thread*/
static void *am_cc_render_thread(void *arg)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)arg;
	struct timespec ts;
	int cnt;
	
    AM_DEBUG(1, "CC rendering thread start.");

	pthread_mutex_lock(&cc->lock);
	
	am_cc_check_flash(cc);
	
	while (cc->running)
	{
		if (cc->timeout > 0)
		{
			AM_TIME_GetTimeSpecTimeout(cc->timeout,  &ts);
			pthread_cond_timedwait(&cc->cond, &cc->lock, &ts);
		}
		else
		{
			pthread_cond_wait(&cc->cond, &cc->lock);
		}
		
		if (cc->evt >= 0)
		{
			am_cc_handle_event(cc, cc->evt);
			cc->evt = 0;
		}
		
		if (cc->render_flag)
		{
			am_cc_render(cc);
		}
	}

	pthread_mutex_unlock(&cc->lock);
	
	AM_DEBUG(1, "CC rendering thread exit now");
	return NULL;
}

 /****************************************************************************
 * API functions
 ****************************************************************************/
/**\brief 创建CC
 * \param [in] para 创建参数
 * \param [out] handle 返回句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
AM_ErrorCode_t AM_CC_Create(AM_CC_CreatePara_t *para, AM_CC_Handle_t *handle)
{
	AM_CC_Decoder_t *cc;
	
	if (para == NULL || handle == NULL)
		return AM_CC_ERR_INVALID_PARAM;
	
	cc = (AM_CC_Decoder_t*)malloc(sizeof(AM_CC_Decoder_t));
	if (cc == NULL)
		return AM_CC_ERR_NO_MEM;
	
	memset(cc, 0, sizeof(AM_CC_Decoder_t));
		
	/* init the tv cc decoder */
	tvcc_init(&cc->decoder);
	if (cc->decoder.vbi == NULL)
		return AM_CC_ERR_LIBZVBI;
	
	vbi_event_handler_register(cc->decoder.vbi, VBI_EVENT_CAPTION, am_cc_vbi_event_handler, cc);
	
	pthread_mutex_init(&cc->lock, NULL);
	pthread_cond_init(&cc->cond, NULL);

	cc->cpara = *para;
	
	*handle = cc;

	return AM_SUCCESS;
}

/**\brief 销毁CC
 * \param [out] handle CC句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
AM_ErrorCode_t AM_CC_Destroy(AM_CC_Handle_t handle)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)handle;

	if (cc == NULL)
		return AM_CC_ERR_INVALID_PARAM;

	AM_CC_Stop(handle);

	tvcc_destroy(&cc->decoder);
	
	pthread_mutex_destroy(&cc->lock);
	pthread_cond_destroy(&cc->cond);

	free(cc);

	return AM_SUCCESS;
}
 
/**\brief 开始CC数据接收处理
 * \param handle CC handle
  * \param [in] para 启动参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
AM_ErrorCode_t AM_CC_Start(AM_CC_Handle_t handle, AM_CC_StartPara_t *para)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)handle;
	int rc, ret = AM_SUCCESS;
	
	if (cc == NULL || para == NULL)
		return AM_CC_ERR_INVALID_PARAM;
	
	pthread_mutex_lock(&cc->lock);
	if (cc->running)
	{
		ret = AM_CC_ERR_BUSY;
		goto start_done;
	}

	if (para->caption <= AM_CC_CAPTION_DEFAULT ||
		para->caption >= AM_CC_CAPTION_MAX)
		para->caption = AM_CC_CAPTION_CC1;

	cc->evt = -1;
	cc->spara = *para;
	cc->vbi_pgno = para->caption;
	cc->running = AM_TRUE;
	
	/* start the rendering thread */
	rc = pthread_create(&cc->render_thread, NULL, am_cc_render_thread, (void*)cc);
	if (rc)
	{
		cc->running = AM_FALSE;
		AM_DEBUG(0, "%s:%s", __func__, strerror(rc));
		ret = AM_CC_ERR_SYS;
	}
	else
	{
		/* start the data source thread */
		rc = pthread_create(&cc->data_thread, NULL, am_cc_data_thread, (void*)cc);
		if (rc)
		{
			cc->running = AM_FALSE;
			pthread_join(cc->render_thread, NULL);
			AM_DEBUG(0, "%s:%s", __func__, strerror(rc));
			ret = AM_CC_ERR_SYS;
		}
	}

start_done:	
	pthread_mutex_unlock(&cc->lock);
	return ret;	
}

/**\brief 停止CC处理
 * \param handle CC handle
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
AM_ErrorCode_t AM_CC_Stop(AM_CC_Handle_t handle)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)handle;
	int ret = AM_SUCCESS;
	AM_Bool_t join = AM_FALSE;

	if (cc == NULL)
		return AM_CC_ERR_INVALID_PARAM;
	
	pthread_mutex_lock(&cc->lock);
	if (cc->running)
	{
		cc->running = AM_FALSE;
		join = AM_TRUE;
	}
	pthread_mutex_unlock(&cc->lock);

	pthread_cond_broadcast(&cc->cond);
	
	if (join)
	{
		pthread_join(cc->data_thread, NULL);	
		pthread_join(cc->render_thread, NULL);
	}
	
	return ret;	
}

/**\brief 设置CC用户选项，用户选项可以覆盖运营商的设置,这些options由应用保存管理
 * \param handle CC handle
 * \param [in] options 选项集
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
AM_ErrorCode_t AM_CC_SetUserOptions(AM_CC_Handle_t handle, AM_CC_UserOptions_t *options)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)handle;
	
	if (cc == NULL)
		return AM_CC_ERR_INVALID_PARAM;
	
	pthread_mutex_lock(&cc->lock);
	
	cc->spara.user_options = *options;
	cc->evt = AM_CC_EVT_SET_USER_OPTIONS;
		
	pthread_mutex_unlock(&cc->lock);
	pthread_cond_signal(&cc->cond);
	
	return AM_SUCCESS;
}

/**\brief 获取用户数据
 * \param handle CC 句柄
 * \return [out] 用户数据
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
void *AM_CC_GetUserData(AM_CC_Handle_t handle)
{
	AM_CC_Decoder_t *cc = (AM_CC_Decoder_t*)handle;

	if (cc == NULL)
		return NULL;

	return cc->cpara.user_data;
}




