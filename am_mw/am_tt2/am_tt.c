#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#define AM_DEBUG_LEVEL 1

#include <am_tt2.h>
#include <am_debug.h>
#include <am_util.h>
#include <am_misc.h>
#include <libzvbi.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <am_time.h>

#define AM_TT2_MAX_SLICES (32)
#define AM_TT2_MAX_CACHED_PAGES (200)

typedef struct AM_TT2_CachedPage_s
{
	int			count;
	vbi_page	page;
	uint64_t	pts;
	struct AM_TT2_CachedPage_s *next;
}AM_TT2_CachedPage_t;

typedef struct
{
	vbi_decoder       *dec;
	vbi_search        *search;
	AM_TT2_Para_t      para;
	int                page_no;
	int                sub_page_no;
	AM_Bool_t          disp_update;
	AM_Bool_t          running;
	uint64_t           pts;
	pthread_mutex_t    lock;
	pthread_cond_t     cond;
	pthread_t          thread;
	AM_TT2_CachedPage_t *cached_pages;
	AM_TT2_CachedPage_t *cached_tail;
	AM_TT2_CachedPage_t *display_page;
}AM_TT2_Parser_t;

enum systems {
	SYSTEM_525 = 0,
	SYSTEM_625
};

static uint64_t tt2_get_pts(const char *pts_file, int base)
{
	char buf[32];
	AM_ErrorCode_t ret;
	uint32_t v;
	uint64_t r;
	
	ret=AM_FileRead(pts_file, buf, sizeof(buf));
	if(!ret){
		v = strtoul(buf, 0, base);
		r = (uint64_t)v;
	}else{
		r = 0LL;
	}

	return r;
}

static void tt2_add_cached_page(AM_TT2_Parser_t *parser, vbi_page *vp)
{
	AM_TT2_CachedPage_t *tmp;
	
	if (parser->cached_pages && parser->cached_tail)
	{
		if ((parser->cached_tail->count-parser->cached_pages->count) >= AM_TT2_MAX_CACHED_PAGES)
		{
			AM_DEBUG(1, "Reach max teletext cached display pages !!");
			return;
		}
	}
	
	tmp = (AM_TT2_CachedPage_t *)malloc(sizeof(AM_TT2_CachedPage_t));
	if (tmp == NULL)
	{
		AM_DEBUG(1, "Cannot alloc memory for new cached page");\
		return;
	}
	memset(tmp, 0, sizeof(AM_TT2_CachedPage_t));
	if (parser->cached_tail == NULL)
	{
		parser->cached_pages = tmp;
		parser->cached_tail = tmp;
		tmp->count = 0;
	}
	else
	{
		parser->cached_tail->next = tmp;
		tmp->count = parser->cached_tail->count + 1;
		parser->cached_tail = tmp;
	}
	tmp->page = *vp;
	tmp->pts = tt2_get_pts("/sys/class/stb/video_pts", 10);
	AM_DEBUG(1, "Cache page, pts 0x%llx, total cache %d pages", tmp->pts,
		parser->cached_tail->count - parser->cached_pages->count);
}

static void tt2_step_cached_page(AM_TT2_Parser_t *parser)
{
	if (parser->cached_pages != NULL)
	{
		AM_TT2_CachedPage_t *tmp = parser->cached_pages;
		parser->cached_pages = tmp->next;
		vbi_unref_page(&tmp->page);
		free(tmp);
	}
	
	if (parser->cached_pages == NULL)
		parser->cached_tail = NULL;
}

static void tt_lofp_to_line(unsigned int *field, unsigned int *field_line, unsigned int *frame_line, unsigned int lofp, enum systems system)
{
	unsigned int line_offset;

	*field = !(lofp & (1 << 5));
	line_offset = lofp & 31;

	if(line_offset > 0)
	{
		static const unsigned int field_start [2][2] = {
			{ 0, 263 },
			{ 0, 313 },
		};
		*field_line = line_offset;
		*frame_line = field_start[system][*field] + line_offset;
	}
	else
	{
		*field_line = 0;
		*frame_line = 0;
	}
}

static void tt2_check(AM_TT2_Parser_t *parser)
{
	if (parser->cached_pages != NULL)
	{
		if (vbi_bcd2dec(parser->cached_pages->page.pgno) != parser->page_no)
		{
			AM_DEBUG(1, "expired page no:%d, current %d", 
				vbi_bcd2dec(parser->cached_pages->page.pgno), parser->page_no);
			/* step to next cached page */
			tt2_step_cached_page(parser);
		}
		else if (parser->cached_pages->pts <= tt2_get_pts("/sys/class/tsync/pts_pcrscr", 16))
		{	
			AM_DEBUG(1, "show ttx page, remain cached %d pages", 
				parser->cached_tail->count - parser->cached_pages->count);

			if(parser->para.draw_begin)
				parser->para.draw_begin(parser);
	
			/*{
				char buf[256];
				int i, j;

				for(i=0; i<page.rows; i++){
					char *ptr = buf;
					for(j=0; j<page.columns; j++){
						sprintf(ptr, "%02x", page.text[i*page.columns+j].unicode);
						ptr += 2;
					}

					AM_DEBUG(1, "text %02d: %s", i, buf);
				}
			}*/

			vbi_draw_vt_page_region(&parser->cached_pages->page, 
					VBI_PIXFMT_RGBA32_LE, parser->para.bitmap, parser->para.pitch,
					0, 0, parser->cached_pages->page.columns, 
					parser->cached_pages->page.rows, 1, 1, parser->para.is_subtitle);

			if(parser->para.draw_end)
				parser->para.draw_end(parser);

			parser->display_page = parser->cached_pages;
			
			/* step to next cached page */
			tt2_step_cached_page(parser);
		}
		else
		{
			AM_DEBUG(2, "pts 0x%llx, pcrscr %llx, diff %lld", parser->cached_pages->pts,
				tt2_get_pts("/sys/class/tsync/pts_pcrscr", 16), 
				parser->cached_pages->pts - tt2_get_pts("/sys/class/tsync/pts_pcrscr", 16));
		}
	}
	else if (parser->display_page != NULL)
	{
		uint64_t pts = tt2_get_pts("/sys/class/tsync/pts_pcrscr", 16);
		
		if (pts == 0llu)
		{
			AM_DEBUG(1, "Teletext clear screen.");
			/*clear the screen*/
			if(parser->para.draw_begin)
				parser->para.draw_begin(parser);
			if(parser->para.draw_end)
				parser->para.draw_end(parser);

			parser->display_page = NULL;
		}
	}
}

static void tt2_show(AM_TT2_Parser_t *parser)
{
	vbi_page page;
	AM_Bool_t cached;

	cached = vbi_fetch_vt_page(parser->dec, &page, vbi_dec2bcd(parser->page_no), parser->sub_page_no, VBI_WST_LEVEL_3p5, 25, AM_TRUE);
	if(!cached)
		return;
	
	parser->sub_page_no      = page.subno;

	tt2_add_cached_page(parser, &page);
}

static void* tt2_thread(void *arg)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)arg;

	pthread_mutex_lock(&parser->lock);

	while(parser->running)
	{
		if (parser->running)
		{
			struct timespec ts;
			int timeout = 20;

			AM_TIME_GetTimeSpecTimeout(timeout, &ts);
			pthread_cond_timedwait(&parser->cond, &parser->lock, &ts);
		}

		if(parser->disp_update){
			tt2_show(parser);
			parser->disp_update = AM_FALSE;
		}
		
		tt2_check(parser);
	}

	pthread_mutex_unlock(&parser->lock);

	return NULL;
}

static void tt2_event_handler(vbi_event *ev, void *user_data)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)user_data;
	int pgno, subno;

	if(ev->type != VBI_EVENT_TTX_PAGE)
		return;
	
	pgno  = vbi_bcd2dec(ev->ev.ttx_page.pgno);
	subno = ev->ev.ttx_page.subno;
		
	AM_DEBUG(2, "TT event handler: pgno %d, subno %d, parser->page_no %d, parser->sub_page_no %d",
		pgno, subno, parser->page_no, parser->sub_page_no	);
	AM_DEBUG(2, "header_update %d, clock_update %d, roll_header %d", 
		ev->ev.ttx_page.header_update, ev->ev.ttx_page.clock_update, ev->ev.ttx_page.roll_header);
	if(ev->ev.ttx_page.clock_update || ((pgno==parser->page_no) && (parser->sub_page_no==AM_TT2_ANY_SUBNO || parser->sub_page_no==subno)))
	{
		parser->disp_update = AM_TRUE;
		pthread_cond_signal(&parser->cond);
	}
}

/**\brief 创建teletext解析句柄
 * \param[out] handle 返回创建的新句柄
 * \param[in] para teletext解析参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Create(AM_TT2_Handle_t *handle, AM_TT2_Para_t *para)
{
	AM_TT2_Parser_t *parser;

	if(!para || !handle)
	{
		return AM_TT2_ERR_INVALID_PARAM;
	}

	parser = (AM_TT2_Parser_t*)malloc(sizeof(AM_TT2_Parser_t));
	if(!parser)
	{
		return AM_TT2_ERR_NO_MEM;
	}

	memset(parser, 0, sizeof(AM_TT2_Parser_t));

	parser->dec = vbi_decoder_new();
	if(!parser->dec)
	{
		free(parser);
		return AM_TT2_ERR_CREATE_DECODE;
	}
	
	/* Set teletext default region, See libzvbi/src/lang.c */
	vbi_teletext_set_default_region(parser->dec, para->default_region);
	
	vbi_event_handler_register(parser->dec, VBI_EVENT_TTX_PAGE, tt2_event_handler, parser);

	pthread_mutex_init(&parser->lock, NULL);
	pthread_cond_init(&parser->cond, NULL);

	parser->page_no = 100;
	parser->sub_page_no = AM_TT2_ANY_SUBNO;
	parser->para    = *para;

	*handle = parser;

	return AM_SUCCESS;
}

/**\brief 释放teletext解析句柄
 * \param handle 要释放的句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Destroy(AM_TT2_Handle_t handle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	AM_TT2_Stop(handle);
	
	/* Free all cached pages */
	while (parser->cached_pages != NULL)
	{
		tt2_step_cached_page(parser);
	}

	pthread_cond_destroy(&parser->cond);
	pthread_mutex_destroy(&parser->lock);

	if(parser->search)
	{
		vbi_search_delete(parser->search);
	}

	if(parser->dec)
	{
		vbi_decoder_delete(parser->dec);
	}

	free(parser);

	return AM_SUCCESS;
}

/**\brief 设定是否为字幕
 * \param handle 要释放的句柄
 * \param subtitle 是否为字幕
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t
AM_TT2_SetSubtitleMode(AM_TT2_Handle_t handle, AM_Bool_t subtitle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	parser->para.is_subtitle = subtitle;
	parser->disp_update = AM_TRUE;

	pthread_mutex_unlock(&parser->lock);

	pthread_cond_signal(&parser->cond);


	return AM_SUCCESS;
}


/**\brief 取得用户定义数据
 * \param handle 句柄
 * \return 用户定义数据
 */
void* AM_TT2_GetUserData(AM_TT2_Handle_t handle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return NULL;
	}

	return parser->para.user_data;
}

/**\brief 分析teletext数据
 * \param handle 句柄
 * \param[in] buf PES数据缓冲区
 * \param size 缓冲区内数据大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Decode(AM_TT2_Handle_t handle, uint8_t *buf, int size)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	int scrambling_control;
	int pts_dts_flag;
	int pes_header_length;
	uint64_t pts = 0ll;
	vbi_sliced sliced[AM_TT2_MAX_SLICES];
	vbi_sliced *s = sliced;
	int s_count = 0;
	int packet_head;
	uint8_t *ptr;
	int left;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	if(size < 9)
		goto end;
	
	scrambling_control = (buf[6] >> 4) & 0x03;
	pts_dts_flag = (buf[7] >> 6) & 0x03;
	pes_header_length = buf[8];
	if(((pts_dts_flag == 2) || (pts_dts_flag == 3)) && (size > 13))
	{
		pts |= (uint64_t)((buf[9] >> 1) & 0x07) << 30;
		pts |= (uint64_t)((((buf[10] << 8) | buf[11]) >> 1) & 0x7fff) << 15;
		pts |= (uint64_t)((((buf[12] << 8) | buf[13]) >> 1) & 0x7fff);
	}

	parser->pts = pts;

	packet_head = buf[8] + 9;
	ptr  = buf + packet_head + 1;
	left = size - packet_head - 1;

	pthread_mutex_lock(&parser->lock);

	while(left >= 2)
	{
		unsigned int field;
		unsigned int field_line;
		unsigned int frame_line;
		int data_unit_length;
		int data_unit_id;
		int i;
		
		data_unit_id = ptr[0];
		data_unit_length = ptr[1];
		if((data_unit_id != 0x02) && (data_unit_id != 0x03))
			goto next_packet;
		if(data_unit_length > left)
			break;
		if(data_unit_length < 44)
			goto next_packet;
		if(ptr[3] != 0xE4)
			goto next_packet;

		tt_lofp_to_line(&field, &field_line, &frame_line, ptr[2], SYSTEM_625);
		if(0 != frame_line)
		{
			s->line = frame_line;
		}
		else
		{
			s->line = 0;
		}

		s->id = VBI_SLICED_TELETEXT_B;
		for (i = 0; i < 42; ++i)
			s->data[i] = vbi_rev8 (ptr[4 + i]);
		
		s++;
		s_count++;

		if(s_count == AM_TT2_MAX_SLICES)
		{
			vbi_decode(parser->dec, sliced, s_count, pts/90000.);
			s = sliced;
			s_count = 0;
		}
next_packet:
		ptr += data_unit_length + 2;
		left -= data_unit_length + 2;
	}

	if(s_count)
	{
		vbi_decode(parser->dec, sliced, s_count, pts/90000.);
	}

	pthread_mutex_unlock(&parser->lock);

end:
	return AM_SUCCESS;
}

/**\brief 开始teletext显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Start(AM_TT2_Handle_t handle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	if(!parser->running)
	{
		parser->running = AM_TRUE;
		if(pthread_create(&parser->thread, NULL, tt2_thread, parser))
		{
			parser->running = AM_FALSE;
			ret = AM_TT2_ERR_CANNOT_CREATE_THREAD;
		}
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

/**\brief 停止teletext显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Stop(AM_TT2_Handle_t handle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	pthread_t th;
	AM_Bool_t wait = AM_FALSE;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	if(parser->running)
	{
		parser->running = AM_FALSE;
		wait = AM_TRUE;
		th = parser->thread;
	}

	pthread_mutex_unlock(&parser->lock);
	pthread_cond_signal(&parser->cond);

	if(wait)
	{
		pthread_join(th, NULL);
	}

	return AM_SUCCESS;
}

/**\brief 跳转到指定页
 * \param handle 句柄
 * \param page_no 页号
 * \param sub_page_no 子页号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_GotoPage(AM_TT2_Handle_t handle, int page_no, int sub_page_no)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	if(page_no<100 || page_no>899)
	{
		return AM_TT2_ERR_INVALID_PARAM;
	}

	if(sub_page_no>0xFF && sub_page_no!=AM_TT2_ANY_SUBNO)
	{
		return AM_TT2_ERR_INVALID_PARAM;
	}

	pthread_mutex_lock(&parser->lock);

	parser->page_no = page_no;
	parser->sub_page_no = sub_page_no;
	parser->disp_update = AM_TRUE;

	pthread_mutex_unlock(&parser->lock);

	pthread_cond_signal(&parser->cond);

	return AM_SUCCESS;
}

/**\brief 跳转到home页
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_GoHome(AM_TT2_Handle_t handle)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	AM_Bool_t cached;
	vbi_page page;
	vbi_link link;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	cached = vbi_fetch_vt_page(parser->dec, &page, vbi_dec2bcd(parser->page_no), parser->sub_page_no, VBI_WST_LEVEL_3p5, 25, AM_TRUE);
	if(cached)
	{
		vbi_resolve_home(&page, &link);
		if(link.type == VBI_LINK_PAGE)
			parser->page_no = vbi_bcd2dec(link.pgno);
		else if(link.type == VBI_LINK_SUBPAGE)
			parser->sub_page_no = link.subno;

		vbi_unref_page(&page);

		parser->disp_update = AM_TRUE;
		pthread_cond_signal(&parser->cond);
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

/**\brief 跳转到下一页
 * \param handle 句柄
 * \param dir 搜索方向，+1为正向，-1为反向
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_NextPage(AM_TT2_Handle_t handle, int dir)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	int pgno, subno;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	pgno  = vbi_dec2bcd(parser->page_no);
	subno = parser->sub_page_no;

	if(vbi_get_next_pgno(parser->dec, dir, &pgno, &subno))
	{
		parser->page_no = vbi_bcd2dec(pgno);
		parser->sub_page_no = subno;

		parser->disp_update = AM_TRUE;
		pthread_cond_signal(&parser->cond);
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

/**\brief 根据颜色跳转到指定链接
 * \param handle 句柄
 * \param color 颜色
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_ColorLink(AM_TT2_Handle_t handle, AM_TT2_Color_t color)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;
	AM_Bool_t cached;
	vbi_page page;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	if(color>=4)
	{
		return AM_TT2_ERR_INVALID_PARAM;
	}

	pthread_mutex_lock(&parser->lock);

	cached = vbi_fetch_vt_page(parser->dec, &page, vbi_dec2bcd(parser->page_no), parser->sub_page_no, VBI_WST_LEVEL_3p5, 25, AM_TRUE);
	if(cached)
	{
		parser->page_no = vbi_bcd2dec(page.nav_link[color].pgno);
		parser->sub_page_no = page.nav_link[color].subno;
		vbi_unref_page(&page);

		parser->disp_update = AM_TRUE;
		pthread_cond_signal(&parser->cond);
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;

}

/**\brief 设定搜索字符串
 * \param handle 句柄
 * \param pattern 搜索字符串
 * \param casefold 是否区分大小写
 * \param regex 是否用正则表达式匹配
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_SetSearchPattern(AM_TT2_Handle_t handle, const char *pattern, AM_Bool_t casefold, AM_Bool_t regex)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	if(parser->search)
	{
		vbi_search_delete(parser->search);
		parser->search = NULL;
	}

	parser->search = vbi_search_new(parser->dec, vbi_dec2bcd(parser->page_no), parser->sub_page_no,
			(uint16_t*)pattern, casefold, regex, NULL);

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

/**\brief 搜索指定页
 * \param handle 句柄
 * \param dir 搜索方向，+1为正向，-1为反向
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
AM_ErrorCode_t AM_TT2_Search(AM_TT2_Handle_t handle, int dir)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)handle;

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

	if(parser->search)
	{
		vbi_page *page;
		int status;

		status = vbi_search_next(parser->search, &page, dir);
		if(status == VBI_SEARCH_SUCCESS){
			parser->page_no = vbi_bcd2dec(page->pgno);
			parser->sub_page_no = page->subno;

			parser->disp_update = AM_TRUE;
			pthread_cond_signal(&parser->cond);
		}
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

