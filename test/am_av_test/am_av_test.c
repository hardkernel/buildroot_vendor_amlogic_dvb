/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 音视频解码测试
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-11: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_av.h>
#include <am_fend.h>
#include <am_dmx.h>
#include <am_vout.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef ANDROID
#include <sys/socket.h>
#endif
#include <arpa/inet.h>

#include <am_misc.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
//#define ENABLE_JPEG_TEST

#define FEND_DEV_NO 0
#define DMX_DEV_NO  1
#define AV_DEV_NO   0
#define OSD_DEV_NO  0

#ifdef ENABLE_JPEG_TEST
#define OSD_FORMAT  AM_OSD_FMT_COLOR_8888
#define OSD_W       1280
#define OSD_H       720
#endif
/****************************************************************************
 * Functions
 ***************************************************************************/

static void usage(void)
{
	printf("usage: am_av_test INPUT\n");
	printf("    INPUT: FILE [loop] [POSITION]\n");
	printf("    INPUT: dvb vpid apid vfmt afmt frequency\n");
	printf("    INPUT: aes FILE\n");
	printf("    INPUT: ves FILE\n");
#ifdef ENABLE_JPEG_TEST	
	printf("    INPUT: jpeg FILE\n");
#endif
	printf("    INPUT: inject IPADDR PORT vpid apid vfmt afmt\n");
	exit(0);
}

static void normal_help(void)
{
	printf("* blackout\n");
	printf("* noblackout\n");
	printf("* enablev\n");
	printf("* disablev\n");
	printf("* window X Y W H\n");
	printf("* contrast VAL\n");
	printf("* saturation VAL\n");
	printf("* brightness VAL\n");
	printf("* fullscreen\n");
	printf("* normaldisp\n");
#ifdef ENABLE_JPEG_TEST		
	printf("* snapshot\n");
#endif
}

static int normal_cmd(const char *cmd)
{
	if(!strncmp(cmd, "blackout", 8))
	{
		AM_AV_EnableVideoBlackout(AV_DEV_NO);
	}
	else if(!strncmp(cmd, "noblackout", 10))
	{
		AM_AV_DisableVideoBlackout(AV_DEV_NO);
	}
	else if(!strncmp(cmd, "enablev", 7))
	{
		AM_AV_EnableVideo(AV_DEV_NO);
	}
	else if(!strncmp(cmd, "disablev", 8))
	{
		AM_AV_DisableVideo(AV_DEV_NO);
	}
	else if(!strncmp(cmd, "window", 6))
	{
		int x, y, w, h;
		
		sscanf(cmd+6, "%d %d %d %d", &x, &y, &w, &h);
		AM_AV_SetVideoWindow(AV_DEV_NO, x, y, w, h);
	}
	else if(!strncmp(cmd, "contrast", 8))
	{
		int v;
		
		sscanf(cmd+8, "%d", &v);
		AM_AV_SetVideoContrast(AV_DEV_NO, v);
	}
	else if(!strncmp(cmd, "saturation", 10))
	{
		int v;
		
		sscanf(cmd+10, "%d", &v);
		AM_AV_SetVideoSaturation(AV_DEV_NO, v);
	}
	else if(!strncmp(cmd, "brightness", 10))
	{
		int v;
		
		sscanf(cmd+10, "%d", &v);
		AM_AV_SetVideoBrightness(AV_DEV_NO, v);
	}
	else if(!strncmp(cmd, "fullscreen", 10))
	{
		AM_AV_SetVideoDisplayMode(AV_DEV_NO, AM_AV_VIDEO_DISPLAY_FULL_SCREEN);
	}
	else if(!strncmp(cmd, "normaldisp", 10))
	{
		AM_AV_SetVideoDisplayMode(AV_DEV_NO, AM_AV_VIDEO_DISPLAY_NORMAL);
	}
	else if(!strncmp(cmd, "snapshot", 8))
	{
#ifndef ANDROID
#ifdef ENABLE_JPEG_TEST
		AM_OSD_Surface_t *s;
		AM_AV_SurfacePara_t para;
		AM_ErrorCode_t ret = AM_SUCCESS;
		
		para.option = 0;
		para.angle  = 0;
		para.width  = 0;
		para.height = 0;
		
		ret = AM_AV_GetVideoFrame(AV_DEV_NO, &para, &s);
		
		if(ret==AM_SUCCESS)
		{
			AM_OSD_Rect_t sr, dr;
			AM_OSD_BlitPara_t bp;
			AM_OSD_Surface_t *disp;
			
			sr.x = 0;
			sr.y = 0;
			sr.w = s->width;
			sr.h = s->height;
			dr.x = 200;
			dr.y = 200;
			dr.w = 200;
			dr.h = 200;
			memset(&bp, 0, sizeof(bp));
			
			AM_OSD_GetSurface(OSD_DEV_NO, &disp);
			AM_OSD_Blit(s, &sr, disp, &dr, &bp);
			AM_OSD_Update(OSD_DEV_NO, NULL);
			AM_OSD_DestroySurface(s);
		}
#endif		
#endif
	}
	else
	{
		return 0;
	}
	
	return 1;
}

static void file_play(const char *name, int loop, int pos)
{
	int running = 1;
	char buf[256];
	int paused = 0;
	AM_DMX_OpenPara_t para;
	
	memset(&para, 0, sizeof(para));
	
	AM_DMX_Open(DMX_DEV_NO, &para);
	AM_DMX_SetSource(DMX_DEV_NO, AM_DMX_SRC_HIU);
	AM_AV_SetTSSource(AV_DEV_NO, AM_AV_TS_SRC_HIU);
	AM_AV_StartFile(AV_DEV_NO, name, loop, pos);
	printf("play \"%s\" loop:%s from:%d\n", name, loop?"y":"n", pos);
	
	while(running)
	{
		printf("********************\n");
		printf("* commands:\n");
		printf("* quit\n");
		printf("* pause\n");
		printf("* resume\n");
		printf("* ff SPEED\n");
		printf("* fb SPEED\n");
		printf("* seek POS\n");
		printf("* status\n");
		printf("* info\n");
		normal_help();
		printf("********************\n");
		
		if(gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				running = 0;
			}
			else if(!strncmp(buf, "pause", 5))
			{
				AM_AV_PauseFile(AV_DEV_NO);
				paused = 1;
			}
			else if(!strncmp(buf, "resume", 6))
			{
				AM_AV_ResumeFile(AV_DEV_NO);
				paused = 0;
			}
			else if(!strncmp(buf, "ff", 2))
			{
				int speed = 0;
				
				sscanf(buf+2, "%d", &speed);
				AM_AV_FastForwardFile(AV_DEV_NO, speed);
			}
			else if(!strncmp(buf, "fb", 2))
			{
				int speed = 0;
				
				sscanf(buf+2, "%d", &speed);
				AM_AV_FastBackwardFile(AV_DEV_NO, speed);
			}
			else if(!strncmp(buf, "seek", 4))
			{
				int pos = 0;
				
				sscanf(buf+4, "%d", &pos);
				AM_AV_SeekFile(AV_DEV_NO, pos, !paused);
			}
			else if(!strncmp(buf, "status", 6))
			{
				AM_AV_PlayStatus_t status;
				
				AM_AV_GetPlayStatus(AV_DEV_NO, &status);
				printf("stats: duration:%d current:%d\n", status.duration, status.position);
			}
			else if(!strncmp(buf, "info", 4))
			{
				AM_AV_FileInfo_t info;
				
				AM_AV_GetCurrFileInfo(AV_DEV_NO, &info);
				printf("info: duration:%d size:%lld\n", info.duration, info.size);
			}
			else
			{
				normal_cmd(buf);
			}
		}
	}
	
	AM_DMX_Close(DMX_DEV_NO);
}

static void dvb_play(int vpid, int apid, int vfmt, int afmt, int freq)
{
	int running = 1;
	char buf[256];
	AM_DMX_OpenPara_t para;
	
	memset(&para, 0, sizeof(para));
	
	AM_DMX_Open(DMX_DEV_NO, &para);
	AM_DMX_SetSource(DMX_DEV_NO, AM_DMX_SRC_TS2);
	AM_AV_SetTSSource(AV_DEV_NO, AM_AV_TS_SRC_TS2);
	
	if(freq>0)
	{
		AM_FEND_OpenPara_t para;
		struct dvb_frontend_parameters p;
		fe_status_t status;
		
		memset(&para, 0, sizeof(para));
		AM_FEND_Open(FEND_DEV_NO, &para);
		
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
		AM_FEND_Lock(FEND_DEV_NO, &p, &status);
		
		printf("lock status: 0x%x\n", status);
	}
	
	AM_AV_StartTS(AV_DEV_NO, vpid, apid, vfmt, afmt);
	
	while(running)
	{
		printf("********************\n");
		printf("* commands:\n");
		printf("* quit\n");
		normal_help();
		printf("********************\n");
		
		if(gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				running = 0;
			}
			else
			{
				normal_cmd(buf);
			}
		}
	}
	
	if(freq>0)
	{
		AM_FEND_Close(FEND_DEV_NO);
	}
	
	AM_DMX_Close(DMX_DEV_NO);
}

static void ves_play(const char *name, int vfmt)
{
	int running = 1;
	char buf[256];
	
	AM_AV_StartVideoES(AV_DEV_NO, vfmt, name);
	
	while(running)
	{
		printf("********************\n");
		printf("* commands:\n");
		printf("* quit\n");
		normal_help();
		printf("********************\n");
		
		if(gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				running = 0;
			}
			else
			{
				normal_cmd(buf);
			}
		}
	}
}

static void aes_play(const char *name, int afmt, int times)
{
	int running = 1;
	char buf[256];
	
	AM_AV_StartAudioES(AV_DEV_NO, afmt, name, times);
	
	while(running)
	{
		printf("********************\n");
		printf("* commands:\n");
		printf("* quit\n");
		printf("********************\n");
		
		if(gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				running = 0;
			}
		}
	}
}

static void jpeg_show(const char *name)
{
#ifndef ANDROID
#ifdef ENABLE_JPEG_TEST
	AM_AV_JPEGInfo_t info;
	AM_OSD_Surface_t *surf, *osd_surf;
	AM_OSD_BlitPara_t blit_para;
	AM_AV_SurfacePara_t s_para;
	
	AM_AV_GetJPEGInfo(AV_DEV_NO, name, &info);
	
	printf("JPEG:\"%s\" width:%d height:%d comp_num:%d\n", name, info.width, info.height, info.comp_num);
	
	s_para.angle  = AM_AV_JPEG_CLKWISE_0;
	s_para.width  = info.width*2;
	s_para.height = info.height*2;
	s_para.option = 0;
	
	if(AM_AV_DecodeJPEG(AV_DEV_NO, name, &s_para, &surf)==AM_SUCCESS)
	{
		AM_OSD_Rect_t sr, dr;
		
		AM_OSD_Enable(AV_DEV_NO);
		AM_OSD_GetSurface(AV_DEV_NO, &osd_surf);
		
		blit_para.enable_alpha = AM_FALSE;
		blit_para.enable_key   = AM_FALSE;
		blit_para.enable_global_alpha = AM_FALSE;
		blit_para.enable_op    = AM_FALSE;
		
		sr.x = 0;
		sr.y = 0;
		sr.w = surf->width;
		sr.h = surf->height;
		dr.x = 0;
		dr.y = 0;
		dr.w = osd_surf->width;
		dr.h = osd_surf->height;
		dr.w = surf->width;
		dr.h = surf->height;
		
		AM_OSD_Blit(surf, &sr, osd_surf, &dr, &blit_para);
		AM_OSD_Update(AV_DEV_NO, NULL);
		AM_OSD_DestroySurface(surf);
	}
#endif
#endif
}

static int inject_running;
static void* inject_entry(void *arg)
{
	int sock = (int)arg;
	static uint8_t buf[32*1024];
	int len, left=0, send, ret;
	
	AM_DEBUG(1, "inject thread start");
	while(inject_running)
	{
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
				//AM_DEBUG(1, "inject %d bytes", send);
			}
		}
	}
	AM_DEBUG(1, "inject thread end");
	
	return NULL;
}

static void inject_play(struct in_addr *addr, int port, int vpid, int apid, int vfmt, int afmt)
{
	int sock, ret;
	struct sockaddr_in in_addr;
	AM_AV_InjectPara_t para;
	pthread_t th;
	int running = 1;
	char buf[256];
	AM_DMX_OpenPara_t dmx_p;
	
	memset(&dmx_p, 0, sizeof(dmx_p));
	
	AM_DMX_Open(DMX_DEV_NO, &dmx_p);
	AM_DMX_SetSource(DMX_DEV_NO, AM_DMX_SRC_HIU);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock==-1)
	{
		AM_DEBUG(1, "create socket failed");
		return;
	}
	
	in_addr.sin_family = AF_INET;
	in_addr.sin_port   = htons(port);
	in_addr.sin_addr   = *addr;
	
	ret = connect(sock, (struct sockaddr*)&in_addr, sizeof(in_addr));
	if(ret==-1)
	{
		AM_DEBUG(1, "connect failed");
		goto end;
	}
	
	AM_DEBUG(1, "connect ok");
	
	para.vid_fmt = vfmt;
	para.aud_fmt = afmt;
	para.pkg_fmt = PFORMAT_TS;
	para.vid_id  = vpid;
	para.aud_id  = apid;
	
	AM_AV_SetTSSource(AV_DEV_NO, AM_AV_TS_SRC_HIU);
	AM_AV_StartInject(AV_DEV_NO, &para);
	
	inject_running = 1;
	pthread_create(&th, NULL, inject_entry, (void*)sock);
	
	while(running)
	{
		printf("********************\n");
		printf("* commands:\n");
		printf("* quit\n");
		normal_help();
		printf("********************\n");
		
		if(gets(buf))
		{
			if(!strncmp(buf, "quit", 4))
			{
				running = 0;
			}
			else
			{
				normal_cmd(buf);
			}
		}
	}
	
	inject_running = 0;
	pthread_join(th, NULL);
	
	AM_AV_StopInject(AV_DEV_NO);
end:
	AM_DMX_Close(DMX_DEV_NO);
	close(sock);
}

int main(int argc, char **argv)
{
	AM_AV_OpenPara_t para;
#ifdef ENABLE_JPEG_TEST	
	AM_OSD_OpenPara_t osd_p;
#endif
	int apid=-1, vpid=-1, vfmt=0, afmt=0, freq=0;
	int i, v, loop=0, pos=0, port=1234;
	struct in_addr addr;
	
	if(argc<2)
		usage();
	
	memset(&para, 0, sizeof(para));
	AM_TRY(AM_AV_Open(AV_DEV_NO, &para));
	
#ifdef ENABLE_JPEG_TEST
	memset(&osd_p, 0, sizeof(osd_p));
	osd_p.format = AM_OSD_FMT_COLOR_8888;
	osd_p.width  = 1280;
	osd_p.height = 720;
	osd_p.output_width  = 1280;
	osd_p.output_height = 720;
	osd_p.enable_double_buffer = AM_FALSE;
#ifndef ANDROID
	AM_TRY(AM_OSD_Open(OSD_DEV_NO, &osd_p));
#endif
#endif
	
	if(strcmp(argv[1], "dvb")==0)
	{
		if(argc<3)
			usage();
		
		vpid = strtol(argv[2], NULL, 0);
		
		if(argc>3)
			apid = strtol(argv[3], NULL, 0);
		
		if(argc>4)
			vfmt = strtol(argv[4], NULL, 0);
		
		if(argc>5)
			afmt = strtol(argv[5], NULL, 0);
		
		if(argc>6)
			freq = strtol(argv[6], NULL, 0);
		
		dvb_play(vpid, apid, vfmt, afmt, freq);
	}
	else if(strcmp(argv[1], "ves")==0)
	{
		char *name;
		int vfmt = 0;
		
		if(argc<3)
			usage();
		
		name = argv[2];
		
		if(argc>3)
			vfmt = strtol(argv[3], NULL, 0);
		
		ves_play(name, vfmt);
	}
	else if(strcmp(argv[1], "aes")==0)
	{
		char *name;
		int afmt = 0;
		int times = 1;
		
		if(argc<3)
			usage();
		
		name = argv[2];
		
		if(argc>3)
			afmt = strtol(argv[3], NULL, 0);
		if(argc>4)
			times = strtol(argv[4], NULL, 0);
		
		aes_play(name, afmt, times);
	}
#ifdef ENABLE_JPEG_TEST	
	else if(strcmp(argv[1], "jpeg")==0)
	{
		if(argc<3)
			usage();
		
		jpeg_show(argv[2]);
	}
#endif	
	else if(strcmp(argv[1], "inject")==0)
	{
		if(argc<5)
			usage();
		
		inet_pton(AF_INET, argv[2], &addr);
		port = strtol(argv[3], NULL, 0);
		vpid = strtol(argv[4], NULL, 0);
		
		if(argc>5)
			apid = strtol(argv[5], NULL, 0);
		
		if(argc>6)
			vfmt = strtol(argv[6], NULL, 0);
		
		if(argc>7)
			afmt = strtol(argv[7], NULL, 0);
		
		inject_play(&addr, port, vpid, apid, vfmt, afmt);
	}
	else
	{
		for(i=2; i<argc; i++)
		{
			if(!strcmp(argv[i], "loop"))
				loop = 1;
			else if(sscanf(argv[i], "%d", &v)==1)
				pos = v;
		}
		
		file_play(argv[1], loop, pos);
	}
	
#ifndef ANDROID
#ifdef ENABLE_JPEG_TEST
	AM_OSD_Close(OSD_DEV_NO);
#endif
#endif
	AM_AV_Close(AV_DEV_NO);
	
	return 0;	
}

