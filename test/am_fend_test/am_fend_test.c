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

static unsigned int blindscan_process = 0;

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
	} else if(info.type == FE_QPSK) {	
		printf("cb parameters: * can get fe type qpsk! *\n");
	}else {
		printf("cb parameters: * can not get fe type! *\n");
	}
	printf("cb status: 0x%x\n", evt->status);
	
	AM_FEND_GetStatus(dev_no, &status);
	AM_FEND_GetBER(dev_no, &ber);
	AM_FEND_GetSNR(dev_no, &snr);
	AM_FEND_GetStrength(dev_no, &strength);
	
	printf("cb status: 0x%0x ber:%d snr:%d, strength:%d\n", status, ber, snr, strength);
}

static void blindscan_cb(int dev_no, AM_FEND_BlindEvent_t *evt, void *user_data)
{
	if(evt->status == AM_FEND_BLIND_START)
	{
		printf("++++++blindscan_start %u\n", evt->freq);
	}
	else if(evt->status == AM_FEND_BLIND_UPDATE)
	{
		blindscan_process = evt->process;
		printf("++++++blindscan_process %u\n", blindscan_process);
	}
}

static void sec(int dev_no)
{
	int sec;
	printf("sec_control\n");
	while(1)
	{
		printf("-----------------------------\n");
		printf("DiseqcResetOverload-0\n");
		printf("DiseqcSendBurst-1\n");
		printf("SetTone-2\n");
		printf("SetVoltage-3\n");
		printf("EnableHighLnbVoltage-4\n");
		printf("Diseqccmd_ResetDiseqcMicro-5\n");
		printf("Diseqccmd_StandbySwitch-6\n");
		printf("Diseqccmd_PoweronSwitch-7\n");
		printf("Diseqccmd_SetLo-8\n");
		printf("Diseqccmd_SetVR-9\n");
		printf("Diseqccmd_SetSatellitePositionA-10\n");
		printf("Diseqccmd_SetSwitchOptionA-11\n");
		printf("Diseqccmd_SetHi-12\n");
		printf("Diseqccmd_SetHL-13\n");
		printf("Diseqccmd_SetSatellitePositionB-14\n");
		printf("Diseqccmd_SetSwitchOptionB-15\n");
		printf("Diseqccmd_SetSwitchInput-16\n");
		printf("Diseqccmd_SetLNBPort4-17\n");
		printf("Diseqccmd_SetLNBPort16-18\n");		
		printf("Diseqccmd_SetChannelFreq-19\n");
		printf("Diseqccmd_SetPositionerHalt-20\n");
		printf("Diseqccmd_DisablePositionerLimit-21\n");
		printf("Diseqccmd_SetPositionerELimit-22\n");
		printf("Diseqccmd_SetPositionerWLimit-23\n");
		printf("Diseqccmd_PositionerGoE-24\n");
		printf("Diseqccmd_PositionerGoW-25\n");	
		printf("Diseqccmd_StorePosition-26\n");
		printf("Diseqccmd_GotoPositioner-27\n");	
		printf("Diseqccmd_GotoAngularPositioner-28\n");			
		printf("Diseqccmd_SetODUChannel-29\n");
		printf("Diseqccmd_SetODUPowerOff-30\n");	
		printf("Diseqccmd_SetODUUbxSignalOn-31\n");
		printf("-----------------------------\n");
		printf("select\n");
		scanf("%d", &sec);
		switch(sec)
		{
			case 0:
				AM_FEND_DiseqcResetOverload(dev_no); 
				break;
				
			case 1:
				{
					int minicmd;
					printf("minicmd_A-0/minicmd_B-1\n");
					scanf("%d", &minicmd);
					AM_FEND_DiseqcSendBurst(dev_no,minicmd);
					break;
				}
				
			case 2:
				{
					int tone;
					printf("on-0/off-1\n");
					scanf("%d", &tone);				
					AM_FEND_SetTone(dev_no, tone); 
					break;
				}

			case 3:
				{
					int voltage;
					printf("v13-0/v18-1/v_off-2\n");
					scanf("%d", &voltage);
					AM_FEND_SetVoltage(dev_no, voltage);				
					break;
				}

			case 4:
				{
					int enable;
					printf("disable-0/enable-1/\n");
					scanf("%d", &enable);				
					AM_FEND_EnableHighLnbVoltage(dev_no, (long)enable);  
					break;
				}

			case 5:
				AM_FEND_Diseqccmd_ResetDiseqcMicro(dev_no);
				break;

			case 6:
				AM_FEND_Diseqccmd_StandbySwitch(dev_no);
				break;
				
			case 7:
				AM_FEND_Diseqccmd_PoweronSwitch(dev_no);
				break;
				
			case 8:
				AM_FEND_Diseqccmd_SetLo(dev_no);
				break;
				
			case 9:
				AM_FEND_Diseqccmd_SetVR(dev_no); 
				break;
				
			case 10:
				AM_FEND_Diseqccmd_SetSatellitePositionA(dev_no); 
				break;
				
			case 11:
				AM_FEND_Diseqccmd_SetSwitchOptionA(dev_no);
				break;
				
			case 12:
				AM_FEND_Diseqccmd_SetHi(dev_no);
				break;
				
			case 13:
				AM_FEND_Diseqccmd_SetHL(dev_no);
				break;
				
			case 14:
				AM_FEND_Diseqccmd_SetSatellitePositionB(dev_no); 
				break;
				
			case 15:
				AM_FEND_Diseqccmd_SetSwitchOptionB(dev_no); 
				break;
				
			case 16:
				{
					int input;
					printf("s1ia-1/s2ia-2/s3ia-3/s4ia-4/s1ib-5/s2ib-6/s3ib-7/s4ib-8/\n");
					scanf("%d", &input);	
					AM_FEND_Diseqccmd_SetSwitchInput(dev_no, input);
					break;
				}
				
			case 17:
				{
					int lnbport, polarisation, local_oscillator_freq;
					printf("lnbport-1-4\n");
					scanf("%d", &lnbport);
					printf("polarisation:H-0/V-1/NO-2\n");
					scanf("%d", &polarisation);
					printf("polarisation:L-0/H-1/NO-2\n");
					scanf("%d", &local_oscillator_freq);				
					AM_FEND_Diseqccmd_SetLNBPort4(dev_no, lnbport, polarisation, local_oscillator_freq);
					break;
				}

			case 18:
				{
					int lnbport, polarisation, local_oscillator_freq;
					printf("lnbport-1-16\n");
					scanf("%d", &lnbport);
					printf("polarisation:H-0/V-1/NO-2\n");
					scanf("%d", &polarisation);
					printf("polarisation:L-0/H-1/NO-2\n");
					scanf("%d", &local_oscillator_freq);				
					AM_FEND_Diseqccmd_SetLNBPort16(dev_no, lnbport, polarisation, local_oscillator_freq);
					break;
				}
                                                                  
			case 19:
				{
					int freq;
					printf("frequency(KHz): ");		
					scanf("%d", &freq);				
					AM_FEND_Diseqccmd_SetChannelFreq(dev_no, freq);
					break;
				}

			case 20:
				AM_FEND_Diseqccmd_SetPositionerHalt(dev_no);
				break;                                                                  

			case 21:
				AM_FEND_Diseqccmd_DisablePositionerLimit(dev_no);
				break;

			case 22:
				AM_FEND_Diseqccmd_SetPositionerELimit(dev_no);
				break;

			case 23:
				AM_FEND_Diseqccmd_SetPositionerWLimit(dev_no);
				break;
				
			case 24:
				{
					unsigned char unit;
					printf("unit continue-0 second-1-127 step-128-255: ");		
					scanf("%d", &unit);				
					AM_FEND_Diseqccmd_PositionerGoE(dev_no, unit);
					break;
				}

			case 25:
				{
					unsigned char unit;
					printf("unit continue-0 second-1-127 step-128-255: ");		
					scanf("%d", &unit);				
					AM_FEND_Diseqccmd_PositionerGoW(dev_no, unit);
					break;
				}

			case 26:
				{
					unsigned char position;
					printf("position 0-255: ");		
					scanf("%d", &position);							
					AM_FEND_Diseqccmd_StorePosition(dev_no, position);
					break;
				}

			case 27:
				{
					unsigned char position;
					printf("position 0-255: ");		
					scanf("%d", &position);				
					AM_FEND_Diseqccmd_GotoPositioner(dev_no, position); 
					break;
				}

			case 28:
				{
					float local_longitude, local_latitude, satellite_longitude;
					printf("local_longitude: ");		
					scanf("%f", &local_longitude);
					printf("local_latitude: ");		
					scanf("%f", &local_latitude);
					printf("satellite_longitude: ");		
					scanf("%f", &satellite_longitude);				
					AM_FEND_Diseqccmd_GotoAngularPositioner(dev_no, local_longitude, local_latitude, satellite_longitude);
	                            break; 
				}                                 

			case 29:
				{
					unsigned char ub_number;
					printf("ub_number 0-7: ");		
					scanf("%d", &ub_number);	
					unsigned char inputbank_number;
					printf("inputbank_number 0-7: ");		
					scanf("%d", &inputbank_number);	
					int transponder_freq;
					printf("transponder_freq(KHz): ");		
					scanf("%d", &transponder_freq);	
					int oscillator_freq;
					printf("oscillator_freq(KHz): ");		
					scanf("%d", &oscillator_freq);	
					int ub_freq;
					printf("ub_freq(KHz): ");		
					scanf("%d", &ub_freq);					
					AM_FEND_Diseqccmd_SetODUChannel(dev_no, ub_number, inputbank_number, transponder_freq, oscillator_freq, ub_freq);
					break;
				}

			case 30:
				{
					unsigned char ub_number;
					printf("ub_number 0-7: ");		
					scanf("%d", &ub_number);					
					AM_FEND_Diseqccmd_SetODUPowerOff(dev_no, ub_number); 
					break;
				}

			case 31:
				AM_FEND_Diseqccmd_SetODUUbxSignalOn(dev_no);
                            break;                           
				
			default:
				break;
		}
	}

	return;
}

int main(int argc, char **argv)
{
	AM_FEND_OpenPara_t para;
	AM_Bool_t loop=AM_TRUE;
	fe_status_t status;
	int fe_id=-1;	
	int blind_scan = 0;
	struct dvb_frontend_parameters blindscan_para[128];
	unsigned int count = 128;
	
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
		
		para.mode = AM_FEND_DEMOD_DVBS;

		if(para.mode != AM_FEND_DEMOD_DVBS){
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
		}else{
			mode = 2;
		}

		AM_TRY(AM_FEND_Open(/*FEND_DEV_NO*/fe_id, &para));

		printf("blindscan(0/1): ");
		scanf("%d", &blind_scan);
		if(blind_scan == 1)
		{
			AM_FEND_BlindScan(fe_id, blindscan_cb, (void *)&fe_id, 950000000, 2150000000);
			while(1){
				if(blindscan_process == 100){
					break;
				}
				//printf("wait process %u\n", blindscan_process);
				usleep(500 * 1000);
			}

			AM_FEND_BlindExit(fe_id); 

			printf("start AM_FEND_BlindGetTPInfo\n");
			
			AM_FEND_BlindGetTPInfo(fe_id, blindscan_para, &count);

			printf("dump TPInfo: %d\n", count);

			int i = 0;
			
			printf("\n\n");
			for(i=0; i < count; i++)
			{
				printf("Ch%2d: RF: %4d SR: %5d ",i+1, (blindscan_para[i].frequency/1000),(blindscan_para[i].u.qpsk.symbol_rate/1000));
				printf("\n");
			}	

			blind_scan = 0;
		}
		
		AM_TRY(AM_FEND_SetCallback(/*FEND_DEV_NO*/fe_id, fend_cb, NULL));
		
		printf("input frontend parameters, frequency=0 quit\n");
		if(para.mode != AM_FEND_DEMOD_DVBS){
			printf("frequency(Hz): ");
		}
		else{
			sec(fe_id);
			
			printf("frequency(KHz): ");
		}
		
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
			}else if(mode==1){
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
			}else{
				printf("dvb sx test\n");

				p.frequency = freq;

				printf("symbol rate: ");
				scanf("%d", &(p.u.qpsk.symbol_rate));
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

