/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_scan.c
 * \brief SCAN模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-27: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 2

#include <errno.h>
#include <assert.h>
#include <iconv.h>
#include <am_debug.h>
#include <am_scan.h>
#include "am_scan_internal.h"
#include <am_dmx.h>
#include <am_time.h>
#include <am_aout.h>
#include <am_av.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define SCAN_FEND_DEV_NO 0
#define SCAN_DMX_DEV_NO 0

/*位操作*/
#define BIT_MASK(b) (1 << ((b) % 8))
#define BIT_SLOT(b) ((b) / 8)
#define BIT_SET(a, b) ((a)[BIT_SLOT(b)] |= BIT_MASK(b))
#define BIT_CLEAR(a, b) ((a)[BIT_SLOT(b)] &= ~BIT_MASK(b))
#define BIT_TEST(a, b) ((a)[BIT_SLOT(b)] & BIT_MASK(b))

/*超时ms定义*/
#define PAT_TIMEOUT 6000
#define PMT_TIMEOUT 3000
#define SDT_TIMEOUT 6000
#define NIT_TIMEOUT 10000
#define CAT_TIMEOUT 3000
#define BAT_TIMEOUT 10000

/*子表最大个数*/
#define MAX_BAT_SUBTABLE_CNT 32

/*多子表时接收重复最小间隔*/
#define BAT_REPEAT_DISTANCE 3000

/*DVBC默认前端参数*/
#define DEFAULT_DVBC_MODULATION QAM_64
#define DEFAULT_DVBC_SYMBOLRATE 6875000

/*DVBT默认前端参数*/
#define DEFAULT_DVBT_BW BANDWIDTH_8_MHZ
#define DEFAULT_DVBT_FREQ_START    474000
#define DEFAULT_DVBT_FREQ_STOP     858000

/*电视广播排序是否统一编号*/
#define SORT_TOGETHER
	
/*清除一个subtable控制数据*/
#define SUBCTL_CLEAR(sc)\
	AM_MACRO_BEGIN\
		memset((sc)->mask, 0, sizeof((sc)->mask));\
		(sc)->ver = 0xff;\
	AM_MACRO_END

/*添加数据列表中*/
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
			AM_SI_ReleaseSection(scanner->hsi, (void*)tmp);\
			tmp = next;\
		}\
		(_l) = NULL;\
	AM_MACRO_END

/*解析section并添加到列表*/
#define COLLECT_SECTION(type, list)\
	AM_MACRO_BEGIN\
		type *p_table;\
		if (AM_SI_DecodeSection(scanner->hsi, sec_ctrl->pid, (uint8_t*)data, len, (void**)&p_table) == AM_SUCCESS)\
		{\
			p_table->p_next = NULL;\
			ADD_TO_LIST(p_table, list); /*添加到搜索结果列表中*/\
			am_scan_tablectl_mark_section(sec_ctrl, &header); /*设置为已接收*/\
		}\
	AM_MACRO_END

/*通知一个事件*/
#define SIGNAL_EVENT(e, d)\
	AM_MACRO_BEGIN\
		pthread_mutex_unlock(&scanner->lock);\
		AM_EVT_Signal((int)scanner, e, d);\
		pthread_mutex_lock(&scanner->lock);\
	AM_MACRO_END

/*触发一个进度事件*/
#define SET_PROGRESS_EVT(t, d)\
	AM_MACRO_BEGIN\
		AM_SCAN_Progress_t prog;\
		prog.evt = t;\
		prog.data = (void*)d;\
		SIGNAL_EVENT(AM_SCAN_EVT_PROGRESS, (void*)&prog);\
	AM_MACRO_END

/****************************************************************************
 * static data
 ***************************************************************************/

/*DVB-C标准频率表*/
static int dvbc_std_freqs[] = 
{
52500,  60250,  68500,  80000,
88000,  115000, 123000, 131000,
139000, 147000, 155000, 163000,
171000, 179000, 187000, 195000,
203000, 211000, 219000, 227000,
235000, 243000, 251000, 259000,
267000, 275000, 283000, 291000,
299000, 307000, 315000, 323000,
331000, 339000, 347000, 355000,
363000, 371000, 379000, 387000,
395000, 403000, 411000, 419000,
427000, 435000, 443000, 451000,
459000, 467000, 474000, 482000,
490000, 498000, 506000, 514000,
522000, 530000, 538000, 546000,
554000, 562000, 570000, 578000,
586000, 594000, 602000, 610000,
618000, 626000, 634000, 642000,
650000, 658000, 666000, 674000,
682000, 690000, 698000, 706000,
714000, 722000, 730000, 738000,
746000, 754000, 762000, 770000,
778000, 786000, 794000, 802000,
810000, 818000, 826000, 834000,
842000, 850000, 858000, 866000,
874000
};


/*SQLite3 stmts*/
const char *sql_stmts[MAX_STMT] = 
{
	"select db_id from net_table where src=? and network_id=?",
	"insert into net_table(network_id, src) values(?,?)",
	"update net_table set name=? where db_id=?",
	"select db_id from ts_table where src=? and freq=?",
	"insert into ts_table(src,freq) values(?,?)",
	"update ts_table set db_net_id=?,ts_id=?,symb=?,mod=?,bw=?,snr=?,ber=?,strength=? where db_id=?",
	"delete  from srv_table where db_ts_id=?",
	"select db_id from srv_table where db_net_id=? and db_ts_id=? and service_id=?",
	"insert into srv_table(db_net_id, db_ts_id,service_id) values(?,?,?)",
	"update srv_table set src=?, name=?,service_type=?,eit_schedule_flag=?, eit_pf_flag=?, running_status=?, free_ca_mode=?, volume=?, aud_track=?, vid_pid=?, aud1_pid=?, aud2_pid=?,vid_fmt=?,aud1_fmt=?,aud2_fmt=?,skip=0,lock=0,chan_num=0 where db_id=?",
	"select db_id,service_type from srv_table where db_ts_id=? order by service_id",
	"update srv_table set chan_num=? where db_id=?",
	"delete  from evt_table where db_ts_id=?",
	"select db_id from ts_table where src=? order by freq",
	"delete from grp_map_table where db_srv_id in (select db_id from srv_table where db_ts_id=?)",
	"insert into subtitle_table(db_srv_id,pid,type,composition_page_id,ancillary_page_id,language) values(?,?,?,?,?,?)",
	"insert into teletext_table(db_srv_id,pid,type,magazine_number,page_number,language) values(?,?,?,?,?,?)",
	"delete from subtitle_table where db_srv_id in (select db_id from srv_table where db_ts_id=?)",
	"delete from teletext_table where db_srv_id in (select db_id from srv_table where db_ts_id=?)",
};

/****************************************************************************
 * static functions
 ***************************************************************************/
static AM_ErrorCode_t am_scan_start_next_ts(AM_SCAN_Scanner_t *scanner);
static AM_ErrorCode_t am_scan_request_next_pmt(AM_SCAN_Scanner_t *scanner);
static AM_ErrorCode_t am_scan_try_nit(AM_SCAN_Scanner_t *scanner);
extern AM_ErrorCode_t AM_EPG_ConvertCode(char *in_code,int in_len,char *out_code,int out_len);

/**\brief 插入一个网络记录，返回其索引*/
static int insert_net(sqlite3_stmt **stmts, int src, int orig_net_id)
{
	int db_id = -1;

	/*query wether it exists*/
	sqlite3_bind_int(stmts[QUERY_NET], 1, src);
	sqlite3_bind_int(stmts[QUERY_NET], 2, orig_net_id);
	if (sqlite3_step(stmts[QUERY_NET]) == SQLITE_ROW)
	{
		db_id = sqlite3_column_int(stmts[QUERY_NET], 0);
	}
	sqlite3_reset(stmts[QUERY_NET]);

	/*if not exist , insert a new record*/
	if (db_id == -1)
	{
		sqlite3_bind_int(stmts[INSERT_NET], 1, orig_net_id);
		sqlite3_bind_int(stmts[INSERT_NET], 2, src);
		if (sqlite3_step(stmts[INSERT_NET]) == SQLITE_DONE)
		{
			sqlite3_bind_int(stmts[QUERY_NET], 1, src);
			sqlite3_bind_int(stmts[QUERY_NET], 2, orig_net_id);
			if (sqlite3_step(stmts[QUERY_NET]) == SQLITE_ROW)
			{
				db_id = sqlite3_column_int(stmts[QUERY_NET], 0);
			}
			sqlite3_reset(stmts[QUERY_NET]);
		}
		sqlite3_reset(stmts[INSERT_NET]);
	}

	return db_id;
}

/**\brief 插入一个TS记录，返回其索引*/
static int insert_ts(sqlite3_stmt **stmts, int src, int freq)
{
	int db_id = -1;

	/*query wether it exists*/
	sqlite3_bind_int(stmts[QUERY_TS], 1, src);
	sqlite3_bind_int(stmts[QUERY_TS], 2, freq);
	if (sqlite3_step(stmts[QUERY_TS]) == SQLITE_ROW)
	{
		db_id = sqlite3_column_int(stmts[QUERY_TS], 0);
	}
	sqlite3_reset(stmts[QUERY_TS]);

	/*if not exist , insert a new record*/
	if (db_id == -1)
	{
		sqlite3_bind_int(stmts[INSERT_TS], 1, src);
		sqlite3_bind_int(stmts[INSERT_TS], 2, freq);
		if (sqlite3_step(stmts[INSERT_TS]) == SQLITE_DONE)
		{
			sqlite3_bind_int(stmts[QUERY_TS], 1, src);
			sqlite3_bind_int(stmts[QUERY_TS], 2, freq);
			if (sqlite3_step(stmts[QUERY_TS]) == SQLITE_ROW)
			{
				db_id = sqlite3_column_int(stmts[QUERY_TS], 0);
			}
			sqlite3_reset(stmts[QUERY_TS]);
		}
		sqlite3_reset(stmts[INSERT_TS]);
	}

	return db_id;
}

/**\brief 插入一个SRV记录，返回其索引*/
static int insert_srv(sqlite3_stmt **stmts, int db_net_id, int db_ts_id, int srv_id)
{
	int db_id = -1;

	/*query wether it exists*/
	sqlite3_bind_int(stmts[QUERY_SRV], 1, db_net_id);
	sqlite3_bind_int(stmts[QUERY_SRV], 2, db_ts_id);
	sqlite3_bind_int(stmts[QUERY_SRV], 3, srv_id);
	if (sqlite3_step(stmts[QUERY_SRV]) == SQLITE_ROW)
	{
		db_id = sqlite3_column_int(stmts[QUERY_SRV], 0);
	}
	sqlite3_reset(stmts[QUERY_SRV]);

	/*if not exist , insert a new record*/
	if (db_id == -1)
	{
		sqlite3_bind_int(stmts[INSERT_SRV], 1, db_net_id);
		sqlite3_bind_int(stmts[INSERT_SRV], 2, db_ts_id);
		sqlite3_bind_int(stmts[INSERT_SRV], 3, srv_id);
		if (sqlite3_step(stmts[INSERT_SRV]) == SQLITE_DONE)
		{
			sqlite3_bind_int(stmts[QUERY_SRV], 1, db_net_id);
			sqlite3_bind_int(stmts[QUERY_SRV], 2, db_ts_id);
			sqlite3_bind_int(stmts[QUERY_SRV], 3, srv_id);
			if (sqlite3_step(stmts[QUERY_SRV]) == SQLITE_ROW)
			{
				db_id = sqlite3_column_int(stmts[QUERY_SRV], 0);
			}
			sqlite3_reset(stmts[QUERY_SRV]);
		}
		sqlite3_reset(stmts[INSERT_SRV]);
	}

	return db_id;
}

/**\brief 插入一个Subtitle记录*/
static int insert_subtitle(sqlite3 * hdb, sqlite3_stmt **stmts, int db_srv_id, int pid, dvbpsi_subtitle_t *psd)
{
	sqlite3_bind_int(stmts[INSERT_SUBTITLE], 1, db_srv_id);
	sqlite3_bind_int(stmts[INSERT_SUBTITLE], 2, pid);
	sqlite3_bind_int(stmts[INSERT_SUBTITLE], 3, psd->i_subtitling_type);
	sqlite3_bind_int(stmts[INSERT_SUBTITLE], 4, psd->i_composition_page_id);
	sqlite3_bind_int(stmts[INSERT_SUBTITLE], 5, psd->i_ancillary_page_id);
	sqlite3_bind_text(stmts[INSERT_SUBTITLE], 6, (const char*)psd->i_iso6392_language_code, 3, SQLITE_STATIC);
	AM_DEBUG(1, "Insert a new subtitle");
	sqlite3_step(stmts[INSERT_SUBTITLE]);
	sqlite3_reset(stmts[INSERT_SUBTITLE]);

	return 0;
}

/**\brief 插入一个Teletext记录*/
static int insert_teletext(sqlite3_stmt **stmts, int db_srv_id, int pid, dvbpsi_teletextpage_t *ptd)
{
	sqlite3_bind_int(stmts[INSERT_TELETEXT], 1, db_srv_id);
	sqlite3_bind_int(stmts[INSERT_TELETEXT], 2, pid);
	if (ptd)
	{
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 3, ptd->i_teletext_type);
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 4, ptd->i_teletext_magazine_number);
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 5, ptd->i_teletext_page_number);
		sqlite3_bind_text(stmts[INSERT_TELETEXT], 6, (const char*)ptd->i_iso6392_language_code, 3, SQLITE_STATIC);
	}
	else
	{
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 3, 0);
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 4, 0);
		sqlite3_bind_int(stmts[INSERT_TELETEXT], 5, 0);
		sqlite3_bind_text(stmts[INSERT_TELETEXT], 6, "", -1, SQLITE_STATIC);
	}
	AM_DEBUG(1, "Insert a new teletext");
	sqlite3_step(stmts[INSERT_TELETEXT]);
	sqlite3_reset(stmts[INSERT_TELETEXT]);

	return 0;
}

/**\brief 存储一个TS到数据库*/
static void store_ts(sqlite3_stmt **stmts, AM_SCAN_Result_t *result, AM_SCAN_TS_t *ts)
{
	dvbpsi_pmt_t *pmt;
	dvbpsi_sdt_t *sdt;
	dvbpsi_pmt_es_t *es;
	dvbpsi_sdt_service_t *srv;
	dvbpsi_descriptor_t *descr;
	dvbpsi_nit_t *nit;
	int net_dbid,dbid, srv_dbid;
	int orig_net_id = -1;
	char selbuf[256];
	char insbuf[400];
	sqlite3 *hdb = result->hdb;
	
	/*没有PAT，不存储*/
	if (!ts->pats)
	{
		AM_DEBUG(1, "No PAT found in ts, will not store to dbase");
		return;
	}

	/*获取所在网络*/
	if (ts->sdts)
	{
		/*按照SDT中描述的orignal_network_id查找网络记录，不存在则新添加一个network记录*/
		orig_net_id = ts->sdts->i_network_id;
	}
	else if (result->nits)
	{
		/*在自动搜索时按NIT表描述的network_id查找网络*/
		orig_net_id = result->nits->i_network_id;
		
	}

	if (orig_net_id != -1)
	{
		net_dbid = insert_net(stmts, result->src, orig_net_id);
		if (net_dbid != -1)
		{
			char netname[256];

			netname[0] = '\0';
			/*新增加一个network记录*/
			if (result->nits && (orig_net_id == result->nits->i_network_id))
			{
				AM_SI_LIST_BEGIN(result->nits, nit)
				AM_SI_LIST_BEGIN(nit->p_first_descriptor, descr)
				if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_NETWORK_NAME)
				{
					dvbpsi_network_name_dr_t *pnn = (dvbpsi_network_name_dr_t*)descr->p_decoded;

					/*取网络名称*/
					if (descr->i_length > 0)
					{
						AM_EPG_ConvertCode((char*)pnn->i_network_name, descr->i_length,\
									netname, 255);
						netname[255] = 0;
						break;
					}
				}
				AM_SI_LIST_END()	
				AM_SI_LIST_END()
			}
			
			AM_DEBUG(0, "###Network Name is '%s'", netname);
			sqlite3_bind_text(stmts[UPDATE_NET], 1, netname, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmts[UPDATE_NET], 2, net_dbid);
			sqlite3_step(stmts[UPDATE_NET]);
			sqlite3_reset(stmts[UPDATE_NET]);
		}
		else
		{
			AM_DEBUG(1, "insert new network error");
			return;
		}
	}
	else
	{
		/*没有找到有效的orignal_network_id,则网络标识为无效*/
		net_dbid = -1;
	}
		
	/*检查该TS是否已经添加*/
	dbid = insert_ts(stmts, result->src, (int)ts->fend_para.frequency);
	if (dbid == -1)
	{
		AM_DEBUG(1, "insert new ts error");
		return;
	}
	/*更新TS数据*/
	sqlite3_bind_int(stmts[UPDATE_TS], 1, net_dbid);
	sqlite3_bind_int(stmts[UPDATE_TS], 2, ts->pats->i_ts_id);
	sqlite3_bind_int(stmts[UPDATE_TS], 3, (int)ts->fend_para.u.qam.symbol_rate);
	sqlite3_bind_int(stmts[UPDATE_TS], 4, (int)ts->fend_para.u.qam.modulation);
	sqlite3_bind_int(stmts[UPDATE_TS], 5, (int)ts->fend_para.u.ofdm.bandwidth);
	sqlite3_bind_int(stmts[UPDATE_TS], 6, ts->snr);
	sqlite3_bind_int(stmts[UPDATE_TS], 7, ts->ber);
	sqlite3_bind_int(stmts[UPDATE_TS], 8, ts->strength);
	sqlite3_bind_int(stmts[UPDATE_TS], 9, dbid);
	sqlite3_step(stmts[UPDATE_TS]);
	sqlite3_reset(stmts[UPDATE_TS]);

	/*清除与该TS下service关联的分组数据*/
	sqlite3_bind_int(stmts[DELETE_SRV_GRP], 1, dbid);
	sqlite3_step(stmts[DELETE_SRV_GRP]);
	sqlite3_reset(stmts[DELETE_SRV_GRP]);

	/*清除该TS下所有subtitles,teletexts*/
	AM_DEBUG(1, "Delete all subtitles & teletexts in TS %d", dbid);
	sqlite3_bind_int(stmts[DELETE_TS_SUBTITLES], 1, dbid);
	sqlite3_step(stmts[DELETE_TS_SUBTITLES]);
	sqlite3_reset(stmts[DELETE_TS_SUBTITLES]);
	sqlite3_bind_int(stmts[DELETE_TS_TELETEXTS], 1, dbid);
	sqlite3_step(stmts[DELETE_TS_TELETEXTS]);
	sqlite3_reset(stmts[DELETE_TS_TELETEXTS]);
	
	/*清除该TS下所有service*/
	sqlite3_bind_int(stmts[DELETE_TS_SRVS], 1, dbid);
	sqlite3_step(stmts[DELETE_TS_SRVS]);
	sqlite3_reset(stmts[DELETE_TS_SRVS]);

	/*清除该TS下所有events*/
	AM_DEBUG(1, "Delete all events in TS %d", dbid);
	sqlite3_bind_int(stmts[DELETE_TS_EVTS], 1, dbid);
	sqlite3_step(stmts[DELETE_TS_EVTS]);
	sqlite3_reset(stmts[DELETE_TS_EVTS]);

	
	/*遍历PMT表*/
	AM_SI_LIST_BEGIN(ts->pmts, pmt)
		uint16_t vid, aid1, aid2, srv_id;
		uint8_t srv_type, eit_sche, eit_pf, rs, free_ca;
		int afmt1, afmt2, vfmt, avfmt;
		char name[AM_DB_MAX_SRV_NAME_LEN + 1];

		name[0] = '\0';
		vid = aid1 = aid2 = 0x1fff;
		srv_id = pmt->i_program_number;
		srv_type = 0;
		eit_sche = 0;
		eit_pf = 0;
		rs = 0;
		free_ca = 1;
		vfmt = 0;
		afmt1 = afmt2 = 0;
		
		/*添加新业务到数据库*/
		srv_dbid = insert_srv(stmts, net_dbid, dbid, srv_id);
		if (srv_dbid == -1)
		{
			AM_DEBUG(1, "insert new srv error");
			continue;
		}

		/*取ES流信息*/
		AM_SI_LIST_BEGIN(pmt->p_first_es, es)
			avfmt = -1;
			
			switch (es->i_type)
			{
				/*override by parse descriptor*/
				case 0x6:
					AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
						if (descr->i_tag == AM_SI_DESCR_AC3 || 
						descr->i_tag == AM_SI_DESCR_ENHANCED_AC3)
						{
							AM_DEBUG(0, "!!Found AC3 Descriptor!!!");
							avfmt = AFORMAT_AC3;
						}
						else if (descr->i_tag == AM_SI_DESCR_AAC)
						{
							AM_DEBUG(0, "!!Found AAC Descriptor!!!");
							avfmt = AFORMAT_AAC;
						}
						else if (descr->i_tag == AM_SI_DESCR_DTS)
						{
							AM_DEBUG(0, "!!Found DTS Descriptor!!!");
							avfmt = AFORMAT_DTS;
						}
						
						if (avfmt != -1)
						{
							if (aid1 == 0x1fff)
							{
								aid1 = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
								afmt1 = avfmt;
							}
							else if (aid2 == 0x1fff)
							{
								aid2 = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
								afmt2 = avfmt;
							}
							break;
						}
						
					AM_SI_LIST_END()
					break;
				/*video pid and video format*/
				case 0x1:
				case 0x2:
					if (avfmt == -1)
						avfmt = VFORMAT_MPEG12;
				case 0x10:
					if (avfmt == -1)
						avfmt = VFORMAT_MPEG4;
				case 0x1b:
					if (avfmt == -1)
						avfmt = VFORMAT_H264;
				case 0xea:
					if (avfmt == -1)
						avfmt = VFORMAT_VC1;
					if (vid == 0x1fff)
					{
						vid = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
						AM_DEBUG(0, "Set video format to %d", avfmt);
						vfmt = avfmt;
					}
					break;
				/*audio pid and audio format*/
				case 0x3:
				case 0x4:
					if (avfmt == -1)
						avfmt = AFORMAT_MPEG;
				case 0x0f:
				case 0x11:
					if (avfmt == -1)
						avfmt = AFORMAT_AAC;
				case 0x81:
					if (avfmt == -1)
						avfmt = AFORMAT_AC3;
				case 0x8A:
		                case 0x82:
		                case 0x85:
		                case 0x86:
		                	if (avfmt == -1)
		                		avfmt = AFORMAT_DTS;
	                		if (aid1 == 0x1fff)
	                		{
						aid1 = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
						AM_DEBUG(0, "Set audio1 format to %d", avfmt);
						afmt1 = avfmt;
					}
					else if (aid2 == 0x1fff)
					{
						AM_DEBUG(0, "Set audio2 format to %d", avfmt);
						aid2 = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;
						afmt2 = avfmt;
					}
					break;
				default:
					break;
			}

			/*查找Subtilte和Teletext描述符，并添加相关记录*/
			AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
				if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SUBTITLING)
				{
					int isub;
					dvbpsi_subtitling_dr_t *psd = (dvbpsi_subtitling_dr_t*)descr->p_decoded;

					AM_DEBUG(0, "Find subtitle descriptor, number:%d",psd->i_subtitles_number);
					for (isub=0; isub<psd->i_subtitles_number; isub++)
					{
						insert_subtitle(result->hdb, stmts, srv_dbid, es->i_pid, &psd->p_subtitle[isub]);
					}
				}
				else if (descr->i_tag == AM_SI_DESCR_TELETEXT)
				{
					int itel;
					dvbpsi_teletext_dr_t *ptd = (dvbpsi_teletext_dr_t*)descr->p_decoded;

					if (ptd)
					{
						for (itel=0; itel<ptd->i_pages_number; itel++)
						{
							insert_teletext(stmts, srv_dbid, es->i_pid, &ptd->p_pages[itel]);
						}
					}
					else
					{
						insert_teletext(stmts, srv_dbid, es->i_pid, NULL);
					}
				}
			AM_SI_LIST_END()
		AM_SI_LIST_END()

		AM_SI_LIST_BEGIN(ts->sdts, sdt)
		AM_SI_LIST_BEGIN(sdt->p_first_service, srv)
			/*从SDT表中查找该service并获取信息*/
			if (srv->i_service_id == srv_id)
			{
				AM_DEBUG(0 ,"SDT for service %d found!", srv_id);
				eit_sche = (uint8_t)srv->b_eit_schedule;
				eit_pf = (uint8_t)srv->b_eit_present;
				rs = srv->i_running_status;
				free_ca = (uint8_t)srv->b_free_ca;
				
				AM_SI_LIST_BEGIN(srv->p_first_descriptor, descr)
				if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SERVICE)
				{
					dvbpsi_service_dr_t *psd = (dvbpsi_service_dr_t*)descr->p_decoded;
				
					/*取节目名称*/
					if (psd->i_service_name_length > 0)
					{
						AM_EPG_ConvertCode((char*)psd->i_service_name, psd->i_service_name_length,\
									name, AM_DB_MAX_SRV_NAME_LEN);
						name[AM_DB_MAX_SRV_NAME_LEN] = 0;
					}
					/*业务类型*/
					srv_type = psd->i_service_type;
					
					/*跳出多层循环*/
					goto SDT_END;
				}
				AM_SI_LIST_END()
			}
		AM_SI_LIST_END()
		AM_SI_LIST_END()
		
SDT_END:
	//if (vid != 0x1fff || aid1 != 0x1fff)
	{
		/*对于SDT没有描述srv_type但音视频PID有效的节目，按电视或广播节目存储*/
		if (vid != 0x1fff && srv_type == 0)
			srv_type = 0x1;
		else if (vid == 0x1fff && aid1 != 0x1fff && srv_type == 0)
			srv_type = 0x2;

		/*SDT描述为TV 或 Radio的节目，但音视频PID无效，不存储为TV或Radio*/
		if (vid == 0x1fff && aid1 == 0x1fff && (srv_type == 1 || srv_type == 2))
		{
			srv_type = 0;
		}

		if (!strcmp(name, "") && (srv_type == 0x1 || srv_type == 0x2))
			strcpy(name, "No Name");
				
		sqlite3_bind_int(stmts[UPDATE_SRV], 1, result->src);
		sqlite3_bind_text(stmts[UPDATE_SRV], 2, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_int(stmts[UPDATE_SRV], 3, srv_type);
		sqlite3_bind_int(stmts[UPDATE_SRV], 4, eit_sche);
		sqlite3_bind_int(stmts[UPDATE_SRV], 5, eit_pf);
		sqlite3_bind_int(stmts[UPDATE_SRV], 6, rs);
		sqlite3_bind_int(stmts[UPDATE_SRV], 7, free_ca);
		sqlite3_bind_int(stmts[UPDATE_SRV], 8, 50);
		sqlite3_bind_int(stmts[UPDATE_SRV], 9, AM_AOUT_OUTPUT_DUAL_LEFT);
		sqlite3_bind_int(stmts[UPDATE_SRV], 10, vid);
		sqlite3_bind_int(stmts[UPDATE_SRV], 11, aid1);
		sqlite3_bind_int(stmts[UPDATE_SRV], 12, aid2);
		sqlite3_bind_int(stmts[UPDATE_SRV], 13, vfmt);
		sqlite3_bind_int(stmts[UPDATE_SRV], 14, afmt1);
		sqlite3_bind_int(stmts[UPDATE_SRV], 15, afmt2);
		sqlite3_bind_int(stmts[UPDATE_SRV], 16, srv_dbid);
		sqlite3_step(stmts[UPDATE_SRV]);
		sqlite3_reset(stmts[UPDATE_SRV]);
	
		AM_DEBUG(1, "Updating '%s' OK", name);
	}
	
	AM_SI_LIST_END()
}

/**\brief 默认搜索完毕存储函数*/
static void am_scan_default_store(AM_SCAN_Result_t *result)
{
	AM_SCAN_TS_t *ts;
	char sqlstr[128];
	sqlite3 *hdb = result->hdb;
	sqlite3_stmt	*stmts[MAX_STMT];
	int i;
	
	assert(result);

	/*Prepare sqlite3 stmts*/
	memset(stmts, 0, sizeof(sqlite3_stmt*) * MAX_STMT);
	for (i=0; i<MAX_STMT; i++)
	{
		if (sqlite3_prepare(hdb, sql_stmts[i], -1, &stmts[i], NULL) != SQLITE_OK)
		{
			AM_DEBUG(0, "Prepare sqlite3 failed");
			goto store_end;
		}
	}
	
	/*自动搜索和全频段搜索时删除该源下的所有信息*/
	if (!(result->mode & AM_SCAN_MODE_MANUAL) && result->tses)
	{
		/*删除network记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from net_table where src=%d",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		/*清空TS记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from ts_table where src=%d",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		/*清空service group记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from grp_map_table where db_srv_id in (select db_id from srv_table where src=%d)",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		/*清空subtitle teletext记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from subtitle_table where db_srv_id in (select db_id from srv_table where src=%d)",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		snprintf(sqlstr, sizeof(sqlstr), "delete from teletext_table where db_srv_id in (select db_id from srv_table where src=%d)",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		/*清空SRV记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from srv_table where src=%d",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
		/*清空event记录*/
		snprintf(sqlstr, sizeof(sqlstr), "delete from evt_table where src=%d",result->src);
		sqlite3_exec(hdb, sqlstr, NULL, NULL, NULL);
	}
	
	/*依次存储每个TS*/
	AM_SI_LIST_BEGIN(result->tses, ts)
		store_ts(stmts, result, ts);
	AM_SI_LIST_END()

	if (result->tses)
	{
		int *srv_dbids;
		int row = AM_DB_MAX_SRV_CNT_PER_SRC, i, j;
		int r, db_ts_id, db_id, rr, srv_type;
		
		/*重新对srv_table排序以生成新的频道号*/
		/*首先按频点排序*/
		sqlite3_bind_int(stmts[QUERY_TS_BY_FREQ_ORDER], 1, result->src);
		r = sqlite3_step(stmts[QUERY_TS_BY_FREQ_ORDER]);
		i=1;
		j=1;
		while (r == SQLITE_ROW)
		{
			/*同频点下按service_id排序*/
			db_ts_id = sqlite3_column_int(stmts[QUERY_TS_BY_FREQ_ORDER], 0);
			sqlite3_bind_int(stmts[QUERY_SRV_BY_TYPE], 1, db_ts_id);
			rr = sqlite3_step(stmts[QUERY_SRV_BY_TYPE]);
			while (rr == SQLITE_ROW)
			{
				db_id = sqlite3_column_int(stmts[QUERY_SRV_BY_TYPE], 0);
				srv_type = sqlite3_column_int(stmts[QUERY_SRV_BY_TYPE], 1);
				if (srv_type == 1)
				{
					/*电视节目*/
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 1, i);
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 2, db_id);
					sqlite3_step(stmts[UPDATE_CHAN_NUM]);
					sqlite3_reset(stmts[UPDATE_CHAN_NUM]);
					i++;
				}
#ifndef SORT_TOGETHER 
				else if (srv_type == 2)
				{
					/*广播节目*/
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 1, j);
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 2, db_id);
					sqlite3_step(stmts[UPDATE_CHAN_NUM]);
					sqlite3_reset(stmts[UPDATE_CHAN_NUM]);
					j++;
				}

				rr = sqlite3_step(stmts[QUERY_SRV_BY_TYPE]);
			}
			sqlite3_reset(stmts[QUERY_SRV_BY_TYPE]);
#else
				rr = sqlite3_step(stmts[QUERY_SRV_BY_TYPE]);
			}
			sqlite3_reset(stmts[QUERY_SRV_BY_TYPE]);
			r = sqlite3_step(stmts[QUERY_TS_BY_FREQ_ORDER]);
		}
		sqlite3_reset(stmts[QUERY_TS_BY_FREQ_ORDER]);
		sqlite3_bind_int(stmts[QUERY_TS_BY_FREQ_ORDER], 1, result->src);
		r = sqlite3_step(stmts[QUERY_TS_BY_FREQ_ORDER]);
		while (r == SQLITE_ROW)
		{
			/*广播节目放到最后*/
			db_ts_id = sqlite3_column_int(stmts[QUERY_TS_BY_FREQ_ORDER], 0);
			sqlite3_bind_int(stmts[QUERY_SRV_BY_TYPE], 1, db_ts_id);
			rr = sqlite3_step(stmts[QUERY_SRV_BY_TYPE]);
			while (rr == SQLITE_ROW)
			{
				db_id = sqlite3_column_int(stmts[QUERY_SRV_BY_TYPE], 0);
				srv_type = sqlite3_column_int(stmts[QUERY_SRV_BY_TYPE], 1);
				if (srv_type == 2)
				{
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 1, i);
					sqlite3_bind_int(stmts[UPDATE_CHAN_NUM], 2, db_id);
					sqlite3_step(stmts[UPDATE_CHAN_NUM]);
					sqlite3_reset(stmts[UPDATE_CHAN_NUM]);
					i++;
				}
				rr = sqlite3_step(stmts[QUERY_SRV_BY_TYPE]);
			}
			sqlite3_reset(stmts[QUERY_SRV_BY_TYPE]);
#endif
			r = sqlite3_step(stmts[QUERY_TS_BY_FREQ_ORDER]);
		}
		sqlite3_reset(stmts[QUERY_TS_BY_FREQ_ORDER]);
	}

store_end:
	for (i=0; i<MAX_STMT; i++)
	{
		if (stmts[i] != NULL)
			sqlite3_finalize(stmts[i]);
	}
}

/**\brief 清空一个表控制标志*/
static void am_scan_tablectl_clear(AM_SCAN_TableCtl_t * scl)
{
	scl->data_arrive_time = 0;
	
	if (scl->subs && scl->subctl)
	{
		int i;

		memset(scl->subctl, 0, sizeof(AM_SCAN_SubCtl_t) * scl->subs);
		for (i=0; i<scl->subs; i++)
		{
			scl->subctl[i].ver = 0xff;
		}
	}
}

/**\brief 初始化一个表控制结构*/
static AM_ErrorCode_t am_scan_tablectl_init(AM_SCAN_TableCtl_t * scl, int recv_flag, int evt_flag,
											int timeout, uint16_t pid, uint8_t tid, const char *name,
											uint16_t sub_cnt, void (*done)(struct AM_SCAN_Scanner_s *),
											int distance)
{
	memset(scl, 0, sizeof(AM_SCAN_TableCtl_t));\
	scl->fid = -1;
	scl->recv_flag = recv_flag;
	scl->evt_flag = evt_flag;
	scl->timeout = timeout;
	scl->pid = pid;
	scl->tid = tid;
	scl->done = done;
	scl->repeat_distance = distance;
	strcpy(scl->tname, name);

	scl->subs = sub_cnt;
	if (scl->subs)
	{
		scl->subctl = (AM_SCAN_SubCtl_t*)malloc(sizeof(AM_SCAN_SubCtl_t) * scl->subs);
		if (!scl->subctl)
		{
			scl->subs = 0;
			AM_DEBUG(1, "Cannot init tablectl, no enough memory");
			return AM_SCAN_ERR_NO_MEM;
		}

		am_scan_tablectl_clear(scl);
	}

	return AM_SUCCESS;
}

/**\brief 反初始化一个表控制结构*/
static void am_scan_tablectl_deinit(AM_SCAN_TableCtl_t * scl)
{
	if (scl->subctl)
	{
		free(scl->subctl);
		scl->subctl = NULL;
	}
}

/**\brief 判断一个表的所有section是否收齐*/
static AM_Bool_t am_scan_tablectl_test_complete(AM_SCAN_TableCtl_t * scl)
{
	static uint8_t test_array[32] = {0};
	int i;

	for (i=0; i<scl->subs; i++)
	{
		if ((scl->subctl[i].ver != 0xff) &&
			memcmp(scl->subctl[i].mask, test_array, sizeof(test_array)))
			return AM_FALSE;
	}

	return AM_TRUE;
}

/**\brief 判断一个表的指定section是否已经接收*/
static AM_Bool_t am_scan_tablectl_test_recved(AM_SCAN_TableCtl_t * scl, AM_SI_SectionHeader_t *header)
{
	int i;
	
	if (!scl->subctl)
		return AM_TRUE;

	for (i=0; i<scl->subs; i++)
	{
		if ((scl->subctl[i].ext == header->extension) && 
			(scl->subctl[i].ver == header->version) && 
			(scl->subctl[i].last == header->last_sec_num) && 
			!BIT_TEST(scl->subctl[i].mask, header->sec_num))
			return AM_TRUE;
	}
	
	return AM_FALSE;
}

/**\brief 在一个表中增加一个section已接收标识*/
static AM_ErrorCode_t am_scan_tablectl_mark_section(AM_SCAN_TableCtl_t * scl, AM_SI_SectionHeader_t *header)
{
	int i;
	AM_SCAN_SubCtl_t *sub, *fsub;

	if (!scl->subctl)
		return AM_SUCCESS;

	sub = fsub = NULL;
	for (i=0; i<scl->subs; i++)
	{
		if (scl->subctl[i].ext == header->extension)
		{
			sub = &scl->subctl[i];
			break;
		}
		/*记录一个空闲的结构*/
		if ((scl->subctl[i].ver == 0xff) && !fsub)
			fsub = &scl->subctl[i];
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
		
		for (i=0; i<(sub->last+1); i++)
			BIT_SET(sub->mask, i);
	}

	/*设置已接收标识*/
	BIT_CLEAR(sub->mask, header->sec_num);

	if (scl->data_arrive_time == 0)
		AM_TIME_GetClock(&scl->data_arrive_time);

	return AM_SUCCESS;
}

/**\brief 释放过滤器，并保证在此之后不会再有无效数据*/
static void am_scan_free_filter(AM_SCAN_Scanner_t *scanner, int *fid)
{
	if (*fid == -1)
		return;
		
	AM_DMX_FreeFilter(scanner->dmx_dev, *fid);
	*fid = -1;
	pthread_mutex_unlock(&scanner->lock);
	/*等待无效数据处理完毕*/
	AM_DMX_Sync(scanner->dmx_dev);
	pthread_mutex_lock(&scanner->lock);
}

/**\brief NIT搜索完毕(包括超时)处理*/
static void am_scan_nit_done(AM_SCAN_Scanner_t *scanner)
{
	dvbpsi_nit_t *nit;
	dvbpsi_nit_ts_t *ts;
	dvbpsi_descriptor_t *descr;
	struct dvb_frontend_parameters *param;

	am_scan_free_filter(scanner, &scanner->nitctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->nitctl.evt_flag;
	
	if (! scanner->result.nits)
	{
		AM_DEBUG(1, "No NIT found ,try next frequency");
		am_scan_try_nit(scanner);
		return;
	}

	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->nitctl.recv_flag;
	
	if (scanner->start_freqs)
		free(scanner->start_freqs);
	scanner->start_freqs = NULL;
	scanner->start_freqs_cnt = 0;
	scanner->curr_freq = -1;
	
	/*首先统计NIT中描述的TS个数*/
	AM_SI_LIST_BEGIN(scanner->result.nits, nit)
		AM_SI_LIST_BEGIN(nit->p_first_ts, ts)
		scanner->start_freqs_cnt++;
		AM_SI_LIST_END()
	AM_SI_LIST_END()
	
	if (scanner->start_freqs_cnt == 0)
	{
		AM_DEBUG(1, "No TS in NIT");
		goto NIT_END;
	}
	scanner->start_freqs = (struct dvb_frontend_parameters*)\
							malloc(sizeof(struct dvb_frontend_parameters) * scanner->start_freqs_cnt);
	if (!scanner->start_freqs)
	{
		AM_DEBUG(1, "No enough memory for building ts list");
		scanner->start_freqs_cnt = 0;
		goto NIT_END;
	}

	scanner->start_freqs_cnt = 0;
	
	/*从NIT搜索结果中取出频点列表存到start_freqs中*/
	AM_SI_LIST_BEGIN(scanner->result.nits, nit)
		/*遍历每个TS*/
		AM_SI_LIST_BEGIN(nit->p_first_ts, ts)
			AM_SI_LIST_BEGIN(ts->p_first_descriptor, descr)
			/*取DVBC频点信息*/
			if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_CABLE_DELIVERY)
			{
				dvbpsi_cable_delivery_dr_t *pcd = (dvbpsi_cable_delivery_dr_t*)descr->p_decoded;

				param = &scanner->start_freqs[scanner->start_freqs_cnt];
				param->frequency = pcd->i_frequency/1000;
				param->u.qam.modulation = pcd->i_modulation_type;
				param->u.qam.symbol_rate = pcd->i_symbol_rate;
				scanner->start_freqs_cnt++;
				AM_DEBUG(1, "Add frequency %u, symbol_rate %u, modulation %u, onid %d, ts_id %d", param->frequency,
						param->u.qam.symbol_rate, param->u.qam.modulation, ts->i_orig_network_id, ts->i_ts_id);
				break;
			}
			if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_TERRESTRIAL_DELIVERY)
			{
				dvbpsi_terr_deliv_sys_dr_t *pcd = (dvbpsi_terr_deliv_sys_dr_t*)descr->p_decoded;

				param = &scanner->start_freqs[scanner->start_freqs_cnt];
				param->frequency = pcd->i_centre_frequency/1000;
				param->u.ofdm.bandwidth = pcd->i_bandwidth;
				scanner->start_freqs_cnt++;
				AM_DEBUG(1, "Add frequency %u, bw %u, onid %d, ts_id %d", param->frequency,
						param->u.ofdm.bandwidth, ts->i_orig_network_id, ts->i_ts_id);
				break;
			}
			AM_SI_LIST_END()
		AM_SI_LIST_END()
	AM_SI_LIST_END()

NIT_END:
	AM_DEBUG(1, "Total found %d frequencies in NIT", scanner->start_freqs_cnt);
	if (scanner->start_freqs_cnt == 0)
	{
		AM_DEBUG(1, "No Delivery system descriptor in NIT");
	}

	if (scanner->recv_status == AM_SCAN_RECVING_COMPLETE)
	{
		/*开始搜索TS*/
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, NULL);
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_NIT_END, NULL);
		scanner->stage = AM_SCAN_STAGE_TS;
		am_scan_start_next_ts(scanner);
	}
}

/**\brief BAT搜索完毕(包括超时)处理*/
static void am_scan_bat_done(AM_SCAN_Scanner_t *scanner)
{
	am_scan_free_filter(scanner, &scanner->batctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->batctl.evt_flag;
	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->batctl.recv_flag;
	
	if (scanner->recv_status == AM_SCAN_RECVING_COMPLETE)
	{
		/*开始搜索TS*/
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, NULL);
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_NIT_END, NULL);
		scanner->stage = AM_SCAN_STAGE_TS;
		am_scan_start_next_ts(scanner);
	}
}

/**\brief PAT搜索完毕(包括超时)处理*/
static void am_scan_pat_done(AM_SCAN_Scanner_t *scanner)
{
	am_scan_free_filter(scanner, &scanner->patctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->patctl.evt_flag;
	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->patctl.recv_flag;

	SET_PROGRESS_EVT(AM_SCAN_PROGRESS_PAT_DONE, (void*)scanner->curr_ts->pats);
	
	/*开始搜索PMT表*/
	am_scan_request_next_pmt(scanner);
}

/**\brief PMT搜索完毕(包括超时)处理*/
static void am_scan_pmt_done(AM_SCAN_Scanner_t *scanner)
{
	am_scan_free_filter(scanner, &scanner->pmtctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->pmtctl.evt_flag;
	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->pmtctl.recv_flag;
	
	/*开始搜索下一个PMT表*/
	am_scan_request_next_pmt(scanner);
}

/**\brief CAT搜索完毕(包括超时)处理*/
static void am_scan_cat_done(AM_SCAN_Scanner_t *scanner)
{
	am_scan_free_filter(scanner, &scanner->catctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->catctl.evt_flag;
	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->catctl.recv_flag;

	SET_PROGRESS_EVT(AM_SCAN_PROGRESS_CAT_DONE, (void*)scanner->curr_ts->cats);
}

/**\brief SDT搜索完毕(包括超时)处理*/
static void am_scan_sdt_done(AM_SCAN_Scanner_t *scanner)
{
	am_scan_free_filter(scanner, &scanner->sdtctl.fid);
	/*清除事件标志*/
	//scanner->evt_flag &= ~scanner->sdtctl.evt_flag;
	/*清除搜索标识*/
	scanner->recv_status &= ~scanner->sdtctl.recv_flag;

	SET_PROGRESS_EVT(AM_SCAN_PROGRESS_SDT_DONE, (void*)scanner->curr_ts->sdts);
}

/**\brief 根据过滤器号取得相应控制数据*/
static AM_SCAN_TableCtl_t *am_scan_get_section_ctrl_by_fid(AM_SCAN_Scanner_t *scanner, int fid)
{
	AM_SCAN_TableCtl_t *scl = NULL;

	
	if (scanner->patctl.fid == fid)
		scl = &scanner->patctl;
	else if (scanner->catctl.fid == fid)
		scl = &scanner->catctl;
	else if (scanner->pmtctl.fid == fid)
		scl = &scanner->pmtctl;
	else if (scanner->sdtctl.fid == fid)
		scl = &scanner->sdtctl;
	else if (scanner->nitctl.fid == fid)
		scl = &scanner->nitctl;
	else if (scanner->batctl.fid == fid)
		scl = &scanner->batctl;
	
	
	return scl;
}

/**\brief 数据处理函数*/
static void am_scan_section_handler(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)user_data;
	AM_SCAN_TableCtl_t * sec_ctrl;
	AM_SI_SectionHeader_t header;
	
	if (scanner == NULL)
	{
		AM_DEBUG(1, "Scan: Invalid param user_data in dmx callback");
		return;
	}
	if (data == NULL)
		return;
		
	pthread_mutex_lock(&scanner->lock);
	/*获取接收控制数据*/
	sec_ctrl = am_scan_get_section_ctrl_by_fid(scanner, fid);
	if (sec_ctrl)
	{
		if (AM_SI_GetSectionHeader(scanner->hsi, (uint8_t*)data, len, &header) != AM_SUCCESS)
		{
			AM_DEBUG(1, "Scan: section header error");
			goto parse_end;
		}
		
		/*该section是否已经接收过*/
		if (am_scan_tablectl_test_recved(sec_ctrl, &header))
		{
			AM_DEBUG(1,"%s section %d repeat!", sec_ctrl->tname, header.sec_num);
			/*当有多个子表时，判断收齐的条件为 收到重复section + 所有子表收齐 + 重复section间隔时间大于某个值*/
			if (sec_ctrl->subs > 1)
			{
				int now;
				
				AM_TIME_GetClock(&now);
				if (am_scan_tablectl_test_complete(sec_ctrl) && 
					((now - sec_ctrl->data_arrive_time) > sec_ctrl->repeat_distance))
				{
					AM_DEBUG(1, "%s Done!", sec_ctrl->tname);
					scanner->evt_flag |= sec_ctrl->evt_flag;
					pthread_cond_signal(&scanner->cond);
				}
			}
			
			goto parse_end;
		}
		/*数据处理*/
		switch (header.table_id)
		{
			case AM_SI_TID_PAT:
				if (scanner->curr_ts)
					COLLECT_SECTION(dvbpsi_pat_t, scanner->curr_ts->pats);
				break;
			case AM_SI_TID_PMT:
				if (scanner->curr_ts)
				{
					AM_DEBUG(0, "PMT %d arrived", header.extension);
					COLLECT_SECTION(dvbpsi_pmt_t, scanner->curr_ts->pmts);
				}
				break;
			case AM_SI_TID_SDT_ACT:
				if (scanner->curr_ts)
					COLLECT_SECTION(dvbpsi_sdt_t, scanner->curr_ts->sdts);
				break;
			case AM_SI_TID_CAT:
				if (scanner->curr_ts)
					COLLECT_SECTION(dvbpsi_cat_t, scanner->curr_ts->cats);
				break;
			case AM_SI_TID_NIT_ACT:
				COLLECT_SECTION(dvbpsi_nit_t, scanner->result.nits);
				break;
			case AM_SI_TID_BAT:
				AM_DEBUG(1, "BAT ext 0x%x, sec %d, last %d", header.extension, header.sec_num, header.last_sec_num);
				COLLECT_SECTION(dvbpsi_bat_t, scanner->result.bats);
				break;
			default:
				AM_DEBUG(1, "Scan: Unkown section data, table_id 0x%x", data[0]);
				goto parse_end;
				break;
		}
			
		/*数据处理完毕，查看该表是否已接收完毕所有section*/
		if (am_scan_tablectl_test_complete(sec_ctrl) && sec_ctrl->subs == 1)
		{
			/*该表接收完毕*/
			AM_DEBUG(1, "%s Done!", sec_ctrl->tname);
			scanner->evt_flag |= sec_ctrl->evt_flag;
			pthread_cond_signal(&scanner->cond);
		}
	}
	else
	{
		AM_DEBUG(1, "Scan: Unknown filter id %d in dmx callback", fid);
	}

parse_end:
	pthread_mutex_unlock(&scanner->lock);

}

/**\brief 请求一个表的section数据*/
static AM_ErrorCode_t am_scan_request_section(AM_SCAN_Scanner_t *scanner, AM_SCAN_TableCtl_t *scl)
{
	struct dmx_sct_filter_params param;
	
	if (scl->fid != -1)
	{
		am_scan_free_filter(scanner, &scl->fid);
	}

	/*分配过滤器*/
	AM_TRY(AM_DMX_AllocateFilter(scanner->dmx_dev, &scl->fid));
	/*设置处理函数*/
	AM_TRY(AM_DMX_SetCallback(scanner->dmx_dev, scl->fid, am_scan_section_handler, (void*)scanner));

	/*设置过滤器参数*/
	memset(&param, 0, sizeof(param));
	param.pid = scl->pid;
	param.filter.filter[0] = scl->tid;
	param.filter.mask[0] = 0xff;

	/*Current next indicator must be 1*/
	param.filter.filter[3] = 0x01;
	param.filter.mask[3] = 0x01;
	
	param.flags = DMX_CHECK_CRC;
	/*设置超时时间*/
	AM_TIME_GetTimeSpecTimeout(scl->timeout, &scl->end_time);

	AM_TRY(AM_DMX_SetSecFilter(scanner->dmx_dev, scl->fid, &param));
	AM_TRY(AM_DMX_SetBufferSize(scanner->dmx_dev, scl->fid, 16*1024));
	AM_TRY(AM_DMX_StartFilter(scanner->dmx_dev, scl->fid));

	/*设置接收状态*/
	scanner->recv_status |= scl->recv_flag; 

	return AM_SUCCESS;
}

/**\brief 请求下一个program的PMT表*/
static AM_ErrorCode_t am_scan_request_next_pmt(AM_SCAN_Scanner_t *scanner)
{
	if (! scanner->curr_ts)
	{
		/*A serious error*/
		AM_DEBUG(1, "Error, no current ts selected");
		return AM_SCAN_ERROR_BASE;
	}
	if (! scanner->curr_ts->pats)
		return AM_SCAN_ERROR_BASE;
		
	if (! scanner->curr_ts->pats->p_first_program)
	{
		AM_DEBUG(1,"Scan: no PMT found in PAT");
		return AM_SUCCESS;
	}
	if (! scanner->cur_prog)
		scanner->cur_prog = scanner->curr_ts->pats->p_first_program;
	else
		scanner->cur_prog = scanner->cur_prog->p_next;

	while (scanner->cur_prog && scanner->cur_prog->i_number == 0)
		scanner->cur_prog = scanner->cur_prog->p_next;

	if (! scanner->cur_prog)
	{
		AM_DEBUG(1,"All PMTs Done!");
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_PMT_DONE, (void*)scanner->curr_ts->pmts);
		return AM_SUCCESS;
	}

	AM_DEBUG(1, "Start PMT for program %d, pmt_pid 0x%x", scanner->cur_prog->i_number, scanner->cur_prog->i_pid);
	/*初始化接收控制数据*/
	am_scan_tablectl_clear(&scanner->pmtctl);
	scanner->pmtctl.pid = scanner->cur_prog->i_pid;

	AM_TRY(am_scan_request_section(scanner, &scanner->pmtctl));

	return AM_SUCCESS;
}

/**\brief 从下一个主频点中搜索NIT表*/
static AM_ErrorCode_t am_scan_try_nit(AM_SCAN_Scanner_t *scanner)
{
	AM_ErrorCode_t ret;
	AM_SCAN_TSProgress_t tp;

	if (scanner->stage != AM_SCAN_STAGE_NIT)
	{
		scanner->stage = AM_SCAN_STAGE_NIT;
		/*开始搜索NIT*/
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_NIT_BEGIN, NULL);
	}

	if (scanner->curr_freq >= 0)
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, NULL);
		
	do
	{
		scanner->curr_freq++;
		if (scanner->curr_freq < 0 || scanner->curr_freq >= scanner->start_freqs_cnt)
		{
			AM_DEBUG(1, "Cannot get nit after all trings!");
			if (scanner->start_freqs)
			{
				free(scanner->start_freqs);
				scanner->start_freqs = NULL;
				scanner->start_freqs_cnt = 0;
				scanner->curr_freq = -1;
			}
			/*搜索结束*/
			scanner->stage = AM_SCAN_STAGE_DONE;
			SET_PROGRESS_EVT(AM_SCAN_PROGRESS_SCAN_END, (void*)scanner->end_code);
			
			return AM_SCAN_ERR_CANNOT_GET_NIT;
		}

		AM_DEBUG(1, "Tring to recv NIT in frequency %u ...", scanner->start_freqs[scanner->curr_freq].frequency);

		tp.index = scanner->curr_freq;
		tp.total = scanner->start_freqs_cnt;
		tp.fend_para = scanner->start_freqs[scanner->curr_freq];
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_BEGIN, (void*)&tp);
		
		ret = AM_FEND_SetPara(scanner->fend_dev, &scanner->start_freqs[scanner->curr_freq]);
		if (ret == AM_SUCCESS)
		{
			scanner->recv_status |= AM_SCAN_RECVING_WAIT_FEND;
		}
		else
		{
			SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, NULL);
			AM_DEBUG(1, "AM_FEND_SetPara Failed, try next frequency");
		}
			
	}while(ret != AM_SUCCESS);
	
	return ret;
}

/**\brief 从频率列表中选择下一个频点开始搜索*/
static AM_ErrorCode_t am_scan_start_next_ts(AM_SCAN_Scanner_t *scanner)
{
	AM_ErrorCode_t ret;
	AM_SCAN_TSProgress_t tp;

	if (scanner->stage != AM_SCAN_STAGE_TS)
		scanner->stage = AM_SCAN_STAGE_TS;

	if (scanner->curr_freq >= 0)
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, scanner->curr_ts);

	scanner->curr_ts = NULL;
			
	do
	{
		scanner->curr_freq++;
		if (scanner->curr_freq < 0 || scanner->curr_freq >= scanner->start_freqs_cnt)
		{
			AM_DEBUG(1, "All TSes Complete!");
			if (scanner->start_freqs)
			{
				free(scanner->start_freqs);
				scanner->start_freqs = NULL;
				scanner->start_freqs_cnt = 0;
				scanner->curr_freq = -1;
			}

			/*搜索结束*/
			scanner->stage = AM_SCAN_STAGE_DONE;
			SET_PROGRESS_EVT(AM_SCAN_PROGRESS_SCAN_END, (void*)scanner->end_code);

			return AM_SUCCESS;
		}

		AM_DEBUG(1, "Start scanning frequency %u ...", scanner->start_freqs[scanner->curr_freq].frequency);

		
		tp.index = scanner->curr_freq;
		tp.total = scanner->start_freqs_cnt;
		tp.fend_para = scanner->start_freqs[scanner->curr_freq];
		SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_BEGIN, (void*)&tp);
		
		ret = AM_FEND_SetPara(scanner->fend_dev, &scanner->start_freqs[scanner->curr_freq]);
		if (ret == AM_SUCCESS)
		{
			scanner->recv_status |= AM_SCAN_RECVING_WAIT_FEND;
		}
		else
		{
			SET_PROGRESS_EVT(AM_SCAN_PROGRESS_TS_END, NULL);
			AM_DEBUG(1, "AM_FEND_SetPara Failed, try next frequency");
			AM_DEBUG(1, ">>>>[total:%dcur:%d, f:%ds=%dm=%d]", 
				scanner->start_freqs_cnt, scanner->curr_freq, 
				scanner->start_freqs[scanner->curr_freq].frequency,
				scanner->start_freqs[scanner->curr_freq].u.qam.symbol_rate,
				scanner->start_freqs[scanner->curr_freq].u.qam.modulation);
		}
			
	}while(ret != AM_SUCCESS);

	return AM_SUCCESS;
}

static void am_scan_fend_callback(int dev_no, int event_type, void *param, void *user_data)
{
	struct dvb_frontend_event *evt = (struct dvb_frontend_event*)param;
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)user_data;

	if (!scanner || !evt || (evt->status == 0))
		return;

	pthread_mutex_lock(&scanner->lock);
	scanner->fe_evt = *evt;
	scanner->evt_flag |= AM_SCAN_EVT_FEND;
	pthread_cond_signal(&scanner->cond);
	pthread_mutex_unlock(&scanner->lock);
}

/**\brief 启动搜索*/
static AM_ErrorCode_t am_scan_start(AM_SCAN_Scanner_t *scanner)
{
	if (scanner->hsi != 0)
	{
		AM_DEBUG(1, "Scan already start");
		return AM_SUCCESS;
	}

	/*注册前端事件*/
	AM_EVT_Subscribe(scanner->fend_dev, AM_FEND_EVT_STATUS_CHANGED, am_scan_fend_callback, (void*)scanner);
	
	/*创建SI解析器*/
	AM_TRY(AM_SI_Create(&scanner->hsi));

	/*接收控制数据初始化*/								
	am_scan_tablectl_init(&scanner->patctl, AM_SCAN_RECVING_PAT, AM_SCAN_EVT_PAT_DONE, PAT_TIMEOUT, 
						AM_SI_PID_PAT, AM_SI_TID_PAT, "PAT", 1, am_scan_pat_done, 0);
	am_scan_tablectl_init(&scanner->pmtctl, AM_SCAN_RECVING_PMT, AM_SCAN_EVT_PMT_DONE, PMT_TIMEOUT, 
						0x1fff, AM_SI_TID_PMT, "PMT", 1, am_scan_pmt_done, 0);
	am_scan_tablectl_init(&scanner->catctl, AM_SCAN_RECVING_CAT, AM_SCAN_EVT_CAT_DONE, CAT_TIMEOUT, 
						AM_SI_PID_CAT, AM_SI_TID_CAT, "CAT", 1, am_scan_cat_done, 0);
	am_scan_tablectl_init(&scanner->sdtctl, AM_SCAN_RECVING_SDT, AM_SCAN_EVT_SDT_DONE, SDT_TIMEOUT, 
						AM_SI_PID_SDT, AM_SI_TID_SDT_ACT, "SDT", 1, am_scan_sdt_done, 0);
	am_scan_tablectl_init(&scanner->nitctl, AM_SCAN_RECVING_NIT, AM_SCAN_EVT_NIT_DONE, NIT_TIMEOUT, 
						AM_SI_PID_NIT, AM_SI_TID_NIT_ACT, "NIT", 1, am_scan_nit_done, 0);
	am_scan_tablectl_init(&scanner->batctl, AM_SCAN_RECVING_BAT, AM_SCAN_EVT_BAT_DONE, BAT_TIMEOUT, 
						AM_SI_PID_BAT, AM_SI_TID_BAT, "BAT", MAX_BAT_SUBTABLE_CNT, 
						am_scan_bat_done, BAT_REPEAT_DISTANCE);

	scanner->curr_freq = -1;
	scanner->end_code = AM_SCAN_RESULT_UNLOCKED;

	/*自动搜索模式时按指定主频点列表开始NIT表请求,其他模式直接按指定频点开始搜索*/
	if (scanner->result.mode & AM_SCAN_MODE_AUTO)
		AM_TRY(am_scan_try_nit(scanner));
	else
		AM_TRY(am_scan_start_next_ts(scanner));

	return AM_SUCCESS;
}

/**\brief SCAN前端事件处理*/
static void am_scan_solve_fend_evt(AM_SCAN_Scanner_t *scanner)
{
	AM_SCAN_SignalInfo_t si;
	
	if ((scanner->curr_freq >= 0) && 
		(scanner->curr_freq < scanner->start_freqs_cnt) &&
		(scanner->start_freqs[scanner->curr_freq].frequency != \
		scanner->fe_evt.parameters.frequency))
	{
		AM_DEBUG(1, "Unexpected fend_evt arrived");
		return;
	}

	if ( ! (scanner->recv_status & AM_SCAN_RECVING_WAIT_FEND))
	{
		AM_DEBUG(1, "Discard Fend event.");
		return;
	}
	
	AM_DEBUG(1, "FEND event: %u %s!", scanner->fe_evt.parameters.frequency, \
			(scanner->fe_evt.status&FE_HAS_LOCK) ? "Locked" : "Unlocked");
			
	scanner->recv_status &= ~AM_SCAN_RECVING_WAIT_FEND;

	/*获取前端信号质量等信息并通知*/
	AM_FEND_GetSNR(scanner->fend_dev, &si.snr);
	AM_FEND_GetBER(scanner->fend_dev, &si.ber);
	AM_FEND_GetStrength(scanner->fend_dev, &si.strength);
	si.frequency = scanner->fe_evt.parameters.frequency;
	SIGNAL_EVENT(AM_SCAN_EVT_SIGNAL, (void*)&si);

	if (scanner->fe_evt.status & FE_HAS_LOCK)
	{
		if (scanner->end_code == AM_SCAN_RESULT_UNLOCKED)
		{
			scanner->end_code = AM_SCAN_RESULT_OK;
		}

		if (scanner->stage == AM_SCAN_STAGE_NIT)
		{
			am_scan_tablectl_clear(&scanner->nitctl);
			am_scan_tablectl_clear(&scanner->batctl);
			
			/*请求NIT, BAT数据*/
			am_scan_request_section(scanner, &scanner->nitctl);
			if (scanner->result.mode & AM_SCAN_MODE_SEARCHBAT)
				am_scan_request_section(scanner, &scanner->batctl);
				
			return;
		}
		else if (scanner->stage == AM_SCAN_STAGE_TS)
		{
			/*频点锁定，新申请内存以添加该频点信息*/
			scanner->curr_ts = (AM_SCAN_TS_t*)malloc(sizeof(AM_SCAN_TS_t));
			if (scanner->curr_ts == NULL)
			{
				AM_DEBUG(1, "Error, no enough memory for adding a new ts");\
				goto try_next;
			}
			memset(scanner->curr_ts, 0, sizeof(AM_SCAN_TS_t));

			/*存储信号信息*/
			scanner->curr_ts->snr = si.snr;
			scanner->curr_ts->ber = si.ber;
			scanner->curr_ts->strength = si.strength;
			
			am_scan_tablectl_clear(&scanner->patctl);
			am_scan_tablectl_clear(&scanner->pmtctl);
			am_scan_tablectl_clear(&scanner->catctl);
			am_scan_tablectl_clear(&scanner->sdtctl);

			/*请求数据*/
			am_scan_request_section(scanner, &scanner->patctl);
			am_scan_request_section(scanner, &scanner->catctl);
			am_scan_request_section(scanner, &scanner->sdtctl);

			scanner->curr_ts->fend_para = scanner->start_freqs[scanner->curr_freq];
			/*添加到搜索结果列表*/
			ADD_TO_LIST(scanner->curr_ts, scanner->result.tses);

			return;
		}
		else
		{
			AM_DEBUG(1, "FEND event arrive at unexpected scan stage %d", scanner->stage);
			return;
		}
	}

try_next:
	/*尝试下一频点*/	
	if (scanner->stage == AM_SCAN_STAGE_NIT)
		am_scan_try_nit(scanner);
	else if (scanner->stage == AM_SCAN_STAGE_TS)
		am_scan_start_next_ts(scanner);

}

/**\brief 比较2个timespec*/
static int am_scan_compare_timespec(const struct timespec *ts1, const struct timespec *ts2)
{
	assert(ts1 && ts2);

	if ((ts1->tv_sec > ts2->tv_sec) ||
		((ts1->tv_sec == ts2->tv_sec)&&(ts1->tv_nsec > ts2->tv_nsec)))
		return 1;
		
	if ((ts1->tv_sec == ts2->tv_sec) && (ts1->tv_nsec == ts2->tv_nsec))
		return 0;
	
	return -1;
}

/**\brief 计算下一等待超时时间，存储到ts所指结构中*/
static void am_scan_get_wait_timespec(AM_SCAN_Scanner_t *scanner, struct timespec *ts)
{
	int rel;
	struct timespec now;

#define TIMEOUT_CHECK(table)\
	AM_MACRO_BEGIN\
		struct timespec end;\
		if (scanner->recv_status & scanner->table##ctl.recv_flag){\
			end = scanner->table##ctl.end_time;\
			rel = am_scan_compare_timespec(&scanner->table##ctl.end_time, &now);\
			if (rel <= 0){\
				AM_DEBUG(1, "%s timeout", scanner->table##ctl.tname);\
				scanner->table##ctl.done(scanner);\
			}\
			if (rel > 0 || memcmp(&end, &scanner->table##ctl.end_time, sizeof(struct timespec))){\
				if ((ts->tv_sec == 0 && ts->tv_nsec == 0) || \
					(am_scan_compare_timespec(&scanner->table##ctl.end_time, ts) < 0))\
					*ts = scanner->table##ctl.end_time;\
			}\
		}\
	AM_MACRO_END

	ts->tv_sec = 0;
	ts->tv_nsec = 0;
	AM_TIME_GetTimeSpec(&now);
	
	/*超时检查*/
	TIMEOUT_CHECK(pat);
	TIMEOUT_CHECK(pmt);
	TIMEOUT_CHECK(cat);
	TIMEOUT_CHECK(sdt);
	TIMEOUT_CHECK(nit);
	TIMEOUT_CHECK(bat);
	
	if (scanner->stage == AM_SCAN_STAGE_TS && \
		scanner->recv_status == AM_SCAN_RECVING_COMPLETE)
		am_scan_start_next_ts(scanner);
	
}

/**\brief SCAN线程*/
static void *am_scan_thread(void *para)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)para;
	AM_SCAN_TS_t *ts, *tnext;
	struct timespec to;
	int ret, evt_flag;
	AM_Bool_t go = AM_TRUE;

	pthread_mutex_lock(&scanner->lock);
	
	while (go)
	{
		/*检查超时，并计算下一个超时时间*/
		am_scan_get_wait_timespec(scanner, &to);

		ret = 0;
		/*等待事件*/
		if(scanner->evt_flag == 0)
		{
			if (to.tv_sec == 0 && to.tv_nsec == 0)
				ret = pthread_cond_wait(&scanner->cond, &scanner->lock);
			else
				ret = pthread_cond_timedwait(&scanner->cond, &scanner->lock, &to);
		}
		
		if (ret != ETIMEDOUT)
		{
handle_events:
			evt_flag = scanner->evt_flag;
			AM_DEBUG(2, "Evt flag 0x%x", scanner->evt_flag);

			/*开始搜索事件*/
			if ((evt_flag&AM_SCAN_EVT_START) && (scanner->hsi == 0))
			{
				SET_PROGRESS_EVT(AM_SCAN_PROGRESS_SCAN_BEGIN, (void*)scanner);
				am_scan_start(scanner);
			}

			/*前端事件*/
			if (evt_flag & AM_SCAN_EVT_FEND)
				am_scan_solve_fend_evt(scanner);

			/*PAT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_PAT_DONE)
				scanner->patctl.done(scanner);
			/*PMT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_PMT_DONE)
				scanner->pmtctl.done(scanner);
			/*CAT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_CAT_DONE)
				scanner->catctl.done(scanner);
			/*SDT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_SDT_DONE)
				scanner->sdtctl.done(scanner);
			/*NIT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_NIT_DONE)
				scanner->nitctl.done(scanner);
			/*BAT表收齐事件*/
			if (evt_flag & AM_SCAN_EVT_BAT_DONE)
				scanner->batctl.done(scanner);

			/*退出事件*/
			if (evt_flag & AM_SCAN_EVT_QUIT)
			{
				if (scanner->result.tses/* && scanner->stage == AM_SCAN_STAGE_DONE*/)
				{
					AM_DEBUG(1, "Call store proc");
					SET_PROGRESS_EVT(AM_SCAN_PROGRESS_STORE_BEGIN, NULL);
					if (scanner->store_cb)
						scanner->store_cb(&scanner->result);
					SET_PROGRESS_EVT(AM_SCAN_PROGRESS_STORE_END, NULL);
				}
				
				go = AM_FALSE;
				
				continue;
			}
			
			/*在调用am_scan_free_filter时可能会产生新事件*/
			scanner->evt_flag &= ~evt_flag;
			if (scanner->evt_flag)
			{
				goto handle_events;
			}
		}
	}

	/*反注册前端事件*/
	AM_EVT_Unsubscribe(scanner->fend_dev, AM_FEND_EVT_STATUS_CHANGED, am_scan_fend_callback, (void*)scanner);

	/*DMX释放*/
	if (scanner->patctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->patctl.fid);
	if (scanner->pmtctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->pmtctl.fid);
	if (scanner->catctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->catctl.fid);
	if (scanner->nitctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->nitctl.fid);
	if (scanner->sdtctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->sdtctl.fid);
	if (scanner->batctl.fid != -1)
		AM_DMX_FreeFilter(scanner->dmx_dev, scanner->batctl.fid);
	pthread_mutex_unlock(&scanner->lock);
	AM_DEBUG(0, "Waiting for dmx callback...");
	/*等待回调函数执行完毕*/
	AM_DMX_Sync(scanner->dmx_dev);
	AM_DEBUG(0, "OK");
	pthread_mutex_lock(&scanner->lock);
	/*表控制释放*/
	am_scan_tablectl_deinit(&scanner->patctl);
	am_scan_tablectl_deinit(&scanner->pmtctl);
	am_scan_tablectl_deinit(&scanner->catctl);
	am_scan_tablectl_deinit(&scanner->nitctl);
	am_scan_tablectl_deinit(&scanner->sdtctl);
	am_scan_tablectl_deinit(&scanner->batctl);
	
	if (scanner->start_freqs != NULL)
		free(scanner->start_freqs);
	/*释放SI资源*/
	RELEASE_TABLE_FROM_LIST(dvbpsi_nit_t, scanner->result.nits);
	RELEASE_TABLE_FROM_LIST(dvbpsi_bat_t, scanner->result.bats);
	ts = scanner->result.tses;
	while (ts)
	{
		tnext = ts->p_next;
		RELEASE_TABLE_FROM_LIST(dvbpsi_pat_t, ts->pats);
		RELEASE_TABLE_FROM_LIST(dvbpsi_pmt_t, ts->pmts);
		RELEASE_TABLE_FROM_LIST(dvbpsi_cat_t, ts->cats);
		RELEASE_TABLE_FROM_LIST(dvbpsi_sdt_t, ts->sdts);
		free(ts);

		ts = tnext;
	}
	
	AM_SI_Destroy(scanner->hsi);
	pthread_mutex_unlock(&scanner->lock);

	pthread_mutex_destroy(&scanner->lock);
	pthread_cond_destroy(&scanner->cond);
	free(scanner);		
	
	return NULL;
}

 
/****************************************************************************
 * API functions
 ***************************************************************************/

 /**\brief 创建节目搜索
 * \param [in] para 创建参数
 * \param [out] handle 返回SCAN句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
AM_ErrorCode_t AM_SCAN_Create(AM_SCAN_CreatePara_t *para, int *handle)
{
	AM_SCAN_Scanner_t *scanner;
	int smode = para->mode;
	int rc;
	pthread_mutexattr_t mta;
	int use_default=0;
	
	if (para->start_para_cnt < 0 || !handle)
		return AM_SCAN_ERR_INVALID_PARAM;
		
	*handle = 0;	
	/*分析搜索模式*/
	smode &= 0x07;
	if (smode != AM_SCAN_MODE_AUTO && 
		smode != AM_SCAN_MODE_MANUAL && 
		smode != AM_SCAN_MODE_ALLBAND)
	{
		AM_DEBUG(1, "Scan: unknown scan mode[%d]", smode);
		return AM_SCAN_ERR_INVALID_PARAM;
	}
	if ((para->mode&AM_SCAN_MODE_MANUAL)  && (para->start_para_cnt == 0 || !para->start_para))
		return AM_SCAN_ERR_INVALID_PARAM;
	if (/*(para->mode&AM_SCAN_MODE_AUTO) &&*/ para->start_para_cnt && !para->start_para)
		return AM_SCAN_ERR_INVALID_PARAM;
		
	/*Create a scanner*/
	scanner = (AM_SCAN_Scanner_t*)malloc(sizeof(AM_SCAN_Scanner_t));
	if (scanner == NULL)
	{
		AM_DEBUG(1, "Scan: scan create error, no enough memory");
		return AM_SCAN_ERR_NO_MEM;
	}
	/*数据初始化*/
	memset(scanner, 0, sizeof(AM_SCAN_Scanner_t));
	if (para->mode & AM_SCAN_MODE_MANUAL)
		para->start_para_cnt = 1;
	else if (((para->mode & AM_SCAN_MODE_ALLBAND)
				||(para->mode & AM_SCAN_MODE_AUTO)) 
			&& (! para->start_para_cnt)) {
		use_default = 1;
		para->start_para_cnt = 
			(para->source==AM_SCAN_SRC_DVBC)? AM_ARRAY_SIZE(dvbc_std_freqs) 
			: (para->source==AM_SCAN_SRC_DVBT) ? (DEFAULT_DVBT_FREQ_STOP-DEFAULT_DVBT_FREQ_START)/8000+1
			: 0/*Fix me*/;
	}

	/*配置起始频点*/
	scanner->start_freqs = (struct dvb_frontend_parameters*)\
							malloc(sizeof(struct dvb_frontend_parameters) * para->start_para_cnt);
	if (!scanner->start_freqs)
	{
		AM_DEBUG(1, "Scan: scan create error, no enough memory");
		free(scanner);
		return AM_SCAN_ERR_NO_MEM;
	}
	scanner->start_freqs_cnt = para->start_para_cnt;
	/*全频段搜索时按标准频率表搜索*/
	if (((para->mode & AM_SCAN_MODE_ALLBAND) 
		    ||(para->mode & AM_SCAN_MODE_AUTO))
		 && use_default )
	{
		int i;
		struct dvb_frontend_parameters tpara;
		
		if(para->source==AM_SCAN_SRC_DVBC) {
			tpara.u.qam.modulation = DEFAULT_DVBC_MODULATION;
			tpara.u.qam.symbol_rate = DEFAULT_DVBC_SYMBOLRATE;
			for (i=0; i<scanner->start_freqs_cnt; i++)
			{
				tpara.frequency = dvbc_std_freqs[i]*1000;
				scanner->start_freqs[i] = tpara;
			}
		} else if(para->source==AM_SCAN_SRC_DVBT) {
			tpara.u.ofdm.bandwidth = DEFAULT_DVBT_BW;
			for (i=0; i<scanner->start_freqs_cnt; i++)
			{
				tpara.frequency = (DEFAULT_DVBT_FREQ_START+(8000*i))*1000;
				scanner->start_freqs[i] = tpara;
			}
		}
	}
	else
	{
		memcpy(scanner->start_freqs, para->start_para, sizeof(struct dvb_frontend_parameters) * para->start_para_cnt);
	}

	scanner->result.mode = para->mode;
	scanner->result.src = para->source;
	scanner->result.hdb = para->hdb;
	scanner->fend_dev = para->fend_dev_id;
	scanner->dmx_dev = para->dmx_dev_id;
	if (! para->store_cb)
		scanner->store_cb = am_scan_default_store;
	else
		scanner->store_cb = para->store_cb;

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&scanner->lock, &mta);
	pthread_cond_init(&scanner->cond, NULL);
	pthread_mutexattr_destroy(&mta);
	
	rc = pthread_create(&scanner->thread, NULL, am_scan_thread, (void*)scanner);
	if(rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));
		pthread_mutex_destroy(&scanner->lock);
		pthread_cond_destroy(&scanner->cond);
		free(scanner->start_freqs);
		free(scanner);
		return AM_SCAN_ERR_CANNOT_CREATE_THREAD;
	}
	
	*handle = (int)scanner;
	return AM_SUCCESS;
}

/**\brief 启动节目搜索
 * \param handle Scan句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
AM_ErrorCode_t AM_SCAN_Start(int handle)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)handle;

	if (scanner)
	{
		pthread_mutex_lock(&scanner->lock);
		/*启动搜索*/
		scanner->evt_flag |= AM_SCAN_EVT_START;
		pthread_cond_signal(&scanner->cond);
		pthread_mutex_unlock(&scanner->lock);
	}

	return AM_SUCCESS;
}

/**\brief 销毀节目搜索
 * \param handle Scan句柄
 * \param store 是否存储
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
AM_ErrorCode_t AM_SCAN_Destroy(int handle, AM_Bool_t store)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)handle;

	if (scanner)
	{
		pthread_t t;
		
		pthread_mutex_lock(&scanner->lock);
		/*等待搜索线程退出*/
		scanner->evt_flag |= AM_SCAN_EVT_QUIT;
		scanner->store = store;
		t = scanner->thread;
		pthread_cond_signal(&scanner->cond);
		pthread_mutex_unlock(&scanner->lock);

		if (t != pthread_self())
			pthread_join(t, NULL);
	}

	return AM_SUCCESS;
}

/**\brief 设置用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
AM_ErrorCode_t AM_SCAN_SetUserData(int handle, void *user_data)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)handle;

	if (scanner)
	{
		pthread_mutex_lock(&scanner->lock);
		scanner->user_data = user_data;
		pthread_mutex_unlock(&scanner->lock);
	}

	return AM_SUCCESS;
}

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
AM_ErrorCode_t AM_SCAN_GetUserData(int handle, void **user_data)
{
	AM_SCAN_Scanner_t *scanner = (AM_SCAN_Scanner_t*)handle;

	assert(user_data);
	
	if (scanner)
	{
		pthread_mutex_lock(&scanner->lock);
		*user_data = scanner->user_data;
		pthread_mutex_unlock(&scanner->lock);
	}

	return AM_SUCCESS;
}

