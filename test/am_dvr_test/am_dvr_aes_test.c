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


#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <am_debug.h>
#include <am_dmx.h>
#include <am_av.h>
#include <am_fend.h>
#include <am_dvr.h>
#include <errno.h>
#include <am_dsc.h>
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


void start_dsc(int dsc, int src, int pid_cnt, int *pids)
{
	AM_DSC_OpenPara_t dsc_para;
	int dscc[8];
	int i;
	int ret;

	memset(&dsc_para, 0, sizeof(AM_DSC_OpenPara_t));
	ret = AM_DSC_Open(dsc, &dsc_para);

	for(i=0; i<pid_cnt; i++) {
		if(pids[i]>0) {
			int ret;
			//char key[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
			char key[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

			ret = AM_DSC_SetSource(dsc, src);
			ret=AM_DSC_AllocateChannel(dsc, &dscc[i]);
			ret=AM_DSC_SetChannelPID(dsc, dscc[i], pids[i]);
			ret=AM_DSC_SetKey(dsc,dscc[i],AM_DSC_KEY_TYPE_EVEN, (const uint8_t*)key);
			ret=AM_DSC_SetKey(dsc,dscc[i],AM_DSC_KEY_TYPE_ODD, (const uint8_t*)key);
			printf("dsc[%d] set 8 Byte aes default key for pid[%d]\n", dsc, pids[i]);
		}
	}
}
void reverse(unsigned char key[16])
{
    unsigned char tmp[16];
    int i;
    for(i=0;i<16;i++)
        tmp[i] = key[15-i];
    memcpy(key, tmp, 16);
}
static int g_from_kl = 1;

void start_aes(int dsc, int src, int pid_cnt, int *pids)
{
	AM_DSC_OpenPara_t dsc_para;
	int dscc[8];
	int i;
	int ret;
	struct meson_kl_config kl_config = {0};
	uint8_t ekey2[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	uint8_t ekey1[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
	uint8_t ecw[16] =
	//{0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68,0x64,0xb0,0x53,0x75,0x10,0x2e,0x55,0x18}; //For 000000001
	//{0xe0,0x63,0xfb,0x95,0xae,0xf1,0xef,0xe5,0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68}; //For 1000000000000000
	//{0xe9,0x66,0x0d,0x3a,0x4e,0xfe,0x60,0xdb,0xf4,0x0e,0x26,0xab,0x67,0x7d,0xd5,0xa5}; //For fedcba0987654321
	{0xba,0x36,0x77,0x53,0x86,0xb6,0x09,0xea,0x8f,0xe6,0xf2,0xc5,0x8d,0xe7,0x98,0x60}; //For 1234567890abcdef
	//{0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68,0xd3,0x4b,0x29,0xa3,0x7d,0xd4,0x51,0x68}; //For key 00000000
	memset(&dsc_para, 0, sizeof(AM_DSC_OpenPara_t));
	ret = AM_DSC_Open(dsc, &dsc_para);
	
	memcpy(kl_config.ekey2, ekey2, 16);
	memcpy(kl_config.ekey1, ekey1, 16);
	memcpy(kl_config.ecw, ecw, 16);
	kl_config.kl_levels = 3;

	if(g_from_kl)
	{	
		ret = set_keyladder(&kl_config);
		if(ret==AM_FAILURE)
			printf("set_keyladder failed\n");
	}

	memset(&dsc_para, 0, sizeof(AM_DSC_OpenPara_t));
	ret = AM_DSC_Open(dsc, &dsc_para);
	for(i=0; i<pid_cnt; i++) {
		if(pids[i]>0) {
#if 1
			char key[16] = 
			{1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
			char iv_key[16] = 
			{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
			//{0x1, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf}; 
			//{0x8, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0f, 0xe, 0xd, 0xc, 0xb, 0xa, 0x0, 0x9}; 
#else
			char key1[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
			char key2[16] = {0x00};//, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};
			char *key;
			if(i==0)  key = key1;
			else if(i==1) key = key2;
#endif
			ret = AM_DSC_SetSource(dsc, src);
			ret=AM_DSC_AllocateChannel(dsc, &dscc[i]);
			ret=AM_DSC_SetChannelPID(dsc, dscc[i], pids[i]);
			//Set iv key
			ret = AM_DSC_SetKey(dsc, dscc[i], 
				AM_DSC_KEY_TYPE_AES_IV_EVEN | (g_from_kl ? AM_DSC_KEY_FROM_KL : 0), 
				iv_key);
			ret = AM_DSC_SetKey(dsc, dscc[i], 
				AM_DSC_KEY_TYPE_AES_IV_ODD |( g_from_kl ? AM_DSC_KEY_FROM_KL : 0), 
				iv_key);
			//Set key
			ret=AM_DSC_SetKey(dsc,dscc[i],
				AM_DSC_KEY_TYPE_AES_EVEN | (g_from_kl ? AM_DSC_KEY_FROM_KL : 0),
				(const uint8_t*)key);
			ret=AM_DSC_SetKey(dsc,dscc[i],
				AM_DSC_KEY_TYPE_AES_ODD | (g_from_kl ? AM_DSC_KEY_FROM_KL : 0), 
				(const uint8_t*)key);
			printf("dsc[%d] set 16Byte aes default key for pid[%d]\n", dsc, pids[i]);
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
static int inject_running;
static int stop = 0;
static void* inject_entry(void *arg)
{
	int sock = open("/data/cbc.ts", O_RDONLY);
	static uint8_t buf[32*1024];
	int len, left=0, send, ret;

	AM_DEBUG(1, "inject thread start");
	while(inject_running)
	{
		//config PID
		//AM_FileEcho("/sys/class/amlogic/debug", "write 0 c 0x16f3");//#define TS_PL_PID_INDEX (STB_CBUS_BASE + 0xf3) // 0x16f3
		//AM_FileEcho("/sys/class/amlogic/debug", "write 0x880088 c 0x16f4");//#define TS_PL_PID_DATA          (STB_CBUS_BASE + 0xf4) // 0x16f4

		//config FEC_INPUT_CONTROL
		AM_FileEcho("/sys/class/amlogic/debug", "write 0xf000 c 0x1602");
		//config DEMUX_CONTROL
		//AM_FileEcho("/sys/class/amlogic/debug", "write 0x650 c 0x1604");
		
		#if 0
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x1602");////#define FEC_INPUT_CONTROL       (STB_CBUS_BASE + DEMUX_1_OFFSET + 0x02)  // 0x1602
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16f0");//#define STB_TOP_CONFIG          (STB_CBUS_BASE + 0xf0) // 0x16f0
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16fd");//#define CIPLUS_CONFIG 0x16fd
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x1604");//#define DEMUX_CONTROL           (STB_CBUS_BASE + DEMUX_1_OFFSET + 0x04)  // 0x1604
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16f3"); //#define TS_PL_PID_INDEX (STB_CBUS_BASE + 0xf3) // 0x16f3
		#endif
		//#define CIPLUS_KEY1   0x16f9
		//#define CIPLUS_KEY2   0x16fa
		//#define CIPLUS_KEY3   0x16fb
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16f8"); //#define CIPLUS_KEY0   0x16f8
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16f9"); //#define CIPLUS_KEY0   
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16fa"); //#define CIPLUS_KEY0  
		AM_FileEcho("/sys/class/amlogic/debug", "read c 0x16fb"); //#define CIPLUS_KEY0   

		len = sizeof(buf)-left;
		ret = read(sock, buf+left, len);
		if(ret>0)
		{
			//AM_DEBUG(1, "recv %d bytes", ret);
			left += ret;
		}
		else if(ret<0)
		{
			AM_DEBUG(1, "read failed");
			//break;
		}
		if(stop == 1){
			break;
		}
		if(left)
		{
			send = left;
			AM_AV_InjectData(AV_DEV_NO, AM_AV_INJECT_MULTIPLEX, buf, &send, -1);
			if(send)
			{
				left -= send;
				if(left)
					memmove(buf, buf+send, left);
				AM_DEBUG(0, "inject %d bytes", send);
			}
		}
	}
	close(sock);
	AM_DEBUG(1, "inject thread end");

	return NULL;
}

int main(int argc, char **argv)
{
	AM_FEND_OpenPara_t fpara;
	AM_DMX_OpenPara_t para;
	AM_DVR_OpenPara_t dpara;
	struct dvb_frontend_parameters p;
	fe_status_t status;
	int freq = 0;
	int i;
	AM_DVR_StartRecPara_t spara;
	char buf[256];
	pthread_t th;
	DVRData *dd = &data_threads[0];
	int dsc, src, pid_cnt;
	int pids[8] = {0};
	char *name = "/data/record.ts";

	pid_cnt = strtoul(argv[1], NULL, 10);
	if(pid_cnt >= 1)
		pids[0]= strtoul(argv[2], NULL, 10);
	else if(pid_cnt == 2)
		pids[1]= strtoul(argv[3], NULL, 10);
	else{
		printf("temporarily support 2 pids.\n");
		exit(0);
	}
	AM_FileEcho("/sys/class/graphics/fb0/blank","1");

	AM_AV_InjectPara_t av_para;
	AM_AV_OpenPara_t av_open_para;
	av_para.vid_fmt = VFORMAT_MPEG12;//VFORMAT_H264
	av_para.aud_fmt = AFORMAT_MPEG;
	av_para.pkg_fmt = PFORMAT_TS;
	av_para.vid_id  = pids[0];
	av_para.aud_id  = pids[1];

	for (i=0; i< DVR_DEV_COUNT; i++)
	{
		AM_DEBUG(1, "Openning DMX%d...", i);
		memset(&para, 0, sizeof(para));
		AM_TRY(AM_DMX_Open(i, &para));
		AM_DMX_SetSource(i, AM_DMX_SRC_HIU);
		AM_DEBUG(1, "Openning DVR%d...", i);
		memset(&dpara, 0, sizeof(dpara));
		AM_TRY(AM_DVR_Open(i, &dpara));

		data_threads[i].id = i;
		data_threads[i].fd = -1;
		data_threads[i].running = 0;
	}

	AM_TRY(AM_AV_Open(AV_DEV_NO, &av_open_para));
	AM_AV_SetTSSource(AV_DEV_NO, AM_AV_TS_SRC_HIU);
	AM_AV_StartInject(AV_DEV_NO, &av_para);
	AM_DVR_SetSource(0, 0);
	if(g_from_kl){
		if(keyladder_init() != AM_SUCCESS){
			printf("Keyladder init failed\n");
			return -1;
		}
	}

	printf("cnt %d pid %d %d\n", pid_cnt, pids[0], pids[1]);
	dsc = 0;
	src = 0;
	start_aes(dsc, src, pid_cnt, pids);

	strcpy(data_threads[0].file_name, name);
	spara.pid_count = pid_cnt;
	memcpy(&spara.pids, pids, sizeof(pids));

	if (AM_DVR_StartRecord(0, &spara) == AM_SUCCESS)
		start_data_thread(0);

	inject_running = 1;
	pthread_create(&th, NULL, inject_entry, NULL);
	sleep(1);

	stop_dsc(0);

	int dev_no = 0;
	AM_DEBUG(1, "stop record for %d", dev_no);
	AM_DVR_StopRecord(dev_no);
	AM_DEBUG(1, "stop data thread for %d", dev_no);
	stop_data_thread(dev_no);
	stop = 1;

	for (i=0; i< DVR_DEV_COUNT; i++)
	{
		AM_DEBUG(1, "Closing DVR%d...", i);
		if (data_threads[i].running)
			stop_data_thread(i);
		AM_DVR_Close(i);
		AM_DEBUG(1, "Closing DMX%d...", i);
		AM_DMX_Close(i);
	}
	close(dd->fd);
	AM_FileEcho("/sys/class/graphics/fb0/blank","0");
	if(g_from_kl)
		keyladder_release();
	return 0;
}
