/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 解复用设备测试程序
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-07: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_dmx.h>
#include <am_fend.h>
#include <string.h>
#include <unistd.h>

#define FEND_DEV_NO   (0)
#define DMX_DEV_NO    (0)

//#define  PMT_TEST
#define  PAT_TEST
#define  EIT_TEST

#if 0
static FILE *fp;
static int sec_mask[255];
static int sec_cnt = 0;
#else
typedef struct {
	char data[4096];
	int len;
	int got;
} Section;
static Section sections[255];
#endif

static void dump_bytes(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
	FILE *fp;
	int did = data[8];
	int sec = data[6];
	
	
#if 0
	if(did!=3)
		return;
	
	if(sections[sec].got)
	{
		if((sections[sec].len!=len) || memcmp(sections[sec].data, data, len))
		{
			AM_DEBUG(1, "SECTION %d MISMATCH LEN: %d", sec, len);
		}
	}
	else
	{
		sections[sec].len = len;
		sections[sec].got = 1;
		memcpy(sections[sec].data, data, len);
		AM_DEBUG(1, "GET SECTION %d LEN: %d", sec, len);
	}
#endif
#if 0
	int did = data[8];
	int bid = (data[9]<<24)|(data[10]<<16)|(data[11]<<8)|data[12];
	int sec = data[6];
	
	if(did!=3)
		return;
	if(sec_mask[sec])
		return;
	
	sec_mask[sec] = 1;
	AM_DEBUG(1, "get section %d", sec);
	
	fseek(fp, bid*4030, SEEK_SET);
	fwrite(data+13, 1, len-17, fp);
#endif
#if 1
	int i;
	
	printf("section:\n");
	for(i=0;i<len;i++)
	{
		printf("%02x ", data[i]);
		if(((i+1)%16)==0) printf("\n");
	}
	
	if((i%16)!=0) printf("\n");
#endif
}

static int get_section()
{
#ifdef PAT_TEST
	int fid;
#endif

#ifdef PMT_TEST
	int fid_pmt;
#endif

#ifdef EIT_TEST
	int fid_eit_pf, fid_eit_opf;
#endif

	struct dmx_sct_filter_params param;
	struct dmx_pes_filter_params pparam;
#ifdef PAT_TEST
	AM_TRY(AM_DMX_AllocateFilter(DMX_DEV_NO, &fid));
	AM_TRY(AM_DMX_SetCallback(DMX_DEV_NO, fid, dump_bytes, NULL));
#endif

#ifdef PMT_TEST
	AM_TRY(AM_DMX_AllocateFilter(DMX_DEV_NO, &fid_pmt));
	AM_TRY(AM_DMX_SetCallback(DMX_DEV_NO, fid_pmt, dump_bytes, NULL));
#endif

#ifdef EIT_TEST
	AM_TRY(AM_DMX_AllocateFilter(DMX_DEV_NO, &fid_eit_pf));
	AM_TRY(AM_DMX_SetCallback(DMX_DEV_NO, fid_eit_pf, dump_bytes, NULL));
	AM_TRY(AM_DMX_AllocateFilter(DMX_DEV_NO, &fid_eit_opf));
	AM_TRY(AM_DMX_SetCallback(DMX_DEV_NO, fid_eit_opf, dump_bytes, NULL));
#endif

#ifdef PAT_TEST
	memset(&param, 0, sizeof(param));
	param.pid = 0;
	param.filter.filter[0] = 0;
	param.filter.mask[0] = 0xff;
	//param.filter.filter[2] = 0x08;
	//param.filter.mask[2] = 0xff;

	param.flags = DMX_CHECK_CRC;
	
	AM_TRY(AM_DMX_SetSecFilter(DMX_DEV_NO, fid, &param));
#endif
	//pmt
#ifdef PMT_TEST
	memset(&param, 0, sizeof(param));
	param.pid = 0x3f2;
	param.filter.filter[0] = 0x2;
	param.filter.mask[0] = 0xff;
	param.filter.filter[1] = 0x0;
	param.filter.mask[1] = 0xff;
	param.filter.filter[2] = 0x65;
	param.filter.mask[2] = 0xff;
	param.flags = DMX_CHECK_CRC;
	
	AM_TRY(AM_DMX_SetSecFilter(DMX_DEV_NO, fid_pmt, &param));
#endif

#ifdef EIT_TEST
	memset(&param, 0, sizeof(param));
	param.pid = 0x12;
	param.filter.filter[0] = 0x4E;
	param.filter.mask[0] = 0xff;
	param.flags = DMX_CHECK_CRC;
	AM_TRY(AM_DMX_SetSecFilter(DMX_DEV_NO, fid_eit_pf, &param));
	
	memset(&param, 0, sizeof(param));
	param.pid = 0x12;
	param.filter.filter[0] = 0x4F;
	param.filter.mask[0] = 0xff;
	param.flags = DMX_CHECK_CRC;
	AM_TRY(AM_DMX_SetSecFilter(DMX_DEV_NO, fid_eit_opf, &param));
#endif

#ifdef PAT_TEST
	AM_TRY(AM_DMX_SetBufferSize(DMX_DEV_NO, fid, 32*1024));
	AM_TRY(AM_DMX_StartFilter(DMX_DEV_NO, fid));
#endif
#ifdef PMT_TEST
	AM_TRY(AM_DMX_SetBufferSize(DMX_DEV_NO, fid_pmt, 32*1024));
	AM_TRY(AM_DMX_StartFilter(DMX_DEV_NO, fid_pmt));
#endif
#ifdef EIT_TEST
	AM_TRY(AM_DMX_SetBufferSize(DMX_DEV_NO, fid_eit_pf, 32*1024));
	AM_TRY(AM_DMX_StartFilter(DMX_DEV_NO, fid_eit_pf));
	AM_TRY(AM_DMX_SetBufferSize(DMX_DEV_NO, fid_eit_opf, 32*1024));
	//AM_TRY(AM_DMX_StartFilter(DMX_DEV_NO, fid_eit_opf));
#endif
	
	sleep(100);
#ifdef PAT_TEST
	AM_TRY(AM_DMX_StopFilter(DMX_DEV_NO, fid));
	AM_TRY(AM_DMX_FreeFilter(DMX_DEV_NO, fid));
#endif	
#ifdef PMT_TEST
	AM_TRY(AM_DMX_StopFilter(DMX_DEV_NO, fid_pmt));
	AM_TRY(AM_DMX_FreeFilter(DMX_DEV_NO, fid_pmt));
#endif
#ifdef EIT_TEST
	AM_TRY(AM_DMX_StopFilter(DMX_DEV_NO, fid_eit_pf));
	AM_TRY(AM_DMX_FreeFilter(DMX_DEV_NO, fid_eit_pf));
	AM_TRY(AM_DMX_StopFilter(DMX_DEV_NO, fid_eit_opf));
	AM_TRY(AM_DMX_FreeFilter(DMX_DEV_NO, fid_eit_opf));
#endif
	//fclose(fp);
	
	return 0;
}

int main(int argc, char **argv)
{
	AM_DMX_OpenPara_t para;
	AM_FEND_OpenPara_t fpara;
	struct dvb_frontend_parameters p;
	fe_status_t status;
	int freq = 0;
	
	memset(&fpara, 0, sizeof(fpara));
#if 1
	if(argc>1)
	{
		sscanf(argv[1], "%d", &freq);
	}
	
	if(!freq)
	{
		freq = 618000000;
	}

	if(freq!=-1)
	{
		AM_TRY(AM_FEND_Open(FEND_DEV_NO, &fpara));

		p.frequency = freq;
#if 1
		p.inversion = INVERSION_AUTO;
		p.u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
		p.u.ofdm.code_rate_HP = FEC_AUTO;
		p.u.ofdm.code_rate_LP = FEC_AUTO;
		p.u.ofdm.constellation = QAM_AUTO;
		p.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO;
		p.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
		p.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
#else		
		p.u.qam.symbol_rate = 6875000;
		p.u.qam.fec_inner = FEC_AUTO;
		p.u.qam.modulation = QAM_64;
#endif		
	
		AM_TRY(AM_FEND_Lock(FEND_DEV_NO, &p, &status));
		
		if(status&FE_HAS_LOCK)
		{
			printf("locked\n");
		}
		else
		{
			printf("unlocked\n");
		}
	}
#endif	
	memset(&para, 0, sizeof(para));
	AM_TRY(AM_DMX_Open(DMX_DEV_NO, &para));

	AM_TRY(AM_DMX_SetSource(DMX_DEV_NO,AM_DMX_SRC_TS2));
	
	get_section();
	
	AM_DMX_Close(DMX_DEV_NO);
	if(freq!=-1)
		AM_FEND_Close(FEND_DEV_NO);
	
	return 0;
}

