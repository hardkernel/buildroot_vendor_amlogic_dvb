/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_epg.c
 * \brief 表监控模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-11-04: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 3

#include <errno.h>
#include <time.h>
#include <am_debug.h>
#include <assert.h>
#include <am_epg.h>
#include "am_epg_internal.h"
#include <am_time.h>
#include <am_dmx.h>
#include <am_iconv.h>
#include <am_av.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/*是否使用TDT时间*/
#define USE_TDT_TIME

/*子表最大个数*/
#define MAX_EIT4E_SUBTABLE_CNT 500
#define MAX_EIT_SUBTABLE_CNT 1000

/*多子表时接收重复最小间隔*/
#define EIT4E_REPEAT_DISTANCE 3000
#define EIT4F_REPEAT_DISTANCE 5000
#define EIT50_REPEAT_DISTANCE 10000
#define EIT51_REPEAT_DISTANCE 10000
#define EIT60_REPEAT_DISTANCE 20000
#define EIT61_REPEAT_DISTANCE 20000

/*EIT 数据定时通知时间间隔*/
#define NEW_EIT_CHECK_DISTANCE 4000

/*EIT 自动更新间隔*/
#define EITPF_CHECK_DISTANCE (60*1000)
#define EITSCHE_CHECK_DISTANCE (3*3600*1000)

/*TDT 自动更新间隔*/
#define TDT_CHECK_DISTANCE 1000*3600

/*STT 自动更新间隔*/
#define STT_CHECK_DISTANCE 1000*3600

/*预约播放检查间隔*/
#define EPG_SUB_CHECK_TIME (10*1000)
/*预约播放提前通知时间*/
#define EPG_PRE_NOTIFY_TIME (60*1000)

/*输入字符默认编码*/
#define FORCE_DEFAULT_CODE ""/*"GB2312"*/

 /*位操作*/
#define BIT_MASK(b) (1 << ((b) % 8))
#define BIT_SLOT(b) ((b) / 8)
#define BIT_SET(a, b) ((a)[BIT_SLOT(b)] |= BIT_MASK(b))
#define BIT_CLEAR(a, b) ((a)[BIT_SLOT(b)] &= ~BIT_MASK(b))
#define BIT_TEST(a, b) ((a)[BIT_SLOT(b)] & BIT_MASK(b))
#define BIT_MASK_EX(b) (0xff >> (7 - ((b) % 8)))
#define BIT_CLEAR_EX(a, b) ((a)[BIT_SLOT(b)] &= BIT_MASK_EX(b))

/*Use the following macros to fix type disagree*/
#define dvbpsi_stt_t stt_section_info_t
#define dvbpsi_mgt_t mgt_section_info_t
#define dvbpsi_psip_eit_t eit_section_info_t
#define dvbpsi_rrt_t rrt_section_info_t

/*清除一个subtable控制数据*/
#define SUBCTL_CLEAR(sc)\
	AM_MACRO_BEGIN\
		memset((sc)->mask, 0, sizeof((sc)->mask));\
		(sc)->ver = 0xff;\
	AM_MACRO_END
	
/*添加数据到列表中*/
#define ADD_TO_LIST(_t, _l)\
	AM_MACRO_BEGIN\
		if ((_l) == NULL){\
			(_l) = (_t);\
		}else{\
			(_t)->p_next = (_l)->p_next;\
			(_l)->p_next = (_t);\
		}\
	AM_MACRO_END
	
/*释放一个表的所有SI数据*/
#define RELEASE_TABLE_FROM_LIST(_t, _l)\
	AM_MACRO_BEGIN\
		_t *tmp, *next;\
		tmp = (_l);\
		while (tmp){\
			next = tmp->p_next;\
			AM_SI_ReleaseSection(mon->hsi, tmp->i_table_id, (void*)tmp);\
			tmp = next;\
		}\
		(_l) = NULL;\
	AM_MACRO_END

/*解析section并添加到列表*/
#define COLLECT_SECTION(type, list)\
	AM_MACRO_BEGIN\
		type *p_table;\
		if (AM_SI_DecodeSection(mon->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_table) == AM_SUCCESS)\
		{\
			p_table->p_next = NULL;\
			ADD_TO_LIST(p_table, list); /*添加到搜索结果列表中*/\
			am_epg_tablectl_mark_section(sec_ctrl, &header); /*设置为已接收*/\
		}\
	AM_MACRO_END



/*判断并设置某个表的监控*/
#define SET_MODE(table, ctl, f, reset)\
	AM_MACRO_BEGIN\
		if ((mon->mode & (f)) && (mon->ctl.fid == -1) && !reset)\
		{/*开启监控*/\
			am_epg_request_section(mon, &mon->ctl);\
		}\
		else if (!(mon->mode & (f)) && (mon->ctl.fid != -1))\
		{/*关闭监控*/\
			am_epg_free_filter(mon, &mon->ctl.fid);\
			RELEASE_TABLE_FROM_LIST(dvbpsi_##table##_t, mon->table##s);\
		}\
		else if ((mon->mode & (f)) && reset)\
		{\
			am_epg_free_filter(mon, &mon->ctl.fid);\
			RELEASE_TABLE_FROM_LIST(dvbpsi_##table##_t, mon->table##s);\
			am_epg_tablectl_clear(&mon->ctl);\
			am_epg_request_section(mon, &mon->ctl);\
		}\
	AM_MACRO_END
	
/*表收齐操作*/
#define TABLE_DONE()\
	AM_MACRO_BEGIN\
		if (! (mon->evt_flag & sec_ctrl->evt_flag))\
		{\
			AM_DEBUG(1, "%s Done!", sec_ctrl->tname);\
			mon->evt_flag |= sec_ctrl->evt_flag;\
			pthread_cond_signal(&mon->cond);\
		}\
	AM_MACRO_END

/*通知一个事件*/
#define SIGNAL_EVENT(e, d)\
	AM_MACRO_BEGIN\
		pthread_mutex_unlock(&mon->lock);\
		AM_EVT_Signal((int)mon, e, d);\
		pthread_mutex_lock(&mon->lock);\
	AM_MACRO_END
	
/****************************************************************************
 * Static data
 ***************************************************************************/

#ifdef USE_TDT_TIME
/*当前时间管理*/
static AM_EPG_Time_t curr_time = {0, 0, 0};

/*当前时间锁*/
static pthread_mutex_t time_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

/****************************************************************************
 * Static functions
 ***************************************************************************/
static int am_epg_get_current_service_id(AM_EPG_Monitor_t *mon);

/**\brief 将ISO6937转换为UTF-8编码*/
static AM_ErrorCode_t am_epg_convert_iso6937_to_utf8(const char *src, int src_len, char *dest, int *dest_len)
{     
	uint16_t ch;
	int dlen, i;
	uint8_t b;
	char *ucs2 = NULL;

#define READ_BYTE()\
	({\
		uint8_t ret;\
		if (i >= src_len) ret=0;\
		else ret = (uint8_t)src[i];\
		i++;\
		ret;\
	})
	
	if (!src || !dest || !dest_len || src_len <= 0)
		return -1;
		
	/* first covert to UCS-2, then iconv to utf8 */
	ucs2 = (char *)malloc(src_len*2);
	if (!ucs2)
		return -1;
	dlen = 0;
	i=0;
	b = READ_BYTE();
	if (b < 0x20)
	{
		/* ISO 6937 encoding must start with character between 0x20 and 0xFF
		 otherwise it is dfferent encoding table
		 for example 0x05 means encoding table 8859-9 */
		return -1;
	}
	
	while (b != 0)
	{
		ch = 0x00;
		switch (b)
		{
			/* at first single byte characters */
			case 0xA8: ch = 0x00A4; break;
			case 0xA9: ch = 0x2018; break;
			case 0xAA: ch = 0x201C; break;
			case 0xAC: ch = 0x2190; break;
			case 0xAD: ch = 0x2191; break;
			case 0xAE: ch = 0x2192; break;
			case 0xAF: ch = 0x2193; break;
			case 0xB4: ch = 0x00D7; break;
			case 0xB8: ch = 0x00F7; break;
			case 0xB9: ch = 0x2019; break;
			case 0xBA: ch = 0x201D; break;
			case 0xD0: ch = 0x2014; break;
			case 0xD1: ch = 0xB9; break;
			case 0xD2: ch = 0xAE; break;
			case 0xD3: ch = 0xA9; break;
			case 0xD4: ch = 0x2122; break;
			case 0xD5: ch = 0x266A; break;
			case 0xD6: ch = 0xAC; break;
			case 0xD7: ch = 0xA6; break;
			case 0xDC: ch = 0x215B; break;
			case 0xDD: ch = 0x215C; break;
			case 0xDE: ch = 0x215D; break;
			case 0xDF: ch = 0x215E; break;
			case 0xE0: ch = 0x2126; break;
			case 0xE1: ch = 0xC6; break;
			case 0xE2: ch = 0xD0; break;
			case 0xE3: ch = 0xAA; break;
			case 0xE4: ch = 0x126; break;
			case 0xE6: ch = 0x132; break;
			case 0xE7: ch = 0x013F; break;
			case 0xE8: ch = 0x141; break;
			case 0xE9: ch = 0xD8; break;
			case 0xEA: ch = 0x152; break;
			case 0xEB: ch = 0xBA; break;
			case 0xEC: ch = 0xDE; break;
			case 0xED: ch = 0x166; break;
			case 0xEE: ch = 0x014A; break;
			case 0xEF: ch = 0x149; break;
			case 0xF0: ch = 0x138; break;
			case 0xF1: ch = 0xE6; break;
			case 0xF2: ch = 0x111; break;
			case 0xF3: ch = 0xF0; break;
			case 0xF4: ch = 0x127; break;
			case 0xF5: ch = 0x131; break;
			case 0xF6: ch = 0x133; break;
			case 0xF7: ch = 0x140; break;
			case 0xF8: ch = 0x142; break;
			case 0xF9: ch = 0xF8; break;
			case 0xFA: ch = 0x153; break;
			case 0xFB: ch = 0xDF; break;
			case 0xFC: ch = 0xFE; break;
			case 0xFD: ch = 0x167; break;
			case 0xFE: ch = 0x014B; break;
			case 0xFF: ch = 0xAD; break;
			/* multibyte */
			case 0xC1:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC0; break;
					case 0x45: ch = 0xC8; break;
					case 0x49: ch = 0xCC; break;
					case 0x4F: ch = 0xD2; break;
					case 0x55: ch = 0xD9; break;
					case 0x61: ch = 0xE0; break;
					case 0x65: ch = 0xE8; break;
					case 0x69: ch = 0xEC; break;
					case 0x6F: ch = 0xF2; break;
					case 0x75: ch = 0xF9; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC2:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xB4; break;
					case 0x41: ch = 0xC1; break;
					case 0x43: ch = 0x106; break;
					case 0x45: ch = 0xC9; break;
					case 0x49: ch = 0xCD; break;
					case 0x4C: ch = 0x139; break;
					case 0x4E: ch = 0x143; break;
					case 0x4F: ch = 0xD3; break;
					case 0x52: ch = 0x154; break;
					case 0x53: ch = 0x015A; break;
					case 0x55: ch = 0xDA; break;
					case 0x59: ch = 0xDD; break;
					case 0x5A: ch = 0x179; break;
					case 0x61: ch = 0xE1; break;
					case 0x63: ch = 0x107; break;
					case 0x65: ch = 0xE9; break;
					case 0x69: ch = 0xED; break;
					case 0x6C: ch = 0x013A; break;
					case 0x6E: ch = 0x144; break;
					case 0x6F: ch = 0xF3; break;
					case 0x72: ch = 0x155; break;
					case 0x73: ch = 0x015B; break;
					case 0x75: ch = 0xFA; break;
					case 0x79: ch = 0xFD; break;
					case 0x7A: ch = 0x017A; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;

			case 0xC3:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC2; break;
					case 0x43: ch = 0x108; break;
					case 0x45: ch = 0xCA; break;
					case 0x47: ch = 0x011C; break;
					case 0x48: ch = 0x124; break;
					case 0x49: ch = 0xCE; break;
					case 0x4A: ch = 0x134; break;
					case 0x4F: ch = 0xD4; break;
					case 0x53: ch = 0x015C; break;
					case 0x55: ch = 0xDB; break;
					case 0x57: ch = 0x174; break;
					case 0x59: ch = 0x176; break;
					case 0x61: ch = 0xE2; break;
					case 0x63: ch = 0x109; break;
					case 0x65: ch = 0xEA; break;
					case 0x67: ch = 0x011D; break;
					case 0x68: ch = 0x125; break;
					case 0x69: ch = 0xEE; break;
					case 0x6A: ch = 0x135; break;
					case 0x6F: ch = 0xF4; break;
					case 0x73: ch = 0x015D; break;
					case 0x75: ch = 0xFB; break;
					case 0x77: ch = 0x175; break;
					case 0x79: ch = 0x177; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC4:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC3; break;
					case 0x49: ch = 0x128; break;
					case 0x4E: ch = 0xD1; break;
					case 0x4F: ch = 0xD5; break;
					case 0x55: ch = 0x168; break;
					case 0x61: ch = 0xE3; break;
					case 0x69: ch = 0x129; break;
					case 0x6E: ch = 0xF1; break;
					case 0x6F: ch = 0xF5; break;
					case 0x75: ch = 0x169; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC5:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xAF; break;
					case 0x41: ch = 0x100; break;
					case 0x45: ch = 0x112; break;
					case 0x49: ch = 0x012A; break;
					case 0x4F: ch = 0x014C; break;
					case 0x55: ch = 0x016A; break;
					case 0x61: ch = 0x101; break;
					case 0x65: ch = 0x113; break;
					case 0x69: ch = 0x012B; break;
					case 0x6F: ch = 0x014D; break;
					case 0x75: ch = 0x016B; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC6:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02D8; break;
					case 0x41: ch = 0x102; break;
					case 0x47: ch = 0x011E; break;
					case 0x55: ch = 0x016C; break;
					case 0x61: ch = 0x103; break;
					case 0x67: ch = 0x011F; break;
					case 0x75: ch = 0x016D; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC7:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02D9; break;
					case 0x43: ch = 0x010A; break;
					case 0x45: ch = 0x116; break;
					case 0x47: ch = 0x120; break;
					case 0x49: ch = 0x130; break;
					case 0x5A: ch = 0x017B; break;
					case 0x63: ch = 0x010B; break;
					case 0x65: ch = 0x117; break;
					case 0x67: ch = 0x121; break;
					case 0x7A: ch = 0x017C; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC8:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xA8; break;
					case 0x41: ch = 0xC4; break;
					case 0x45: ch = 0xCB; break;
					case 0x49: ch = 0xCF; break;
					case 0x4F: ch = 0xD6; break;
					case 0x55: ch = 0xDC; break;
					case 0x59: ch = 0x178; break;
					case 0x61: ch = 0xE4; break;
					case 0x65: ch = 0xEB; break;
					case 0x69: ch = 0xEF; break;
					case 0x6F: ch = 0xF6; break;
					case 0x75: ch = 0xFC; break;
					case 0x79: ch = 0xFF; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCA:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DA; break;
					case 0x41: ch = 0xC5; break;
					case 0x55: ch = 0x016E; break;
					case 0x61: ch = 0xE5; break;
					case 0x75: ch = 0x016F; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCB:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xB8; break;
					case 0x43: ch = 0xC7; break;
					case 0x47: ch = 0x122; break;
					case 0x4B: ch = 0x136; break;
					case 0x4C: ch = 0x013B; break;
					case 0x4E: ch = 0x145; break;
					case 0x52: ch = 0x156; break;
					case 0x53: ch = 0x015E; break;
					case 0x54: ch = 0x162; break;
					case 0x63: ch = 0xE7; break;
					case 0x67: ch = 0x123; break;
					case 0x6B: ch = 0x137; break;
					case 0x6C: ch = 0x013C; break;
					case 0x6E: ch = 0x146; break;
					case 0x72: ch = 0x157; break;
					case 0x73: ch = 0x015F; break;
					case 0x74: ch = 0x163; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCD:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DD; break;
					case 0x4F: ch = 0x150; break;
					case 0x55: ch = 0x170; break;
					case 0x6F: ch = 0x151; break;
					case 0x75: ch = 0x171; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCE:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DB; break;
					case 0x41: ch = 0x104; break;
					case 0x45: ch = 0x118; break;
					case 0x49: ch = 0x012E; break;
					case 0x55: ch = 0x172; break;
					case 0x61: ch = 0x105; break;
					case 0x65: ch = 0x119; break;
					case 0x69: ch = 0x012F; break;
					case 0x75: ch = 0x173; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCF:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02C7; break;
					case 0x43: ch = 0x010C; break;
					case 0x44: ch = 0x010E; break;
					case 0x45: ch = 0x011A; break;
					case 0x4C: ch = 0x013D; break;
					case 0x4E: ch = 0x147; break;
					case 0x52: ch = 0x158; break;
					case 0x53: ch = 0x160; break;
					case 0x54: ch = 0x164; break;
					case 0x5A: ch = 0x017D; break;
					case 0x63: ch = 0x010D; break;
					case 0x64: ch = 0x010F; break;
					case 0x65: ch = 0x011B; break;
					case 0x6C: ch = 0x013E; break;
					case 0x6E: ch = 0x148; break;
					case 0x72: ch = 0x159; break;
					case 0x73: ch = 0x161; break;
					case 0x74: ch = 0x165; break;
					case 0x7A: ch = 0x017E; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			/* rest is the same */
			default: ch = b; break;
		}
		if (b != 0)
		{
			b = READ_BYTE();
		}
		if (ch != 0)
		{
			/* dest buffer not enough, and this will not happen */
			if ((dlen+1) >= (src_len*2))
				goto iso6937_end;
			ucs2[dlen++] = (ch&0xff00) >> 8;
			ucs2[dlen++] = ch;
		}
	}
	
iso6937_end:
	if (dlen > 0)
	{
		iconv_t handle;
		char *org_ucs2 = ucs2;
		char **pin=&ucs2;
		char **pout=&dest;
		
		handle=iconv_open("utf-8","ucs-2");

		if (handle == (iconv_t)-1)
		{
			AM_DEBUG(1, "iconv_open err: %s",strerror(errno));
			return AM_FAILURE;
		}

		if(iconv(handle,pin,(size_t *)&dlen,pout,(size_t *)dest_len) == -1)
		{
		    AM_DEBUG(1, "iconv err: %s", strerror(errno));
		    iconv_close(handle);
		    return AM_FAILURE;
		}
		
		free(org_ucs2);

		return iconv_close(handle);
    }
    
    free(ucs2);
    
    return -1;
} 

static inline int am_epg_convert_fetype_to_source(int fe_type)
{
#if 0
	switch(fe_type)
	{
		case FE_QAM:	return AM_FEND_DEMOD_DVBC; break;
		case FE_OFDM:	return AM_FEND_DEMOD_DVBT;  break;
		case FE_QPSK:	return AM_FEND_DEMOD_DVBS;  break;
		case FE_ATSC:	
		default:			break;
	}
	return -1;
#else
	return fe_type;
#endif
}

/**\brief 预约一个EPG事件*/
static AM_ErrorCode_t am_epg_subscribe_event(sqlite3 *hdb, int db_evt_id)
{
	char sql[128];
	char *errmsg;

	if (hdb == NULL)
		return AM_EPG_ERR_INVALID_PARAM;
		
	/*由应用处理时间段冲突*/
	snprintf(sql, sizeof(sql), "update evt_table set sub_flag=1 where db_id=%d", db_evt_id);

	if (sqlite3_exec(hdb, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(0, "Subscribe EPG event for db_id=%d failed, reason: %s", db_evt_id, errmsg ? errmsg : "Unknown");
		if (errmsg)
			sqlite3_free(errmsg);
		return AM_EPG_ERR_SUBSCRIBE_EVENT_FAILED;
	}

	return AM_SUCCESS;
}

/**\brief 取消预约一个EPG事件*/
static AM_ErrorCode_t am_epg_unsubscribe_event(sqlite3 *hdb, int db_evt_id)
{
	char sql[128];
	char *errmsg;

	if (hdb == NULL)
		return AM_EPG_ERR_INVALID_PARAM;
		
	/*由应用处理时间段冲突*/
	snprintf(sql, sizeof(sql), "update evt_table set sub_flag=0,sub_status=0 where db_id=%d", db_evt_id);

	if (sqlite3_exec(hdb, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(0, "Unsubscribe EPG event for db_id=%d failed, reason: %s", db_evt_id, errmsg ? errmsg : "Unknown");
		if (errmsg)
			sqlite3_free(errmsg);
		return AM_EPG_ERR_SUBSCRIBE_EVENT_FAILED;
	}

	return AM_SUCCESS;
}

/**\brief 删除过期的event*/
static AM_ErrorCode_t am_epg_delete_expired_events(sqlite3 *hdb)
{
	int now;
	char sql[128];
	char *errmsg;
	
	if (hdb == NULL)
		return AM_EPG_ERR_INVALID_PARAM;
		
	AM_DEBUG(1, "Deleting expired epg events...");
	AM_EPG_GetUTCTime(&now);
	snprintf(sql, sizeof(sql), "delete from evt_table where end<%d", now);
	if (sqlite3_exec(hdb, sql, NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(0, "Delete expired events failed: %s", errmsg ? errmsg : "Unknown");
		if (errmsg)
			sqlite3_free(errmsg);
		return AM_FAILURE;
	}
	AM_DEBUG(1, "Delete expired epg events done!");
	return AM_SUCCESS;
}

/**\brief 释放过滤器，并保证在此之后不会再有无效数据*/
static void am_epg_free_filter(AM_EPG_Monitor_t *mon,  int *fid)
{
	if (*fid == -1)
		return;
		
	AM_DMX_FreeFilter(mon->dmx_dev, *fid);
	*fid = -1;
	pthread_mutex_unlock(&mon->lock);
	/*等待无效数据处理完毕*/
	AM_DMX_Sync(mon->dmx_dev);
	pthread_mutex_lock(&mon->lock);
}

/**\brief 清空一个表控制标志*/
static void am_epg_tablectl_clear(AM_EPG_TableCtl_t * mcl)
{
	mcl->data_arrive_time = 0;
	mcl->check_time = 0;
	if (mcl->subs && mcl->subctl)
	{
		int i;

		memset(mcl->subctl, 0, sizeof(AM_EPG_SubCtl_t) * mcl->subs);
		for (i=0; i<mcl->subs; i++)
		{
			mcl->subctl[i].ver = 0xff;
		}
	}
}

 /**\brief 初始化一个表控制结构*/
static AM_ErrorCode_t am_epg_tablectl_init(AM_EPG_TableCtl_t * mcl, int evt_flag,
											uint16_t pid, uint8_t tid, uint8_t tid_mask,
											const char *name, uint16_t sub_cnt, 
											void (*done)(struct AM_EPG_Monitor_s *), int distance)
{
	memset(mcl, 0, sizeof(AM_EPG_TableCtl_t));
	mcl->fid = -1;
	mcl->evt_flag = evt_flag;
	mcl->pid = pid;
	mcl->tid = tid;
	mcl->tid_mask = tid_mask;
	mcl->done = done;
	mcl->repeat_distance = distance;
	strcpy(mcl->tname, name);

	mcl->subs = sub_cnt;
	if (mcl->subs)
	{
		mcl->subctl = (AM_EPG_SubCtl_t*)malloc(sizeof(AM_EPG_SubCtl_t) * mcl->subs);
		if (!mcl->subctl)
		{
			mcl->subs = 0;
			AM_DEBUG(1, "Cannot init tablectl, no enough memory");
			return AM_EPG_ERR_NO_MEM;
		}

		am_epg_tablectl_clear(mcl);
	}

	return AM_SUCCESS;
}

/**\brief 反初始化一个表控制结构*/
static void am_epg_tablectl_deinit(AM_EPG_TableCtl_t * mcl)
{
	if (mcl->subctl)
	{
		free(mcl->subctl);
		mcl->subctl = NULL;
	}
}

/**\brief 判断一个表的所有section是否收齐*/
static AM_Bool_t am_epg_tablectl_test_complete(AM_EPG_TableCtl_t * mcl)
{
	static uint8_t test_array[32] = {0};
	int i;

	for (i=0; i<mcl->subs; i++)
	{
		if ((mcl->data_arrive_time == 0) || 
			((mcl->subctl[i].ver != 0xff) &&
			memcmp(mcl->subctl[i].mask, test_array, sizeof(test_array))))
			return AM_FALSE;
	}

	return AM_TRUE;
}

/**\brief 判断一个表的指定section是否已经接收*/
static AM_Bool_t am_epg_tablectl_test_recved(AM_EPG_TableCtl_t * mcl, AM_SI_SectionHeader_t *header)
{
	int i;
	
	if (!mcl->subctl)
		return AM_TRUE;

	for (i=0; i<mcl->subs; i++)
	{
		if ((mcl->subctl[i].ext == header->extension) && 
			(mcl->subctl[i].ver == header->version) && 
			(mcl->subctl[i].last == header->last_sec_num) && 
			!BIT_TEST(mcl->subctl[i].mask, header->sec_num))
		{
			if ((mcl->subs > 1) && (mcl->data_arrive_time == 0))
				AM_TIME_GetClock(&mcl->data_arrive_time);
			
			return AM_TRUE;
		}
	}
	
	return AM_FALSE;
}

/**\brief 在一个表中增加一个EITsection已接收标识*/
static AM_ErrorCode_t am_epg_tablectl_mark_section_eit(AM_EPG_TableCtl_t	 * mcl, 
													   AM_SI_SectionHeader_t *header, 
													   int					 seg_last_sec)
{
	int i;
	AM_EPG_SubCtl_t *sub, *fsub;

	if (!mcl->subctl || seg_last_sec > header->last_sec_num)
		return AM_FAILURE;

	sub = fsub = NULL;
	for (i=0; i<mcl->subs; i++)
	{
		if (mcl->subctl[i].ext == header->extension)
		{
			sub = &mcl->subctl[i];
			break;
		}
		/*记录一个空闲的结构*/
		if ((mcl->subctl[i].ver == 0xff) && !fsub)
			fsub = &mcl->subctl[i];
	}
	
	if (!sub && !fsub)
	{
		AM_DEBUG(1, "No more subctl for adding new %s subtable", mcl->tname);
		return AM_FAILURE;
	}
	if (!sub)
		sub = fsub;
	
	/*发现新版本，重新设置接收控制*/
	if (sub->ver != 0xff && (sub->ver != header->version ||\
		sub->ext != header->extension || sub->last != header->last_sec_num))
		SUBCTL_CLEAR(sub);

	if (sub->ver == 0xff)
	{
		int i;
		
		/*接收到的第一个section*/
		sub->last = header->last_sec_num;
		sub->ver = header->version;	
		sub->ext = header->extension;
		/*设置未接收标识*/
		for (i=0; i<(sub->last+1); i++)
			BIT_SET(sub->mask, i);
	}

	/*设置已接收标识*/
	BIT_CLEAR(sub->mask, header->sec_num);

	/*设置segment中未使用的section标识为已接收*/
	if (seg_last_sec >= 0)
		BIT_CLEAR_EX(sub->mask, (uint8_t)seg_last_sec);

	if (mcl->data_arrive_time == 0)
		AM_TIME_GetClock(&mcl->data_arrive_time);

	return AM_SUCCESS;
}

/**\brief 在一个表中增加一个section已接收标识*/
static AM_ErrorCode_t am_epg_tablectl_mark_section(AM_EPG_TableCtl_t * mcl, AM_SI_SectionHeader_t *header)
{
	return am_epg_tablectl_mark_section_eit(mcl, header, -1);
}

/**\brief 根据过滤器号取得相应控制数据*/
static AM_EPG_TableCtl_t *am_epg_get_section_ctrl_by_fid(AM_EPG_Monitor_t *mon, int fid)
{
	AM_EPG_TableCtl_t *scl = NULL;

	if (mon->patctl.fid == fid)
		scl = &mon->patctl;
	else if (mon->catctl.fid == fid)
		scl = &mon->catctl;
	else if (mon->pmtctl.fid == fid)
		scl = &mon->pmtctl;
	else if (mon->sdtctl.fid == fid)
		scl = &mon->sdtctl;
	else if (mon->nitctl.fid == fid)
		scl = &mon->nitctl;
	else if (mon->totctl.fid == fid)
		scl = &mon->totctl;
	else if (mon->eit4ectl.fid == fid)
		scl = &mon->eit4ectl;
	else if (mon->eit4fctl.fid == fid)
		scl = &mon->eit4fctl;
	else if (mon->eit50ctl.fid == fid)
		scl = &mon->eit50ctl;
	else if (mon->eit51ctl.fid == fid)
		scl = &mon->eit51ctl;
	else if (mon->eit60ctl.fid == fid)
		scl = &mon->eit60ctl;
	else if (mon->eit61ctl.fid == fid)
		scl = &mon->eit61ctl;
	else if (mon->sttctl.fid == fid)
		scl = &mon->sttctl;
	else if (mon->mgtctl.fid == fid)
		scl = &mon->mgtctl;
	else if (mon->rrtctl.fid == fid)
		scl = &mon->rrtctl;
	else
	{
		int i;
		
		for (i=0; i<mon->psip_eit_count; i++)
		{
			if (mon->psip_eitctl[i].fid == fid)
			{
				scl = &mon->psip_eitctl[i];
				break;
			}
		}
	}

	
	return scl;
}

/**\brief 数据处理函数*/
static void am_epg_section_handler(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)user_data;
	AM_EPG_TableCtl_t * sec_ctrl;
	AM_SI_SectionHeader_t header;

	if (mon == NULL)
	{
		AM_DEBUG(1, "EPG: Invalid param user_data in dmx callback");
		return;
	}
	if (!data)
		return;
		
	pthread_mutex_lock(&mon->lock);
	/*获取接收控制数据*/
	sec_ctrl = am_epg_get_section_ctrl_by_fid(mon, fid);
	if (sec_ctrl)
	{
		if (sec_ctrl != &mon->totctl)
		{
			/* for Non-TDT/TOT sections, the section_syntax_indicator bit must be 1 */
			if ((data[1]&0x80) == 0)
			{
				AM_DEBUG(1, "EPG: section_syntax_indicator is 0, skip this section");
				pthread_mutex_unlock(&mon->lock);
				return;
			}
			
			if (AM_SI_GetSectionHeader(mon->hsi, (uint8_t*)data, len, &header) != AM_SUCCESS)
			{
				AM_DEBUG(1, "EPG: section header error");
				pthread_mutex_unlock(&mon->lock);
				return;
			}
		
			/*该section是否已经接收过*/
			if (am_epg_tablectl_test_recved(sec_ctrl, &header))
			{
				AM_DEBUG(5,"%s section %d repeat! last_sec %d", sec_ctrl->tname, header.sec_num, header.last_sec_num);
			
				/*当有多个子表时，判断收齐的条件为 收到重复section + 
				 *所有子表收齐 + 重复section间隔时间大于某个值
				 */
				if (sec_ctrl->subs > 1)
				{
					int now;
				
					AM_TIME_GetClock(&now);
					if (am_epg_tablectl_test_complete(sec_ctrl) && 
						((now - sec_ctrl->data_arrive_time) > sec_ctrl->repeat_distance))
						TABLE_DONE();
				}
				pthread_mutex_unlock(&mon->lock);
				return;
			}
		}
		else
		{
			/* for TDT/TOT section, the section_syntax_indicator bit must be 0 */
			if ((data[1]&0x80) != 0)
			{
				AM_DEBUG(1, "EPG: TDT/TOT section_syntax_indicator is 1, skip this section");
				pthread_mutex_unlock(&mon->lock);
				return;
			}
		}
		/*数据处理*/
		switch (data[0])
		{
			case AM_SI_TID_PAT:
				COLLECT_SECTION(dvbpsi_pat_t, mon->pats);
				break;
			case AM_SI_TID_PMT:
				COLLECT_SECTION(dvbpsi_pmt_t, mon->pmts);
				break;
			case AM_SI_TID_SDT_ACT:
				COLLECT_SECTION(dvbpsi_sdt_t, mon->sdts);
				break;
			case AM_SI_TID_CAT:
				COLLECT_SECTION(dvbpsi_cat_t, mon->cats);
				break;
			case AM_SI_TID_NIT_ACT:
				COLLECT_SECTION(dvbpsi_nit_t, mon->nits);
				break;
			case AM_SI_TID_TOT:
			case AM_SI_TID_TDT:
#ifdef USE_TDT_TIME
			{
				dvbpsi_tot_t *p_tot;
				
				if (AM_SI_DecodeSection(mon->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_tot) == AM_SUCCESS)
				{
					uint16_t mjd;
					uint8_t hour, min, sec;
					
					p_tot->p_next = NULL;

					/*取UTC时间*/
					mjd = (uint16_t)(p_tot->i_utc_time >> 24);
					hour = (uint8_t)(p_tot->i_utc_time >> 16);
					min = (uint8_t)(p_tot->i_utc_time >> 8);
					sec = (uint8_t)p_tot->i_utc_time;

					pthread_mutex_lock(&time_lock);
					curr_time.tdt_utc_time = AM_EPG_MJD2SEC(mjd) + AM_EPG_BCD2SEC(hour, min, sec);
					/*更新system time，用于时间自动累加*/
					AM_TIME_GetClock(&curr_time.tdt_sys_time);
					pthread_mutex_unlock(&time_lock);
	   
					/*触发通知事件*/
					AM_EVT_Signal((int)mon, AM_EPG_EVT_NEW_TDT, (void*)p_tot);
					
					/*释放改TDT/TOT*/
					AM_SI_ReleaseSection(mon->hsi, p_tot->i_table_id, (void*)p_tot);

					TABLE_DONE();
				}
			}
#endif
				pthread_mutex_unlock(&mon->lock);

				return;
				break;
			case AM_SI_TID_EIT_PF_ACT:
			case AM_SI_TID_EIT_PF_OTH:
			case AM_SI_TID_EIT_SCHE_ACT:
			case AM_SI_TID_EIT_SCHE_OTH:
			case (AM_SI_TID_EIT_SCHE_ACT + 1):
			case (AM_SI_TID_EIT_SCHE_OTH + 1):
			{
				dvbpsi_eit_t *p_eit;
				
				if (AM_SI_DecodeSection(mon->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_eit) == AM_SUCCESS)
				{
					AM_DEBUG(5, "EIT tid 0x%x, ext 0x%x, sec %x, last %x", header.table_id,
								header.extension, header.sec_num, header.last_sec_num);
					am_epg_tablectl_mark_section_eit(sec_ctrl, &header, data[12]);
					/*触发通知事件*/
					SIGNAL_EVENT(AM_EPG_EVT_NEW_EIT, (void*)p_eit);
					/*释放该section*/
					AM_SI_ReleaseSection(mon->hsi, p_eit->i_table_id, (void*)p_eit);

					if (! mon->eit_has_data)
						mon->eit_has_data = AM_TRUE;
				}
			}
				break;
			case AM_SI_TID_PSIP_MGT:
				COLLECT_SECTION(dvbpsi_mgt_t, mon->mgts);
				break;
			case AM_SI_TID_PSIP_RRT:
				AM_DEBUG(1, ">>>>>>>>>>>>RRT received! sec %x, last %x <<<<<<<<<<<<<", header.sec_num, header.last_sec_num);
				COLLECT_SECTION(dvbpsi_rrt_t, mon->rrts);
				break;
			case AM_SI_TID_PSIP_EIT:
			{
				eit_section_info_t *p_eit;

				AM_DEBUG(1, "%s: tid 0x%x, source_id 0x%x, sec %x, last %x", sec_ctrl->tname, header.table_id,
								header.extension, header.sec_num, header.last_sec_num);
				if (AM_SI_DecodeSection(mon->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_eit) == AM_SUCCESS)
				{
					/*设置为已接收*/
					am_epg_tablectl_mark_section(sec_ctrl, &header); 
					/*触发通知事件*/
					SIGNAL_EVENT(AM_EPG_EVT_NEW_PSIP_EIT, (void*)p_eit);
					/*释放该section*/
					AM_SI_ReleaseSection(mon->hsi, p_eit->i_table_id, (void*)p_eit);

					if (! mon->eit_has_data)
						mon->eit_has_data = AM_TRUE;
				}
				else
				{
					AM_DEBUG(1, "PSIP EIT decode failed");
				}
			}
				break;
			case AM_SI_TID_PSIP_STT:
#ifdef USE_TDT_TIME
			{
				stt_section_info_t *p_stt;
				
				AM_DEBUG(1, ">>>>>>>New STT found");
				
				if (AM_SI_DecodeSection(mon->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_stt) == AM_SUCCESS)
				{
					uint16_t mjd;
					uint8_t hour, min, sec;
					
					p_stt->p_next = NULL;
					AM_DEBUG(1, ">>>>>>>STT UTC time is %u", p_stt->utc_time);
					/*取UTC时间*/
					pthread_mutex_lock(&time_lock);
					curr_time.tdt_utc_time = p_stt->utc_time;
					/*更新system time，用于时间自动累加*/
					AM_TIME_GetClock(&curr_time.tdt_sys_time);
					pthread_mutex_unlock(&time_lock);
	   
					/*触发通知事件*/
					AM_EVT_Signal((int)mon, AM_EPG_EVT_NEW_STT, (void*)p_stt);
					
					/*释放改STT*/
					AM_SI_ReleaseSection(mon->hsi, p_stt->i_table_id, (void*)p_stt);
					
					/*设置为已接收*/
					am_epg_tablectl_mark_section(sec_ctrl, &header); 
				}
			}
#endif			
				break;
			default:
				AM_DEBUG(1, "EPG: Unkown section data, table_id 0x%x", data[0]);
				pthread_mutex_unlock(&mon->lock);
				return;
				break;
		}
			
		/*数据处理完毕，查看该表是否已接收完毕所有section*/
		if (am_epg_tablectl_test_complete(sec_ctrl) && sec_ctrl->subs == 1)
			TABLE_DONE();
	}
	else
	{
		AM_DEBUG(1, "EPG: Unknown filter id %d in dmx callback", fid);
	}
	
	pthread_mutex_unlock(&mon->lock);

}

/**\brief 请求一个表的section数据*/
static AM_ErrorCode_t am_epg_request_section(AM_EPG_Monitor_t *mon, AM_EPG_TableCtl_t *mcl)
{
	struct dmx_sct_filter_params param;

	if (! mcl->subctl)
		return AM_FAILURE;
		
	if (mcl->fid != -1)
	{
		am_epg_free_filter(mon, &mcl->fid);
	}

	/*分配过滤器*/
	AM_TRY(AM_DMX_AllocateFilter(mon->dmx_dev, &mcl->fid));
	/*设置处理函数*/
	AM_TRY(AM_DMX_SetCallback(mon->dmx_dev, mcl->fid, am_epg_section_handler, (void*)mon));
	
	/*设置过滤器参数*/
	memset(&param, 0, sizeof(param));
	param.pid = mcl->pid;
	param.filter.filter[0] = mcl->tid;
	param.filter.mask[0] = mcl->tid_mask;

	/*当前设置了需要监控的service,则设置PMT和EIT actual pf的extension*/
	if (mon->mon_service != -1 && (mcl->tid == AM_SI_TID_PMT || mcl->tid == AM_SI_TID_EIT_PF_ACT))
	{
		int srv_id = am_epg_get_current_service_id(mon);
		AM_DEBUG(1, "Set filter for service %d", srv_id);
		param.filter.filter[1] = (uint8_t)(srv_id>>8);
		param.filter.filter[2] = (uint8_t)srv_id;
		param.filter.mask[1] = 0xff;
		param.filter.mask[2] = 0xff;
	}

	if (mcl->pid != AM_SI_PID_TDT)
	{
		if (mcl->subctl->ver != 0xff && mcl->subs == 1)
		{
			AM_DEBUG(1, "Start filtering new version (!=%d) for %s table", mcl->subctl->ver, mcl->tname);
			/*Current next indicator must be 1*/
			param.filter.filter[3] = (mcl->subctl->ver << 1) | 0x01;
			param.filter.mask[3] = 0x3f;
			param.filter.mode[3] = 0xff;

			SUBCTL_CLEAR(mcl->subctl);
		}
		else
		{
			/*Current next indicator must be 1*/
			param.filter.filter[3] = 0x01;
			param.filter.mask[3] = 0x01;
		}

		param.flags = DMX_CHECK_CRC;
	}

	AM_TRY(AM_DMX_SetSecFilter(mon->dmx_dev, mcl->fid, &param));
	if (mcl->pid == AM_SI_PID_EIT && mcl->tid >= 0x50 && mcl->tid <= 0x6F)
		AM_TRY(AM_DMX_SetBufferSize(mon->dmx_dev, mcl->fid, 128*1024));
	else
		AM_TRY(AM_DMX_SetBufferSize(mon->dmx_dev, mcl->fid, 16*1024));
	AM_TRY(AM_DMX_StartFilter(mon->dmx_dev, mcl->fid));

	AM_DEBUG(1, "EPG Start filter %d for table %s, tid 0x%x, mask 0x%x", 
				mcl->fid, mcl->tname, mcl->tid, mcl->tid_mask);
	return AM_SUCCESS;
}

static int am_epg_get_current_service_id(AM_EPG_Monitor_t *mon)
{
	int row = 1;
	int service_id = 0xffff;
	char sql[256];
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);
	
	snprintf(sql, sizeof(sql), "select service_id from srv_table where db_id=%d", mon->mon_service);
	AM_DB_Select(hdb, sql, &row, "%d", &service_id);
	return service_id;
}

static int am_epg_get_current_db_ts_id(AM_EPG_Monitor_t *mon)
{
	int row = 1;
	int db_ts_id = -1;
	char sql[256];
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);
	
	snprintf(sql, sizeof(sql), "select db_ts_id from srv_table where db_id=%d", mon->mon_service);
	AM_DB_Select(hdb, sql, &row, "%d", &db_ts_id);
	return db_ts_id;
}

static void add_audio(AM_EPG_AudioInfo_t *ai, int aud_pid, int aud_fmt, char lang[3])
{
	int i;
	
	for (i=0; i<ai->audio_count; i++)
	{
		if (ai->audios[i].pid == aud_pid)
			return;
	}
	if (ai->audio_count >= 32)
	{
		AM_DEBUG(1, "Too many audios, Max count %d", 32);
		return;
	}
	if (ai->audio_count < 0)
		ai->audio_count = 0;
	ai->audios[ai->audio_count].pid = aud_pid;
	ai->audios[ai->audio_count].fmt = aud_fmt;
	memset(ai->audios[ai->audio_count].lang, 0, sizeof(ai->audios[ai->audio_count].lang));
	if (lang[0] != 0)
	{
		memcpy(ai->audios[ai->audio_count].lang, lang, 3);
	}
	else
	{
		sprintf(ai->audios[ai->audio_count].lang, "Audio%d", ai->audio_count+1);
	}
	
	AM_DEBUG(1, "Add a audio: pid %d, language: %s", aud_pid, ai->audios[ai->audio_count].lang);

	ai->audio_count++;
}

static void format_audio_strings(AM_EPG_AudioInfo_t *ai, char *pids, char *fmts, char *langs)
{
	int i;
	
	if (ai->audio_count < 0)
		ai->audio_count = 0;
		
	pids[0] = 0;
	fmts[0] = 0;
	langs[0] = 0;
	for (i=0; i<ai->audio_count; i++)
	{
		if (i == 0)
		{
			sprintf(pids, "%d", ai->audios[i].pid);
			sprintf(fmts, "%d", ai->audios[i].fmt);
			sprintf(langs, "%s", ai->audios[i].lang);
		}
		else
		{
			sprintf(pids, "%s %d", pids, ai->audios[i].pid);
			sprintf(fmts, "%s %d", fmts, ai->audios[i].fmt);
			sprintf(langs, "%s %s", langs, ai->audios[i].lang);
		}
	}
}


/**\brief 插入一个Subtitle记录*/
static int insert_subtitle(sqlite3 * hdb, int db_srv_id, int pid, dvbpsi_subtitle_t *psd)
{
	sqlite3_stmt *stmt;
	const char *sql = "insert into subtitle_table(db_srv_id,pid,type,composition_page_id,ancillary_page_id,language) values(?,?,?,?,?,?)";
	if (sqlite3_prepare(hdb, sql, strlen(sql), &stmt, NULL) != SQLITE_OK)
	{
		AM_DEBUG(1, "Prepare sqlite3 failed for insert new subtitle");
		return -1;
	}

	sqlite3_bind_int(stmt, 1, db_srv_id);
	sqlite3_bind_int(stmt, 2, pid);
	sqlite3_bind_int(stmt, 3, psd->i_subtitling_type);
	sqlite3_bind_int(stmt, 4, psd->i_composition_page_id);
	sqlite3_bind_int(stmt, 5, psd->i_ancillary_page_id);
	sqlite3_bind_text(stmt, 6, (const char*)psd->i_iso6392_language_code, 3, SQLITE_STATIC);
	AM_DEBUG(1, "Insert a new subtitle");
	sqlite3_step(stmt);
	sqlite3_reset(stmt);
	sqlite3_finalize(stmt);

	return 0;
}

/**\brief 插入一个Teletext记录*/
static int insert_teletext(sqlite3 * hdb, int db_srv_id, int pid, dvbpsi_teletextpage_t *ptd)
{
	sqlite3_stmt *stmt;
	const char *sql = "insert into teletext_table(db_srv_id,pid,type,magazine_number,page_number,language) values(?,?,?,?,?,?)";
	if (sqlite3_prepare(hdb, sql, strlen(sql), &stmt, NULL) != SQLITE_OK)
	{
		AM_DEBUG(1, "Prepare sqlite3 failed for insert new teletext");
		return -1;
	}
	
	sqlite3_bind_int(stmt, 1, db_srv_id);
	sqlite3_bind_int(stmt, 2, pid);
	if (ptd)
	{
		sqlite3_bind_int(stmt, 3, ptd->i_teletext_type);
		sqlite3_bind_int(stmt, 4, ptd->i_teletext_magazine_number);
		sqlite3_bind_int(stmt, 5, ptd->i_teletext_page_number);
		sqlite3_bind_text(stmt, 6, (const char*)ptd->i_iso6392_language_code, 3, SQLITE_STATIC);
	}
	else
	{
		sqlite3_bind_int(stmt, 3, 0);
		sqlite3_bind_int(stmt, 4, 1);
		sqlite3_bind_int(stmt, 5, 0);
		sqlite3_bind_text(stmt, 6, "", -1, SQLITE_STATIC);
	}
	AM_DEBUG(1, "Insert a new teletext");
	sqlite3_step(stmt);
	sqlite3_reset(stmt);
	sqlite3_finalize(stmt);

	return 0;
}


/**\brief 插入一个网络记录，返回其索引*/
static int insert_net(sqlite3 * hdb, int src, int orig_net_id)
{
	int db_id = -1;
	int row;
	char query_sql[256];
	char insert_sql[256];
	
	snprintf(query_sql, sizeof(query_sql), 
		"select db_id from net_table where src=%d and network_id=%d",
		src, orig_net_id);
	snprintf(insert_sql, sizeof(insert_sql), 
		"insert into net_table(network_id, src, name) values(%d,%d,'')",
		orig_net_id, src);

	row = 1;
	/*query wether it exists*/
	if (AM_DB_Select(hdb, query_sql, &row, "%d", &db_id) != AM_SUCCESS || row <= 0)
	{
		db_id = -1;
	}

	/*if not exist , insert a new record*/
	if (db_id == -1)
	{
		sqlite3_exec(hdb, insert_sql, NULL, NULL, NULL);
		row = 1;
		if (AM_DB_Select(hdb, query_sql, &row, "%d", &db_id) != AM_SUCCESS )
		{
			db_id = -1;
		}
	}

	return db_id;
}

static void am_epg_check_pmt_update(AM_EPG_Monitor_t *mon)
{
	dvbpsi_pmt_t *pmt;
	dvbpsi_pmt_es_t *es;
	dvbpsi_descriptor_t *descr;
	int vfmt = -1, prev_vfmt = -1;
	int vid = 0x1fff, prev_vid = 0x1fff;
	int afmt_tmp, vfmt_tmp, db_ts_id;
	AM_EPG_AudioInfo_t aud_info;
	char lang_tmp[3];
	char str_apids[256];
	char str_afmts[256];
	char str_alangs[256];
	sqlite3 *hdb;
	
	if (mon->mon_service < 0 || mon->pmts == NULL)
		return;

	AM_DB_HANDLE_PREPARE(hdb);

	if (am_epg_get_current_service_id(mon) != mon->pmts->i_program_number)
	{
		AM_DEBUG(1, "PMT update: Program number dismatch!!!");
		return;
	}
	/* check if ts_id needs update */
	db_ts_id = am_epg_get_current_db_ts_id(mon);
	if (db_ts_id >= 0 && mon->pats)
	{
		char sql[256];
		int ts_id;
		int row = 1;
		
		snprintf(sql, sizeof(sql), "select ts_id from ts_table where db_id=%d", db_ts_id);
		if (AM_DB_Select(hdb, sql, &row, "%d", &ts_id) == AM_SUCCESS && row > 0)
		{
			if (ts_id == 0xffff)
			{
				/* Currently, we only update this invalid ts_id */
				AM_DEBUG(1, "PAT Update: ts %d's ts_id changed: 0x%x -> 0x%x", db_ts_id, ts_id, mon->pats->i_ts_id);
				snprintf(sql, sizeof(sql), "update ts_table set ts_id=%d where db_id=%d",
					mon->pats->i_ts_id, db_ts_id);
				sqlite3_exec(hdb, sql, NULL, NULL, NULL);
				SIGNAL_EVENT(AM_EPG_EVT_UPDATE_TS, (void*)db_ts_id);
			}
		}
	}
	
	aud_info.audio_count = 0;
	/*遍历PMT表*/
	AM_SI_LIST_BEGIN(mon->pmts, pmt)
		/*取ES流信息*/
		AM_SI_LIST_BEGIN(pmt->p_first_es, es)
			afmt_tmp = -1;
			vfmt_tmp = -1;
			memset(lang_tmp, 0, sizeof(lang_tmp));
			switch (es->i_type)
			{
				/*override by parse descriptor*/
				case 0x6:
					AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
						switch (descr->i_tag)
						{
							case AM_SI_DESCR_AC3:
							case AM_SI_DESCR_ENHANCED_AC3:
								AM_DEBUG(0, "!!Found AC3 Descriptor!!!");
								afmt_tmp = AFORMAT_AC3;
								break;
							case AM_SI_DESCR_AAC:
								AM_DEBUG(0, "!!Found AAC Descriptor!!!");
								afmt_tmp = AFORMAT_AAC;
								break;
							case AM_SI_DESCR_DTS:
								AM_DEBUG(0, "!!Found DTS Descriptor!!!");
								afmt_tmp = AFORMAT_DTS;
								break;
							default:
								break;
						}
					AM_SI_LIST_END()
					break;
				/*video pid and video format*/
				case 0x1:
				case 0x2:
					vfmt_tmp = VFORMAT_MPEG12;
					break;
				case 0x10:
					vfmt_tmp = VFORMAT_MPEG4;
					break;
				case 0x1b:
					vfmt_tmp = VFORMAT_H264;
					break;
				case 0xea:
					vfmt_tmp = VFORMAT_VC1;
					break;
				/*audio pid and audio format*/ 
				case 0x3:
				case 0x4:
					afmt_tmp = AFORMAT_MPEG;
					break;
				case 0x0f:
					afmt_tmp = AFORMAT_AAC;
					break;
				case 0x11:
					afmt_tmp = AFORMAT_AAC_LATM;
					break;
				case 0x81:
					afmt_tmp = AFORMAT_AC3;
					break;
				case 0x8A:
                case 0x82:
                case 0x85:
                case 0x86:
                	afmt_tmp = AFORMAT_DTS;
					break;
				default:
					break;
			}
			
			/*添加音视频流*/
			if (vfmt_tmp != -1)
			{
				vid = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
				AM_DEBUG(3, "Set video format to %d", vfmt_tmp);
				vfmt = vfmt_tmp;
			}
			if (afmt_tmp != -1)
			{
				AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
					if (descr->i_tag == AM_SI_DESCR_ISO639 && descr->p_decoded != NULL)
					{
						dvbpsi_iso639_dr_t *pisod = (dvbpsi_iso639_dr_t*)descr->p_decoded;
						if (pisod->i_code_count > 0) 
						{
							memcpy(lang_tmp, pisod->code[0].iso_639_code, sizeof(lang_tmp));
							break;
						}
					}
				AM_SI_LIST_END()
				add_audio(&aud_info, es->i_pid, afmt_tmp, lang_tmp);
			}
		AM_SI_LIST_END()
	AM_SI_LIST_END()
	
	format_audio_strings(&aud_info, str_apids, str_afmts, str_alangs);
	if (vfmt != -1)
	{
		int row = 1;
		char sql[256];
		char str_prev_apids[256];
		char str_prev_afmts[256];
		char str_prev_alangs[256];
		
		/* is video/audio changed ? */
		snprintf(sql, sizeof(sql), "select vid_pid,vid_fmt,aud_pids,aud_fmts,aud_langs \
			from srv_table where db_id=%d", mon->mon_service);
		if (AM_DB_Select(hdb, sql, &row, "%d,%d,%s:256,%s:256,%s:256", &prev_vid, 
			&prev_vfmt, str_prev_apids, str_prev_afmts, str_prev_alangs) == AM_SUCCESS && row > 0)
		{
			if (vid == prev_vid && vfmt == prev_vfmt && 
				!strcmp(str_apids, str_prev_apids) &&
				!strcmp(str_afmts, str_prev_afmts) &&
				!strcmp(str_alangs, str_prev_alangs))
			{
				AM_DEBUG(1, "@ Video & Audio not changed ! @");
			}
			else
			{
				AM_DEBUG(1, "@@ Video/Audio changed @@");
				AM_DEBUG(1, "Video pid/fmt: (%d/%d) -> (%d/%d)", prev_vid, prev_vfmt, vid, vfmt);
				AM_DEBUG(1, "Audio pid/fmt/lang: ('%s'/'%s'/'%s') -> ('%s'/'%s'/'%s')",
					str_prev_apids, str_prev_afmts, str_prev_alangs, str_apids, str_afmts, str_alangs);
				/* update to database */
				snprintf(sql, sizeof(sql), "update srv_table set vid_pid=%d,vid_fmt=%d,\
					aud_pids='%s',aud_fmts='%s',aud_langs='%s', current_aud=-1 where db_id=%d", 
					vid, vfmt, str_apids, str_afmts, str_alangs, mon->mon_service);
				sqlite3_exec(hdb, sql, NULL, NULL, NULL);
				
				/*遍历PMT表*/
				AM_SI_LIST_BEGIN(mon->pmts, pmt)
					/*取ES流信息*/
					AM_SI_LIST_BEGIN(pmt->p_first_es, es)
						/*查找Subtilte和Teletext描述符，并添加相关记录*/
						AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
						if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SUBTITLING)
						{
							int isub;
							dvbpsi_subtitling_dr_t *psd = (dvbpsi_subtitling_dr_t*)descr->p_decoded;

							AM_DEBUG(0, "Find subtitle descriptor, number:%d",psd->i_subtitles_number);
							for (isub=0; isub<psd->i_subtitles_number; isub++)
							{
								insert_subtitle(hdb, mon->mon_service, es->i_pid, &psd->p_subtitle[isub]);
							}
						}
						else if (descr->i_tag == AM_SI_DESCR_TELETEXT)
						{
							int itel;
							dvbpsi_teletext_dr_t *ptd = (dvbpsi_teletext_dr_t*)descr->p_decoded;

							AM_DEBUG(1, "Find teletext descriptor, ptd %p", ptd);
							if (ptd)
							{
								for (itel=0; itel<ptd->i_pages_number; itel++)
								{
									insert_teletext(hdb, mon->mon_service, es->i_pid, &ptd->p_pages[itel]);
								}
							}
							else
							{
								insert_teletext(hdb, mon->mon_service, es->i_pid, NULL);
							}
						}
						AM_SI_LIST_END()
					AM_SI_LIST_END()
				AM_SI_LIST_END()
			
				/*触发通知事件*/
				SIGNAL_EVENT(AM_EPG_EVT_UPDATE_PROGRAM, (void*)mon->mon_service);
			}
		}
	}
}


static void am_epg_check_sdt_update(AM_EPG_Monitor_t *mon)
{
	const char *sql = "select db_id,name from srv_table where db_ts_id=? \
								and service_id=? limit 1";
	const char *update_sql = "update srv_table set name=? where db_id=?";
	sqlite3_stmt *stmt = NULL;
	sqlite3_stmt *update_stmt = NULL;
	int db_srv_id;
	dvbpsi_sdt_t *sdt;
	AM_Bool_t update = AM_FALSE;
	int db_ts_id;
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);

	if (mon->sdts)
	{
		dvbpsi_sdt_service_t *srv;
		dvbpsi_descriptor_t *descr;
		char tmpsql[256];
		int db_net_id;

		db_ts_id = am_epg_get_current_db_ts_id(mon);
		if (sqlite3_prepare(hdb, sql, strlen(sql), &stmt, NULL) != SQLITE_OK)
		{
			AM_DEBUG(1, "Prepare sqlite3 failed for selecting service name");
			return;
		}
		if (sqlite3_prepare(hdb, update_sql, strlen(update_sql), &update_stmt, NULL) != SQLITE_OK)
		{
			AM_DEBUG(1, "Prepare sqlite3 failed for updating service name");
			goto update_end;
		}
		db_net_id = insert_net(hdb, mon->src, mon->sdts->i_network_id);
		AM_DEBUG(1, "SDT Update: insert new network on source %d, db_net_id=%d",mon->src, db_net_id);
		if (db_net_id >= 0)
		{
			AM_DEBUG(1, "SDT Update: insert new network, orginal_network_id=%d",mon->sdts->i_network_id);
			/* check if db_net_id needs update */
			db_ts_id = am_epg_get_current_db_ts_id(mon);
			if (db_ts_id >= 0)
			{
				int prev_net_id;
				int row;
				
				AM_DEBUG(1, "Checking for ts %d", db_ts_id);
				row = 1;	
				snprintf(tmpsql, sizeof(tmpsql), "select db_net_id from ts_table where db_id=%d", db_ts_id);
				if (AM_DB_Select(hdb, tmpsql, &row, "%d", &prev_net_id) == AM_SUCCESS && row > 0)
				{
					AM_DEBUG(1, "prev net id = %d", prev_net_id);
					if (prev_net_id < 0)
					{
						/* Currently, we only update this invalid db_net_id */
						AM_DEBUG(1, "SDT Update: ts %d's db_net_id changed: 0x%x -> 0x%x", 
							db_ts_id, prev_net_id, db_net_id);
						snprintf(tmpsql, sizeof(tmpsql), "update ts_table set db_net_id=%d where db_id=%d",
							db_net_id, db_ts_id);
						sqlite3_exec(hdb, tmpsql, NULL, NULL, NULL);
						/* update services */
						snprintf(tmpsql, sizeof(tmpsql), "update srv_table set db_net_id=%d where db_ts_id=%d",
							db_net_id, db_ts_id);
						sqlite3_exec(hdb, tmpsql, NULL, NULL, NULL);
						SIGNAL_EVENT(AM_EPG_EVT_UPDATE_TS, (void*)db_ts_id);
						update = AM_TRUE;
					}
				}
				else
				{
					AM_DEBUG(1, "select db_net_id failed");
				}
			}
		}
		AM_SI_LIST_BEGIN(mon->sdts, sdt)
		AM_SI_LIST_BEGIN(sdt->p_first_service, srv)
			/*从SDT表中查找该service名称*/
			AM_SI_LIST_BEGIN(srv->p_first_descriptor, descr)
			if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SERVICE)
			{
				dvbpsi_service_dr_t *psd = (dvbpsi_service_dr_t*)descr->p_decoded;
				char name[AM_DB_MAX_SRV_NAME_LEN + 1];
				const unsigned char *old_name;

				/*取节目名称*/
				if (psd->i_service_name_length > 0)
				{
					AM_EPG_ConvertCode((char*)psd->i_service_name, psd->i_service_name_length,\
								name, AM_DB_MAX_SRV_NAME_LEN);
					name[AM_DB_MAX_SRV_NAME_LEN] = 0;

					sqlite3_bind_int(stmt, 1, db_ts_id);
					sqlite3_bind_int(stmt, 2, srv->i_service_id);
					if (sqlite3_step(stmt) == SQLITE_ROW)
					{
						db_srv_id = sqlite3_column_int(stmt, 0);
						old_name = sqlite3_column_text(stmt, 1);
						if (old_name != NULL && !strcmp((const char*)old_name, "No Name"))
						{
							AM_DEBUG(1, "SDT Update: Program name changed: %s -> %s", old_name, name);
							sqlite3_bind_text(update_stmt, 1, (const char*)name, -1, SQLITE_STATIC);
							sqlite3_bind_int(update_stmt, 2, db_srv_id);
							sqlite3_step(update_stmt);
							sqlite3_reset(update_stmt);
							AM_DEBUG(1, "Update '%s' done!", name);
							if (! update)
								update = AM_TRUE;
						}
						else
						{
							AM_DEBUG(1, "SDT Update: Program name '%s' not changed !", old_name);
						}
					}
					else
					{
						AM_DEBUG(1, "SDT Update: Cannot find program for db_ts_id=%d  srv=%d",
							db_ts_id,srv->i_service_id);
					}
					sqlite3_reset(stmt);
				}
			}
			AM_SI_LIST_END()
		AM_SI_LIST_END()
		AM_SI_LIST_END()
update_end:
		if (stmt != NULL)	
			sqlite3_finalize(stmt);
		if (update_stmt != NULL)
			sqlite3_finalize(update_stmt);
		if (update)
		{
			/*触发通知事件*/
			SIGNAL_EVENT(AM_EPG_EVT_UPDATE_PROGRAM, (void*)mon->mon_service);
		}
	}
}

/**\brief NIT搜索完毕处理*/
static void am_epg_nit_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->nitctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_NIT, (void*)mon->nits);

	/*监控下一版本*/
	if (mon->nitctl.subctl)
	{
		//mon->nitctl.subctl->ver++;
		am_epg_request_section(mon, &mon->nitctl);
	}
}

/**\brief PAT搜索完毕处理*/
static void am_epg_pat_done(AM_EPG_Monitor_t *mon)
{
	int db_ts_id;
	
	am_epg_free_filter(mon, &mon->patctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_PAT, (void*)mon->pats);
	
	/*监控下一版本*/
	if (mon->patctl.subctl)
	{
		//mon->patctl.subctl->ver++;
		am_epg_request_section(mon, &mon->patctl);
	}
	
	/* Search for PMT of mon_service */
	if (mon->mode&AM_EPG_SCAN_PMT && mon->mon_service != -1)
	{
		dvbpsi_pat_t *pat;
		dvbpsi_pat_program_t *prog;
		
		AM_SI_LIST_BEGIN(mon->pats, pat)
			AM_SI_LIST_BEGIN(pat->p_first_program, prog)
				if (prog->i_number == am_epg_get_current_service_id(mon))
				{
					AM_DEBUG(1, "Got program %d's PMT, pid is 0x%x", prog->i_number, prog->i_pid);
					mon->pmtctl.pid = prog->i_pid;
					SET_MODE(pmt, pmtctl, AM_EPG_SCAN_PMT, AM_TRUE);
					return;
				}
			AM_SI_LIST_END()
		AM_SI_LIST_END()
	}
			
}

/**\brief PMT搜索完毕处理*/
static void am_epg_pmt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->pmtctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_PMT, (void*)mon->pmts);
	
	am_epg_check_pmt_update(mon);
	
	/*监控下一版本*/
	if (mon->pmtctl.subctl)
	{
		//mon->pmtctl.subctl->ver++;
		am_epg_request_section(mon, &mon->pmtctl);
	}
}

/**\brief CAT搜索完毕处理*/
static void am_epg_cat_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->catctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_CAT, (void*)mon->cats);
	
	/*监控下一版本*/
	if (mon->catctl.subctl)
	{
		//mon->catctl.subctl->ver++;
		am_epg_request_section(mon, &mon->catctl);
	}
}

/**\brief SDT搜索完毕处理*/
static void am_epg_sdt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->sdtctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_SDT, (void*)mon->sdts);
	
	am_epg_check_sdt_update(mon);
	
	/*监控下一版本*/
	if (mon->sdtctl.subctl)
	{
		//mon->sdtctl.subctl->ver++;
		am_epg_request_section(mon, &mon->sdtctl);
	}
}

/**\brief TDT搜索完毕处理*/
static void am_epg_tdt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->totctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->totctl.check_time);
}

/**\brief EIT pf actual 搜索完毕处理*/
static void am_epg_eit4e_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->eit4ectl.fid);

	if (mon->mon_service == -1)
	{
		/*设置完成时间以进行下一次刷新*/
		AM_TIME_GetClock(&mon->eit4ectl.check_time);
		mon->eit4ectl.data_arrive_time = 0;
	}
	else
	{
		/*监控下一版本*/
		if (mon->eit4ectl.subctl)
		{
			//mon->eit4ectl.subctl->ver++;
			am_epg_request_section(mon, &mon->eit4ectl);
		}
	}
}

/**\brief EIT pf other 搜索完毕处理*/
static void am_epg_eit4f_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->eit4fctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit4fctl.check_time);
	mon->eit4fctl.data_arrive_time = 0;
}

/**\brief EIT schedule actual tableid=0x50 搜索完毕处理*/
static void am_epg_eit50_done(AM_EPG_Monitor_t *mon)
{
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);

	am_epg_free_filter(mon, &mon->eit50ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit50ctl.check_time);
	mon->eit50ctl.data_arrive_time = 0;
	
	/*Delete the expired events*/
	am_epg_delete_expired_events(hdb);
}

/**\brief EIT schedule actual tableid=0x51 搜索完毕处理*/
static void am_epg_eit51_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->eit51ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit51ctl.check_time);
	mon->eit51ctl.data_arrive_time = 0;
}

/**\brief EIT schedule other tableid=0x60 搜索完毕处理*/
static void am_epg_eit60_done(AM_EPG_Monitor_t *mon)
{
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);

	am_epg_free_filter(mon, &mon->eit60ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit60ctl.check_time);
	mon->eit60ctl.data_arrive_time = 0;
	/*Delete the expired events*/
	am_epg_delete_expired_events(hdb);		
}

/**\brief EIT schedule other tableid=0x61 搜索完毕处理*/
static void am_epg_eit61_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->eit61ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit61ctl.check_time);
	mon->eit61ctl.data_arrive_time = 0;
}

/**\brief TDT搜索完毕处理*/
static void am_epg_stt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->sttctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->sttctl.check_time);
}

/**\brief RRT搜索完毕处理*/
static void am_epg_rrt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->rrtctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_RRT, (void*)mon->rrts);
	
	/*监控下一版本*/
	if (mon->rrtctl.subctl)
	{
		//mon->rrtctl.subctl->ver++;
		am_epg_request_section(mon, &mon->rrtctl);
	}
}

/**\brief MGT搜索完毕处理*/
static void am_epg_mgt_done(AM_EPG_Monitor_t *mon)
{
	int eit;
	mgt_section_info_t *mgt;
	com_table_info_t *table;
	
	am_epg_free_filter(mon, &mon->mgtctl.fid);

	AM_DEBUG(1, "am_epg_mgt_done");
	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_MGT, (void*)mon->mgts);
	
	/*检查EIT是否需要更新*/
	AM_SI_LIST_BEGIN(mon->mgts, mgt)
	AM_SI_LIST_BEGIN(mgt->com_table_info, table)
	AM_DEBUG(1, "am_epg_mgt_done table_type %d", table->table_type);
	if (table->table_type >= AM_SI_ATSC_TT_EIT0 && 
		table->table_type < (AM_SI_ATSC_TT_EIT0+mon->psip_eit_count))
	{
		eit = table->table_type - AM_SI_ATSC_TT_EIT0;
		
		if (mon->psip_eitctl[eit].subctl)
		{
			if (mon->psip_eitctl[eit].pid != table->table_type_pid || 
				mon->psip_eitctl[eit].subctl->ver != table->table_type_version)
			{
				AM_DEBUG(1, "EIT%d pid/version changed %d/%d -> %d/%d", eit, 
					mon->psip_eitctl[eit].pid, mon->psip_eitctl[eit].subctl->ver,
					table->table_type_pid, table->table_type_version);
				mon->psip_eitctl[eit].pid = table->table_type_pid;
				mon->psip_eitctl[eit].subctl->ver = table->table_type_version;
				am_epg_request_section(mon, &mon->psip_eitctl[eit]);
			}
			else
			{
				AM_DEBUG(1, "EIT%d pid/version(%d/%d) not changed", eit, 
					mon->psip_eitctl[eit].pid, mon->psip_eitctl[eit].subctl->ver);
			}
		}
	}
	AM_SI_LIST_END()
	AM_SI_LIST_END()
	
	/*监控下一版本*/
	if (mon->mgtctl.subctl)
	{
		//mon->mgtctl.subctl->ver++;
		AM_DEBUG(1, "Try next MGT, version = %d", mon->mgtctl.subctl->ver);
		am_epg_request_section(mon, &mon->mgtctl);
	}
}

/**\brief PSIP EIT搜索完毕处理*/
static void am_epg_psip_eit_done(AM_EPG_Monitor_t *mon)
{
	int i;
	int now;
	
	AM_TIME_GetClock(&now);
	for (i=0; i<mon->psip_eit_count; i++)
	{
		if (mon->psip_eitctl[i].fid != -1 && am_epg_tablectl_test_complete(&mon->psip_eitctl[i])
			&& (now - mon->psip_eitctl[i].data_arrive_time) > mon->psip_eitctl[i].repeat_distance)
		{
			AM_DEBUG(1, "Stop filter for PSIP EIT%d", i);
			am_epg_free_filter(mon, &mon->psip_eitctl[i].fid);
		}
	}
}


/**\brief table control data init*/
static void am_epg_tablectl_data_init(AM_EPG_Monitor_t *mon)
{
	int i;
	char name[32];
	
	am_epg_tablectl_init(&mon->patctl, AM_EPG_EVT_PAT_DONE, AM_SI_PID_PAT, AM_SI_TID_PAT,
							 0xff, "PAT", 1, am_epg_pat_done, 0);
	am_epg_tablectl_init(&mon->pmtctl, AM_EPG_EVT_PMT_DONE, 0x1fff, 	   AM_SI_TID_PMT,
							 0xff, "PMT", 1, am_epg_pmt_done, 0);
	am_epg_tablectl_init(&mon->catctl, AM_EPG_EVT_CAT_DONE, AM_SI_PID_CAT, AM_SI_TID_CAT,
							 0xff, "CAT", 1, am_epg_cat_done, 0);
	am_epg_tablectl_init(&mon->sdtctl, AM_EPG_EVT_SDT_DONE, AM_SI_PID_SDT, AM_SI_TID_SDT_ACT,
							 0xff, "SDT", 1, am_epg_sdt_done, 0);
	am_epg_tablectl_init(&mon->nitctl, AM_EPG_EVT_NIT_DONE, AM_SI_PID_NIT, AM_SI_TID_NIT_ACT,
							 0xff, "NIT", 1, am_epg_nit_done, 0);
	am_epg_tablectl_init(&mon->totctl, AM_EPG_EVT_TDT_DONE, AM_SI_PID_TDT, AM_SI_TID_TOT,
							 0xfc, "TDT/TOT", 1, am_epg_tdt_done, 0);
	am_epg_tablectl_init(&mon->eit4ectl, AM_EPG_EVT_EIT4E_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_PF_ACT,
							 0xff, "EIT pf actual", MAX_EIT4E_SUBTABLE_CNT, am_epg_eit4e_done, EIT4E_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->eit4fctl, AM_EPG_EVT_EIT4F_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_PF_OTH,
							 0xff, "EIT pf other", MAX_EIT_SUBTABLE_CNT, am_epg_eit4f_done, EIT4F_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->eit50ctl, AM_EPG_EVT_EIT50_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_SCHE_ACT,
							 0xff, "EIT sche act(50)", MAX_EIT_SUBTABLE_CNT, am_epg_eit50_done, EIT50_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->eit51ctl, AM_EPG_EVT_EIT51_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_SCHE_ACT + 1,
							 0xff, "EIT sche act(51)", MAX_EIT_SUBTABLE_CNT, am_epg_eit51_done, EIT51_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->eit60ctl, AM_EPG_EVT_EIT60_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_SCHE_OTH,
							 0xff, "EIT sche other(60)", MAX_EIT_SUBTABLE_CNT, am_epg_eit60_done, EIT60_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->eit61ctl, AM_EPG_EVT_EIT61_DONE, AM_SI_PID_EIT, AM_SI_TID_EIT_SCHE_OTH + 1,
							 0xff, "EIT sche other(61)", MAX_EIT_SUBTABLE_CNT, am_epg_eit61_done, EIT61_REPEAT_DISTANCE);
	am_epg_tablectl_init(&mon->sttctl, AM_EPG_EVT_STT_DONE, AM_SI_ATSC_BASE_PID, AM_SI_TID_PSIP_STT,
							 0xff, "STT", 1, am_epg_stt_done, 0);
	am_epg_tablectl_init(&mon->mgtctl, AM_EPG_EVT_MGT_DONE, AM_SI_ATSC_BASE_PID, AM_SI_TID_PSIP_MGT,
							 0xff, "MGT", 1, am_epg_mgt_done, 0);
	am_epg_tablectl_init(&mon->rrtctl, AM_EPG_EVT_RRT_DONE, AM_SI_ATSC_BASE_PID, AM_SI_TID_PSIP_RRT,
							 0xff, "RRT", 1, am_epg_rrt_done, 0);
	/*Set for USA
	if (mon->rrtctl.subctl != NULL)
		mon->rrtctl.subctl[0].ext = 0x1;*/
		
	for (i=0; i<(int)AM_ARRAY_SIZE(mon->psip_eitctl); i++)
	{
		snprintf(name, sizeof(name), "ATSC EIT%d", i);
		am_epg_tablectl_init(&mon->psip_eitctl[i], AM_EPG_EVT_PSIP_EIT_DONE, 0x1fff, AM_SI_TID_PSIP_EIT,
							 0xff, name, MAX_EIT_SUBTABLE_CNT, am_epg_psip_eit_done, 5000);
	}
}

/**\brief 按照当前模式重新设置监控*/
static void am_epg_set_mode(AM_EPG_Monitor_t *mon, AM_Bool_t reset)
{
	int i;
	
	AM_DEBUG(1, "EPG Setmode 0x%x", mon->mode);

	SET_MODE(pat, patctl, AM_EPG_SCAN_PAT, reset);
	SET_MODE(cat, catctl, AM_EPG_SCAN_CAT, reset);
	SET_MODE(sdt, sdtctl, AM_EPG_SCAN_SDT, reset);
	SET_MODE(nit, nitctl, AM_EPG_SCAN_NIT, reset);
	SET_MODE(tot, totctl, AM_EPG_SCAN_TDT, reset);
	SET_MODE(eit, eit4ectl, AM_EPG_SCAN_EIT_PF_ACT, reset);
	SET_MODE(eit, eit4fctl, AM_EPG_SCAN_EIT_PF_OTH, reset);
	SET_MODE(eit, eit50ctl, AM_EPG_SCAN_EIT_SCHE_ACT, reset);
	SET_MODE(eit, eit51ctl, AM_EPG_SCAN_EIT_SCHE_ACT, reset);
	SET_MODE(eit, eit60ctl, AM_EPG_SCAN_EIT_SCHE_OTH, reset);
	SET_MODE(eit, eit61ctl, AM_EPG_SCAN_EIT_SCHE_OTH, reset);
	/*For ATSC*/
	SET_MODE(stt, sttctl, AM_EPG_SCAN_STT, reset);
	SET_MODE(mgt, mgtctl, AM_EPG_SCAN_MGT, reset);
	SET_MODE(rrt, rrtctl, AM_EPG_SCAN_RRT, reset);
	for (i=0; i<mon->psip_eit_count; i++)
	{
		SET_MODE(psip_eit, psip_eitctl[i], AM_EPG_SCAN_PSIP_EIT, reset);
	}
}

/**\brief 按照当前模式重置所有表监控*/
static void am_epg_reset(AM_EPG_Monitor_t *mon)
{
	/*重新打开所有监控*/
	am_epg_set_mode(mon, AM_TRUE);
}

/**\brief 前端回调函数*/
static void am_epg_fend_callback(int dev_no, int event_type, void *param, void *user_data)
{
	struct dvb_frontend_event *evt = (struct dvb_frontend_event*)param;
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)user_data;

	if (!mon || !evt || (evt->status == 0))
		return;

	pthread_mutex_lock(&mon->lock);
	mon->fe_evt = *evt;
	mon->evt_flag |= AM_EPG_EVT_FEND;
	pthread_cond_signal(&mon->cond);
	pthread_mutex_unlock(&mon->lock);
}

/**\brief 处理前端事件*/
static void am_epg_solve_fend_evt(AM_EPG_Monitor_t *mon)
{

}

/**\brief 检查并通知已预约的EPG事件*/
static void am_epg_check_sub_events(AM_EPG_Monitor_t *mon)
{
	int now, row, db_evt_id;
	int expired[32];
	char sql[512];
	sqlite3 *hdb;

	AM_DB_HANDLE_PREPARE(hdb);

	AM_EPG_GetUTCTime(&now);
	
	/*从evt_table中查找接下来即将开始播放的事件*/
	snprintf(sql, sizeof(sql), "select evt_table.db_id from evt_table left join srv_table on srv_table.db_id = evt_table.db_srv_id \
						where skip=0 and lock=0 and start>%d and start<=%d and sub_flag=1 and sub_status=0 order by start limit 1", 
						now, now+EPG_PRE_NOTIFY_TIME/1000);
	row = 1;
	if (AM_DB_Select(hdb, sql, &row, "%d", &db_evt_id) == AM_SUCCESS && row > 0)
	{
		/*通知用户*/
		SIGNAL_EVENT(AM_EPG_EVT_NEW_SUB_PLAY, (void*)db_evt_id);

		/*更新预约播放状态*/
		snprintf(sql, sizeof(sql), "update evt_table set sub_status=1 where db_id=%d", db_evt_id);
		sqlite3_exec(hdb, sql, NULL, NULL, NULL);
	}

	/*从evt_table中查找立即开始的事件*/
	snprintf(sql, sizeof(sql), "select evt_table.db_id from evt_table left join srv_table on srv_table.db_id = evt_table.db_srv_id \
						where skip=0 and lock=0 and start<=%d and start>=%d and end>%d and \
						sub_status=1 order by start limit 1", now, now-EPG_SUB_CHECK_TIME/1000, now);
	row = 1;
	if (AM_DB_Select(hdb, sql, &row, "%d", &db_evt_id) == AM_SUCCESS && row > 0)
	{
		/*通知用户*/
		SIGNAL_EVENT(AM_EPG_EVT_SUB_PLAY_START, (void*)db_evt_id);

		/*通知播放后取消预约标志*/
		snprintf(sql, sizeof(sql), "update evt_table set sub_flag=0,sub_status=0 where db_id=%d", db_evt_id);
		sqlite3_exec(hdb, sql, NULL, NULL, NULL);
	}

	/*从evt_table中查找过期的预约*/
	snprintf(sql, sizeof(sql), "select db_id from evt_table where sub_flag=1 and (start<%d or end<%d)", 
						now-EPG_SUB_CHECK_TIME/1000, now);
	row = AM_ARRAY_SIZE(expired);
	if (AM_DB_Select(hdb, sql, &row, "%d", expired) == AM_SUCCESS && row > 0)
	{
		int i;
		for (i=0; i<row; i++)
		{
			/*取消预约标志*/
			AM_DEBUG(1, "@@Subscription %d expired.",  expired[i]);
			snprintf(sql, sizeof(sql), "update evt_table set sub_flag=0,sub_status=0 where db_id=%d", expired[i]);
			sqlite3_exec(hdb, sql, NULL, NULL, NULL);
		}
	}
	/*设置进行下次检查*/
	AM_TIME_GetClock(&mon->sub_check_time);
}

/**\brief 检查EPG更新通知*/
static void am_epg_check_update(AM_EPG_Monitor_t *mon)
{
	/*触发通知事件*/
	if (mon->eit_has_data)
	{
		SIGNAL_EVENT(AM_EPG_EVT_EIT_UPDATE, NULL);
		mon->eit_has_data = AM_FALSE;
	}
	AM_TIME_GetClock(&mon->new_eit_check_time);
}

/**\brief 计算下一等待超时时间*/
static void am_epg_get_next_ms(AM_EPG_Monitor_t *mon, int *ms)
{
	int min = 0, now;
	
#define REFRESH_CHECK(t, table, m, d)\
	AM_MACRO_BEGIN\
		if ((mon->mode & (m)) && mon->table##ctl.check_time != 0)\
		{\
			if ((mon->table##ctl.check_time + (d)) - now<= 0)\
			{\
				AM_DEBUG(2, "Refreshing %s...", mon->table##ctl.tname);\
				mon->table##ctl.check_time = 0;\
				/*AM_DMX_StartFilter(mon->dmx_dev, mon->table##ctl.fid);*/\
				SET_MODE(t, table##ctl, m, AM_FALSE);\
			}\
			else\
			{\
				if (min == 0)\
					min = mon->table##ctl.check_time + (d) - now;\
				else\
					min = AM_MIN(min, mon->table##ctl.check_time + (d) - now);\
			}\
		}\
	AM_MACRO_END

#define EVENT_CHECK(check, dis, func)\
	AM_MACRO_BEGIN\
		if (check) {\
			if (now - (check+dis) >= 0){\
				func(mon);\
				if (min == 0) \
					min = dis;\
				else\
					min = AM_MIN(min, dis);\
			} else if (min == 0){\
				min = (check+dis) - now;\
			} else {\
				min = AM_MIN(min, (check+dis) - now);\
			}\
		}\
	AM_MACRO_END

	AM_TIME_GetClock(&now);

	/*自动更新检查*/
	REFRESH_CHECK(tot, tot, AM_EPG_SCAN_TDT, TDT_CHECK_DISTANCE);
	if (mon->mon_service == -1)
	{
		REFRESH_CHECK(eit, eit4e, AM_EPG_SCAN_EIT_PF_ACT, mon->eitpf_check_time);
	}
	REFRESH_CHECK(eit, eit4f, AM_EPG_SCAN_EIT_PF_OTH, mon->eitpf_check_time);
	REFRESH_CHECK(eit, eit50, AM_EPG_SCAN_EIT_SCHE_ACT, mon->eitsche_check_time);
	REFRESH_CHECK(eit, eit51, AM_EPG_SCAN_EIT_SCHE_ACT, mon->eitsche_check_time);
	REFRESH_CHECK(eit, eit60, AM_EPG_SCAN_EIT_SCHE_OTH, mon->eitsche_check_time);
	REFRESH_CHECK(eit, eit61, AM_EPG_SCAN_EIT_SCHE_OTH, mon->eitsche_check_time);
	
	REFRESH_CHECK(stt, stt, AM_EPG_SCAN_STT, STT_CHECK_DISTANCE);

	/*EIT数据更新通知检查*/
	EVENT_CHECK(mon->new_eit_check_time, NEW_EIT_CHECK_DISTANCE, am_epg_check_update);
	/*EPG预约播放检查*/
	EVENT_CHECK(mon->sub_check_time, EPG_SUB_CHECK_TIME, am_epg_check_sub_events);

	AM_DEBUG(2, "Next timeout is %d ms", min);
	*ms = min;
}

/**\brief MON线程*/
static void *am_epg_thread(void *para)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)para;
	AM_Bool_t go = AM_TRUE;
	int distance, ret, evt_flag, i;
	struct timespec rt;
	struct dvb_frontend_info info;
	int dbopen = 0;
	
	/*Reset the TDT time while epg start*/
	pthread_mutex_lock(&time_lock);
	memset(&curr_time, 0, sizeof(curr_time));
	AM_TIME_GetClock(&curr_time.tdt_sys_time);
	AM_DEBUG(0, "Start EPG Monitor thread now, curr_time.tdt_sys_time %d", curr_time.tdt_sys_time);
	pthread_mutex_unlock(&time_lock);

	/*控制数据初始化*/
	am_epg_tablectl_data_init(mon);

	/*获得当前前端参数*/
	AM_FEND_GetInfo(mon->fend_dev, &info);
	mon->src = am_epg_convert_fetype_to_source(info.type);
	mon->curr_ts = -1;
	
	/*注册前端事件*/
	AM_EVT_Subscribe(mon->fend_dev, AM_FEND_EVT_STATUS_CHANGED, am_epg_fend_callback, (void*)mon);

	AM_TIME_GetClock(&mon->sub_check_time);
			 			
	pthread_mutex_lock(&mon->lock);
	while (go)
	{
		am_epg_get_next_ms(mon, &distance);
		
		/*等待事件*/
		ret = 0;
		if(mon->evt_flag == 0)
		{
			if (distance == 0)
			{
				ret = pthread_cond_wait(&mon->cond, &mon->lock);
			}
			else
			{
				AM_TIME_GetTimeSpecTimeout(distance, &rt);
				ret = pthread_cond_timedwait(&mon->cond, &mon->lock, &rt);
			}
		}

		if (ret != ETIMEDOUT)
		{
handle_events:
			evt_flag = mon->evt_flag;
			AM_DEBUG(3, "Evt flag 0x%x", mon->evt_flag);

			/*前端事件*/
			if (evt_flag & AM_EPG_EVT_FEND)
				am_epg_solve_fend_evt(mon);
				
			/*PAT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_PAT_DONE)
				mon->patctl.done(mon);
			/*PMT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_PMT_DONE)
				mon->pmtctl.done(mon);
			/*CAT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_CAT_DONE)
				mon->catctl.done(mon);
			/*SDT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_SDT_DONE)
				mon->sdtctl.done(mon);
			/*NIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_NIT_DONE)
				mon->nitctl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT4E_DONE)
				mon->eit4ectl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT4F_DONE)
				mon->eit4fctl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT50_DONE)
				mon->eit50ctl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT51_DONE)
				mon->eit51ctl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT60_DONE)
				mon->eit60ctl.done(mon);
			/*EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_EIT61_DONE)
				mon->eit61ctl.done(mon);
			/*TDT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_TDT_DONE)
				mon->totctl.done(mon);
			/*STT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_STT_DONE)
				mon->sttctl.done(mon);
			/*RRT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_RRT_DONE)
				mon->rrtctl.done(mon);
			/*MGT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_MGT_DONE)
				mon->mgtctl.done(mon);
			/*PSIP EIT表收齐事件*/
			if (evt_flag & AM_EPG_EVT_PSIP_EIT_DONE)
				mon->psip_eitctl[0].done(mon);
			/*设置监控模式事件*/
			if (evt_flag & AM_EPG_EVT_SET_MODE)
				am_epg_set_mode(mon, AM_TRUE);
			/*设置EIT PF自动更新间隔*/
			if (evt_flag & AM_EPG_EVT_SET_EITPF_CHECK_TIME)
			{
				/*如果应用将间隔设置为0，则立即启动过滤器，否则等待下次计算超时时启动*/
				if (mon->eitpf_check_time == 0)
				{
					if (mon->mon_service == -1)
						SET_MODE(eit, eit4ectl, AM_EPG_SCAN_EIT_PF_ACT, AM_FALSE);
					SET_MODE(eit, eit4fctl, AM_EPG_SCAN_EIT_PF_OTH, AM_FALSE);
				}
			}
			/*设置EIT Schedule自动更新间隔*/
			if (evt_flag & AM_EPG_EVT_SET_EITSCHE_CHECK_TIME)
			{
				/*如果应用将间隔设置为0，则立即启动过滤器，否则等待下次计算超时时启动*/
				if (mon->eitsche_check_time == 0)
				{
					SET_MODE(eit, eit50ctl, AM_EPG_SCAN_EIT_SCHE_ACT, AM_FALSE);
					SET_MODE(eit, eit51ctl, AM_EPG_SCAN_EIT_SCHE_ACT, AM_FALSE);
					SET_MODE(eit, eit60ctl, AM_EPG_SCAN_EIT_SCHE_ACT, AM_FALSE);
					SET_MODE(eit, eit61ctl, AM_EPG_SCAN_EIT_SCHE_ACT, AM_FALSE);
				}
			}

			/*设置当前监控的service*/
			if (evt_flag & AM_EPG_EVT_SET_MON_SRV)
			{
				int db_ts_id = am_epg_get_current_db_ts_id(mon);
				if (mon->curr_ts != db_ts_id)
				{
					AM_DEBUG(1, "TS changed, %d -> %d", mon->curr_ts, db_ts_id);
					mon->curr_ts = db_ts_id;
					SIGNAL_EVENT(AM_EPG_EVT_CHANGE_TS, (void*)db_ts_id);
				}
				mon->eit4ectl.subs = 1;
				/*重新设置PAT 和 EIT actual pf*/
				SET_MODE(pat, patctl, AM_EPG_SCAN_PAT, AM_TRUE);
				SET_MODE(eit, eit4ectl, AM_EPG_SCAN_EIT_PF_ACT, AM_TRUE);
			}

			/*退出事件*/
			if (evt_flag & AM_EPG_EVT_QUIT)
			{
				go = AM_FALSE;
				continue;
			}
			
			/*在调用am_epg_free_filter时可能会产生新事件*/
			mon->evt_flag &= ~evt_flag;
			if (mon->evt_flag)
			{
				goto handle_events;
			}
		}
	}
	
	/*线程退出，释放资源*/

	/*取消前端事件*/
	AM_EVT_Unsubscribe(mon->fend_dev, AM_FEND_EVT_STATUS_CHANGED, am_epg_fend_callback, (void*)mon);
	mon->mode = 0;
	am_epg_set_mode(mon, AM_FALSE);
	pthread_mutex_unlock(&mon->lock);
	
	/*等待DMX回调执行完毕*/
	AM_DMX_Sync(mon->dmx_dev);

	pthread_mutex_lock(&mon->lock);
	AM_SI_Destroy(mon->hsi);

	am_epg_tablectl_deinit(&mon->patctl);
	am_epg_tablectl_deinit(&mon->pmtctl);
	am_epg_tablectl_deinit(&mon->catctl);
	am_epg_tablectl_deinit(&mon->sdtctl);
	am_epg_tablectl_deinit(&mon->nitctl);
	am_epg_tablectl_deinit(&mon->totctl);
	am_epg_tablectl_deinit(&mon->eit4ectl);
	am_epg_tablectl_deinit(&mon->eit4fctl);
	am_epg_tablectl_deinit(&mon->eit50ctl);
	am_epg_tablectl_deinit(&mon->eit51ctl);
	am_epg_tablectl_deinit(&mon->eit60ctl);
	am_epg_tablectl_deinit(&mon->eit61ctl);
	am_epg_tablectl_deinit(&mon->sttctl);
	am_epg_tablectl_deinit(&mon->mgtctl);
	am_epg_tablectl_deinit(&mon->rrtctl);
	for (i=0; i<mon->psip_eit_count; i++)
	{
		am_epg_tablectl_deinit(&mon->psip_eitctl[i]);
	}
	
	pthread_mutex_unlock(&mon->lock);

	pthread_mutex_destroy(&mon->lock);
	pthread_cond_destroy(&mon->cond);

	return NULL;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 在指定源上创建一个EPG监控
 * \param [in] para 创建参数
 * \param [out] handle 返回句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_Create(AM_EPG_CreatePara_t *para, int *handle)
{
	AM_EPG_Monitor_t *mon;
	int rc;
	pthread_mutexattr_t mta;

	assert(handle && para);

	*handle = 0;

	mon = (AM_EPG_Monitor_t*)malloc(sizeof(AM_EPG_Monitor_t));
	if (! mon)
	{
		AM_DEBUG(1, "Create epg error, no enough memory");
		return AM_EPG_ERR_NO_MEM;
	}
	/*数据初始化*/
	memset(mon, 0, sizeof(AM_EPG_Monitor_t));
	mon->src = para->source;
	mon->fend_dev = para->fend_dev;
	mon->dmx_dev = para->dmx_dev;
	mon->eitpf_check_time = EITPF_CHECK_DISTANCE;
	mon->eitsche_check_time = EITSCHE_CHECK_DISTANCE;
	mon->mon_service = -1;
	mon->psip_eit_count = 2;
	AM_TIME_GetClock(&mon->new_eit_check_time);
	mon->eit_has_data = AM_FALSE;

	if (AM_SI_Create(&mon->hsi) != AM_SUCCESS)
	{
		AM_DEBUG(1, "Create epg error, cannot create si decoder");
		free(mon);
		return AM_EPG_ERR_CANNOT_CREATE_SI;
	}

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&mon->lock, &mta);
	pthread_cond_init(&mon->cond, NULL);
	pthread_mutexattr_destroy(&mta);
	/*创建监控线程*/
	rc = pthread_create(&mon->thread, NULL, am_epg_thread, (void*)mon);
	if(rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));
		pthread_mutex_destroy(&mon->lock);
		pthread_cond_destroy(&mon->cond);
		AM_SI_Destroy(mon->hsi);
		free(mon);
		return AM_EPG_ERR_CANNOT_CREATE_THREAD;
	}

	*handle = (int)mon;

	return AM_SUCCESS;
}

/**\brief 销毀一个EPG监控
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_Destroy(int handle)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t *)handle;
	pthread_t t;
	
	assert(mon);

	pthread_mutex_lock(&mon->lock);
	/*等待搜索线程退出*/
	mon->evt_flag |= AM_EPG_EVT_QUIT;
	t = mon->thread;
	pthread_mutex_unlock(&mon->lock);
	pthread_cond_signal(&mon->cond);

	if (t != pthread_self())
		pthread_join(t, NULL);

	free(mon);

	return AM_SUCCESS;
}

/**\brief 设置EPG监控模式
 * \param handle 句柄
 * \param op	修改操作，见AM_EPG_ModeOp
 * \param mode 监控模式，见AM_EPG_Mode
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_ChangeMode(int handle, int op, int mode)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(mon);

	if (op != AM_EPG_MODE_OP_ADD && op != AM_EPG_MODE_OP_REMOVE && op != AM_EPG_MODE_OP_SET)
	{
		AM_DEBUG(1, "Invalid EPG Mode");
		return AM_EPG_ERR_INVALID_PARAM;
	}
	
	pthread_mutex_lock(&mon->lock);
	if ((op == AM_EPG_MODE_OP_ADD && (mon->mode&mode) != mode) ||
		(op == AM_EPG_MODE_OP_REMOVE && (mon->mode&mode) != 0) ||
		(op == AM_EPG_MODE_OP_SET && mon->mode != mode))
	{
		AM_DEBUG(1, "Change EPG mode, 0x%x -> 0x%x", mon->mode, mode);
		mon->evt_flag |= AM_EPG_EVT_SET_MODE;
		if (op == AM_EPG_MODE_OP_ADD)
			mon->mode |= mode;
		else if (op == AM_EPG_MODE_OP_REMOVE)
			mon->mode &= ~mode;
		else
			mon->mode = mode;
		
		pthread_cond_signal(&mon->cond);
	}
	else
	{
		AM_DEBUG(1, "No need to change EPG mode, 0x%x -> 0x%x", mon->mode, mode);
	}
	
	pthread_mutex_unlock(&mon->lock);

	return AM_SUCCESS;
}

/**\brief 设置当前监控的service，监控其PMT和EIT actual pf
 * \param handle 句柄
 * \param service_id	需要监控的service的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_MonitorService(int handle, int db_srv_id)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(mon);

	pthread_mutex_lock(&mon->lock);
	mon->evt_flag |= AM_EPG_EVT_SET_MON_SRV;
	mon->mon_service = db_srv_id;
	pthread_cond_signal(&mon->cond);
	pthread_mutex_unlock(&mon->lock);

	return AM_SUCCESS;
}

/**\brief 设置EPG PF 自动更新间隔
 * \param handle 句柄
 * \param distance 检查间隔,ms单位
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_SetEITPFCheckDistance(int handle, int distance)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(mon);

	if (distance < 0)
	{
		AM_DEBUG(1, "Invalid check distance");
		return AM_EPG_ERR_INVALID_PARAM;
	}	
	
	pthread_mutex_lock(&mon->lock);
	mon->evt_flag |= AM_EPG_EVT_SET_EITPF_CHECK_TIME;
	mon->eitpf_check_time = distance;
	pthread_cond_signal(&mon->cond);
	pthread_mutex_unlock(&mon->lock);

	return AM_SUCCESS;
}

/**\brief 设置EPG Schedule 自动更新间隔
 * \param handle 句柄
 * \param distance 检查间隔,ms单位, 为0时将一直更新
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_SetEITScheCheckDistance(int handle, int distance)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(mon);

	if (distance < 0)
	{
		AM_DEBUG(1, "Invalid check distance");
		return AM_EPG_ERR_INVALID_PARAM;
	}	
	
	pthread_mutex_lock(&mon->lock);
	mon->evt_flag |= AM_EPG_EVT_SET_EITSCHE_CHECK_TIME;
	mon->eitsche_check_time = distance;
	pthread_cond_signal(&mon->cond);
	pthread_mutex_unlock(&mon->lock);

	return AM_SUCCESS;
}

/**\brief 字符编码转换
 * \param [in] in_code 需要转换的字符数据
 * \param in_len 需要转换的字符数据长度
 * \param [out] out_code 转换后的字符数据
 * \param out_len 输出字符缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_ConvertCode(char *in_code,int in_len,char *out_code,int out_len)
{
    iconv_t handle;
    char **pin=&in_code;
    char **pout=&out_code;
    char fbyte;
    char cod[32];
    
	if (!in_code || !out_code || in_len <= 0 || out_len <= 0)
		return AM_FAILURE;

	memset(out_code,0,out_len);
	 
	/*查找输入编码方式*/
	if (strcmp(FORCE_DEFAULT_CODE, ""))
	{
		/*强制将输入按默认编码处理*/
		strcpy(cod, FORCE_DEFAULT_CODE);
	}
	else if (in_len <= 1)
	{
		pin = &in_code;
		strcpy(cod, "ISO-8859-1");
	}
	else
	{
		fbyte = in_code[0];
		if (fbyte >= 0x01 && fbyte <= 0x0B)
			sprintf(cod, "ISO-8859-%d", fbyte + 4);
		else if (fbyte >= 0x0C && fbyte <= 0x0F)
		{
			/*Reserved for future use, we set to ISO8859-1*/
			strcpy(cod, "ISO-8859-1");
		}
		else if (fbyte == 0x10 && in_len >= 3)
		{
			uint16_t val = (uint16_t)(((uint16_t)in_code[1]<<8) | (uint16_t)in_code[2]);
			if (val >= 0x0001 && val <= 0x000F)
			{
				sprintf(cod, "ISO-8859-%d", val);
			}
			else
			{
				/*Reserved for future use, we set to ISO8859-1*/
				strcpy(cod, "ISO-8859-1");
			}
			in_code += 2;
			in_len -= 2;
		}
		else if (fbyte == 0x11)
			strcpy(cod, "UTF-16");
		else if (fbyte == 0x13)
			strcpy(cod, "GB2312");
		else if (fbyte == 0x14)
			strcpy(cod, "UCS-2BE");
		else if (fbyte == 0x15)
			strcpy(cod, "utf-8");
		else if (fbyte >= 0x20)
			strcpy(cod, "ISO6937");
		else
			return AM_FAILURE;

		/*调整输入*/
		if (fbyte < 0x20)
		{
			in_code++;
			in_len--;
		}
		pin = &in_code;
		
	}

	if (! strcmp(cod, "ISO6937"))
	{
		return am_epg_convert_iso6937_to_utf8(in_code,in_len,out_code,&out_len);
	}
	else
	{
		AM_DEBUG(7, "%s --> utf-8, in_len %d, out_len %d", cod, in_len, out_len);	
		handle=iconv_open("utf-8",cod);

		if (handle == (iconv_t)-1)
		{
			AM_DEBUG(1, "AM_EPG_ConvertCode iconv_open err: %s",strerror(errno));
			return AM_FAILURE;
		}

		if(iconv(handle,pin,(size_t *)&in_len,pout,(size_t *)&out_len) == -1)
		{
		    AM_DEBUG(1, "AM_EPG_ConvertCode iconv err: %s, in_len %d, out_len %d", strerror(errno), in_len, out_len);
		    iconv_close(handle);
		    return AM_FAILURE;
		}

		return iconv_close(handle);
	}
}

/**\brief 获得当前UTC时间
 * \param [out] utc_time UTC时间,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_GetUTCTime(int *utc_time)
{
#ifdef USE_TDT_TIME
	int now;
	
	assert(utc_time);

	AM_TIME_GetClock(&now);
	
	pthread_mutex_lock(&time_lock);
	*utc_time = curr_time.tdt_utc_time + (now - curr_time.tdt_sys_time)/1000;
	pthread_mutex_unlock(&time_lock);
#else
	*utc_time = (int)time(NULL);
#endif

	return AM_SUCCESS;
}

/**\brief 计算本地时间
 * \param utc_time UTC时间，单位为秒
 * \param [out] local_time 本地时间,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_UTCToLocal(int utc_time, int *local_time)
{
#ifdef USE_TDT_TIME
	int now;
	
	assert(local_time);

	AM_TIME_GetClock(&now);
	
	pthread_mutex_lock(&time_lock);
	*local_time = utc_time + curr_time.offset;
	pthread_mutex_unlock(&time_lock);
#else
	time_t utc, local;
	struct tm *gm;

	time(&utc);
	gm = gmtime(&utc);
	local = mktime(gm);
	local = utc_time + (utc - local);

	*local_time = (int)local;
#endif
	return AM_SUCCESS;
}

/**\brief 计算UTC时间
 * \param local_time 本地时间,单位为秒
 * \param [out] utc_time UTC时间，单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_LocalToUTC(int local_time, int *utc_time)
{
#ifdef USE_TDT_TIME
	int now;
	
	assert(local_time);

	AM_TIME_GetClock(&now);
	
	pthread_mutex_lock(&time_lock);
	*utc_time = local_time - curr_time.offset;
	pthread_mutex_unlock(&time_lock);
#else
	time_t local = (time_t)local_time;
	struct tm *gm;

	gm = gmtime(&local);
	*utc_time = (int)mktime(gm);
#endif
	return AM_SUCCESS;
}

/**\brief 设置时区偏移值
 * \param offset 偏移值,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_SetLocalOffset(int offset)
{
#ifdef USE_TDT_TIME
	pthread_mutex_lock(&time_lock);
	curr_time.offset = offset;
	pthread_mutex_unlock(&time_lock);
#endif
	return AM_SUCCESS;
}

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_SetUserData(int handle, void *user_data)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	if (mon)
	{
		pthread_mutex_lock(&mon->lock);
		mon->user_data = user_data;
		pthread_mutex_unlock(&mon->lock);
	}

	return AM_SUCCESS;
}

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_GetUserData(int handle, void **user_data)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(user_data);
	
	if (mon)
	{
		pthread_mutex_lock(&mon->lock);
		*user_data = mon->user_data;
		pthread_mutex_unlock(&mon->lock);
	}

	return AM_SUCCESS;
}

/**\brief 预约一个EPG事件，用于预约播放
 * \param handle EPG句柄
 * \param db_evt_id 事件的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_SubscribeEvent(int handle, int db_evt_id)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;
	AM_ErrorCode_t ret;
	sqlite3 *hdb;

	assert(mon && mon->hdb);
	
	pthread_mutex_lock(&mon->lock);
	AM_DB_HANDLE_PREPARE(hdb);
	ret = am_epg_subscribe_event(hdb, db_evt_id);
	if (ret == AM_SUCCESS && ! mon->evt_flag)
	{
		/*进行一次EPG预约事件时间检查*/
		AM_TIME_GetClock(&mon->sub_check_time);
		mon->sub_check_time -= EPG_SUB_CHECK_TIME;
		pthread_cond_signal(&mon->cond);
	}
	pthread_mutex_unlock(&mon->lock);

	return ret;
}

/**\brief 取消预约一个EPG事件
 * \param handle EPG句柄
 * \param db_evt_id 事件的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_UnsubscribeEvent(int handle, int db_evt_id)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;
	AM_ErrorCode_t ret;
	sqlite3 *hdb;

	assert(mon && mon->hdb);
	
	pthread_mutex_lock(&mon->lock);
	AM_DB_HANDLE_PREPARE(hdb);
	ret = am_epg_unsubscribe_event(hdb, db_evt_id);
	pthread_mutex_unlock(&mon->lock);

	return ret;
}


