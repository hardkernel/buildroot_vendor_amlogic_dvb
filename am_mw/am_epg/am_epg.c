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
#include <iconv.h>
#include <time.h>
#include <am_debug.h>
#include <assert.h>
#include <am_epg.h>
#include "am_epg_internal.h"
#include <am_time.h>
#include <am_dmx.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/*是否使用TDT时间*/
#define USE_TDT_TIME

/*子表最大个数*/
#define MAX_EIT4E_SUBTABLE_CNT 64
#define MAX_EIT_SUBTABLE_CNT 300

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
			AM_SI_ReleaseSection(mon->hsi, (void*)tmp);\
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

	if (sqlite3_exec(hdb, sql, NULL, NULL, &errmsg) != SQLITE_DONE)
	{
		AM_DEBUG(0, "Unsubscribe EPG event for db_id=%d failed, reason: %s", db_evt_id, errmsg ? errmsg : "Unknown");
		if (errmsg)
			sqlite3_free(errmsg);
		return AM_EPG_ERR_SUBSCRIBE_EVENT_FAILED;
	}

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
		if ((mcl->subctl[i].ver != 0xff) &&
			memcmp(mcl->subctl[i].mask, test_array, sizeof(test_array)))
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
		AM_DEBUG(1, "No more subctl for adding new subtable");
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
		if (sec_ctrl->pid != AM_SI_PID_TDT)
		{
			if (AM_SI_GetSectionHeader(mon->hsi, (uint8_t*)data, len, &header) != AM_SUCCESS)
			{
				AM_DEBUG(1, "EPG: section header error");
				pthread_mutex_unlock(&mon->lock);
				return;
			}
		
			/*该section是否已经接收过*/
			if (am_epg_tablectl_test_recved(sec_ctrl, &header))
			{
				AM_DEBUG(3,"%s section %d repeat!", sec_ctrl->tname, header.sec_num);
			
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
					AM_SI_ReleaseSection(mon->hsi, (void*)p_tot);

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
					AM_SI_ReleaseSection(mon->hsi, (void*)p_eit);

					if (! mon->eit_has_data)
						mon->eit_has_data = AM_TRUE;
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
					AM_SI_ReleaseSection(mon->hsi, (void*)p_stt);
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
	if (mon->mon_service != 0xffff && (mcl->tid == AM_SI_TID_PMT || mcl->tid == AM_SI_TID_EIT_PF_ACT))
	{
		AM_DEBUG(1, "Set filter for service %d", mon->mon_service);
		param.filter.filter[1] = (uint8_t)(mon->mon_service>>8);
		param.filter.filter[2] = (uint8_t)mon->mon_service;
		param.filter.mask[1] = 0xff;
		param.filter.mask[2] = 0xff;
	}

	if (mcl->pid != AM_SI_PID_TDT)
	{
		if (mcl->subctl->ver != 0xff && mcl->subs == 1)
		{
			AM_DEBUG(1, "Start filtering new version %d for %s table", mcl->subctl->ver, mcl->tname);
			/*Current next indicator must be 1*/
			param.filter.filter[3] = (mcl->subctl->ver << 1) | 0x01;
			param.filter.mask[3] = 0x3f;

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

/**\brief NIT搜索完毕处理*/
static void am_epg_nit_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->nitctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_NIT, (void*)mon->nits);

	/*监控下一版本*/
	if (mon->nitctl.subctl)
	{
		mon->nitctl.subctl->ver++;
		am_epg_request_section(mon, &mon->nitctl);
	}
}

/**\brief PAT搜索完毕处理*/
static void am_epg_pat_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->patctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_PAT, (void*)mon->pats);
	
	/*监控下一版本*/
	if (mon->patctl.subctl)
	{
		mon->patctl.subctl->ver++;
		am_epg_request_section(mon, &mon->patctl);
	}
}

/**\brief PMT搜索完毕处理*/
static void am_epg_pmt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->pmtctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_PMT, (void*)mon->pmts);
	
	/*监控下一版本*/
	if (mon->pmtctl.subctl)
	{
		mon->pmtctl.subctl->ver++;
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
		mon->catctl.subctl->ver++;
		am_epg_request_section(mon, &mon->catctl);
	}
}

/**\brief SDT搜索完毕处理*/
static void am_epg_sdt_done(AM_EPG_Monitor_t *mon)
{
	am_epg_free_filter(mon, &mon->sdtctl.fid);

	/*触发通知事件*/
	SIGNAL_EVENT(AM_EPG_EVT_NEW_SDT, (void*)mon->sdts);
	
	/*监控下一版本*/
	if (mon->sdtctl.subctl)
	{
		mon->sdtctl.subctl->ver++;
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

	if (mon->mon_service == 0xffff)
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
			mon->eit4ectl.subctl->ver++;
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
	am_epg_free_filter(mon, &mon->eit50ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit50ctl.check_time);
	mon->eit50ctl.data_arrive_time = 0;
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
	am_epg_free_filter(mon, &mon->eit60ctl.fid);

	/*设置完成时间以进行下一次刷新*/
	AM_TIME_GetClock(&mon->eit60ctl.check_time);
	mon->eit60ctl.data_arrive_time = 0;
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

/**\brief table control data init*/
static void am_epg_tablectl_data_init(AM_EPG_Monitor_t *mon)
{
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
							 0xff, "TDT/TOT", 1, am_epg_tdt_done, 0);
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
}

/**\brief 按照当前模式重新设置监控*/
static void am_epg_set_mode(AM_EPG_Monitor_t *mon, AM_Bool_t reset)
{
	SET_MODE(pat, patctl, AM_EPG_SCAN_PAT, reset);
	SET_MODE(pmt, pmtctl, AM_EPG_SCAN_PMT, reset);
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
	/*For ATSC, we only monitor STT now*/
	SET_MODE(stt, sttctl, AM_EPG_SCAN_STT, reset);
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
	if (mon->fe_evt.parameters.frequency == mon->curr_param.frequency)
		return;

	AM_DEBUG(1, "EPG change TS");
	mon->curr_param = mon->fe_evt.parameters;
	am_epg_reset(mon);
}

/**\brief 检查并通知已预约的EPG事件*/
static void am_epg_check_sub_events(AM_EPG_Monitor_t *mon)
{
	int now, row, db_evt_id;
	int expired[32];
	char sql[512];

	AM_EPG_GetUTCTime(&now);
	
	/*从evt_table中查找接下来即将开始播放的事件*/
	snprintf(sql, sizeof(sql), "select evt_table.db_id from evt_table left join srv_table on srv_table.db_id = evt_table.db_srv_id \
						where skip=0 and lock=0 and start>%d and start<=%d and sub_flag=1 and sub_status=0 order by start limit 1", 
						now, now+EPG_PRE_NOTIFY_TIME/1000);
	row = 1;
	if (AM_DB_Select(mon->hdb, sql, &row, "%d", &db_evt_id) == AM_SUCCESS && row > 0)
	{
		/*通知用户*/
		SIGNAL_EVENT(AM_EPG_EVT_NEW_SUB_PLAY, (void*)db_evt_id);

		/*更新预约播放状态*/
		snprintf(sql, sizeof(sql), "update evt_table set sub_status=1 where db_id=%d", db_evt_id);
		sqlite3_exec(mon->hdb, sql, NULL, NULL, NULL);
	}

	/*从evt_table中查找立即开始的事件*/
	snprintf(sql, sizeof(sql), "select evt_table.db_id from evt_table left join srv_table on srv_table.db_id = evt_table.db_srv_id \
						where skip=0 and lock=0 and start<=%d and start>=%d and end>%d and \
						sub_status=1 order by start limit 1", now, now-EPG_SUB_CHECK_TIME/1000, now);
	row = 1;
	if (AM_DB_Select(mon->hdb, sql, &row, "%d", &db_evt_id) == AM_SUCCESS && row > 0)
	{
		/*通知用户*/
		SIGNAL_EVENT(AM_EPG_EVT_SUB_PLAY_START, (void*)db_evt_id);

		/*通知播放后取消预约标志*/
		snprintf(sql, sizeof(sql), "update evt_table set sub_flag=0,sub_status=0 where db_id=%d", db_evt_id);
		sqlite3_exec(mon->hdb, sql, NULL, NULL, NULL);
	}

	/*从evt_table中查找过期的预约*/
	snprintf(sql, sizeof(sql), "select db_id from evt_table where sub_flag=1 and (start<%d or end<%d)", 
						now-EPG_SUB_CHECK_TIME/1000, now);
	row = AM_ARRAY_SIZE(expired);
	if (AM_DB_Select(mon->hdb, sql, &row, "%d", expired) == AM_SUCCESS && row > 0)
	{
		int i;
		for (i=0; i<row; i++)
		{
			/*取消预约标志*/
			AM_DEBUG(1, "@@Subscription %d expired.",  expired[i]);
			snprintf(sql, sizeof(sql), "update evt_table set sub_flag=0,sub_status=0 where db_id=%d", expired[i]);
			sqlite3_exec(mon->hdb, sql, NULL, NULL, NULL);
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
	if (mon->mon_service == 0xffff)
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
	int distance, ret, evt_flag;
	struct timespec rt;

	/*Reset the TDT time while epg start*/
	pthread_mutex_lock(&time_lock);
	memset(&curr_time, 0, sizeof(curr_time));
	AM_TIME_GetClock(&curr_time.tdt_sys_time);
	AM_DEBUG(0, "Start EPG Monitor thread now, curr_time.tdt_sys_time %d", curr_time.tdt_sys_time);
	pthread_mutex_unlock(&time_lock);

	/*控制数据初始化*/
	am_epg_tablectl_data_init(mon);

	/*获得当前前端参数*/
	AM_FEND_GetPara(mon->fend_dev, &mon->curr_param);

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
			/*设置监控模式事件*/
			if (evt_flag & AM_EPG_EVT_SET_MODE)
				am_epg_set_mode(mon, AM_TRUE);
			/*设置EIT PF自动更新间隔*/
			if (evt_flag & AM_EPG_EVT_SET_EITPF_CHECK_TIME)
			{
				/*如果应用将间隔设置为0，则立即启动过滤器，否则等待下次计算超时时启动*/
				if (mon->eitpf_check_time == 0)
				{
					if (mon->mon_service == 0xffff)
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
				mon->eit4ectl.subs = 1;
				/*重新设置PMT 和 EIT actual pf*/
				SET_MODE(pmt, pmtctl, AM_EPG_SCAN_PMT, AM_TRUE);
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
	
	pthread_mutex_unlock(&mon->lock);

	pthread_mutex_destroy(&mon->lock);
	pthread_cond_destroy(&mon->cond);
	free(mon);
	
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
	mon->dmx_dev = para-> dmx_dev;
	mon->hdb = para->hdb;
	mon->eitpf_check_time = EITPF_CHECK_DISTANCE;
	mon->eitsche_check_time = EITSCHE_CHECK_DISTANCE;
	mon->mon_service = 0xffff;
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

	if (op != AM_EPG_MODE_OP_ADD && op != AM_EPG_MODE_OP_REMOVE)
	{
		AM_DEBUG(1, "Invalid EPG Mode");
		return AM_EPG_ERR_INVALID_PARAM;
	}
	
	pthread_mutex_lock(&mon->lock);
	mon->evt_flag |= AM_EPG_EVT_SET_MODE;
	if (op == AM_EPG_MODE_OP_ADD)
		mon->mode |= mode;
	else
		mon->mode &= ~mode;
	pthread_cond_signal(&mon->cond);
	pthread_mutex_unlock(&mon->lock);

	return AM_SUCCESS;
}

/**\brief 设置当前监控的service，监控其PMT和EIT actual pf
 * \param handle 句柄
 * \param service_id	需要监控的service的service_id
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
AM_ErrorCode_t AM_EPG_MonitorService(int handle, uint16_t service_id)
{
	AM_EPG_Monitor_t *mon = (AM_EPG_Monitor_t*)handle;

	assert(mon);

	pthread_mutex_lock(&mon->lock);
	mon->evt_flag |= AM_EPG_EVT_SET_MON_SRV;
	mon->mon_service = service_id;
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
		return AM_SUCCESS;
	}
	else
	{
		fbyte = in_code[0];
		if (fbyte >= 0x01 && fbyte <= 0x0B)
			sprintf(cod, "ISO-8859-%d", fbyte + 4);
		else if (fbyte >= 0x0C && fbyte <= 0x0F)
		{
			/*Reserved for future use, we set to ISO-8859-1*/
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
				/*Reserved for future use, we set to ISO-8859-1*/
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
			strcpy(cod, "ISO-8859-1");
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

	AM_DEBUG(6, "%s --> utf-8, in_len %d, out_len %d", cod, in_len, out_len);	
    handle=iconv_open("utf-8",cod);

    if (handle == (iconv_t)-1)
    {
    	AM_DEBUG(1, "AM_EPG_ConvertCode iconv_open err");
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

	assert(mon && mon->hdb);
	
	pthread_mutex_lock(&mon->lock);
	ret = am_epg_subscribe_event(mon->hdb, db_evt_id);
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

	assert(mon && mon->hdb);
	
	pthread_mutex_lock(&mon->lock);
	ret = am_epg_unsubscribe_event(mon->hdb, db_evt_id);
	pthread_mutex_unlock(&mon->lock);

	return ret;
}


