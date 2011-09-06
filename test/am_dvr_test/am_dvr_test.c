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

static void section_cbf(int dev_no, int fid, const uint8_t *data, int len, void *user_data)
{
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
	int i;

#if 0	
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
		if (gets(buf))
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
			else
			{
				printf("Unkown command: %s\n", buf);
				display_usage();
			}
		}
	}
	
	return 0;
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
	
	if(argc>1)
	{
		sscanf(argv[1], "%d", &freq);
	}
	
	if(!freq)
	{
		freq = 427000;
	}

	if(freq!=-1)
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

	for (i=0; i< DVR_DEV_COUNT; i++)
	{
		AM_DEBUG(1, "Openning DMX%d...", i);
		memset(&para, 0, sizeof(para));
		AM_TRY(AM_DMX_Open(i, &para));
		AM_DMX_SetSource(i, AM_DMX_SRC_TS2);
		AM_DEBUG(1, "Openning DVR%d...", i);
		memset(&dpara, 0, sizeof(dpara));
		AM_TRY(AM_DVR_Open(i, &dpara));

		data_threads[i].id = i;
		data_threads[i].fd = -1;
		data_threads[i].running = 0;
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
	
	AM_FEND_Close(FEND_DEV_NO);

	return 0;
}
