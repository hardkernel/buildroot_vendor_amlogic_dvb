/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_tv_test.c
 * \brief TV模块测试程序
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-11-12: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <string.h>
#include <assert.h>
#include <am_debug.h>
#include <am_fend.h>
#include <am_dmx.h>
#include <am_db.h>
#include <am_av.h>
#include <am_tv.h>
#include <am_scan.h>
#include <am_epg.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define FEND_DEV_NO 0
#define AV_DEV_NO 0
#define DMX_DEV_NO 0

#define MAX_PROG_CNT 300
#define MAX_NAME_LEN 64

/****************************************************************************
 * Type definitions
 ***************************************************************************/


enum
{
	MENU_TYPE_MENU,
	MENU_TYPE_APP
};


typedef struct 
{
	const int	id;
	const int	parent;
	const char	*title;
	const char	*cmd;
	const int	type;
	void (*app)(void);
	void (*leave)(void);
	void (*input)(const char *cmd);
}CommandMenu;



static CommandMenu *curr_menu;
static struct dvb_frontend_parameters fend_param;
static sqlite3 *hdb;
static AM_Bool_t go = AM_TRUE;
static int hscan, htv, hepg;
static int chan_num[MAX_PROG_CNT];
static char programs[MAX_PROG_CNT][MAX_NAME_LEN];

static const char *weeks[]=
{
	"星期天",
	"星期一",
	"星期二",
	"星期三",
	"星期四",
	"星期五",
	"星期六",
};
	
/****************************************************************************
 * static Functions
 ***************************************************************************/
static void show_main_menu(void);


static void epg_evt_call_back(int dev_no, int event_type, void *param, void *user_data)
{
	char *errmsg = NULL;
	
	switch (event_type)
	{
		case AM_EPG_EVT_NEW_EIT:
		{
			dvbpsi_eit_t *eit = (dvbpsi_eit_t*)param;
			dvbpsi_eit_event_t *event;
			dvbpsi_descriptor_t *descr;
			char name[64+1];
			char sql[256];
			int row = 1;
			int srv_dbid, net_dbid, ts_dbid;
			int start, end;
			uint16_t mjd;
			uint8_t hour, min, sec;
			
			AM_DEBUG(2, "got event AM_EPG_EVT_NEW_EIT ");
			AM_DEBUG(2, "Service id 0x%x:", eit->i_service_id);

			/*查询service*/
			snprintf(sql, sizeof(sql), "select db_net_id,db_ts_id,db_id from srv_table where db_ts_id=\
										(select db_id from ts_table where db_net_id=\
										(select db_id from net_table where network_id='%d') and ts_id='%d')\
										and service_id='%d' limit 1", eit->i_network_id, eit->i_ts_id, eit->i_service_id);
			if (AM_DB_Select(hdb, sql, &row, "%d,%d,%d", &net_dbid, &ts_dbid, &srv_dbid) != AM_SUCCESS || row == 0)
			{
				/*No such service*/
				break;
			}

			AM_SI_LIST_BEGIN(eit->p_first_event, event)
				row = 1;
				/*查找该事件是否已经被添加*/
				snprintf(sql, sizeof(sql), "select db_id from evt_table where db_srv_id='%d' \
											and event_id='%d'", srv_dbid, event->i_event_id);
				if (AM_DB_Select(hdb, sql, &row, "%d", &srv_dbid) == AM_SUCCESS && row > 0)
				{
					continue;
				}
				name[0] = 0;

				/*取UTC时间*/
				mjd = (uint16_t)(event->i_start_time >> 24);
				hour = (uint8_t)(event->i_start_time >> 16);
				min = (uint8_t)(event->i_start_time >> 8);
				sec = (uint8_t)event->i_start_time;
				start = AM_EPG_MJD2SEC(mjd) + AM_EPG_BCD2SEC(hour, min, sec);
				/*取持续事件*/
				hour = (uint8_t)(event->i_duration >> 16);
				min = (uint8_t)(event->i_duration >> 8);
				sec = (uint8_t)event->i_duration;
				end = AM_EPG_BCD2SEC(hour, min, sec);

				end += start;
				
				AM_SI_LIST_BEGIN(event->p_first_descriptor, descr)
					if (descr->i_tag == AM_SI_DESCR_SHORT_EVENT && descr->p_decoded)
					{
						dvbpsi_short_event_dr_t *pse = (dvbpsi_short_event_dr_t*)descr->p_decoded;

						AM_EPG_ConvertCode((char*)pse->i_event_name, pse->i_event_name_length,\
										name, 64);
						name[64] = 0;
						AM_DEBUG(2, "event_id 0x%x, name '%s'", event->i_event_id, name);
						
						break;
					}
				AM_SI_LIST_END()
				snprintf(sql, sizeof(sql), "insert into evt_table(db_net_id, db_ts_id, db_srv_id, event_id, name, start, end) \
											values('%d','%d','%d','%d','%s','%d','%d')", net_dbid, ts_dbid, srv_dbid, event->i_event_id,\
											name, start, end);
				if (sqlite3_exec(hdb, sql, NULL, NULL, &errmsg) != SQLITE_OK)
					AM_DEBUG(1, "%s", errmsg);
				
			AM_SI_LIST_END()
			break;
		}
		default:
			break;
	}
}

static void display_pf_info(void)
{
	char evt[64];
	char sql[200];
	int srv_dbid;
	int now, start, end, lnow;
	int row = 1;
	int chan_num;
	struct tm stim, etim;
	
	if (AM_TV_GetCurrentChannel(htv, &srv_dbid) != AM_SUCCESS)
		return;
		
	AM_EPG_GetUTCTime(&now);

	printf("------------------------------------------------------------------------------------\n");
	/*频道号，频道名称*/
	snprintf(sql, sizeof(sql), "select chan_num,name from srv_table where db_id='%d'", srv_dbid);
	if (AM_DB_Select(hdb, sql, &row, "%d,%s:64", &chan_num, evt) == AM_SUCCESS && row > 0)
	{
		printf("  %03d  %s\t\t|", chan_num, evt);
	}
	
	/*当前事件信息*/
	snprintf(sql, sizeof(sql), "select name,start,end from evt_table where db_srv_id='%d' and \
								start<='%d' and end>='%d'", srv_dbid, now, now);
	if (AM_DB_Select(hdb, sql, &row, "%s:64,%d,%d", evt, &start, &end) == AM_SUCCESS && row > 0)
	{
		AM_EPG_UTCToLocal(start, &start);
		AM_EPG_UTCToLocal(end, &end);
		gmtime_r((time_t*)&start, &stim);
		gmtime_r((time_t*)&end, &etim);
		printf("\t%02d:%02d - %02d:%02d  %s", stim.tm_hour, stim.tm_min, etim.tm_hour, etim.tm_min, evt);
	}
	
	/*当前时间*/
	AM_EPG_UTCToLocal(now, &lnow);
	gmtime_r((time_t*)&lnow, &stim);
	printf("\n%d-%02d-%02d %02d:%02d\t|", 
						   stim.tm_year + 1900, stim.tm_mon+1,
						   stim.tm_mday, stim.tm_hour,
						   stim.tm_min);	
	/*下一事件信息*/
	snprintf(sql, sizeof(sql), "select name,start,end from evt_table where db_srv_id='%d' and \
								start>='%d' order by start limit 1", srv_dbid, now);
	if (AM_DB_Select(hdb, sql, &row, "%s:64,%d,%d", evt, &start, &end) == AM_SUCCESS && row > 0)
	{
		AM_EPG_UTCToLocal(start, &start);
		AM_EPG_UTCToLocal(end, &end);
		gmtime_r((time_t*)&start, &stim);
		gmtime_r((time_t*)&end, &etim);
		printf("\t%02d:%02d - %02d:%02d  %s", stim.tm_hour, stim.tm_min, etim.tm_hour, etim.tm_min, evt);
	}
	printf("\n------------------------------------------------------------------------------------\n");
}

static void display_detail_epg(int day)
{
	static char aevt[200][64];
	static int st[64];
	static int et[64];
	char sql[200];
	int srv_dbid;
	int now, start, end, lnow;
	int row = 1;
	struct tm stim, etim;

	if (day < 0)
		return ;
		
	if (AM_TV_GetCurrentChannel(htv, &srv_dbid) != AM_SUCCESS)
		return;
		
	AM_EPG_GetUTCTime(&now);

	AM_EPG_UTCToLocal(now, &lnow);
	gmtime_r((time_t*)&lnow, &stim);
	/*计算今天零点时间*/
	lnow -= stim.tm_hour*3600 + stim.tm_min*60 + stim.tm_sec;

	/*计算需要显示的那一天的起始时间和结束时间*/
	start = lnow + day * 3600 * 24;
	end = start + 3600 * 24;

	gmtime_r((time_t*)&start, &stim);

	printf("------------------------------------------------------------------------------------\n");
	printf("\t%d-%02d-%02d %s\n", 
						   stim.tm_year + 1900, stim.tm_mon+1,
						   stim.tm_mday,weeks[stim.tm_wday]);	
	printf("------------------------------------------------------------------------------------\n");
						   
	AM_EPG_LocalToUTC(start, &start);
	AM_EPG_LocalToUTC(end, &end);
	
	/*取事件信息*/
	snprintf(sql, sizeof(sql), "select name,start,end from evt_table where db_srv_id='%d' and \
								start>='%d' and start<='%d' order by start", srv_dbid, start, end);
	row = 200;
	if (AM_DB_Select(hdb, sql, &row, "%s:64,%d,%d", aevt, st, et) == AM_SUCCESS && row > 0)
	{
		int i;

		for (i=0; i<row; i++)
		{
			AM_EPG_UTCToLocal(st[i], &st[i]);
			AM_EPG_UTCToLocal(et[i], &et[i]);
			gmtime_r((time_t*)&st[i], &stim);
			gmtime_r((time_t*)&et[i], &etim);
			printf("\t%02d:%02d - %02d:%02d  %s\n", stim.tm_hour, stim.tm_min, etim.tm_hour, etim.tm_min, aevt[i]);
		}
	}

	printf("------------------------------------------------------------------------------------\n");
	printf("Input '0' '1' '2' '3' '4' '5' '6' to view epg offset today\n");
	printf("------------------------------------------------------------------------------------\n");
}

static void auto_scan_input(const char *cmd)
{

}

static void manual_scan_input(const char *cmd)
{

}

static void tv_list_input(const char *cmd)
{
	if(!strncmp(cmd, "u", 1))
	{
		AM_TV_ChannelUp(htv);
	}
	else if(!strncmp(cmd, "d", 1))
	{
		AM_TV_ChannelDown(htv);
	}
	else if(!strncmp(cmd, "n", 1))
	{
		int num;

		sscanf(cmd+1, "%d", &num);
		AM_TV_PlayChannel(htv, num);
	}
	else if (!strncmp(cmd, "radio", 5))
	{
		
	}

	display_pf_info();
}

static void radio_list_input(const char *cmd)
{
	if(!strncmp(cmd, "u", 1))
	{
		AM_TV_ChannelUp(htv);
	}
	else if(!strncmp(cmd, "d", 1))
	{
		AM_TV_ChannelDown(htv);
	}
	else if(!strncmp(cmd, "n", 1))
	{
		int num;

		sscanf(cmd+1, "%d", &num);
		AM_TV_PlayChannel(htv, num);
	}
	else if (!strncmp(cmd, "tv", 2))
	{
		
	}
	display_pf_info();
}

static void freq_input(const char *cmd)
{
	
}

static void restore_input(const char *cmd)
{
	
}

static void epg_input(const char *cmd)
{
	int day;

	sscanf(cmd, "%d", &day);

	display_detail_epg(day);
}


static void store_db_to_file(void)
{
	char *errmsg;

	if (sqlite3_exec(hdb, "attach 'database.db' as filedb", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("error, %s", errmsg);
		return;
	}

	/*删除原来数据*/
	sqlite3_exec(hdb, "drop table if exists filedb.net_table", NULL, NULL, NULL);
	sqlite3_exec(hdb, "drop table if exists filedb.ts_table", NULL, NULL, NULL);
	sqlite3_exec(hdb, "drop table if exists filedb.srv_table", NULL, NULL, NULL);

	/*从内存数据库加载数据到文件中*/
	if (sqlite3_exec(hdb, "create table filedb.net_table as select * from net_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("error, %s", errmsg);
	}
	if (sqlite3_exec(hdb, "create table filedb.ts_table as select * from ts_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("error, %s", errmsg);
	}
	if (sqlite3_exec(hdb, "create table filedb.srv_table as select * from srv_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("error, %s", errmsg);
	}
	
	if (sqlite3_exec(hdb, "detach filedb", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		printf("error, %s", errmsg);
	}
}

static void load_db_from_file(void)
{
	char *errmsg;

	if (sqlite3_exec(hdb, "attach 'database.db' as filedb", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "error, %s", errmsg);
		return;
	}

	/*从文件中加载数据到内存*/
	if (sqlite3_exec(hdb, "insert into net_table select * from filedb.net_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "error, %s", errmsg);
	}
	if (sqlite3_exec(hdb, "insert into ts_table select * from filedb.ts_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "error, %s", errmsg);
	}
	if (sqlite3_exec(hdb, "insert into srv_table select * from filedb.srv_table", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "error, %s", errmsg);
	}
	
	if (sqlite3_exec(hdb, "detach filedb", NULL, NULL, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "error, %s", errmsg);
	}
}

static void progress_evt_callback(int dev_no, int event_type, void *param, void *user_data)
{
	switch (event_type)
	{
		case AM_SCAN_EVT_PROGRESS:
		{
			AM_SCAN_Progress_t *prog = (AM_SCAN_Progress_t*)param;

			if (!prog)
				return;

			switch (prog->evt)
			{
				case AM_SCAN_PROGRESS_SCAN_END:
					/*播放*/
					AM_TV_Play(htv);
					
					printf("-------------------------------------------------------------\n");
					printf("Scan End, input 'b' back to main menu\n");
					printf("-------------------------------------------------------------\n\n");
					
					break;
				case AM_SCAN_PROGRESS_STORE_END:
					/*保存到文件*/
					store_db_to_file();
					break;
				default:
					//AM_DEBUG(1, "Unkonwn EVT");
					break;
			}
		}
	}

}

static void run_quit(void)
{
	go = AM_FALSE;
}

static void run_freq_setting(void)
{
	printf("Input main frequency(kHz):");
	scanf("%u", &fend_param.frequency);
	printf("New main frequency is %u kHz\n", fend_param.frequency);
	printf("-------------------------------------------------------------\n");
	printf("Input 'b' back to main menu\n");
	printf("-------------------------------------------------------------\n\n");
}

static void run_restore(void)
{
	char asw;
	
	printf("Are you sure to restore?(y/n)\n");
	scanf("%c", &asw);

	if (asw == 'y')
	{
		AM_TV_StopPlay(htv);
	
		AM_DEBUG(1, "Restoring...");
		sqlite3_exec(hdb, "delete from net_table", NULL, NULL, NULL);
		sqlite3_exec(hdb, "delete from ts_table", NULL, NULL, NULL);
		sqlite3_exec(hdb, "delete from srv_table", NULL, NULL, NULL);
		sqlite3_exec(hdb, "delete from evt_table", NULL, NULL, NULL);
		store_db_to_file();
		AM_DEBUG(1, "OK");
	}
	
	printf("-------------------------------------------------------------\n");
	printf("Input 'b' back to main menu\n");
	printf("-------------------------------------------------------------\n\n");
}

static void run_auto_scan(void)
{
	AM_TV_StopPlay(htv);

	/*停止监控所有表*/
	AM_EPG_ChangeMode(hepg, AM_EPG_MODE_OP_REMOVE,  AM_EPG_SCAN_ALL);
	
	AM_SCAN_Create(FEND_DEV_NO, DMX_DEV_NO, 0, hdb, &fend_param, 1, AM_SCAN_MODE_AUTO, NULL, &hscan);
	/*注册搜索进度通知事件*/
	AM_EVT_Subscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);

	AM_SCAN_Start(hscan);
}

static void run_manual_scan(void)
{
	struct dvb_frontend_parameters param;
	
	AM_TV_StopPlay(htv);
	/*停止监控所有表*/
	AM_EPG_ChangeMode(hepg, AM_EPG_MODE_OP_REMOVE,  AM_EPG_SCAN_ALL);
	
	param = fend_param;
	printf("Input frequency(kHz):");
	scanf("%u", &param.frequency);
	
	AM_SCAN_Create(FEND_DEV_NO, DMX_DEV_NO, 0, hdb, &param, 1, AM_SCAN_MODE_MANUAL, NULL, &hscan);
	/*注册搜索进度通知事件*/
	AM_EVT_Subscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);

	AM_SCAN_Start(hscan);
}

static void leave_scan(void)
{
	if (hscan != 0)
	{
		AM_DEBUG(1, "Destroying scan");
		AM_EVT_Unsubscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);
		AM_SCAN_Destroy(hscan);
		hscan = 0;

		/*开启EPG监控*/
		AM_EPG_ChangeMode(hepg, AM_EPG_MODE_OP_ADD,  AM_EPG_SCAN_ALL /*& (~AM_EPG_SCAN_EIT_SCHE_ALL)*/);
	}
}

static void run_tv_list(void)
{
	int row = MAX_PROG_CNT;
	int i;
	
	/*从数据库取节目*/
	if (AM_DB_Select(hdb, "select chan_num,name from srv_table where service_type='1' order by chan_num", 
					&row, "%d,%s:64", chan_num, programs) != AM_SUCCESS)
	{
		AM_DEBUG(1, "Get program from dbase failed");
		return;
	}
	if (row == 0)
	{
		AM_DEBUG(1, "No TV program stored");
	}
	/*打印TV列表*/
	printf("-------------------------------------------------------------\n");
	for (i=0; i<row; i++)
	{
		printf("\t\t%03d	%s\n", chan_num[i], programs[i]);
	}
	printf("-------------------------------------------------------------\n");
	printf("Input \t'b' back to main menu, \n\t'u' channel plus,\
			 \n\t'd' channel minus, \n\t'n [number]' play channel 'number',\
			 \n\t'e' 7 days epg\n");
	printf("-------------------------------------------------------------\n\n");
}

static void run_radio_list(void)
{
	int row = MAX_PROG_CNT;
	int i;
	
	/*从数据库取节目*/
	if (AM_DB_Select(hdb, "select chan_num,name from srv_table where service_type='2' order by chan_num", 
					&row, "%d,%s:64", chan_num, programs) != AM_SUCCESS)
	{
		AM_DEBUG(1, "Get program from dbase failed");
		return;
	}
	if (row == 0)
	{
		AM_DEBUG(1, "No Radio program stored");
	}
	/*打印TV列表*/
	printf("-------------------------------------------------------------\n");
	for (i=0; i<row; i++)
	{
		printf("\t\t%03d	%s\n", chan_num[i], programs[i]);
	}
	printf("-------------------------------------------------------------\n");
	printf("Input 'b' back to main menu, 'u' channel plus, 'd' channel minus, n [number] play channel 'number'\n");
	printf("-------------------------------------------------------------\n\n");
}

static void run_epg(void)
{
	/*显示当前频道当天的EPG*/
	display_detail_epg(0);
}

static CommandMenu menus[]=
{
	{0, -1,"Main Menu", 		NULL,	MENU_TYPE_MENU,	NULL,				NULL, 		NULL},
	{1, 0, "1. Auto Scan", 		"1",	MENU_TYPE_APP, 	run_auto_scan,		leave_scan,	auto_scan_input},
	{2, 0, "2. Manual Scan",	"2",	MENU_TYPE_APP,	run_manual_scan,	leave_scan,	manual_scan_input},
	{3, 0, "3. TV List", 		"3",	MENU_TYPE_APP, 	run_tv_list,		NULL,		tv_list_input},
	{4, 0, "4. Radio List", 	"4",	MENU_TYPE_APP, 	run_radio_list,		NULL,		radio_list_input},
	{5, 0, "5. Freq Setting", 	"5",	MENU_TYPE_APP,	run_freq_setting,	NULL,		freq_input},
	{6, 0, "6. Restore",		"6",	MENU_TYPE_APP, 	run_restore,		NULL,		restore_input},
	{7, 0, "7. Quit",			"7",	MENU_TYPE_APP,	run_quit,			NULL,		NULL},
	{8, 3, NULL,				"e",	MENU_TYPE_APP,	run_epg,			NULL,		epg_input},
};

static void display_menu(void)
{
	int i;

	assert(curr_menu);

	printf("-------------------------------------------\n");
	printf("\t\t%s\n", curr_menu->title);
	printf("-------------------------------------------\n");
	for (i=0; i<AM_ARRAY_SIZE(menus); i++)
	{
		if (menus[i].parent == curr_menu->id)
			printf("\t\t%s\n", menus[i].title);
	}
	printf("-------------------------------------------\n");
}


static void show_main_menu(void)
{
		
	curr_menu = &menus[0];
	display_menu();
}

static int menu_input(const char *cmd)
{
	int i;

	assert(curr_menu);

	if(!strcmp(cmd, "b"))
	{
		if (curr_menu->leave)
			curr_menu->leave();
		
		/*退回上级菜单*/
		if (curr_menu->parent != -1)
		{
			curr_menu = &menus[curr_menu->parent];
			if (curr_menu->type == MENU_TYPE_MENU)
			{
				display_menu();
			}
			else if (curr_menu->app)
			{
				curr_menu->app();
			}
		}
		return 0;
	}

	/*从预定义中查找按键处理*/		
	for (i=0; i<AM_ARRAY_SIZE(menus); i++)
	{
		if (menus[i].parent == curr_menu->id && !strcmp(cmd, menus[i].cmd))
		{
			curr_menu = &menus[i];
			if (menus[i].type == MENU_TYPE_MENU)
			{
				display_menu();
			}
			else if (menus[i].app)
			{
				menus[i].app();
			}
			return 0;
		}
	}

	return -1;
}

static int start_tv_test()
{
	char buf[256];
	
	AM_TRY(AM_DB_Init(&hdb));
	load_db_from_file();
	
	AM_TV_Create(FEND_DEV_NO, AV_DEV_NO, hdb, &htv);

	AM_TRY(AM_EPG_Create(FEND_DEV_NO, DMX_DEV_NO, 0, &hepg));

	AM_EVT_Subscribe(hepg, AM_EPG_EVT_NEW_EIT, epg_evt_call_back, NULL);

	AM_EPG_ChangeMode(hepg, AM_EPG_MODE_OP_ADD,  AM_EPG_SCAN_ALL/* & (~AM_EPG_SCAN_EIT_SCHE_ALL)*/);

	/*设置时间偏移*/
	AM_EPG_SetLocalOffset(8*3600);
	
	fend_param.frequency = 419000;
	fend_param.u.qam.symbol_rate = 6875*1000;
	fend_param.u.qam.modulation = QAM_64;
	fend_param.u.qam.fec_inner = FEC_AUTO;

	hscan = 0;
	show_main_menu();
	
	AM_TV_Play(htv);
	
	while (go)
	{
		if (gets(buf) && curr_menu)
		{
			if (menu_input(buf) < 0)
			{
				/*交由应用处理*/
				if (curr_menu->input)
				{
					curr_menu->input(buf);
				}
			}
		}
	}
	AM_EVT_Unsubscribe(hepg, AM_EPG_EVT_NEW_EIT, epg_evt_call_back, NULL);

	AM_EPG_Destroy(hepg);
	
	AM_TV_Destroy(htv);
	
	AM_TRY(AM_DB_Quit(hdb));

	return 0;
}
int main(int argc, char **argv)
{
	AM_DMX_OpenPara_t para;
	AM_FEND_OpenPara_t fpara;
	AM_AV_OpenPara_t apara;
	
	
	
	memset(&fpara, 0, sizeof(fpara));
	AM_TRY(AM_FEND_Open(FEND_DEV_NO, &fpara));
	
	memset(&para, 0, sizeof(para));
	AM_TRY(AM_DMX_Open(DMX_DEV_NO, &para));

	memset(&apara, 0, sizeof(apara));
	AM_TRY(AM_AV_Open(AV_DEV_NO, &apara));
	
	start_tv_test();

	AM_AV_Close(AV_DEV_NO);
	AM_DMX_Close(DMX_DEV_NO);
	AM_FEND_Close(FEND_DEV_NO);
	
	return 0;
}


/****************************************************************************
 * API functions
 ***************************************************************************/

