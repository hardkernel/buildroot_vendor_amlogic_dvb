#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief DVR测试程序
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-12-10: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#define PLAY_FILE


#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <am_debug.h>
#include <am_dmx.h>
#include <am_av.h>
#include <am_fend.h>
#include <am_dvr.h>
#include <am_misc.h>
#include <errno.h>
#include <am_dsc.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "am_kl.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#ifdef CHIP_8226H
#define DVR_DEV_COUNT      (2)
#elif defined(CHIP_8226M) || defined(CHIP_8626X)
#define DVR_DEV_COUNT      (3)
#else
#define DVR_DEV_COUNT      (2)
#endif

#undef DVR_DEV_COUNT
#define DVR_DEV_COUNT      (1)

#define FEND_DEV_NO 0
#define AV_DEV_NO 0
#define PLAY_DMX_DEV_NO 1

typedef struct
{
	int id;
	char file_name[256];
	pthread_t thread;
	int running;
	int fd;
}DVRData;

static int pat_fid, sdt_fid;
static DVRData data_threads[DVR_DEV_COUNT];

static void section_cbf(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
	UNUSED(dev_no);
	UNUSED(fid);
	UNUSED(len);
	UNUSED(user_data);
	int i;
	
	printf("section:\n");
	for(i=0;i<8;i++)
	{
		printf("%02x ", data[i]);
		if(((i+1)%16)==0) printf("\n");
	}
	
	if((i%16)!=0) printf("\n");
}

static void pes_cbf(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
	UNUSED(dev_no);
	UNUSED(fid);
	UNUSED(data);
	UNUSED(len);
	UNUSED(user_data);

#if 0	
	int i;
	printf("pes:\n");
	for(i=0;i<len;i++)
	{
		printf("%02x ", data[i]);
		if(((i+1)%16)==0) printf("\n");
	}
	
	if((i%16)!=0) printf("\n");
#endif
}

static void display_usage(void)
{
	printf("usages:\n");
	printf("\tsetsrc\t[dvr_no async_fifo_no]\n");
	printf("\tstart\t[dvr_no FILE pid_cnt pids]\n");
	printf("\tstop\t[dvr_no]\n");
	printf("\thelp\n");
	printf("\tquit\n");
}

static void start_si(void)
{
#if 0
	struct dmx_sct_filter_params param;
#define SET_FILTER(f, p, t)\
	AM_DMX_AllocateFilter(DMX_DEV_NO, &(f));\
	AM_DMX_SetCallback(DMX_DEV_NO, f, section_cbf, NULL);\
	memset(&param, 0, sizeof(param));\
	param.pid = p;\
	param.filter.filter[0] = t;\
	param.filter.mask[0] = 0xff;\
	param.flags = DMX_CHECK_CRC;\
	AM_DMX_SetSecFilter(DMX_DEV_NO, f, &param);\
	AM_DMX_SetBufferSize(DMX_DEV_NO, f, 32*1024);\
	AM_DMX_StartFilter(DMX_DEV_NO, f);
		
	if (pat_fid == -1)
	{
		SET_FILTER(pat_fid, 0, 0)
	}
	if (sdt_fid == -1)
	{
		SET_FILTER(sdt_fid, 0x11, 0x42)
	}
#endif
}

static void stop_si(void)
{
#if 0
#define FREE_FILTER(f)\
	AM_DMX_StopFilter(DMX_DEV_NO, f);\
	AM_DMX_FreeFilter(DMX_DEV_NO, f);
	
	if (pat_fid != -1)
	{
		FREE_FILTER(pat_fid)
		pat_fid = -1;
	}

	if (sdt_fid != -1)
	{
		FREE_FILTER(sdt_fid)
		sdt_fid = -1;
	}
#endif
}
#if 1
static void reverse_arr(char *src, char *dest, int len)
{
	int i;
	for(i=0;i<len;i++)	
	{
		dest[len-i-1] = src[i];
	}
}

#endif

static int g_from_kl = 0;
void start_dsc(int dsc, int src, int pid_cnt, int *pids)
{
	AM_DSC_OpenPara_t dsc_para;
	int dscc[8];
	int i;
	int ret;
	struct meson_kl_config kl_config = {0};
	
	uint8_t ekey2[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	uint8_t ekey1[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	uint8_t ecw[16] = {0xe7,0xda,0x69,0xed,0xb7,0x64,0x6b,0xcf,0xe7,0xda,0x69,0xed,0xb7,0x64,0x6b,0xcf}; //For 1234567890abcdef
	//{0xe0,0x63,0xfb,0x95,0xae,0xf1,0xef,0xe5,0xe0,0x63,0xfb,0x95,0xae,0xf1,0xef,0xe5}; //For 10
	//{0x64,0xb0,0x53,0x75,0x10,0x2e,0x55,0x18,0x64,0xb0,0x53,0x75,0x10,0x2e,0x55,0x18}; //For 01
	//{0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68,0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68}; //For key 00000000
	
	memcpy(kl_config.ekey2, ekey2, 16);
	memcpy(kl_config.ekey1, ekey1, 16);
	memcpy(kl_config.ecw, ecw, 16);
	kl_config.kl_levels = 3;
	
	if(g_from_kl)
	{	
		ret = set_keyladder(&kl_config);
		if(ret==AM_FAILURE)
			printf("Set_keyladder failed!!\n");
	}

	memset(&dsc_para, 0, sizeof(AM_DSC_OpenPara_t));
	ret = AM_DSC_Open(dsc, &dsc_para);

	for(i=0; i<pid_cnt; i++) {
		for(i=0; i<pid_cnt; i++) {
			if(pids[i]>0) {
				int ret;
				char key[8] = {0};//{0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef};

				ret = AM_DSC_SetSource(dsc, src);
				ret = AM_DSC_AllocateChannel(dsc, &dscc[i]);
				ret = AM_DSC_SetChannelPID(dsc, dscc[i], pids[i]);
#if 1	
				ret = AM_DSC_SetKey(dsc, dscc[i],
					AM_DSC_KEY_TYPE_ODD | (g_from_kl ? AM_DSC_KEY_FROM_KL : 0),
					(const uint8_t*)key);
				ret = AM_DSC_SetKey(dsc, dscc[i],
					AM_DSC_KEY_TYPE_EVEN | (g_from_kl ? AM_DSC_KEY_FROM_KL : 0),
					(const uint8_t*)key);
				printf("dsc[%d] set default key for pid[%d] kl=%d\n", dsc, pids[i], g_from_kl);
#endif
			}
		}
	}
}

void stop_dsc(int dsc)
{
	AM_DSC_Close(dsc);
}

static int dvr_data_write(int fd, uint8_t *buf, int size)
{
	int ret;
	int left = size;
	uint8_t *p = buf;

	while (left > 0)
	{
		ret = write(fd, p, left);
		if (ret == -1)
		{
			if (errno != EINTR)
			{
				AM_DEBUG(0, "Write DVR data failed: %s", strerror(errno));
				break;
			}
			ret = 0;
		}
		
		left -= ret;
		p += ret;
	}

	return (size - left);
}
static void* dvr_data_thread(void *arg)
{
	DVRData *dd = (DVRData*)arg;
	int cnt;
	uint8_t buf[256*1024];

	AM_DEBUG(1, "Data thread for DVR%d start ,record file will save to '%s'", dd->id, dd->file_name);
	
	while (dd->running)
	{
		cnt = AM_DVR_Read(dd->id, buf, sizeof(buf), 1000);
		if (cnt <= 0)
		{
			AM_DEBUG(1, "No data available from DVR%d", dd->id);
			usleep(200*1000);
			continue;
		}
		//AM_DEBUG(1, "read from DVR%d return %d bytes", dd->id, cnt);
		if (dd->fd != -1)
		{
			dvr_data_write(dd->fd, buf, cnt);
		}
	}

	if (dd->fd != -1)
	{
		close(dd->fd);
		dd->fd = -1;
	}
	AM_DEBUG(1, "Data thread for DVR%d now exit", dd->id);

	return NULL;
}
	
static void start_data_thread(int dev_no)
{
	DVRData *dd = &data_threads[dev_no];
	
	if (dd->running)
		return;
	dd->fd = open(dd->file_name, O_TRUNC | O_WRONLY | O_CREAT, 0666);
	if (dd->fd == -1)
	{
		AM_DEBUG(1, "Cannot open record file '%s' for DVR%d", dd->file_name, dd->id);
		return;
	}
	dd->running = 1;
	pthread_create(&dd->thread, NULL, dvr_data_thread, dd);
}

static void stop_data_thread(int dev_no)
{
	DVRData *dd = &data_threads[dev_no];

	if (! dd->running)
		return;
	dd->running = 0;
	pthread_join(dd->thread, NULL);
	AM_DEBUG(1, "Data thread for DVR%d has exit", dd->id);
}

int start_dvr_test(void)
{
	AM_DVR_StartRecPara_t spara;
	AM_Bool_t go = AM_TRUE;
	char buf[256];
	int pid, apid;

	display_usage();
	pat_fid = sdt_fid = -1;
	
	while (go)
	{
		if (fgets(buf, sizeof(buf), stdin))
		{
			if(!strncmp(buf, "quit", 4))
			{
				go = AM_FALSE;
				continue;
			}
			else if (!strncmp(buf, "start", 5))
			{
				int dev_no, pid_cnt, i;
				int pids[8];
				char name[256];

				/**仅测试最多8个PID*/
				sscanf(buf + 5, "%d %s %d %d %d %d %d %d %d %d %d", &dev_no, name, &pid_cnt, &pids[0], &pids[1],&pids[2],&pids[3],
						&pids[4],&pids[5],&pids[6],&pids[7]);

				if (dev_no < 0 || dev_no >= DVR_DEV_COUNT)
				{
					AM_DEBUG(1, "Invalid DVR dev no, must [%d-%d]", 0, DVR_DEV_COUNT-1);
					continue;
				}
				if (pid_cnt > 8)
					pid_cnt = 8;
					
				strcpy(data_threads[dev_no].file_name, name);
				spara.pid_count = pid_cnt;
				memcpy(&spara.pids, pids, sizeof(pids));
				
				if (AM_DVR_StartRecord(dev_no, &spara) == AM_SUCCESS)
				{
					start_data_thread(dev_no);
				}
				printf("started dev_no=%d pid=%d..\n", dev_no, pids[0]);
			}
			else if (!strncmp(buf, "stop", 4))
			{
				int dev_no;
				
				sscanf(buf + 4, "%d", &dev_no);
				if (dev_no < 0 || dev_no >= DVR_DEV_COUNT)
				{
					AM_DEBUG(1, "Invalid DVR dev no, must [%d-%d]", 0, DVR_DEV_COUNT-1);
					continue;
				}
				AM_DEBUG(1, "stop record for %d", dev_no);
				AM_DVR_StopRecord(dev_no);
				AM_DEBUG(1, "stop data thread for %d", dev_no);
				stop_data_thread(dev_no);
				printf("stopped\n");
			}
			else if (!strncmp(buf, "help", 4))
			{
				display_usage();
			}
			else if (!strncmp(buf, "sistart", 7))
			{
				start_si();
			}
			else if (!strncmp(buf, "sistop", 6))
			{
				stop_si();
			}
			else if (!strncmp(buf, "setsrc", 6))
			{
				int dev_no, fifo_id;
				
				sscanf(buf + 6, "%d %d", &dev_no, &fifo_id);

				AM_DVR_SetSource(dev_no, fifo_id);
			}
			else if (!strncmp(buf, "dscstart", 8))
			{
				int dsc, src, pid_cnt;
				int pids[8];
				sscanf(buf + 8, "%d %d %d %d %d %d %d %d %d %d %d", &dsc, &src, &pid_cnt, &pids[0], &pids[1],&pids[2],&pids[3],
					&pids[4],&pids[5],&pids[6],&pids[7]);
				start_dsc(dsc, src, pid_cnt, pids);
			}
			else if (!strncmp(buf, "dscstop", 7))
			{
				int dsc;
				sscanf(buf + 7, "%d", &dsc);
				stop_dsc(dsc);
			}
			else
			{
				printf("Unkown command: %s\n", buf);
				display_usage();
			}
		}
	}
	
	return 0;
}

static void handle_signal(int signal)
{
	int i;
	UNUSED(signal);
	AM_FileEcho("/sys/class/graphics/fb0/blank","0");
	for (i=0; i< DVR_DEV_COUNT; i++) {
		AM_DVR_Close(i);
		AM_DEBUG(1, "Closing DMX%d...", i);
		AM_DMX_Close(i);
	}
#ifndef PLAY_FILE
	AM_FEND_Close(FEND_DEV_NO);
#endif
	if(g_from_kl)
		keyladder_release();
	exit(0);
}

static void init_signal_handler()
{
	struct sigaction act;
	act.sa_handler = handle_signal;
	sigaction(SIGINT, &act, NULL);
}

static int start_record(int pid0, int pid1, int len)
{
	AM_DVR_StartRecPara_t spara;
	int dev_no, pid_cnt, i;
	int pids[8];
	char name[256] = "a.ts";

	dev_no = 0;
	pid_cnt = len;
	pids[0] = pid0;
	pids[1] = pid1;
	
	strcpy(data_threads[dev_no].file_name, name);
	spara.pid_count = pid_cnt;
	memcpy(&spara.pids, pids, sizeof(pids));
	if (AM_DVR_StartRecord(dev_no, &spara) == AM_SUCCESS)
	{
		start_data_thread(dev_no);
	}
	printf("started dvr dev_no=%d pid=%d %d..\n", dev_no, pids[0], pids[1]);
	return 0;
}

void arg_usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [-r] [-d] file\n"
					"  -r record\n"
					"  -d descramble\n",
			argv0);
}

static int inject_running=0;
static int inject_loop=0;
static int inject_type=AM_AV_INJECT_MULTIPLEX;
static void* inject_entry(void *arg)
{
	int sock = (int)(long)arg;
	uint8_t buf[32712];
	int len, left=0, send, ret;
	int cnt=50;
	AM_AV_VideoStatus_t vStat;

	AM_DEBUG(1, "inject thread start");
	while (inject_running) {
		len = sizeof(buf) - left;
		ret = read(sock, buf+left, len);
		if (ret > 0) {
			AM_DEBUG(1, "recv %d bytes", ret);
			if(!cnt){
				cnt=50;
				printf("recv %d\n", ret);
			}
			cnt--;
			left += ret;
		} else {
			if (inject_loop && ((ret==0) ||(errno == EAGAIN))) {
				printf("loop\n");
				lseek(sock, 0, SEEK_SET);
				left=0;
				continue;
			} else {
				fprintf(stderr, "read file failed [%d:%s] ret=%d left=%d\n", errno, strerror(errno), ret, left);
				break;
			}
		}
		if (left) {
			AM_AV_GetVideoStatus(AV_DEV_NO, &vStat);
			while ( vStat.vb_data > vStat.vb_size*0.6 ) {
				AM_DEBUG(1, "bufferrate too high, vBuf_size:%d, data:%d, free:%d, (%f)",
						vStat.vb_size, vStat.vb_data, vStat.vb_free, vStat.vb_size*0.6);
				usleep(100*1000);
				AM_AV_GetVideoStatus(AV_DEV_NO, &vStat);
			}
			send = left;
			AM_AV_InjectData(AV_DEV_NO, inject_type, buf, &send, -1);
			if (send) {
				left -= send;
				if (left)
					memmove(buf, buf+send, left);
				//AM_DEBUG(1, "inject %d bytes", send);
			}
		}
	}
	AM_DEBUG(1, "inject thread end");

	return NULL;
}

static void inject_play_file(char *name, int fmt, int vpid, int apid, int vfmt, int afmt, int loop)
{
	AM_AV_InjectPara_t para;
	pthread_t th;
	int running = 1;
	char buf[256];
	int fd;

	//AM_DMX_Open(DMX_DEV_NO, &dmx_p);
	//AM_DMX_SetSource(DMX_DEV_NO, AM_DMX_SRC_HIU);

	fd = open(name, O_RDWR, S_IRUSR);
	if (fd < 0) {
		printf("file[%s] open fail. [%d:%s]\n", name, errno, strerror(errno));
		return;
	} else
		printf("file[%s] opened\n", name);

	para.vid_fmt = vfmt;
	para.aud_fmt = afmt;
	para.pkg_fmt = fmt;
	para.vid_id  = vpid;
	para.aud_id  = apid;

	//AM_AV_SetTSSource(AV_DEV_NO, AM_AV_TS_SRC_HIU);
	AM_AV_StartInject(AV_DEV_NO, &para);

	inject_loop = loop;
	inject_running = 1;
	inject_type = (apid==-1)? AM_AV_INJECT_VIDEO :
		(vpid==-1)? AM_AV_INJECT_AUDIO :
		AM_AV_INJECT_MULTIPLEX;

	printf("file inject: [fmt:%d][file:%s][v:%d-%d][a:%d-%d][loop:%d]\n", fmt, name, vpid, vfmt, apid, afmt, loop);
	pthread_create(&th, NULL, inject_entry, (void*)(long)fd);

	//get_section(DMX_DEV_NO, timeout);

//quit:
	//running = 0;
	//inject_running = 0;
	//pthread_join(th, NULL);

	//AM_AV_StopInject(AV_DEV_NO);
//end:
	//AM_DMX_Close(DMX_DEV_NO);
	//close(fd);
}

int main(int argc, char **argv)
{
	AM_FEND_OpenPara_t fpara;
	AM_DMX_OpenPara_t para;
	AM_DVR_OpenPara_t dpara;
	AM_AV_OpenPara_t avpara;
	int i;
	int pid_cnt;
	int pids[8];

#ifdef PLAY_FILE
	AM_ErrorCode_t err;
	char *name = NULL;
	int loop = 0;
	int pos = 0;
	int rec = 0;
	int dsc = 0;
	int kl = 0;

	int opt;
	while ((opt = getopt(argc, argv, "rdk")) != -1) {
		switch (opt) {
			case 'r':
				rec = 1;
				break;
			case 'd':
				dsc = 1;
				break;
			case 'k':
				g_from_kl = kl = 1;
				break;
			default: /* '?' */
				arg_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	printf("record=%d; descramble=%d; keyladder=%d; optind=%d\n", rec, dsc, kl, optind);
	if (optind >= argc) {
		fprintf(stderr, "Expected argument after options\n");
		arg_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	name = argv[optind];
	printf("file name=%s\n", name);

	//VillageFlowSampleTS3, 2nd program
	//pids[0] = 0xd2;
	//pids[1] = 0xdc;
	//dvr orig / redbull video
	pids[0] = 0x206;
	//pids[0] = 0x81;
	//pids[0] = 128;
	pids[1] = -1; //no audio //69;
	pid_cnt = 1;

	if (kl) {
		char buf[8] = {0};
		/*
		AM_FileRead("/sys/module/aml/parameters/debug_kl", buf, sizeof(buf));
		if (buf[0] != '1') {
			fprintf(stderr, "/sys/module/aml/parameters/debug_kl is not 1\n");
			exit(EXIT_FAILURE);
		}
		*/
	}

	init_signal_handler();
	AM_FileEcho("/sys/class/graphics/fb0/blank","1");
#else //else PLAY_FILE
	struct dvb_frontend_parameters p;
	fe_status_t status;
	int freq = 0;
	if(argc>1)
	{
		sscanf(argv[1], "%d", &freq);
	}
	
	if(freq)
	{
		memset(&fpara, 0, sizeof(fpara));
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
#endif //end PLAY_FILE

	if(g_from_kl){
		if(keyladder_init() != AM_SUCCESS){
			printf("Keyladder init failed\n");
			return AM_FAILURE;
		}
	}

	for (i=0; i< DVR_DEV_COUNT; i++)
//	for (i=0; i< 1; i++)
	{
		AM_DEBUG(1, "Openning DMX%d...", i);
		memset(&para, 0, sizeof(para));
		AM_TRY(AM_DMX_Open(i, &para));
#ifndef PLAY_FILE
		AM_DMX_SetSource(i, AM_DMX_SRC_TS0);
#else
		if ((err = AM_AV_Open(i, &avpara)) != AM_SUCCESS)                   { printf("av open err L%d\n", __LINE__); goto end; };
		if ((err = AM_DMX_SetSource(i, AM_DMX_SRC_HIU)) != AM_SUCCESS)      { printf("dmx set_source err L%d\n", __LINE__); goto end; };
		if ((err = AM_AV_SetTSSource(i, AM_AV_TS_SRC_HIU)) != AM_SUCCESS)   { printf("av set_ts_source err L%d\n", __LINE__); goto end; };
#endif
		AM_DEBUG(1, "Openning DVR%d...", i);
		memset(&dpara, 0, sizeof(dpara));
		AM_TRY(AM_DVR_Open(i, &dpara));
		AM_DVR_SetSource(0 /*dev_no*/, 0 /*fifo_id*/);
		data_threads[i].id = i;
		data_threads[i].fd = -1;
		data_threads[i].running = 0;

		if (rec)
			start_record(68, -1, 1);

		if (dsc) {
			start_dsc(0 /*dsc*/, 0 /*src*/, pid_cnt, pids);
		}

		//if ((err = AM_AV_StartFile(i, name, loop, pos)) != AM_SUCCESS)      { printf("err L%d\n", __LINE__); goto end; };
		int fmt = PFORMAT_TS;
		inject_play_file(name, fmt, pids[0] /*vpid*/, pids[1] /*apid*/,
				/*VFORMAT_H264*/VFORMAT_MPEG12 /*vfmt*/, AFORMAT_MPEG /*afmt*/, 0 /*loop*/);
	}
	
	start_dvr_test();

	for (i=0; i< DVR_DEV_COUNT; i++)
	{
		AM_DEBUG(1, "Closing DVR%d...", i);
		if (data_threads[i].running)
			stop_data_thread(i);
		AM_DVR_Close(i);
		AM_DEBUG(1, "Closing DMX%d...", i);
		AM_DMX_Close(i);
	}
	
#ifndef PLAY_FILE
	AM_FEND_Close(FEND_DEV_NO);
#endif

end:
	AM_FileEcho("/sys/class/graphics/fb0/blank","0");
	if(g_from_kl)
		keyladder_release();
	return 0;
}
