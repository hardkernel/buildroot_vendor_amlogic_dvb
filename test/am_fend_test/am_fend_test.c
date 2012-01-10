/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief DVB前端测试
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-08: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_fend.h>
#include <string.h>
#include <stdio.h>
#include <am_misc.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define FEND_DEV_NO    (0)

/****************************************************************************
 * Functions
 ***************************************************************************/

static void fend_cb(int dev_no, struct dvb_frontend_event *evt, void *user_data)
{
	fe_status_t status;
	int ber, snr, strength;
	struct dvb_frontend_info info;

	AM_FEND_GetInfo(dev_no, &info);
	if(info.type == FE_QAM) {
		printf("cb parameters: freq:%d srate:%d modulation:%d fec_inner:%d\n",
			evt->parameters.frequency, evt->parameters.u.qam.symbol_rate,
			evt->parameters.u.qam.modulation, evt->parameters.u.qam.fec_inner);
	} else if(info.type == FE_OFDM) {
		printf("cb parameters: freq:%d bandwidth:%d \n",
			evt->parameters.frequency, evt->parameters.u.ofdm.bandwidth);
	} else {
		printf("cb parameters: * can not get fe type! *\n");
	}
	printf("cb status: 0x%x\n", evt->status);
	
	AM_FEND_GetStatus(dev_no, &status);
	AM_FEND_GetBER(dev_no, &ber);
	AM_FEND_GetSNR(dev_no, &snr);
	AM_FEND_GetStrength(dev_no, &strength);
	
	printf("cb status: 0x%0x ber:%d snr:%d, strength:%d\n", status, ber, snr, strength);
}

int main(int argc, char **argv)
{
	AM_FEND_OpenPara_t para;
	AM_Bool_t loop=AM_TRUE;
	fe_status_t status;
	int fe_id=-1;	

	
	while(loop)
	{
		struct dvb_frontend_parameters p;
		int mode, current_mode;
		int freq, srate, qam;
		int bw;
		char buf[64], name[64];
		
		memset(&para, 0, sizeof(para));
		
		printf("Input fontend id, id=-1 quit\n");
		printf("id: ");
		scanf("%d", &fe_id);
		if(fe_id<0)
			return 0;
		
		para.mode = AM_FEND_DEMOD_AUTO;
		
		printf("Input fontend mode: (0-DVBC, 1-DVBT)\n");
		printf("mode(0/1): ");
		scanf("%d", &mode);
		
		sprintf(name, "/sys/class/amlfe-%d/mode", fe_id);
		if(AM_FileRead(name, buf, sizeof(buf))==AM_SUCCESS) {
			if(sscanf(buf, ":%d", &current_mode)==1) {
				if(current_mode != mode) {
					int r;
					printf("Frontend(%d) dose not match the mode expected, change it?: (y/N) ", fe_id);
					getchar();/*CR*/
					r=getchar();
					if((r=='y') || (r=='Y'))
						para.mode = (mode==0)?AM_FEND_DEMOD_DVBC : 
									(mode==1)? AM_FEND_DEMOD_DVBT : 
									AM_FEND_DEMOD_AUTO;
				}
			}
		}

		AM_TRY(AM_FEND_Open(/*FEND_DEV_NO*/fe_id, &para));
		AM_TRY(AM_FEND_SetCallback(/*FEND_DEV_NO*/fe_id, fend_cb, NULL));
		
		printf("input frontend parameters, frequency=0 quit\n");
		printf("frequency(Hz): ");
		scanf("%d", &freq);
		if(freq!=0)
		{
			if(mode==0) {
				printf("symbol rate(kbps): ");
				scanf("%d", &srate);
				printf("QAM(16/32/64/128/256): ");
				scanf("%d", &qam);
				
				p.frequency = freq;
				p.u.qam.symbol_rate = srate*1000;
				p.u.qam.fec_inner = FEC_AUTO;
				switch(qam)
				{
					case 16:
						p.u.qam.modulation = QAM_16;
					break;
					case 32:
						p.u.qam.modulation = QAM_32;
					break;
					case 64:
					default:
						p.u.qam.modulation = QAM_64;
					break;
					case 128:
						p.u.qam.modulation = QAM_128;
					break;
					case 256:
						p.u.qam.modulation = QAM_256;
					break;
				}
			}else {
				printf("BW[8/7/6/5(AUTO) MHz]: ");
				scanf("%d", &bw);

				p.frequency = freq;
				switch(bw)
				{
					case 8:
					default:
						p.u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
					break;
					case 7:
						p.u.ofdm.bandwidth = BANDWIDTH_7_MHZ;
					break;
					case 6:
						p.u.ofdm.bandwidth = BANDWIDTH_6_MHZ;
					break;
					case 5:
						p.u.ofdm.bandwidth = BANDWIDTH_AUTO;
					break;
				}

				p.u.ofdm.code_rate_HP = FEC_AUTO;
				p.u.ofdm.code_rate_LP = FEC_AUTO;
				p.u.ofdm.constellation = QAM_AUTO;
				p.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO;
				p.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
				p.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
			}
#if 0
			AM_TRY(AM_FEND_SetPara(/*FEND_DEV_NO*/fe_id, &p));
#else
			AM_TRY(AM_FEND_Lock(/*FEND_DEV_NO*/fe_id, &p, &status));
			printf("lock status: 0x%x\n", status);
#endif
		}
		else
		{
			loop = AM_FALSE;
		}
		AM_TRY(AM_FEND_Close(/*FEND_DEV_NO*/fe_id));
	}
	
	
	return 0;
}

