/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_monitor_test.c 
 * \brief 监控模块测试程序
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-11-04: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#define FEND_DEV_NO 0
#define DMX_DEV_NO 0

#include <stdio.h>
#include <am_debug.h>
#include <am_dmx.h>
#include <am_epg.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

static void epg_evt_call_back(int dev_no, int event_type, void *param, void *user_data)
{
	switch (event_type)
	{
		case AM_EPG_EVT_NEW_PAT:
		{
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_PAT ");
			break;
		}
		case AM_EPG_EVT_NEW_PMT:
		{
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_PMT ");
			break;
		}
		case AM_EPG_EVT_NEW_CAT:
		{
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_CAT ");
			break;
		}
		case AM_EPG_EVT_NEW_SDT:
		{
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_SDT ");
			break;
		}
		case AM_EPG_EVT_NEW_TDT:
		{
			dvbpsi_tot_t *tot = (dvbpsi_tot_t*)param;
			dvbpsi_descriptor_t *descr;
			
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_TDT ");
			
			AM_SI_LIST_BEGIN(tot->p_first_descriptor, descr)
				if (descr->i_tag == AM_SI_DESCR_LOCAL_TIME_OFFSET && descr->p_decoded)
				{
					dvbpsi_local_time_offset_dr_t *plto = (dvbpsi_local_time_offset_dr_t*)descr->p_decoded;

					plto = plto;
					AM_DEBUG(2, "Local time offset Descriptor");
				}
			AM_SI_LIST_END()
			
			break;
		}
		case AM_EPG_EVT_NEW_NIT:
		{
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_NIT ");
			break;
		}
		case AM_EPG_EVT_NEW_EIT:
		{
			dvbpsi_eit_t *eit = (dvbpsi_eit_t*)param;
			dvbpsi_eit_event_t *event;
			dvbpsi_descriptor_t *descr;
			char name[256+1];
			
			AM_DEBUG(1, "got event AM_EPG_EVT_NEW_EIT ");
			AM_DEBUG(1, "Service id 0x%x:", eit->i_service_id);
			AM_SI_LIST_BEGIN(eit->p_first_event, event)
				AM_SI_LIST_BEGIN(event->p_first_descriptor, descr)
					if (descr->i_tag == AM_SI_DESCR_SHORT_EVENT && descr->p_decoded)
					{
						dvbpsi_short_event_dr_t *pse = (dvbpsi_short_event_dr_t*)descr->p_decoded;

						AM_EPG_ConvertCode((char*)pse->i_event_name, pse->i_event_name_length,\
										name, 256);
						name[256] = 0;
						AM_DEBUG(1, "event_id 0x%x, name '%s'", event->i_event_id, name);
						break;
					}
				AM_SI_LIST_END()
			AM_SI_LIST_END()
			break;
		}
		default:
			break;
	}
}

static int start_epg_test()
{
	int hmon, mode, op, distance;
	char buf[256];
	char buf1[32];
	AM_Bool_t go = AM_TRUE;
	AM_EPG_CreatePara_t epara;

	epara.fend_dev = FEND_DEV_NO;
	epara.dmx_dev = DMX_DEV_NO;
	epara.source = 0;
	
	AM_TRY(AM_EPG_Create(&epara, &hmon));
	/*注册通知事件*/
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_PAT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_PMT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_CAT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_SDT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_NIT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_TDT, epg_evt_call_back, NULL);
	AM_EVT_Subscribe(hmon, AM_EPG_EVT_NEW_EIT, epg_evt_call_back, NULL);

	printf("------------------------------------------\n");
	printf("commands:\n");
	printf("\t'start [all pat pmt cat sdt nit tdt eit4e eit4f eit50 eit60]'	start recv table\n");
	printf("\t'stop [all pat pmt cat sdt nit tdt eit4e eit4f eit50 eit60]'	stop recv table\n");
	printf("\t'seteitpf [check distance in ms]'	set the eit pf auto check distance\n");
	printf("\t'seteitsche [check distance in ms]'	set the eit schedule auto check distance\n");
	printf("\t'tune [frequency in khz]'	tune the frequency\n");
	printf("\t'utc'	get current UTC time\n");
	printf("\t'local'	get current local time\n");
	printf("\t'offset [offset value in hour]'		set the local offset\n");
	printf("\t'quit'\n");
	printf("------------------------------------------\n");
	
	while (go)
	{
		if (gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				go = AM_FALSE;
				continue;
			}
			else if(!strncmp(buf, "start", 5) || !strncmp(buf, "stop", 4))
			{
				sscanf(buf+5, "%s", buf1);
				if (!strncmp(buf1, "pat", 3))
				{
					mode = AM_EPG_SCAN_PAT;
				}
				else if (!strncmp(buf1, "pmt", 3))
				{
					mode = AM_EPG_SCAN_PMT;
				}
				else if (!strncmp(buf1, "cat", 3))
				{
					mode = AM_EPG_SCAN_CAT;
				}
				else if (!strncmp(buf1, "sdt", 3))
				{
					mode = AM_EPG_SCAN_SDT;
				}
				else if (!strncmp(buf1, "nit", 3))
				{
					mode = AM_EPG_SCAN_NIT;
				}
				else if (!strncmp(buf1, "tdt", 3))
				{
					mode = AM_EPG_SCAN_TDT;
				}
				else if (!strncmp(buf1, "eit4e", 5))
				{
					mode = AM_EPG_SCAN_EIT_PF_ACT;
				}
				else if (!strncmp(buf1, "eit4f", 5))
				{
					mode = AM_EPG_SCAN_EIT_PF_OTH;
				}
				else if (!strncmp(buf1, "eit50", 5))
				{
					mode = AM_EPG_SCAN_EIT_SCHE_ACT;
				}
				else if (!strncmp(buf1, "eit60", 5))
				{
					mode = AM_EPG_SCAN_EIT_SCHE_OTH;
				}
				else if (!strncmp(buf1, "eitall", 6))
				{
					mode = AM_EPG_SCAN_EIT_PF_ACT|AM_EPG_SCAN_EIT_PF_OTH|AM_EPG_SCAN_EIT_SCHE_ACT|AM_EPG_SCAN_EIT_SCHE_OTH;
				}
				else if (!strncmp(buf1, "all", 3))
				{
					mode = 0xFFFFFFFF;
				}
				else
				{
					AM_DEBUG(1, "Invalid command");
					continue;
				}

				if(!strncmp(buf, "start", 5))
					op = AM_EPG_MODE_OP_ADD;
				else
					op = AM_EPG_MODE_OP_REMOVE;

				AM_EPG_ChangeMode(hmon, op, mode);
			}
			else if(!strncmp(buf, "seteitpf", 8))
			{
				sscanf(buf+8, "%d", &distance);
				AM_EPG_SetEITPFCheckDistance(hmon, distance);
			}
			else if(!strncmp(buf, "seteitsche", 10))
			{
				sscanf(buf+10, "%d", &distance);
				AM_EPG_SetEITScheCheckDistance(hmon, distance);
			}
			else if(!strncmp(buf, "tune", 4))
			{
				struct dvb_frontend_parameters p;
				fe_status_t status;

				p.u.qam.symbol_rate = 6875*1000;
				p.u.qam.modulation = QAM_64;
				p.u.qam.fec_inner = FEC_AUTO;

				sscanf(buf+4, "%d", &distance);
				p.frequency = distance;
				AM_FEND_Lock(FEND_DEV_NO, &p, &status);
			}
			else if(!strncmp(buf, "utc", 3))
			{
				int t;
				struct tm *tim;
				
				AM_EPG_GetUTCTime(&t);
			
				tim = gmtime((time_t*)&t);								
				AM_DEBUG(1, "UTC Time: %d-%02d-%02d %02d:%02d:%02d", 
						   tim->tm_year + 1900, tim->tm_mon + 1,
						   tim->tm_mday, tim->tm_hour,
						   tim->tm_min, tim->tm_sec);	
			}
			else if(!strncmp(buf, "local", 5))
			{
				int t;
				struct tm *tim;

				AM_EPG_GetUTCTime(&t);
				AM_EPG_UTCToLocal(t, &t);
			
				tim = gmtime((time_t*)&t);								
				AM_DEBUG(1, "Local Time: %d-%02d-%02d %02d:%02d:%02d", 
						   tim->tm_year + 1900, tim->tm_mon+1,
						   tim->tm_mday, tim->tm_hour,
						   tim->tm_min, tim->tm_sec);	
			}
			else if(!strncmp(buf, "offset", 6))
			{
				int offset;

				sscanf(buf+6, "%d", &offset);
				AM_EPG_SetLocalOffset(offset * 3600);
			}
		}
	}

	AM_DEBUG(1, "Start destroy EPG...");
	AM_TRY(AM_EPG_Destroy(hmon));
	AM_DEBUG(1, "OK");

	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_PAT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_PMT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_CAT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_SDT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_NIT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_TDT, epg_evt_call_back, NULL);
	AM_EVT_Unsubscribe(hmon, AM_EPG_EVT_NEW_EIT, epg_evt_call_back, NULL);

	return 0;
}

int main(int argc, char **argv)
{
	AM_FEND_OpenPara_t fpara;
	AM_DMX_OpenPara_t para;
	
	memset(&fpara, 0, sizeof(fpara));
	AM_TRY(AM_FEND_Open(FEND_DEV_NO, &fpara));

	memset(&para, 0, sizeof(para));
	AM_TRY(AM_DMX_Open(DMX_DEV_NO, &para));
	
	start_epg_test();

	AM_DEBUG(1, "2");
	AM_DMX_Close(DMX_DEV_NO);
	AM_DEBUG(1, "3");
	AM_FEND_Close(FEND_DEV_NO);
	AM_DEBUG(1, "4");
	return 0;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

