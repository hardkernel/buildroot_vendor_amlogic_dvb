#include <am_tt2.h>
#include <am_debug.h>
#include <am_util.h>
#include <am_misc.h>
#include <libzvbi.h>
#include <pthread.h>
#include <unistd.h>

#define AM_TT2_MAX_SLICES (32)

typedef struct
{
	vbi_decoder       *dec;
	vbi_search        *search;
	AM_TT2_Para_t      para;
	int                page_no;
	int                sub_page_no;
	int                disp_page_no;
	int                disp_sub_page_no;
	AM_Bool_t          disp_update;
	AM_Bool_t          running;
	uint64_t           pts;
	pthread_mutex_t    lock;
	pthread_t          thread;
}AM_TT2_Parser_t;

enum systems {
	SYSTEM_525 = 0,
	SYSTEM_625
};

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

static void tt2_show(AM_TT2_Parser_t *parser)
{
	vbi_page page;
	AM_Bool_t cached;

	cached = vbi_fetch_vt_page(parser->dec, &page, vbi_dec2bcd(parser->page_no), parser->sub_page_no, VBI_WST_LEVEL_3p5, 25, AM_TRUE);
	if(!cached)
		return;
	
	parser->disp_page_no     = vbi_bcd2dec(page.pgno);
	parser->disp_sub_page_no = page.subno;
	parser->disp_update      = AM_FALSE;
	parser->sub_page_no      = page.subno;
	
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
	
	vbi_draw_vt_page_region(&page, VBI_PIXFMT_RGBA32_LE, parser->para.bitmap, parser->para.pitch, 0, 0, page.columns, page.rows, 1, 1);

	if(parser->para.draw_end)
		parser->para.draw_end(parser);
	
	vbi_unref_page(&page);

	AM_DEBUG(1, "draw page %d %d", parser->disp_page_no, parser->disp_sub_page_no);
}

static void* tt2_thread(void *arg)
{
	AM_TT2_Parser_t *parser = (AM_TT2_Parser_t*)arg;

	pthread_mutex_lock(&parser->lock);

	while(parser->running)
	{
		if((parser->disp_page_no != parser->page_no) || (parser->disp_sub_page_no != parser->sub_page_no) || parser->disp_update){
			tt2_show(parser);
		}

		pthread_mutex_unlock(&parser->lock);
		usleep(20000);
		pthread_mutex_lock(&parser->lock);
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

	if((pgno==parser->page_no) && (parser->sub_page_no==AM_TT2_ANY_SUBNO || parser->sub_page_no==subno))
	{
		AM_DEBUG(1, "update %d %d", pgno, subno);
		parser->disp_update = AM_TRUE;
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

	vbi_event_handler_register(parser->dec, VBI_EVENT_TTX_PAGE, tt2_event_handler, parser);

	pthread_mutex_init(&parser->lock, NULL);

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
			return AM_TT2_ERR_CANNOT_CREATE_THREAD;
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

	pthread_mutex_unlock(&parser->lock);

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

	if(!parser)
	{
		return AM_TT2_ERR_INVALID_HANDLE;
	}

	pthread_mutex_lock(&parser->lock);

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
		}
	}

	pthread_mutex_unlock(&parser->lock);

	return AM_SUCCESS;
}

