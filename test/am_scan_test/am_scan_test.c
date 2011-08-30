/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_scan_test.c
 * \brief 节目搜索模块测试程序
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-25: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#define FEND_DEV_NO 0
#define DMX_DEV_NO 0

#include <am_debug.h>
#include <iconv.h>
#include <am_scan.h>
#include <am_dmx.h>


sqlite3 *hdb;

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
extern AM_ErrorCode_t AM_EPG_ConvertCode(char *in_code,int in_len,char *out_code,int out_len);

static int print_result_callback(void *param, int col, char **values, char **names)
{
	int i, j, tc, tc1 = 0, tc2 = 0;

	for (i=0; i<col; i++)
	{
		printf("%s ", names[i]);
		if (names[i])
			tc1 = strlen(names[i]);
		if (values[i])
			tc2 = strlen(values[i]);
		tc = tc2 - tc1;
		for (j=0; j<tc; j++)
			printf(" ");
	}
	printf("\n");

	for (i=0; i<col; i++)
	{
		printf("%s ", values[i]);
		if (names[i])
			tc1 = strlen(names[i]);
		if (values[i])
			tc2 = strlen(values[i]);
		tc = tc1 - tc2;
		for (j=0; j<tc; j++)
			printf(" ");
	}
	printf("\n\n");
	
	return 0;
}

static void progress_evt_callback(int dev_no, int event_type, void *param, void *user_data)
{
	char *errmsg = NULL;
	
	switch (event_type)
	{
		case AM_SCAN_EVT_PROGRESS:
		{
			AM_SCAN_Progress_t *prog = (AM_SCAN_Progress_t*)param;

			if (!prog)
				return;

			switch (prog->evt)
			{
				case AM_SCAN_PROGRESS_SCAN_BEGIN:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_SCAN_BEGIN EVT");
					break;
				case AM_SCAN_PROGRESS_SCAN_END:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_SCAN_END EVT");
					break;
				case AM_SCAN_PROGRESS_NIT_BEGIN:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_NIT_BEGIN EVT");
					break;
				case AM_SCAN_PROGRESS_NIT_END:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_NIT_END EVT");
					break;
				case AM_SCAN_PROGRESS_TS_BEGIN:
				{
					struct dvb_frontend_parameters *p = (struct dvb_frontend_parameters*)prog->data;
					
					AM_DEBUG(1, "AM_SCAN_PROGRESS_TS_BEGIN EVT, frequency %u", p->frequency);
				}
					break;
				case AM_SCAN_PROGRESS_TS_END:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_TS_END EVT");
					break;
				case AM_SCAN_PROGRESS_PAT_DONE:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_PAT_DONE EVT");
					break;
				case AM_SCAN_PROGRESS_PMT_DONE:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_PMT_DONE EVT");
					break;
				case AM_SCAN_PROGRESS_CAT_DONE:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_CAT_DONE EVT");
					break;
				case AM_SCAN_PROGRESS_SDT_DONE:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_SDT_DONE EVT");
					{
						dvbpsi_sdt_t *sdts, *sdt;
						dvbpsi_sdt_service_t *srv;
						dvbpsi_descriptor_t *descr;
						char name[64];

						sdts = (dvbpsi_sdt_t *)prog->data;
		
						/*依次取出节目名称并显示*/
						AM_SI_LIST_BEGIN(sdts, sdt)
						AM_SI_LIST_BEGIN(sdt->p_first_service, srv)
						AM_SI_LIST_BEGIN(srv->p_first_descriptor, descr)
							if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SERVICE)
							{
								dvbpsi_service_dr_t *psd = (dvbpsi_service_dr_t*)descr->p_decoded;
			
								/*取节目名称*/
								if (psd->i_service_name_length > 0)
								{
									AM_EPG_ConvertCode((char*)psd->i_service_name, psd->i_service_name_length,\
												name, sizeof(name));
									AM_DEBUG(1,"Service [%s]", name);
								}
							}
						AM_SI_LIST_END()
						AM_SI_LIST_END()
						AM_SI_LIST_END()
					}
					break;
				case AM_SCAN_PROGRESS_STORE_BEGIN:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_STORE_BEGIN EVT");
					break;
				case AM_SCAN_PROGRESS_STORE_END:
					AM_DEBUG(1, "AM_SCAN_PROGRESS_STORE_END EVT");
					/*print the result select from dbase*/
					printf("====================net_table=====================\n");
					sqlite3_exec(hdb, "select * from net_table", print_result_callback, NULL, &errmsg);
					printf("====================ts_table=====================\n");
					sqlite3_exec(hdb, "select * from ts_table", print_result_callback, NULL, &errmsg);
					printf("====================srv_table=====================\n");
					sqlite3_exec(hdb, "select * from srv_table order by chan_num", print_result_callback, NULL, &errmsg);
					break;
				default:
					AM_DEBUG(1, "Unkonwn EVT");
					break;
			}
		}
			break;
		default:
			break;
	}
}
static int start_scan_test()
{
	int hscan = 0;
	int i, mode;
	struct dvb_frontend_parameters p[10];
	char buf[256];
	AM_Bool_t go = AM_TRUE, new_scan = AM_FALSE;
	
	AM_DB_Init(&hdb);

	printf("------------------------------------------------\n");
	printf("commands:\n");
	printf("\tauto [main freq start value]\n");
	printf("\tmanual [freq value]\n");
	printf("\tallband\n");
	printf("\tquit\n");
	printf("------------------------------------------------\n");

	p[0].u.qam.symbol_rate = 6875*1000;
	p[0].u.qam.modulation = QAM_64;
	p[0].u.qam.fec_inner = FEC_AUTO;
	while (go)
	{
		if (gets(buf))
		{
			if(!strncmp(buf, "auto", 4))
			{
				sscanf(buf+4, "%u", &p[0].frequency);
				
				for (i=1; i<10; i++)
				{
					memcpy(&p[i], &p[i-1], sizeof(struct dvb_frontend_parameters));
					p[i].frequency += 1000;
				}
				mode = AM_SCAN_MODE_AUTO|AM_SCAN_MODE_SEARCHBAT;
				new_scan = AM_TRUE;
			}
			else if(!strncmp(buf, "manual", 6))
			{
				sscanf(buf+6, "%u", &p[0].frequency);
				mode = AM_SCAN_MODE_MANUAL;
				new_scan = AM_TRUE;
			}
			else if(!strncmp(buf, "allband", 7))
			{
				mode = AM_SCAN_MODE_ALLBAND;
				new_scan = AM_TRUE;
			}
			else if(!strncmp(buf, "save", 4))
			{
				if (hscan)
				{
					AM_EVT_Unsubscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);
					AM_SCAN_Destroy(hscan, AM_TRUE);
					hscan == 0;
				}
				continue;
			}
			else if(!strncmp(buf, "quit", 4))
			{
				go = AM_FALSE;
				if (hscan)
				{
					AM_EVT_Unsubscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);
					AM_SCAN_Destroy(hscan, AM_FALSE);
				}
				continue;
			}
			else
			{
				AM_DEBUG(1, "Unkonwn command");
			}
		}
		if (new_scan)
		{
			AM_SCAN_CreatePara_t para;
			
			if (hscan)
			{
				AM_EVT_Unsubscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);
				AM_SCAN_Destroy(hscan, AM_FALSE);
				hscan = 0;
			}
			para.fend_dev_id = FEND_DEV_NO;
			para.dmx_dev_id = DMX_DEV_NO;
			para.source = 0;
			para.hdb = hdb;
			para.start_para_cnt = AM_ARRAY_SIZE(p);
			para.start_para = p;
			para.mode = mode;
			para.store_cb = NULL;
			
			AM_SCAN_Create(&para, &hscan);
			/*注册搜索进度通知事件*/
			AM_EVT_Subscribe(hscan, AM_SCAN_EVT_PROGRESS, progress_evt_callback, NULL);

			AM_SCAN_Start(hscan);

			new_scan = AM_FALSE;
		}
	}

	AM_TRY(AM_DB_Quit(hdb));

	return 0;
}

int main(int argc, char **argv)
{
	AM_DMX_OpenPara_t para;
	AM_FEND_OpenPara_t fpara;
	
	memset(&fpara, 0, sizeof(fpara));
	AM_TRY(AM_FEND_Open(FEND_DEV_NO, &fpara));
	
	memset(&para, 0, sizeof(para));
	AM_TRY(AM_DMX_Open(DMX_DEV_NO, &para));

	AM_DMX_SetSource(DMX_DEV_NO, AM_DMX_SRC_TS2);
	start_scan_test();
	
	AM_DMX_Close(DMX_DEV_NO);
	AM_FEND_Close(FEND_DEV_NO);
	
	return 0;
}

/***************************************************** ***********************
 * API functions
 ***************************************************************************/

