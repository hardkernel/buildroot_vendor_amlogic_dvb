#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief AMLogic 音视频解码驱动
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-09: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_misc.h>
#include <am_mem.h>
#include <am_evt.h>
#include <am_time.h>
#include <am_thread.h>
#include "../am_av_internal.h"
#include "../../am_aout/am_aout_internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <amports/amstream.h>

#ifdef ANDROID
#include <sys/system_properties.h>
#endif


#ifdef CHIP_8226H
#include <linux/jpegdec.h>
#endif
#if defined(CHIP_8226M) || defined(CHIP_8626X)
#include <linux/amports/jpegdec.h>
#endif

#if defined(ANDROID) || defined(CHIP_8626X)
  #define PLAYER_API_NEW
  #define ADEC_API_NEW
#endif

#if !defined(ANDROID) && !defined(CHIP_8626X)
#define MEDIA_PLAYER
#include <player_plugins/mp_api.h>
#include <player_plugins/mp_types.h>
#include <player_plugins/mp_utils.h>
#else
#include "player.h"
#define PLAYER_INFO_POP_INTERVAL 500
#define FILENAME_LENGTH_MAX 2048

#include <codec_type.h>
#include <adec-external-ctrl.h>

void *adec_handle;
#endif

#ifndef TRICKMODE_NONE
#define TRICKMODE_NONE      0x00
#define TRICKMODE_I         0x01
#define TRICKMODE_FFFB      0x02
#define TRICK_STAT_DONE     0x01
#define TRICK_STAT_WAIT     0x00
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define AOUT_DEV_NO 0

#define STREAM_VBUF_FILE    "/dev/amstream_vbuf"
#define STREAM_ABUF_FILE    "/dev/amstream_abuf"
#define STREAM_TS_FILE      "/dev/amstream_mpts"
#define STREAM_PS_FILE      "/dev/amstream_mpps"
#define STREAM_RM_FILE      "/dev/amstream_rm"
#define JPEG_DEC_FILE       "/dev/amjpegdec"
#define AMVIDEO_FILE        "/dev/amvideo"

#define PPMGR_FILE			"/dev/ppmgr"

#define VID_AXIS_FILE       "/sys/class/video/axis"
#define VID_CONTRAST_FILE   "/sys/class/video/contrast"
#define VID_SATURATION_FILE "/sys/class/video/saturation"
#define VID_BRIGHTNESS_FILE "/sys/class/video/brightness"
#define VID_DISABLE_FILE    "/sys/class/video/disable_video"
#define VID_BLACKOUT_FILE   "/sys/class/video/blackout_policy"
#define VID_SCREEN_MODE_FILE  "/sys/class/video/screen_mode"

/*{"none", "ignore", "pan-scan", "letter_box", "combined"}*/
#define VID_SCREEN_MODE_FILE  "/sys/class/video/screen_mode" 
#define VID_ASPECT_RATIO_FILE "/sys/class/video/aspect_ratio"   

#ifdef CHIP_8226H
#define VID_ASPECT_MATCH_FILE "/sys/class/video/aspect_match_mode"
#else
/*{"normal", "full stretch", "4-3", "16-9"}*/
#define VID_ASPECT_MATCH_FILE "/sys/class/video/matchmethod" 
#endif

#define DISP_MODE_FILE      "/sys/class/display/mode"
#define ASTREAM_FORMAT_FILE "/sys/class/astream/format"

#if defined(ANDROID) || defined(CHIP_8626X)
#define PCR_RECOVER_FILE	"/sys/class/tsync/pcr_recover"
#endif

#define ENABLE_AUDIO_RESAMPLE
//#define ENABLE_PCR_RECOVER
#define AUDIO_CACHE_TIME        200

#ifdef ENABLE_AUDIO_RESAMPLE
#define ENABLE_RESAMPLE_FILE    "/sys/class/amaudio/enable_resample"
#define RESAMPLE_TYPE_FILE      "/sys/class/amaudio/resample_type"
#endif

#define AUDIO_DMX_PTS_FILE	"/sys/class/stb/audio_pts"
#define VIDEO_DMX_PTS_FILE	"/sys/class/stb/video_pts"
#define AUDIO_PTS_FILE	"/sys/class/tsync/pts_audio"
#define VIDEO_PTS_FILE	"/sys/class/tsync/pts_video"

#define CANVAS_ALIGN(x)    (((x)+7)&~7)
#define JPEG_WRTIE_UNIT    (32*1024)
#define AUDIO_START_LEN (0*1024)
#define AUDIO_LOW_LEN (1*1024)
#define AV_SYNC_THRESHOLD	300
#define AV_SMOOTH_SYNC_VAL "300"

#ifdef ANDROID
#define audio_decode_start(h)\
	AM_MACRO_BEGIN\
	audio_decode_start(h);\
	audio_decode_set_volume(h, 1.);\
	AM_MACRO_END

#define open(a...)\
	({\
	 int ret, times=300;\
	 do{\
	 	ret = open(a);\
	 	if(ret==-1)\
	 		usleep(10000);\
	 }while(ret==-1 && times--);\
	 ret;\
	 })
#endif

#define PPMGR_IOC_MAGIC         'P'
#define PPMGR_IOC_2OSD0         _IOW(PPMGR_IOC_MAGIC, 0x00, unsigned int)
#define PPMGR_IOC_ENABLE_PP     _IOW(PPMGR_IOC_MAGIC, 0X01, unsigned int)
#define PPMGR_IOC_CONFIG_FRAME  _IOW(PPMGR_IOC_MAGIC, 0X02, unsigned int)
#define PPMGR_IOC_VIEW_MODE     _IOW(PPMGR_IOC_MAGIC, 0X03, unsigned int)

#define MODE_3D_DISABLE         0x00000000
#define MODE_3D_ENABLE          0x00000001
#define MODE_AUTO               0x00000002
#define MODE_2D_TO_3D           0x00000004
#define MODE_LR                 0x00000008
#define MODE_BT                 0x00000010
#define MODE_LR_SWITCH          0x00000020
#define MODE_FIELD_DEPTH        0x00000040
#define MODE_3D_TO_2D_L         0x00000080
#define MODE_3D_TO_2D_R         0x00000100
#define LR_FORMAT_INDICATOR     0x00000200
#define BT_FORMAT_INDICATOR     0x00000400



/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 音视频数据注入*/
typedef struct
{
	int             fd;
	int             video_fd;
	pthread_t       thread;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	const uint8_t  *data;
	int             len;
	int             times;
	int             pos;
	int             dev_no;
	AM_Bool_t       is_audio;
	AM_Bool_t       running;
} AV_DataSource_t;

/**\brief JPEG解码相关数据*/
typedef struct
{
	int             vbuf_fd;
	int             dec_fd;
} AV_JPEGData_t;

/**\brief JPEG解码器状态*/
typedef enum {
	AV_JPEG_DEC_STAT_DECDEV,
	AV_JPEG_DEC_STAT_INFOCONFIG,
	AV_JPEG_DEC_STAT_INFO,
	AV_JPEG_DEC_STAT_DECCONFIG,
	AV_JPEG_DEC_STAT_RUN
} AV_JPEGDecState_t;
#ifdef  MEDIA_PLAYER
/**\brief 文件播放器应答类型*/
typedef enum
{
	AV_MP_RESP_OK,
	AV_MP_RESP_ERR,
	AV_MP_RESP_STOP,
	AV_MP_RESP_DURATION,
	AV_MP_RESP_POSITION,
	AV_MP_RESP_MEDIA
} AV_MpRespType_t;

/**\brief 文件播放器数据*/
typedef struct
{
	int                media_id;
	AV_MpRespType_t    resp_type;
	union
	{
		int        duration;
		int        position;
		MP_AVMediaFileInfo media;
	} resp_data;
	pthread_mutex_t    lock;
	pthread_cond_t     cond;
	MP_PlayBackState   state;
	AM_AV_Device_t    *av;
} AV_FilePlayerData_t;
#elif defined(PLAYER_API_NEW)
/**\brief 文件播放器数据*/
typedef struct
{
	int                media_id;
	play_control_t pctl;
	pthread_mutex_t    lock;
	pthread_cond_t     cond;
	AM_AV_Device_t    *av;
} AV_FilePlayerData_t;
#endif

/**\brief 数据注入播放模式相关数据*/
typedef struct
{
	AM_AV_PFormat_t    pkg_fmt;
	int                aud_fd;
	int                vid_fd;
} AV_InjectData_t;

/**\brief Timeshift播放状态*/
typedef enum
{
	AV_TIMESHIFT_STAT_STOP,
	AV_TIMESHIFT_STAT_PLAY,
	AV_TIMESHIFT_STAT_PAUSE,
	AV_TIMESHIFT_STAT_FFFB,
	AV_TIMESHIFT_STAT_EXIT,
	AV_TIMESHIFT_STAT_INITOK,
	AV_TIMESHIFT_STAT_SEARCHOK,
} AV_TimeshiftState_t;

typedef struct AV_TimeshiftSubfile
{
	int rfd;
	int wfd;
	int findex;
	loff_t size;

	struct AV_TimeshiftSubfile *next;
}AV_TimeshiftSubfile_t;

/**\brief Timeshift文件*/
typedef struct
{
	int		opened;	/**< 是否已打开*/
	loff_t	size;	/**< 文件长度*/
	loff_t	avail;	/**< 当前可读数据长度*/
	loff_t	total;	/**< 当前已写入的数据长度*/
	loff_t	start;	/**< 文件起始位置*/
	loff_t	read;	/**< 当前读位置*/
	loff_t	write;	/**< 当前写位置*/
	pthread_mutex_t lock;	/*读写锁*/
	pthread_cond_t		cond;
	AM_Bool_t	loop;	/*是否使用循环文件*/
	AM_Bool_t	is_timeshift;
	char	*name;	/*文件名称*/

	/* sub files control */
	int last_sub_index;
	loff_t sub_file_size;
	AV_TimeshiftSubfile_t *sub_files;
	AV_TimeshiftSubfile_t *cur_rsub_file;
	AV_TimeshiftSubfile_t *cur_wsub_file;
} AV_TimeshiftFile_t;

/**\brief Timeshift播放模式相关数据*/
typedef struct
{
	int					av_fd;
	int					speed;
	int					running;
	int					rate;	/**< 码率 bytes/s*/
	int					duration;	/**< 时移时长，固定值*/
	int					dmxfd;
	int					cntl_fd;
	int					fffb_time;
	int					fffb_base;
	int					left;
	int					inject_size;
	int					timeout;
	int					seek_pos;
	loff_t				rtotal;
	int					rtime;
	int					aud_pid;
	int					aud_fmt;
	AV_PlayCmd_t		cmd;
	AV_PlayCmd_t		last_cmd;
	pthread_t			thread;
	pthread_mutex_t		lock;
	pthread_cond_t		cond;
	AV_TimeshiftState_t	state;
	AV_TimeshiftFile_t	file;
	AM_AV_TimeshiftPara_t para;
	AM_AV_Device_t       *dev;
	AM_AV_TimeshiftInfo_t	info;
	char 				last_stb_src[16];
} AV_TimeshiftData_t;

/****************************************************************************
 * Static data
 ***************************************************************************/

/*音视频设备操作*/
static AM_ErrorCode_t aml_open(AM_AV_Device_t *dev, const AM_AV_OpenPara_t *para);
static AM_ErrorCode_t aml_close(AM_AV_Device_t *dev);
static AM_ErrorCode_t aml_open_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode);
static AM_ErrorCode_t aml_start_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode, void *para);
static AM_ErrorCode_t aml_close_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode);
static AM_ErrorCode_t aml_ts_source(AM_AV_Device_t *dev, AM_AV_TSSource_t src);
static AM_ErrorCode_t aml_file_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *data);
static AM_ErrorCode_t aml_inject_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *data);
static AM_ErrorCode_t aml_file_status(AM_AV_Device_t *dev, AM_AV_PlayStatus_t *status);
static AM_ErrorCode_t aml_file_info(AM_AV_Device_t *dev, AM_AV_FileInfo_t *info);
static AM_ErrorCode_t aml_set_video_para(AM_AV_Device_t *dev, AV_VideoParaType_t para, void *val);
static AM_ErrorCode_t aml_inject(AM_AV_Device_t *dev, AM_AV_InjectType_t type, uint8_t *data, int *size, int timeout);
static AM_ErrorCode_t aml_video_frame(AM_AV_Device_t *dev, const AM_AV_SurfacePara_t *para, AM_OSD_Surface_t **s);
static AM_ErrorCode_t aml_get_astatus(AM_AV_Device_t *dev, AM_AV_AudioStatus_t *para);
static AM_ErrorCode_t aml_get_vstatus(AM_AV_Device_t *dev, AM_AV_VideoStatus_t *para);
static AM_ErrorCode_t aml_timeshift_fill_data(AM_AV_Device_t *dev, uint8_t *data, int size);
static AM_ErrorCode_t aml_timeshift_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *para);
static AM_ErrorCode_t aml_timeshift_get_info(AM_AV_Device_t *dev, AM_AV_TimeshiftInfo_t *info);
static AM_ErrorCode_t aml_set_vpath(AM_AV_Device_t *dev);
static AM_ErrorCode_t aml_switch_ts_audio(AM_AV_Device_t *dev, uint16_t apid, AM_AV_AFormat_t afmt);

const AM_AV_Driver_t aml_av_drv =
{
.open        = aml_open,
.close       = aml_close,
.open_mode   = aml_open_mode,
.start_mode  = aml_start_mode,
.close_mode  = aml_close_mode,
.ts_source   = aml_ts_source,
.file_cmd    = aml_file_cmd,
.inject_cmd  = aml_inject_cmd,
.file_status = aml_file_status,
.file_info   = aml_file_info,
.set_video_para = aml_set_video_para,
.inject      = aml_inject,
.video_frame = aml_video_frame,
.get_audio_status = aml_get_astatus,
.get_video_status = aml_get_vstatus,
.timeshift_fill = aml_timeshift_fill_data,
.timeshift_cmd = aml_timeshift_cmd,
.get_timeshift_info = aml_timeshift_get_info,
.set_vpath   = aml_set_vpath,
.switch_ts_audio = aml_switch_ts_audio
};

/*音频控制（通过解码器）操作*/
static AM_ErrorCode_t adec_open(AM_AOUT_Device_t *dev, const AM_AOUT_OpenPara_t *para);
static AM_ErrorCode_t adec_set_volume(AM_AOUT_Device_t *dev, int vol);
static AM_ErrorCode_t adec_set_mute(AM_AOUT_Device_t *dev, AM_Bool_t mute);
static AM_ErrorCode_t adec_set_output_mode(AM_AOUT_Device_t *dev, AM_AOUT_OutputMode_t mode);
static AM_ErrorCode_t adec_close(AM_AOUT_Device_t *dev);

const AM_AOUT_Driver_t adec_aout_drv =
{
.open         = adec_open,
.set_volume   = adec_set_volume,
.set_mute     = adec_set_mute,
.set_output_mode = adec_set_output_mode,
.close        = adec_close
};

/*音频控制（通过amplayer2）操作*/
static AM_ErrorCode_t amp_open(AM_AOUT_Device_t *dev, const AM_AOUT_OpenPara_t *para);
static AM_ErrorCode_t amp_set_volume(AM_AOUT_Device_t *dev, int vol);
static AM_ErrorCode_t amp_set_mute(AM_AOUT_Device_t *dev, AM_Bool_t mute);
static AM_ErrorCode_t amp_set_output_mode(AM_AOUT_Device_t *dev, AM_AOUT_OutputMode_t mode);
static AM_ErrorCode_t amp_close(AM_AOUT_Device_t *dev);

const AM_AOUT_Driver_t amplayer_aout_drv =
{
.open         = amp_open,
.set_volume   = amp_set_volume,
.set_mute     = amp_set_mute,
.set_output_mode = amp_set_output_mode,
.close        = amp_close
};

/*监控AV buffer, PTS 操作*/
static pthread_mutex_t gAVMonLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gAVMonCond = PTHREAD_COND_INITIALIZER;

static void* aml_av_monitor_thread(void *arg);
static AM_ErrorCode_t aml_get_pts(const char *class_file,  uint32_t *pts);

/*Timeshift 操作*/
static void *aml_timeshift_thread(void *arg);

/****************************************************************************
 * Static functions
 ***************************************************************************/

static int get_amstream(AM_AV_Device_t *dev)
{
	if(dev->mode&AV_PLAY_TS)
	{
		return (int)dev->ts_player.drv_data;
	}
	else if(dev->mode&AV_INJECT)
	{
		AV_InjectData_t *inj = (AV_InjectData_t*)dev->inject_player.drv_data;
		if(inj->vid_fd!=-1)
			return inj->vid_fd;
		else
			return inj->aud_fd; 
	}
	else if(dev->mode&AV_PLAY_VIDEO_ES)
	{
		AV_DataSource_t *src = (AV_DataSource_t*)dev->vid_player.drv_data;
		return src->fd;
	}
	else if(dev->mode&AV_PLAY_AUDIO_ES)
	{
		AV_DataSource_t *src = (AV_DataSource_t*)dev->aud_player.drv_data;
		return src->fd;
	}
	else if(dev->mode&(AV_GET_JPEG_INFO|AV_DECODE_JPEG))
	{
		AV_JPEGData_t *jpeg = (AV_JPEGData_t*)dev->vid_player.drv_data;
		return jpeg->vbuf_fd;
	}
	
	return -1;
}

/*音频控制（通过解码器）操作*/
static AM_ErrorCode_t adec_cmd(const char *cmd)
{
#if !defined(ADEC_API_NEW)
	AM_ErrorCode_t ret;
	char buf[32];
	int fd;
	
	ret = AM_LocalConnect("/tmp/amadec_socket", &fd);
	if(ret!=AM_SUCCESS)
		return ret;
	
	ret = AM_LocalSendCmd(fd, cmd);
	
	if(ret==AM_SUCCESS)
	{
		ret = AM_LocalGetResp(fd, buf, sizeof(buf));
	}
	
	close(fd);
	
	return ret;
#else
	return 0;
#endif
}

static AM_ErrorCode_t adec_open(AM_AOUT_Device_t *dev, const AM_AOUT_OpenPara_t *para)
{
	return AM_SUCCESS;
}

static AM_ErrorCode_t adec_set_volume(AM_AOUT_Device_t *dev, int vol)
{
#ifndef ADEC_API_NEW
	char buf[32];
	
	snprintf(buf, sizeof(buf), "volset:%d", vol);
	
	return adec_cmd(buf);
#else
	int ret=0;
#ifdef CHIP_8626X
	ret = audio_decode_set_volume(vol);
#else
	ret = audio_decode_set_volume(adec_handle,((float)vol)/100);
#endif
	if(ret==-1)
		return AM_FAILURE;
	else
		return AM_SUCCESS;
#endif	
}

static AM_ErrorCode_t adec_set_mute(AM_AOUT_Device_t *dev, AM_Bool_t mute)
{
#ifndef ADEC_API_NEW
	const char *cmd = mute?"mute":"unmute";
	
	return adec_cmd(cmd);
#else
	int ret=0;
	ret = audio_decode_set_mute(adec_handle, mute?1:0);

	if(ret==-1)
		return AM_FAILURE;
	else
		return AM_SUCCESS;
#endif	
}

static AM_ErrorCode_t adec_set_output_mode(AM_AOUT_Device_t *dev, AM_AOUT_OutputMode_t mode)
{
#ifndef ADEC_API_NEW
	
	
	switch(mode)
	{
		case AM_AOUT_OUTPUT_STEREO:
		default:
			cmd = "stereo";
		break;
		case AM_AOUT_OUTPUT_DUAL_LEFT:
			cmd = "leftmono";
		break;
		case AM_AOUT_OUTPUT_DUAL_RIGHT:
			cmd = "rightmono";
		break;
		case AM_AOUT_OUTPUT_SWAP:
			cmd = "swap";
		break;
	}
	
	return adec_cmd(cmd);
#else
	int ret=0;
	switch(mode)
	{
		case AM_AOUT_OUTPUT_STEREO:
		default:
			ret=audio_channel_stereo(adec_handle);
		break;
		case AM_AOUT_OUTPUT_DUAL_LEFT:
			ret=audio_channel_left_mono(adec_handle);
		break;
		case AM_AOUT_OUTPUT_DUAL_RIGHT:
			ret=audio_channel_right_mono(adec_handle);
		break;
		case AM_AOUT_OUTPUT_SWAP:
			ret=audio_channels_swap(adec_handle);
		break;
	}
	
	if(ret==-1)
		return AM_FAILURE;
	else
		return AM_SUCCESS;
#endif	
}

static AM_ErrorCode_t adec_close(AM_AOUT_Device_t *dev)
{
	return AM_SUCCESS;
}

/*音频控制（通过amplayer2）操作*/
static AM_ErrorCode_t amp_open(AM_AOUT_Device_t *dev, const AM_AOUT_OpenPara_t *para)
{
	return AM_SUCCESS;
}

static AM_ErrorCode_t amp_set_volume(AM_AOUT_Device_t *dev, int vol)
{
#ifdef  MEDIA_PLAYER
	int media_id = (int)dev->drv_data;

	if(MP_SetVolume(media_id, vol)==-1)
	{
		AM_DEBUG(1, "MP_SetVolume failed");
		return AM_AV_ERR_SYS;
	}
#endif
#ifdef PLAYER_API_NEW
	int media_id = (int)dev->drv_data;

	if(audio_set_volume(media_id, vol)==-1)
	{
		AM_DEBUG(1, "audio_set_volume failed");
		return AM_AV_ERR_SYS;
	}
#endif
	return AM_SUCCESS;
}

static AM_ErrorCode_t amp_set_mute(AM_AOUT_Device_t *dev, AM_Bool_t mute)
{
#ifdef  MEDIA_PLAYER
	int media_id = (int)dev->drv_data;

	if(MP_mute(media_id, mute)==-1)
	{
		AM_DEBUG(1, "MP_SetVolume failed");
		return AM_AV_ERR_SYS;
	}
#endif
#ifdef PLAYER_API_NEW
	int media_id = (int)dev->drv_data;

	printf("audio_set_mute %d\n", mute);
	if(audio_set_mute(media_id, mute?0:1)==-1)
	{
		AM_DEBUG(1, "audio_set_mute failed");
		return AM_AV_ERR_SYS;
	}
#endif
	return AM_SUCCESS;
}

static AM_ErrorCode_t amp_set_output_mode(AM_AOUT_Device_t *dev, AM_AOUT_OutputMode_t mode)
{
#ifdef  MEDIA_PLAYER
	int media_id = (int)dev->drv_data;

	MP_Tone tone;
	
	switch(mode)
	{
		case AM_AOUT_OUTPUT_STEREO:
		default:
			tone = MP_TONE_STEREO;
		break;
		case AM_AOUT_OUTPUT_DUAL_LEFT:
			tone = MP_TONE_LEFTMONO;
		break;
		case AM_AOUT_OUTPUT_DUAL_RIGHT:
			tone = MP_TONE_RIGHTMONO;
		break;
		case AM_AOUT_OUTPUT_SWAP:
			tone = MP_TONE_SWAP;
		break;
	}
	
	if(MP_SetTone(media_id, tone)==-1)
	{
		AM_DEBUG(1, "MP_SetTone failed");
		return AM_AV_ERR_SYS;
	}
#endif
#ifdef PLAYER_API_NEW
	int media_id = (int)dev->drv_data;
	int ret=0;
	
	switch(mode)
	{
		case AM_AOUT_OUTPUT_STEREO:
		default:
			ret = audio_stereo(media_id);
		break;
		case AM_AOUT_OUTPUT_DUAL_LEFT:
			ret = audio_left_mono(media_id);
		break;
		case AM_AOUT_OUTPUT_DUAL_RIGHT:
			ret = audio_right_mono(media_id);
		break;
		case AM_AOUT_OUTPUT_SWAP:
			ret = audio_swap_left_right(media_id);
		break;
	}
	if(ret==-1)
	{
		AM_DEBUG(1, "audio_set_mode failed");
		return AM_AV_ERR_SYS;
	}
#endif

	return AM_SUCCESS;
}

static AM_ErrorCode_t amp_close(AM_AOUT_Device_t *dev)
{
	return AM_SUCCESS;
}

/**\brief 音视频数据注入线程*/
static void* aml_data_source_thread(void *arg)
{
	AV_DataSource_t *src = (AV_DataSource_t*)arg;
	struct pollfd pfd;
	struct timespec ts;
	int cnt;
	
	while(src->running)
	{
		pthread_mutex_lock(&src->lock);
		
		if(src->pos>=0)
		{
			pfd.fd     = src->fd;
			pfd.events = POLLOUT;
			
			if(poll(&pfd, 1, 20)==1)
			{
				cnt = src->len-src->pos;
				cnt = write(src->fd, src->data+src->pos, cnt);
				if(cnt<=0)
				{
					AM_DEBUG(1, "write data failed");
				}
				else
				{
					src->pos += cnt;
				}
			}
		
			if(src->pos==src->len)
			{
				if(src->times>0)
				{
					src->times--;
					if(src->times)
					{
						src->pos = 0;
						AM_EVT_Signal(src->dev_no, src->is_audio?AM_AV_EVT_AUDIO_ES_END:AM_AV_EVT_VIDEO_ES_END, NULL);
					}
					else
					{
						src->pos = -1;
					}
				}
			}
			
			AM_TIME_GetTimeSpecTimeout(20, &ts);
			pthread_cond_timedwait(&src->cond, &src->lock, &ts);
		}
		else
		{
			pthread_cond_wait(&src->cond, &src->lock);
		}
		
		pthread_mutex_unlock(&src->lock);
		
	}
	
	return NULL;
}

/**\brief 创建一个音视频数据注入数据*/
static AV_DataSource_t* aml_create_data_source(const char *fname, int dev_no, AM_Bool_t is_audio)
{
	AV_DataSource_t *src;
	
	src = (AV_DataSource_t*)malloc(sizeof(AV_DataSource_t));
	if(!src)
	{
		AM_DEBUG(1, "not enough memory");
		return NULL;
	}
	
	memset(src, 0, sizeof(AV_DataSource_t));
	
	src->fd = open(fname, O_RDWR);
	if(src->fd==-1)
	{
		AM_DEBUG(1, "cannot open file \"%s\"", fname);
		free(src);
		return NULL;
	}

	src->dev_no   = dev_no;
	src->is_audio = is_audio;
	
	pthread_mutex_init(&src->lock, NULL);
	pthread_cond_init(&src->cond, NULL);
	
	return src;
}

/**\brief 运行数据注入线程*/
static AM_ErrorCode_t aml_start_data_source(AV_DataSource_t *src, const uint8_t *data, int len, int times)
{
	int ret;
	
	if(!src->running)
	{
		src->running = AM_TRUE;
		src->data    = data;
		src->len     = len;
		src->times   = times;
		src->pos     = 0;
		
		ret = pthread_create(&src->thread, NULL, aml_data_source_thread, src);
		if(ret)
		{
			AM_DEBUG(1, "create the thread failed");
			src->running = AM_FALSE;
			return AM_AV_ERR_SYS;
		}
	}
	else
	{
		pthread_mutex_lock(&src->lock);
		
		src->data  = data;
		src->len   = len;
		src->times = times;
		src->pos   = 0;
		
		pthread_mutex_unlock(&src->lock);
		pthread_cond_signal(&src->cond);
	}
	
	return AM_SUCCESS;
}

/**\brief 释放数据注入数据*/
static void aml_destroy_data_source(AV_DataSource_t *src)
{
	if(src->running)
	{
		src->running = AM_FALSE;
		pthread_cond_signal(&src->cond);
		pthread_join(src->thread, NULL);
	}
	
	close(src->fd);
	pthread_mutex_destroy(&src->lock);
	pthread_cond_destroy(&src->cond);
	free(src);
}

#ifdef  MEDIA_PLAYER

/**\brief 播放器消息回调*/
static int aml_fp_msg_cb(void *obj,MP_ResponseMsg *msgt)
{
	AV_FilePlayerData_t *fp = (AV_FilePlayerData_t*)obj;
	
	if(msgt !=NULL)
	{
		AM_DEBUG(1, "MPlayer event %d", msgt->head.type);
		switch(msgt->head.type)
		{
			case MP_RESP_STATE_CHANGED:
				AM_DEBUG(1, "MPlayer state changed to %d", ((MP_StateChangedRespBody*)msgt->auxDat)->state);
              			pthread_mutex_lock(&fp->lock);
              			
				fp->state = ((MP_StateChangedRespBody*)msgt->auxDat)->state;
				AM_EVT_Signal(fp->av->dev_no, AM_AV_EVT_PLAYER_STATE_CHANGED, (void*)((MP_StateChangedRespBody*)msgt->auxDat)->state);
				
				pthread_mutex_unlock(&fp->lock);
				pthread_cond_broadcast(&fp->cond);
			break;
			case MP_RESP_DURATION:
				pthread_mutex_lock(&fp->lock);
				if(fp->resp_type==AV_MP_RESP_DURATION)
				{
					fp->resp_data.duration = *(int*)msgt->auxDat;
					fp->resp_type = AV_MP_RESP_OK;
					pthread_cond_broadcast(&fp->cond);
				}
				pthread_mutex_unlock(&fp->lock);
			break;
			case MP_RESP_TIME_CHANGED:	
				pthread_mutex_lock(&fp->lock);
				if(fp->resp_type==AV_MP_RESP_POSITION)
				{
					fp->resp_data.position = *(int*)msgt->auxDat;
					fp->resp_type = AV_MP_RESP_OK;
					pthread_cond_broadcast(&fp->cond);
				}
				AM_EVT_Signal(fp->av->dev_no, AM_AV_EVT_PLAYER_TIME_CHANGED, (void*)*(int*)msgt->auxDat);
				pthread_mutex_unlock(&fp->lock);
			break;
			case MP_RESP_MEDIAINFO:
				pthread_mutex_lock(&fp->lock);
				if(fp->resp_type==AV_MP_RESP_MEDIA)
				{
					MP_AVMediaFileInfo *info = (MP_AVMediaFileInfo*)msgt->auxDat;
					fp->resp_data.media = *info;
					fp->resp_type = AV_MP_RESP_OK;
					pthread_cond_broadcast(&fp->cond);
				}
				pthread_mutex_unlock(&fp->lock);
			break;
			default:
			break;
		}
		
		MP_free_response_msg(msgt);
	}
	
	return 0;
}

/**\brief 释放文件播放数据*/
static void aml_destroy_fp(AV_FilePlayerData_t *fp)
{
	int rc;
	
	/*等待播放器停止*/
	if(fp->media_id!=-1)
	{
		pthread_mutex_lock(&fp->lock);
		
		fp->resp_type= AV_MP_RESP_STOP;
		rc = MP_stop(fp->media_id);
		
		if(rc==0)
		{
			while((fp->state!=MP_STATE_STOPED) && (fp->state!=MP_STATE_NORMALERROR) &&
					(fp->state!=MP_STATE_FATALERROR) && (fp->state!=MP_STATE_FINISHED))
				pthread_cond_wait(&fp->cond, &fp->lock);
		}
		
		pthread_mutex_unlock(&fp->lock);
		
		MP_CloseMediaID(fp->media_id);
	}
	
	pthread_mutex_destroy(&fp->lock);
	pthread_cond_destroy(&fp->cond);
	
	MP_ReleaseMPClient();
	
	free(fp);
}

/**\brief 创建文件播放器数据*/
static AV_FilePlayerData_t* aml_create_fp(AM_AV_Device_t *dev)
{
	AV_FilePlayerData_t *fp;
	
	fp = (AV_FilePlayerData_t*)malloc(sizeof(AV_FilePlayerData_t));
	if(!fp)
	{
		AM_DEBUG(1, "not enough memory");
		return NULL;
	}
	
	if(MP_CreateMPClient()==-1)
	{
		AM_DEBUG(1, "MP_CreateMPClient failed");
		free(fp);
		return NULL;
	}
	
	memset(fp, 0, sizeof(AV_FilePlayerData_t));
	pthread_mutex_init(&fp->lock, NULL);
	pthread_cond_init(&fp->cond, NULL);
	
	fp->av       = dev;
	fp->media_id = -1;
	
	MP_RegPlayerRespMsgListener(fp, aml_fp_msg_cb);
	
	return fp;
}
#elif defined(PLAYER_API_NEW)
/**\brief 释放文件播放数据*/
static void aml_destroy_fp(AV_FilePlayerData_t *fp)
{
	/*等待播放器停止*/
	if(fp->media_id >= 0)
	{
		player_exit(fp->media_id);
	}
	
	pthread_mutex_destroy(&fp->lock);
	pthread_cond_destroy(&fp->cond);
	
	free(fp);
}

/**\brief 创建文件播放器数据*/
static AV_FilePlayerData_t* aml_create_fp(AM_AV_Device_t *dev)
{
	AV_FilePlayerData_t *fp;
	
	fp = (AV_FilePlayerData_t*)malloc(sizeof(AV_FilePlayerData_t));
	if(!fp)
	{
		AM_DEBUG(1, "not enough memory");
		return NULL;
	}
	
	if(player_init() < 0)
	{
		AM_DEBUG(1, "player_init failed");
		free(fp);
		return NULL;
	}
	
	memset(fp, 0, sizeof(AV_FilePlayerData_t));
	pthread_mutex_init(&fp->lock, NULL);
	pthread_cond_init(&fp->cond, NULL);
	
	fp->av       = dev;
	fp->media_id = -1;
	
	return fp;
}

static int aml_update_player_info_callback(int pid,player_info_t * info)
{
	if (info)
	{
		AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)info);
	}

	return 0;
}

int aml_set_tsync_enable(int enable)
{
	int fd;
	char *path = "/sys/class/tsync/enable"; 
	char  bcmd[16];
	
	fd=open(path, O_CREAT|O_RDWR | O_TRUNC, 0644);
	if(fd>=0)
	{
		sprintf(bcmd,"%d",enable);
		write(fd,bcmd,strlen(bcmd));
		close(fd);
		return 0;
	}
	
	return -1;
}


#endif
/**\brief 初始化JPEG解码器*/
static AM_ErrorCode_t aml_init_jpeg(AV_JPEGData_t *jpeg)
{
	if(jpeg->dec_fd!=-1)
	{
		close(jpeg->dec_fd);
		jpeg->dec_fd = -1;
	}
	
	if(jpeg->vbuf_fd!=-1)
	{
		close(jpeg->vbuf_fd);
		jpeg->vbuf_fd = -1;
	}
	
	jpeg->vbuf_fd = open(STREAM_VBUF_FILE, O_RDWR|O_NONBLOCK);
	if(jpeg->vbuf_fd==-1)
	{
		AM_DEBUG(1, "cannot open amstream_vbuf");
		goto error;
	}
	
	if(ioctl(jpeg->vbuf_fd, AMSTREAM_IOC_VFORMAT, VFORMAT_JPEG)==-1)
	{
		AM_DEBUG(1, "set jpeg video format failed (\"%s\")", strerror(errno));
		goto error;
	}
	
	if(ioctl(jpeg->vbuf_fd, AMSTREAM_IOC_PORT_INIT)==-1)
	{
		AM_DEBUG(1, "amstream init failed (\"%s\")", strerror(errno));
		goto error;
	}
	
	return AM_SUCCESS;
error:
	if(jpeg->dec_fd!=-1)
	{
		close(jpeg->dec_fd);
		jpeg->dec_fd = -1;
	}
	if(jpeg->vbuf_fd!=-1)
	{
		close(jpeg->vbuf_fd);
		jpeg->vbuf_fd = -1;
	}
	return AM_AV_ERR_SYS;
}

/**\brief 创建JPEG解码相关数据*/
static AV_JPEGData_t* aml_create_jpeg_data(void)
{
	AV_JPEGData_t *jpeg;
	
	jpeg = malloc(sizeof(AV_JPEGData_t));
	if(!jpeg)
	{
		AM_DEBUG(1, "not enough memory");
		return NULL;
	}
	
	jpeg->vbuf_fd = -1;
	jpeg->dec_fd  = -1;
	
	if(aml_init_jpeg(jpeg)!=AM_SUCCESS)
	{
		free(jpeg);
		return NULL;
	}
	
	return jpeg;
}

/**\brief 释放JPEG解码相关数据*/
static void aml_destroy_jpeg_data(AV_JPEGData_t *jpeg)
{
	if(jpeg->dec_fd!=-1)
		close(jpeg->dec_fd);
	if(jpeg->vbuf_fd!=-1)
		close(jpeg->vbuf_fd);
	
	free(jpeg);
}

/**\brief 创建数据注入相关数据*/
static AV_InjectData_t* aml_create_inject_data(void)
{
	AV_InjectData_t *inj;
	
	inj = (AV_InjectData_t*)malloc(sizeof(AV_InjectData_t));
	if(!inj)
		return NULL;
	
	inj->aud_fd = -1;
	inj->vid_fd = -1;
	
	return inj;
}

/**\brief 设置数据注入参赛*/
static AM_ErrorCode_t aml_start_inject(AV_InjectData_t *inj, AM_AV_InjectPara_t *para)
{
	int vfd=-1, afd=-1;
	
	inj->pkg_fmt = para->pkg_fmt;
	
	switch(para->pkg_fmt)
	{
		case PFORMAT_ES:
			if(para->vid_id>=0)
			{
				vfd = open(STREAM_VBUF_FILE, O_RDWR);
				if(vfd==-1)
				{
					AM_DEBUG(1, "cannot open device \"%s\"", STREAM_VBUF_FILE);
					return AM_AV_ERR_CANNOT_OPEN_FILE;
				}
				inj->vid_fd = vfd;
			}
			if(para->aud_id>=0)
			{
				afd = open(STREAM_ABUF_FILE, O_RDWR);
				if(afd==-1)
				{
					AM_DEBUG(1, "cannot open device \"%s\"", STREAM_ABUF_FILE);
					return AM_AV_ERR_CANNOT_OPEN_FILE;
				}
				inj->aud_fd = afd;
			}
		break;
		case PFORMAT_PS:
			vfd = open(STREAM_PS_FILE, O_RDWR);
			if(vfd==-1)
			{
				AM_DEBUG(1, "cannot open device \"%s\"", STREAM_PS_FILE);
				return AM_AV_ERR_CANNOT_OPEN_FILE;
			}
			inj->vid_fd = afd = vfd;
		break;
		case PFORMAT_TS:
			vfd = open(STREAM_TS_FILE, O_RDWR);
			if(vfd==-1)
			{
				AM_DEBUG(1, "cannot open device \"%s\"", STREAM_TS_FILE);
				return AM_AV_ERR_CANNOT_OPEN_FILE;
			}
			inj->vid_fd = afd = vfd;
		break;
		case PFORMAT_REAL:
			vfd = open(STREAM_RM_FILE, O_RDWR);
			if(vfd==-1)
			{
				AM_DEBUG(1, "cannot open device \"%s\"", STREAM_RM_FILE);
				return AM_AV_ERR_CANNOT_OPEN_FILE;
			}
			inj->vid_fd = afd = vfd;
		break;
		default:
			AM_DEBUG(1, "unknown package format %d", para->pkg_fmt);
		return AM_AV_ERR_NOT_SUPPORTED;
	}
	
	if(para->vid_id>=0)
	{
		if(ioctl(vfd, AMSTREAM_IOC_VFORMAT, para->vid_fmt)==-1)
		{
			AM_DEBUG(1, "set video format failed");
			return AM_AV_ERR_SYS;
		}
		if(ioctl(vfd, AMSTREAM_IOC_VID, para->vid_id)==-1)
		{
			AM_DEBUG(1, "set video PID failed");
			return AM_AV_ERR_SYS;
		}
	}
	
	if(para->aud_id>=0)
	{
		if(ioctl(afd, AMSTREAM_IOC_AFORMAT, para->aud_fmt)==-1)
		{
			AM_DEBUG(1, "set audio format failed");
			return AM_AV_ERR_SYS;
		}
		if(ioctl(afd, AMSTREAM_IOC_AID, para->aud_id)==-1)
		{
			AM_DEBUG(1, "set audio PID failed");
			return AM_AV_ERR_SYS;
		}
		if((para->aud_fmt==AFORMAT_PCM_S16LE)||(para->aud_fmt==AFORMAT_PCM_S16BE)||
				(para->aud_fmt==AFORMAT_PCM_U8)) {
			ioctl(afd, AMSTREAM_IOC_ACHANNEL, para->channel);
			ioctl(afd, AMSTREAM_IOC_SAMPLERATE, para->sample_rate);
			ioctl(afd, AMSTREAM_IOC_DATAWIDTH, para->data_width);
		}
	}
	
	if(vfd!=-1)
	{
		if(ioctl(vfd, AMSTREAM_IOC_PORT_INIT, 0)==-1)
		{
			AM_DEBUG(1, "amport init failed");
			return AM_AV_ERR_SYS;
		}
	}
	
	if((afd!=-1) && (afd!=vfd))
	{
		if(ioctl(afd, AMSTREAM_IOC_PORT_INIT, 0)==-1)
		{
			AM_DEBUG(1, "amport init failed");
			return AM_AV_ERR_SYS;
		}
	}
	
	if(para->aud_id>=0)
	{
#if !defined(ADEC_API_NEW)
		adec_cmd("start");
#else
		{
			codec_para_t cp;
			memset(&cp, 0, sizeof(cp));
			audio_decode_init(&adec_handle, &cp);
		}
//#ifdef CHIP_8626X
		audio_set_av_sync_threshold(adec_handle, AV_SYNC_THRESHOLD);
//#endif
		
		audio_decode_start(adec_handle);
#endif
		AM_AOUT_SetDriver(AOUT_DEV_NO, &adec_aout_drv, NULL);
	}
	
	return AM_SUCCESS;
}

/**\brief 释放数据注入相关数据*/
static void aml_destroy_inject_data(AV_InjectData_t *inj)
{
	if(inj->aud_fd!=-1)
	{
#if !defined(ADEC_API_NEW)
		adec_cmd("stop");
#else
		audio_decode_stop(adec_handle);
		audio_decode_release(&adec_handle);
		adec_handle = NULL;
#endif		
		close(inj->aud_fd);
	}
	if(inj->vid_fd!=-1)
		close(inj->vid_fd);
	free(inj);
}

static int aml_timeshift_data_write(int fd, uint8_t *buf, int size)
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
				AM_DEBUG(0, "Timeshift inject data failed: %s", strerror(errno));
				break;
			}
			ret = 0;
		}
		
		left -= ret;
		p += ret;
	}

	return (size - left);
}

static AV_TimeshiftSubfile_t *aml_timeshift_new_subfile(AV_TimeshiftFile_t *tfile)
{
	int flags;
	struct stat st;
	char fname[1024];
	AM_Bool_t is_timeshift = tfile->is_timeshift;
	AV_TimeshiftSubfile_t *sub_file, *ret = NULL;

	if (is_timeshift)
	{
		/* create new timeshifting sub files */
		flags = O_WRONLY|O_CREAT|O_TRUNC;
	}
	else
	{
		/* gathering all pvr playback sub files */
		flags = O_WRONLY;
	}

	sub_file = (AV_TimeshiftSubfile_t*)malloc(sizeof(AV_TimeshiftSubfile_t));
	if (sub_file == NULL)
	{
		AM_DEBUG(1, "Cannot write sub file , no memory!\n");
		return ret;
	}

	sub_file->next = NULL;
	

	if (tfile->last_sub_index == 0 && !is_timeshift)
	{
		/* To be compatible with the old version */
		sub_file->findex = tfile->last_sub_index++;
		snprintf(fname, sizeof(fname), "%s", tfile->name);
	}
	else
	{
		sub_file->findex = tfile->last_sub_index++;
		snprintf(fname, sizeof(fname), "%s.%d", tfile->name, sub_file->findex);
	}

	if (!is_timeshift && stat(fname, &st) < 0)
	{
		AM_DEBUG(1, "PVR sub file '%s': %s", fname, strerror(errno));
		sub_file->wfd = -1;
		sub_file->rfd = -1;
	}
	else
	{
		AM_DEBUG(1, "openning %s\n", fname);
		sub_file->wfd = open(fname, flags, 0666);
		sub_file->rfd = open(fname, O_RDONLY, 0666);
	}
	
	if (sub_file->wfd < 0 || sub_file->rfd < 0)
	{
		AM_DEBUG(1, "Open file failed: %s", strerror(errno));
		if (is_timeshift)
		{
			AM_DEBUG(1, "Cannot open new sub file: %s\n", strerror(errno));
		}
		else
		{
			/* To be compatible with the old version */
			if (sub_file->findex == 0)
			{
				ret = aml_timeshift_new_subfile(tfile);
			}
			else
			{
				AM_DEBUG(1, "PVR playback sub files end at index %d", sub_file->findex-1);
			}
		}

		if (sub_file->wfd >= 0)
			close(sub_file->wfd);
		if (sub_file->rfd >= 0)
			close(sub_file->rfd);

		free(sub_file);
	}
	else
	{
		ret = sub_file;
	}

	return ret;
}

static ssize_t aml_timeshift_subfile_read(AV_TimeshiftFile_t *tfile, uint8_t *buf, size_t size)
{
	ssize_t ret = 0;

	if (tfile->sub_files == NULL)
	{
		AM_DEBUG(1, "No sub files, cannot read\n");
		return -1;
	}
	
	if (tfile->cur_rsub_file == NULL)
	{
		tfile->cur_rsub_file = tfile->sub_files;
		AM_DEBUG(1, "Reading from the start, file index %d", tfile->cur_rsub_file->findex);
	}
	
	ret = read(tfile->cur_rsub_file->rfd, buf, size);
	if (ret == 0)
	{
		/* reach the end, automatically turn to next sub file */
		if (tfile->cur_rsub_file->next != NULL)
		{
			tfile->cur_rsub_file = tfile->cur_rsub_file->next;
			AM_DEBUG(1, "Reading from file index %d ...", tfile->cur_rsub_file->findex);
			lseek64(tfile->cur_rsub_file->rfd, 0, SEEK_SET);
			ret = aml_timeshift_subfile_read(tfile, buf, size);
		}
	}

	return ret;
}

static ssize_t aml_timeshift_subfile_write(AV_TimeshiftFile_t *tfile, uint8_t *buf, size_t size)
{
	ssize_t ret = 0;
	loff_t fsize;

	if (tfile->sub_files == NULL)
	{
		AM_DEBUG(1, "No sub files, cannot write\n");
		return -1;
	}
	
	if (tfile->cur_wsub_file == NULL)
	{
		tfile->cur_wsub_file = tfile->sub_files;
		AM_DEBUG(1, "Switching to file index %d for writing...\n", 
			tfile->cur_wsub_file->findex);
	}

	fsize = lseek64(tfile->cur_wsub_file->wfd, 0, SEEK_CUR);
	if (fsize >= tfile->sub_file_size)
	{
		AM_Bool_t start_new = AM_FALSE;
		
		if (tfile->cur_wsub_file->next != NULL)
		{
			tfile->cur_wsub_file = tfile->cur_wsub_file->next;
			start_new = AM_TRUE;
		}
		else
		{
			AV_TimeshiftSubfile_t *sub_file = aml_timeshift_new_subfile(tfile);
			if (sub_file != NULL)
			{
				tfile->cur_wsub_file->next = sub_file;
				tfile->cur_wsub_file = sub_file;
				start_new = AM_TRUE;
			}
		}

		if (start_new)
		{
			AM_DEBUG(1, "Switching to file index %d for writing...\n", 
				tfile->cur_wsub_file->findex);
			lseek64(tfile->cur_wsub_file->wfd, 0, SEEK_SET);
			ret = aml_timeshift_subfile_write(tfile, buf, size);
		}
	}
	else
	{
		ret = aml_timeshift_data_write(tfile->cur_wsub_file->wfd, buf, size);
	}

	return ret;
}

static int aml_timeshift_subfile_close(AV_TimeshiftFile_t *tfile)
{
	AV_TimeshiftSubfile_t *sub_file, *next;
	char fname[1024];

	sub_file = tfile->sub_files;
	while (sub_file != NULL)
	{
		next = sub_file->next;
		if (sub_file->rfd >= 0)
		{
			close(sub_file->rfd);
		}
		if (sub_file->wfd >= 0)
		{
			close(sub_file->wfd);
		}

		if (tfile->is_timeshift)
		{
			snprintf(fname, sizeof(fname), "%s.%d", tfile->name, sub_file->findex);
			AM_DEBUG(1, "unlinking file: %s", fname);
			unlink(fname);
		}
		
		free(sub_file);
		sub_file = next;
	}

	return 0;
}

static int aml_timeshift_subfile_open(AV_TimeshiftFile_t *tfile)
{
	int index = 1, flags;
	char fname[1024];
	AV_TimeshiftSubfile_t *sub_file, *prev_sub_file, *exist_sub_file;
	AM_Bool_t is_timeshift = tfile->is_timeshift;
	loff_t left = is_timeshift ? tfile->size : 1/*just keep the while going*/;


	if (is_timeshift)
	{
		tfile->sub_files = aml_timeshift_new_subfile(tfile);
	}
	else
	{
		prev_sub_file = NULL;
		do
		{
			sub_file = aml_timeshift_new_subfile(tfile);
			if (sub_file != NULL)
			{
				if (prev_sub_file == NULL)
					tfile->sub_files = sub_file;
				else
					prev_sub_file->next = sub_file;
				prev_sub_file = sub_file;

				tfile->size += lseek64(sub_file->rfd, 0, SEEK_END);
				lseek64(sub_file->rfd, 0, SEEK_SET);
			}
		}while(sub_file != NULL);
	}

	return 0;
}

static loff_t aml_timeshift_subfile_seek(AV_TimeshiftFile_t *tfile, loff_t offset, AM_Bool_t read)
{
	int sub_index = offset/tfile->sub_file_size; /* start from 0 */
	loff_t sub_offset = offset%tfile->sub_file_size;
	AV_TimeshiftSubfile_t *sub_file = tfile->sub_files;

	while (sub_file != NULL)
	{
		if (sub_file->findex == sub_index)
			break;
		sub_file = sub_file->next;
	}

	if (sub_file != NULL)
	{
		AM_DEBUG(1, "Seek to sub file %d at %lld\n", sub_index, sub_offset);
		if (read)
		{
			tfile->cur_rsub_file = sub_file;
			lseek64(sub_file->rfd, sub_offset, SEEK_SET);
		}
		else
		{
			tfile->cur_wsub_file = sub_file;
			lseek64(sub_file->wfd, sub_offset, SEEK_SET);
		}
		return offset;
	}

	return (loff_t)-1;
}


static AM_ErrorCode_t aml_timeshift_file_open(AV_TimeshiftFile_t *tfile, const char *file_name)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	const loff_t SUB_FILE_SIZE = 1024*1024*1024ll;
	
	if (tfile->opened)
	{
		AM_DEBUG(1, "Timeshift file has already opened");
		return ret;
	}
		
	tfile->name = strdup(file_name);
	tfile->sub_file_size = SUB_FILE_SIZE;
	tfile->is_timeshift = (strstr(tfile->name, "TimeShifting") != NULL);

	if (tfile->loop)
	{
		tfile->size = SUB_FILE_SIZE;
		
		if (aml_timeshift_subfile_open(tfile) < 0)
			return AM_AV_ERR_CANNOT_OPEN_FILE;
		
		tfile->avail = 0;
		tfile->total = 0;
	}
	else
	{
		if (aml_timeshift_subfile_open(tfile) < 0)
			return AM_AV_ERR_CANNOT_OPEN_FILE;
		
		tfile->avail = tfile->size;
		tfile->total = tfile->size;
		AM_DEBUG(1, "Playback total subfiles size %lld", tfile->size);
	}
	
	tfile->start = 0;
	tfile->read = tfile->write = 0;
	
	pthread_mutex_init(&tfile->lock, NULL);
	pthread_cond_init(&tfile->cond, NULL);

	tfile->opened = 1;

	return ret;
}

static AM_ErrorCode_t aml_timeshift_file_close(AV_TimeshiftFile_t *tfile)
{
	if (! tfile->opened)
	{
		AM_DEBUG(1, "Timeshift file has not opened");
		return AM_AV_ERR_INVAL_ARG;
	}
	tfile->opened = 0;
	pthread_mutex_lock(&tfile->lock);
	aml_timeshift_subfile_close(tfile);
	
	/*remove timeshift file*/
	if (tfile->name != NULL)
	{
		free(tfile->name);
		tfile->name = NULL;
	}
	pthread_mutex_unlock(&tfile->lock);
	pthread_mutex_destroy(&tfile->lock);

	return AM_SUCCESS;
}

static ssize_t aml_timeshift_file_read(AV_TimeshiftFile_t *tfile, uint8_t *buf, size_t size, int timeout)
{
	ssize_t ret = -1;
	size_t todo, split;
	struct timespec rt;
	
	if (! tfile->opened)
	{
		AM_DEBUG(1, "Timeshift file has not opened");
		return AM_AV_ERR_INVAL_ARG;
	}

	if (! tfile->loop)
	{
		ret = aml_timeshift_subfile_read(tfile, buf, size);
		if (ret > 0)
		{
			tfile->avail -= ret;
			if (tfile->avail < 0)
				tfile->avail = 0;
		}
		return ret;
	}

	pthread_mutex_lock(&tfile->lock);
	if (tfile->avail <= 0)
	{
		AM_TIME_GetTimeSpecTimeout(timeout, &rt);
		pthread_cond_timedwait(&tfile->cond, &tfile->lock, &rt);
	}
	if (tfile->avail <= 0)
	{
		tfile->avail = 0;
		goto read_done;
	}
	todo = (size < tfile->avail) ? size : tfile->avail;
	size = todo;
	split = ((tfile->read+size) > tfile->size) ? (tfile->size - tfile->read) : 0;
	if (split > 0)
	{
		/*read -> end*/
		ret = aml_timeshift_subfile_read(tfile, buf, split);
		if (ret < 0)
			goto read_done;
		if (ret != (ssize_t)split)
		{
			tfile->read += ret;
			goto read_done;
		}

		tfile->read = 0;
		todo -= ret;
		buf += ret;
	}
	/*rewind the file*/
	if (split > 0)
		aml_timeshift_subfile_seek(tfile, 0, AM_TRUE);
	ret = aml_timeshift_subfile_read(tfile, buf, todo);
	if (ret > 0)
	{
		todo -= ret;
		tfile->read += ret;
		tfile->read %= tfile->size;
		ret = size - todo;
	}
	
read_done:
	if (ret > 0)
	{
		tfile->avail -= ret;
		if (tfile->avail < 0)
			tfile->avail = 0;
	}
	pthread_mutex_unlock(&tfile->lock);
	return ret;
}

static ssize_t aml_timeshift_file_write(AV_TimeshiftFile_t *tfile, uint8_t *buf, size_t size)
{
	ssize_t ret = -1;
	size_t  split;
	loff_t fsize, wpos;
	
	if (! tfile->opened)
	{
		AM_DEBUG(1, "Timeshift file has not opened");
		return AM_AV_ERR_INVAL_ARG;
	}
	
	if (! tfile->loop)
	{
		/* Normal write */
		ret = aml_timeshift_subfile_write(tfile, buf, size);
		if (ret > 0)
		{
			pthread_mutex_lock(&tfile->lock);
			tfile->size += ret;
			tfile->write += ret;
			tfile->total += ret;
			tfile->avail += ret;
			pthread_mutex_unlock(&tfile->lock);
		}
		goto write_done;
	}
	
	pthread_mutex_lock(&tfile->lock);
	fsize = tfile->size;
	wpos = tfile->write;
	pthread_mutex_unlock(&tfile->lock);
	/*is the size exceed the file size?*/
	if (size > fsize)
	{
		size = fsize;
		buf += fsize - size;
	}
	
	split = ((wpos+size) > fsize) ? (fsize - wpos) : 0;
	if (split > 0)
	{
		/*write -> end*/
		ret = aml_timeshift_subfile_write(tfile, buf, split);
		if (ret < 0)
			goto write_done;
		if (ret != (ssize_t)split)
			goto adjust_pos;

		size -= ret;
		buf += ret;
	}
	/*rewind the file*/
	if (split > 0)
		aml_timeshift_subfile_seek(tfile, 0, AM_FALSE);
	ret = aml_timeshift_subfile_write(tfile, buf, size);
	if (ret > 0)
		ret += split;
	else if (ret == 0)
		ret = split;
	
adjust_pos:
	if (ret > 0)
	{
		pthread_mutex_lock(&tfile->lock);
		/*now, ret bytes actually writen*/
		off_t rleft = tfile->size - tfile->avail;
		off_t sleft = tfile->size - tfile->total;

		tfile->write = (tfile->write + ret) % tfile->size;
		if (ret > rleft)
			tfile->read = tfile->write;
		if (ret > sleft)
			tfile->start = tfile->write;

		if (tfile->avail < tfile->size)
		{
			tfile->avail += ret;
			if (tfile->avail > tfile->size)
				tfile->avail = tfile->size;
			pthread_cond_signal(&tfile->cond);
		}
		if (tfile->total < tfile->size)
		{
			tfile->total += ret;
			if (tfile->total > tfile->size)
				tfile->total = tfile->size;
		}
		pthread_mutex_unlock(&tfile->lock);
	}
write_done:
	
	return ret;
}

/**\brief seek到指定的偏移，如越界则返回1*/
static int aml_timeshift_file_seek(AV_TimeshiftFile_t *tfile, loff_t offset)
{
	int ret = 1;

	pthread_mutex_lock(&tfile->lock);
	if (tfile->size <= 0)
	{
		pthread_mutex_unlock(&tfile->lock);
		return ret;
	}	
	if (offset > tfile->total)
		offset = tfile->total - 1;
	else if (offset < 0)
		offset = 0;
	else
		ret = 0;

	tfile->read = (tfile->start + offset)%tfile->size;
	tfile->avail = tfile->total - offset;
	if (tfile->avail > 0)
		pthread_cond_signal(&tfile->cond);
	
	aml_timeshift_subfile_seek(tfile, tfile->read, AM_TRUE);

	AM_DEBUG(0, "Timeshift file Seek: start %lld, read %lld, write %lld, avail %lld, total %lld", 
				tfile->start, tfile->read, tfile->write, tfile->avail, tfile->total);
	pthread_mutex_unlock(&tfile->lock);
	return ret;
}
/**\brief 创建Timeshift相关数据*/
static AV_TimeshiftData_t* aml_create_timeshift_data(void)
{
	AV_TimeshiftData_t *tshift;
	
	tshift = (AV_TimeshiftData_t*)malloc(sizeof(AV_TimeshiftData_t));
	if(!tshift)
		return NULL;
	
	memset(tshift, 0, sizeof(AV_TimeshiftData_t));
	tshift->av_fd = -1;
	
	return tshift;
}

static int aml_timeshift_inject(AV_TimeshiftData_t *tshift, uint8_t *data, int size, int timeout)
{
	int ret;
	int real_written = 0;
	int fd = tshift->av_fd;

	if(timeout>=0)
	{
		struct pollfd pfd;
		
		pfd.fd = fd;
		pfd.events = POLLOUT;
		
		ret = poll(&pfd, 1, timeout);
		if(ret!=1)
		{
			AM_DEBUG(1, "timeshift poll timeout");
			goto inject_end;
		}
	}
	
	if(size)
	{
		ret = write(fd, data, size);
		if((ret==-1) && (errno!=EAGAIN))
		{
			AM_DEBUG(1, "inject data failed errno:%d msg:%s", errno, strerror(errno));
			goto inject_end;
		}
		else if((ret==-1) && (errno==EAGAIN))
		{
			real_written = 0;
		}
		else if (ret >= 0)
		{
			real_written = ret;
		}
	}

inject_end:
	return real_written;
}

static unsigned int aml_get_pts_video()
{
    int handle;
    int size;
    char s[16];
    unsigned int value = 0;

    handle = open("/sys/class/tsync/pts_video", O_RDONLY);
    if (handle < 0) {
        return -1;
    }
    size = read(handle, s, sizeof(s));
    if (size > 0) {
        value = strtoul(s, NULL, 16);
    }
    close(handle);

    return value;
}

static unsigned int aml_get_pts_audio()
{
    int handle;
    int size;
    char s[16];
    unsigned int value;

    handle = open("/sys/class/tsync/pts_audio", O_RDONLY);
    if (handle < 0) {
        return -1;
    }
    size = read(handle, s, sizeof(s));
    if (size > 0) {
        value = strtoul(s, NULL, 16);
    }
    close(handle);

    return value;
}

static int aml_set_black_policy(int blackout)
{
	int fd;
	char  bcmd[16];

	fd = open("/sys/class/video/blackout_policy", O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (fd >= 0) 
	{
		sprintf(bcmd, "%d", blackout);
		write(fd, bcmd, strlen(bcmd));
		close(fd);
		return 0;
	}

	return -1;
}

static int aml_get_black_policy()
{
    int fd;
    int val = 0;
    char  bcmd[16];

    fd = open("/sys/class/video/blackout_policy", O_RDONLY);
    if (fd >= 0) 
	{
        read(fd, bcmd, sizeof(bcmd));
        val = strtol(bcmd, NULL, 16);
        close(fd);
    }

    return (val & 0x1);
}

static int aml_timeshift_get_trick_stat(AV_TimeshiftData_t *tshift)
{
	int state;

	if (tshift->cntl_fd == -1)
		return -1;

	ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICK_STAT, (unsigned long)&state);

	return state;
}

/**\brief 设置Timeshift参数*/
static AM_ErrorCode_t aml_start_timeshift(AV_TimeshiftData_t *tshift, AM_AV_TimeshiftPara_t *para, AM_Bool_t create_thread, AM_Bool_t start_audio)
{
	char buf[64];
	
	AM_DEBUG(1, "Openning demux%d",para->dmx_id);
	snprintf(buf, sizeof(buf), "/dev/dvb0.demux%d", para->dmx_id);
	tshift->dmxfd = open(buf, O_RDWR);
	if(tshift->dmxfd==-1)
	{
		AM_DEBUG(1, "cannot open \"/dev/dvb0.demux%d\" error:%d \"%s\"", para->dmx_id, errno, strerror(errno));
		return AM_AV_ERR_CANNOT_OPEN_DEV;
	}

	AM_FileRead("/sys/class/stb/source", tshift->last_stb_src, 16);
	AM_FileEcho("/sys/class/stb/source", "hiu");
	snprintf(buf, sizeof(buf), "/sys/class/stb/demux%d_source", para->dmx_id);
	AM_FileEcho(buf, "hiu");

	AM_DEBUG(1, "Openning mpts");
	tshift->av_fd = open(STREAM_TS_FILE, O_RDWR);
	if(tshift->av_fd==-1)
	{
		AM_DEBUG(1, "cannot open device \"%s\"", STREAM_TS_FILE);
		return AM_AV_ERR_CANNOT_OPEN_FILE;
	}
	AM_DEBUG(1, "Openning video");
	tshift->cntl_fd = open(AMVIDEO_FILE, O_RDWR);
	if(tshift->cntl_fd ==-1)
	{
		AM_DEBUG(1, "cannot create data source \"/dev/amvideo\"");
		return AM_AV_ERR_CANNOT_OPEN_DEV;
	}
	AM_DEBUG(1, "Setting play param");
#if defined(ANDROID) || defined(CHIP_8626X)
	/*Set tsync enable/disable*/
	if (para->media_info.vid_pid >= 0x1fff || para->media_info.aud_cnt <= 0)
	{
		AM_DEBUG(1, "Set tsync enable to 0");
		aml_set_tsync_enable(0);
	}
	else
	{
		AM_DEBUG(1, "Set tsync enable to 1");
		aml_set_tsync_enable(1);
	}
#endif

	if(para->media_info.vid_pid < 0x1fff)
	{
		if(ioctl(tshift->av_fd, AMSTREAM_IOC_VFORMAT, para->media_info.vid_fmt)==-1)
		{
			AM_DEBUG(1, "set video format failed");
			return AM_AV_ERR_SYS;
		}
		if(ioctl(tshift->av_fd, AMSTREAM_IOC_VID, para->media_info.vid_pid)==-1)
		{
			AM_DEBUG(1, "set video PID failed");
			return AM_AV_ERR_SYS;
		}
	}
	
	if(para->media_info.aud_cnt > 0)
	{		
		if(ioctl(tshift->av_fd, AMSTREAM_IOC_AFORMAT, tshift->aud_fmt)==-1)
		{
			AM_DEBUG(1, "set audio format failed");
			return AM_AV_ERR_SYS;
		}
		if(ioctl(tshift->av_fd, AMSTREAM_IOC_AID, tshift->aud_pid)==-1)
		{
			AM_DEBUG(1, "set audio PID failed");
			return AM_AV_ERR_SYS;
		}
		/*if((para->aud_fmt==AFORMAT_PCM_S16LE)||(para->aud_fmt==AFORMAT_PCM_S16BE)||
				(para->aud_fmt==AFORMAT_PCM_U8)) {
			ioctl(afd, AMSTREAM_IOC_ACHANNEL, para->channel);
			ioctl(afd, AMSTREAM_IOC_SAMPLERATE, para->sample_rate);
			ioctl(afd, AMSTREAM_IOC_DATAWIDTH, para->data_width);
		}*/
	}

	if(ioctl(tshift->av_fd, AMSTREAM_IOC_PORT_INIT, 0)==-1)
	{
		AM_DEBUG(1, "amport init failed");
		return AM_AV_ERR_SYS;
	}

	if(para->media_info.aud_cnt > 0 && start_audio)
	{
#if !defined(ADEC_API_NEW)
		adec_cmd("start");
#else
		{
			audio_info_t info;

			/*Set audio info*/
			memset(&info, 0, sizeof(info));
			info.valid  = 1;
			if(ioctl(tshift->av_fd, AMSTREAM_IOC_AUDIO_INFO, (unsigned long)&info)==-1)
			{
				AM_DEBUG(1, "set audio info failed");
			}
		}
		{
			codec_para_t cp;
			memset(&cp, 0, sizeof(cp));

			audio_decode_init(&adec_handle, &cp);
		}
//#ifdef CHIP_8626X
		audio_set_av_sync_threshold(adec_handle, AV_SYNC_THRESHOLD);
//#endif
		
		audio_decode_start(adec_handle);
#endif
		AM_AOUT_SetDriver(AOUT_DEV_NO, &adec_aout_drv, NULL);
	}
	
	if (create_thread)
	{
		tshift->state = AV_TIMESHIFT_STAT_STOP;
		tshift->speed = 0;
		tshift->duration = para->media_info.duration;

		if (para->media_info.aud_cnt > 0)
		{
			tshift->aud_pid = para->media_info.audios[0].pid;
			tshift->aud_fmt = para->media_info.audios[0].fmt;
		}
		
		AM_DEBUG(1, "timeshifting mode %d, duration %d", para->mode, tshift->duration);
		if (para->mode == AM_AV_TIMESHIFT_MODE_PLAYBACK || tshift->duration <= 0)
			tshift->file.loop = AM_FALSE;
		else
			tshift->file.loop = AM_TRUE;
		
		AM_TRY(aml_timeshift_file_open(&tshift->file, para->file_path));
		
		if (! tshift->file.loop && tshift->duration)
		{
			tshift->rate = tshift->file.size/tshift->duration;
			AM_DEBUG(1, "@@@Playback file size %lld, duration %d s, the bitrate is %d bps", tshift->file.size, tshift->duration, tshift->rate);
		}
		tshift->running = 1;
		if(pthread_create(&tshift->thread, NULL, aml_timeshift_thread, (void*)tshift))
		{
			AM_DEBUG(1, "create the timeshift thread failed");
			tshift->running = 0;
		}
		else
		{
			tshift->para = *para;
			pthread_mutex_init(&tshift->lock, NULL);
			pthread_cond_init(&tshift->cond, NULL);
		}
	}
	
	return AM_SUCCESS;
}

/**\brief 释放Timeshift相关数据*/
static void aml_destroy_timeshift_data(AV_TimeshiftData_t *tshift, AM_Bool_t destroy_thread)
{
	if (tshift->running && destroy_thread) 
	{
		tshift->running = 0;
		pthread_cond_broadcast(&tshift->cond);
		pthread_join(tshift->thread, NULL);
		aml_timeshift_file_close(&tshift->file);
	}

	if(tshift->av_fd!=-1)
	{

		AM_DEBUG(1, "Stopping Audio decode");
#if!defined(ADEC_API_NEW)
		adec_cmd("stop");
#else
		if (adec_handle)
		{
			audio_decode_stop(adec_handle);
			audio_decode_release(&adec_handle);
			adec_handle = NULL;
		}
#endif		
		AM_DEBUG(1, "Closing mpts");
		close(tshift->av_fd);
	}
	AM_DEBUG(1, "Closing demux 1");
	if (tshift->dmxfd != -1)
		close(tshift->dmxfd);
	AM_DEBUG(1, "Closing video");
	if (tshift->cntl_fd != -1)
		close(tshift->cntl_fd);

	AM_FileEcho("/sys/class/stb/source", tshift->last_stb_src);

	if (destroy_thread)
		free(tshift);
}

static int am_timeshift_reset(AV_TimeshiftData_t *tshift, int deinterlace_val, AM_Bool_t start_audio)
{
	aml_destroy_timeshift_data(tshift, AM_FALSE);

	aml_start_timeshift(tshift, &tshift->para, AM_FALSE, start_audio);

	/*Set the left to 0, we will read from the new point*/
	tshift->left = 0;
	
	return 0;
}

static int am_timeshift_fffb(AV_TimeshiftData_t *tshift)
{
	int now, ret, next_time;
	loff_t offset;
	int speed = tshift->speed/*(tshift->speed/AM_ABS(tshift->speed))*(2<<(AM_ABS(tshift->speed) - 1))*/;

	if (speed)
	{
		AM_TIME_GetClock(&now);
		if (speed < 0)
			speed += -1;
		next_time = tshift->fffb_base + speed *(now - tshift->fffb_time);

		tshift->fffb_base = next_time;
		tshift->fffb_time = now;

		next_time /= 1000;
		offset = (loff_t)next_time * (loff_t)tshift->rate;
		AM_DEBUG(1, "fffb_base is %d, distance is %d, speed is %d", tshift->fffb_base, speed *(now - tshift->fffb_time)/1000, speed);
		ret = aml_timeshift_file_seek(&tshift->file, offset);
		AM_DEBUG(1,"FFFB next offset is %lld, next time is %d, reach end %d, speed %d, distance %d", offset, next_time, ret, tshift->speed, now - tshift->fffb_time);
	
		am_timeshift_reset(tshift, 0, AM_FALSE);
	}
	else
	{
		/*speed is 0, turn to play*/
		ret = 1;
	}

	if (ret)
	{
		/*FF FB reach end or speed is 0, so after this loop, we will turn to PLAY*/
		pthread_mutex_lock(&tshift->lock);
		tshift->cmd = AV_PLAY_START;
		pthread_mutex_unlock(&tshift->lock);
	}

	return 0;
}

static int aml_timeshift_pause_av(AV_TimeshiftData_t *tshift)
{
	if (tshift->aud_pid < 0x1fff)
	{
#if defined(ADEC_API_NEW)
		audio_decode_pause(adec_handle);
#else
		//TODO
#endif
	}
	
	if (tshift->para.media_info.vid_pid < 0x1fff)
	{
		ioctl(tshift->cntl_fd, AMSTREAM_IOC_VPAUSE, 1);
	}

	return 0;
}

static int aml_timeshift_resume_av(AV_TimeshiftData_t *tshift)
{
	if (tshift->aud_pid < 0x1fff)
	{
#if defined(ADEC_API_NEW)
		audio_decode_resume(adec_handle);
#else
		//TODO
#endif
	}
	if (tshift->para.media_info.vid_pid < 0x1fff)
	{
		ioctl(tshift->cntl_fd, AMSTREAM_IOC_VPAUSE, 0);
	}

	
	return 0;
}

static int aml_timeshift_do_play_cmd(AV_TimeshiftData_t *tshift, AV_PlayCmd_t cmd, AM_AV_TimeshiftInfo_t *info)
{
	AV_TimeshiftState_t	last_state = tshift->state;
	loff_t offset;

	if (! tshift->rate && (cmd == AV_PLAY_FF || cmd == AV_PLAY_FB || cmd == AV_PLAY_SEEK))
	{
		AM_DEBUG(1, "Have not got the rate, skip this command");
		return -1;
	}

	if (tshift->last_cmd != cmd)
	{
		switch (cmd)
		{
		case AV_PLAY_START:
			if (tshift->state != AV_TIMESHIFT_STAT_PLAY)
			{
				tshift->inject_size = 64*1024;
				tshift->timeout = 0;
				tshift->state = AV_TIMESHIFT_STAT_PLAY;
				AM_DEBUG(1, "@@@Timeshift start normal play, seek to time %d s...", info->current_time);
				offset = (loff_t)info->current_time * (loff_t)tshift->rate ;
				aml_timeshift_file_seek(&tshift->file, offset);
				ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
				am_timeshift_reset(tshift, 2, AM_TRUE);
				
				if (tshift->last_cmd == AV_PLAY_FF || tshift->last_cmd == AV_PLAY_FB)
				{
					usleep(200*1000);
					AM_DEBUG(1, "set di bypass_all to 0");
					AM_FileEcho("/sys/module/di/parameters/bypass_all","0");
				}
			}
			break;
		case AV_PLAY_PAUSE:
			if (tshift->state != AV_TIMESHIFT_STAT_PAUSE)
			{
				tshift->inject_size = 0;
				tshift->timeout = 1000;
				tshift->state = AV_TIMESHIFT_STAT_PAUSE;
				aml_timeshift_pause_av(tshift);
				AM_DEBUG(1, "@@@Timeshift Paused");
			}
			break;
		case AV_PLAY_RESUME:
			if (tshift->state == AV_TIMESHIFT_STAT_PAUSE)
			{
				tshift->inject_size = 64*1024;
				tshift->timeout = 0;
				tshift->state = AV_TIMESHIFT_STAT_PLAY;
				aml_timeshift_resume_av(tshift);
				AM_DEBUG(1, "@@@Timeshift Resumed");
			}
			break;
		case AV_PLAY_FF:
		case AV_PLAY_FB:
			if (tshift->state != AV_TIMESHIFT_STAT_FFFB)
			{
				if (tshift->speed == 0 && tshift->state == AV_TIMESHIFT_STAT_PLAY)
					return 0;
				tshift->inject_size = 64*1024;
				tshift->timeout = 0;
				tshift->state = AV_TIMESHIFT_STAT_FFFB;
				
				if (tshift->last_cmd == AV_PLAY_START)
				{
					AM_DEBUG(1, "set di bypass_all to 1");	
					AM_FileEcho("/sys/module/di/parameters/bypass_all","1");
					usleep(200*1000);
				}
			
				ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_FFFB);

				AM_TIME_GetClock(&tshift->fffb_time);
				tshift->fffb_base = info->current_time*1000;
				am_timeshift_fffb(tshift);
			}
			break;
		case AV_PLAY_SEEK:
			offset = (loff_t)tshift->seek_pos * (loff_t)tshift->rate;
			AM_DEBUG(1, "Timeshift seek to offset %lld, time %d s", offset, tshift->seek_pos);
			aml_timeshift_file_seek(&tshift->file, offset);
			ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
			am_timeshift_reset(tshift, 0, AM_TRUE);
			tshift->inject_size = 0;
			tshift->timeout = 0;
			tshift->state = AV_TIMESHIFT_STAT_SEARCHOK;
			info->current_time = (tshift->file.total - tshift->file.avail)*tshift->duration/tshift->file.size;
			break;
		case AV_PLAY_RESET_VPATH:
			ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
			/*stop play first*/
			aml_destroy_timeshift_data(tshift, AM_FALSE);
			/*reset the vpath*/
			aml_set_vpath(tshift->dev);
			/*restart play now*/
			aml_start_timeshift(tshift, &tshift->para, AM_FALSE, AM_TRUE);

			/*Set the left to 0, we will read from the new point*/
			tshift->left = 0;
			tshift->inject_size = 0;
			tshift->timeout = 0;
			/* will turn to play state from current_time */
			tshift->state = AV_TIMESHIFT_STAT_SEARCHOK;
			break;
		case AV_PLAY_SWITCH_AUDIO:
			/* just restart play using the new audio para */
			AM_DEBUG(1, "Switch audio to pid=%d, fmt=%d",tshift->aud_pid, tshift->aud_fmt);
			/*Set the left to 0, we will read from the new point*/
			tshift->left = 0;
			tshift->inject_size = 0;
			tshift->timeout = 0;
			/* will turn to play state from current_time */
			tshift->state = AV_TIMESHIFT_STAT_SEARCHOK;
			break;
		default:
			AM_DEBUG(1, "Unsupported timeshift play command %d", cmd);
			return -1;
			break;
		}
	}

	if (tshift->last_cmd != cmd)
		tshift->last_cmd = cmd;
	
	if (tshift->state != last_state)
	{
		/*Notify status changed*/
		AM_DEBUG(1, "Notify status changed: 0x%x->0x%x", last_state, tshift->state);
		info->status = tshift->state;
		AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)info);

		/*if (tshift->state != AV_TIMESHIFT_STAT_FFFB && tshift->state != AV_TIMESHIFT_STAT_SEARCHOK)
		{
			ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
		}
		if (last_state == AV_TIMESHIFT_STAT_FFFB || last_state == AV_TIMESHIFT_STAT_SEARCHOK)
		{
			am_timeshift_reset(tshift, 2);
		}*/
		if (tshift->state == AV_TIMESHIFT_STAT_SEARCHOK)
		{
			pthread_mutex_lock(&tshift->lock);
			tshift->cmd = AV_PLAY_START;
			pthread_mutex_unlock(&tshift->lock);
		}
	}

	

	return 0;
}

static void aml_timeshift_update_info(AV_TimeshiftData_t *tshift, AM_AV_TimeshiftInfo_t *info)
{
	pthread_mutex_lock(&tshift->lock);
	tshift->info = *info;
	pthread_mutex_unlock(&tshift->lock);
}

/**\brief Timeshift 线程*/
static void *aml_timeshift_thread(void *arg)
{
	AV_TimeshiftData_t *tshift = (AV_TimeshiftData_t *)arg;
	int speed, update_time, now/*, fffb_time*/;
	int len, ret;
	int /*dmxfd,*/ trick_stat;
	uint8_t buf[64*1024];
	const int FFFB_STEP = 150;
	struct timespec rt;
	AM_AV_TimeshiftInfo_t info;
	struct am_io_param astatus;
	struct am_io_param vstatus;
	AV_PlayCmd_t cmd;
	int blackout = aml_get_black_policy();
	int playback_alen=0, playback_vlen=0;
	AM_Bool_t is_playback_mode = (tshift->para.mode == AM_AV_TIMESHIFT_MODE_PLAYBACK);

	memset(&info, 0, sizeof(info));
	tshift->last_cmd = -1;
	AM_DEBUG(1, "Starting timeshift player thread ...");
	info.status = AV_TIMESHIFT_STAT_INITOK;
	AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)&info);
	AM_TIME_GetClock(&update_time);

	aml_set_black_policy(0);
	while (tshift->running||tshift->cmd!= tshift->last_cmd )
	{
		pthread_mutex_lock(&tshift->lock);
		cmd = tshift->cmd;
		speed = tshift->speed;
		pthread_mutex_unlock(&tshift->lock);

		aml_timeshift_do_play_cmd(tshift, cmd, &info);

		/*read some bytes*/
		len = sizeof(buf) - tshift->left;
		if (len > 0)
		{
			ret = aml_timeshift_file_read(&tshift->file, buf+tshift->left, len, 100);
			if (ret > 0)
			{
				tshift->left += ret;
			}
		}
		
		/*Inject*/
		if (tshift->inject_size > 0)
		{
			if (tshift->state == AV_TIMESHIFT_STAT_PLAY &&
				ioctl(tshift->av_fd, AMSTREAM_IOC_AB_STATUS, (unsigned long)&astatus) != -1 && 
				ioctl(tshift->av_fd, AMSTREAM_IOC_VB_STATUS, (unsigned long)&vstatus) != -1) 
			{
				if (vstatus.status.data_len == 0 && astatus.status.data_len > 100*1024)
				{
					tshift->timeout = 10;
					goto wait_for_next_loop;
				}
				else
				{
					tshift->timeout = 0;
				}
			}
			ret = AM_MIN(tshift->left , tshift->inject_size);
			if (ret > 0)
				ret = aml_timeshift_inject(tshift, buf, ret, -1);

			if (ret > 0)
			{
				/*ret bytes written*/
				tshift->left -= ret;
				if (tshift->left > 0)
					memmove(buf, buf+ret, tshift->left);
			}
		}
		
		AM_TIME_GetClock(&now);

		/*Update the playing info*/
		if (tshift->state == AV_TIMESHIFT_STAT_FFFB)
		{
			tshift->timeout = 0;
			if (tshift->para.media_info.vid_pid < 0x1fff)
				trick_stat = aml_timeshift_get_trick_stat(tshift);
			else
				trick_stat = 1;
			AM_DEBUG(7, "trick_stat is %d", trick_stat);
			if (trick_stat > 0)
			{
				tshift->timeout = FFFB_STEP;
				info.current_time = tshift->fffb_base/1000;

				if (tshift->rate)
					info.full_time = tshift->file.total/tshift->rate;
				else
					info.full_time = 0;
				info.status = tshift->state;

				AM_DEBUG(1, "Notify time update");
				AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)&info);
				aml_timeshift_update_info(tshift, &info);
				am_timeshift_fffb(tshift);
			}
			else if ((now - tshift->fffb_time) > 2000)
			{
				AM_DEBUG(1, "FFFB seek frame timeout, maybe no data available");
				am_timeshift_fffb(tshift);
			}
			
		}
		else if ((now - update_time) >= 1000)
		{
			if(ioctl(tshift->av_fd, AMSTREAM_IOC_AB_STATUS, (unsigned long)&astatus) != -1 && 
			ioctl(tshift->av_fd, AMSTREAM_IOC_VB_STATUS, (unsigned long)&vstatus) != -1)
			{
				update_time = now;
				if (tshift->rate)
					info.full_time = tshift->file.total/tshift->rate;
				else
					info.full_time = 0;
				if (!is_playback_mode && !tshift->file.loop)
					tshift->duration = info.full_time;
				
				AM_DEBUG(1, "total %lld, avail %lld, alen %d, vlen %d, duration %d, size %lld", 
					tshift->file.total , tshift->file.avail , astatus.status.data_len , 
					vstatus.status.data_len, tshift->duration, tshift->file.size);
				
				if (tshift->rate)
				{
					/* approximate, not accurate */
					loff_t buffered_es_len = astatus.status.data_len+vstatus.status.data_len;
					loff_t buffered_ts_en = 188*buffered_es_len/184;
					
					info.current_time = (tshift->file.total - tshift->file.avail - buffered_ts_en)
										* (loff_t)tshift->duration / tshift->file.size;
				}
				else
				{
					info.current_time = 0;
				}
				
				if (info.current_time < 0)
				{
					/*Exceed, play from the start*/
					aml_timeshift_file_seek(&tshift->file, 0);
					am_timeshift_reset(tshift, -1, AM_TRUE);
					info.current_time = 0;
				}
				
				info.status = tshift->state;

				AM_DEBUG(1, "Notify time update, current %d, total %d", info.current_time, info.full_time);
				AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)&info);
				aml_timeshift_update_info(tshift, &info);
				
				/*If there is no data available in playback only mode, we send exit event*/
				if (is_playback_mode && !tshift->file.avail)
				{
					if (playback_alen == astatus.status.data_len && 
						playback_vlen == vstatus.status.data_len)
					{
						AM_DEBUG(1, "Playback End");
						info.current_time = info.full_time;
						break;
					}
					else
					{
						playback_alen = astatus.status.data_len;
						playback_vlen = vstatus.status.data_len;
					}
				}
			}
		}

wait_for_next_loop:
		if (tshift->timeout == -1)
		{
			pthread_mutex_lock(&tshift->lock);
			pthread_cond_wait(&tshift->cond, &tshift->lock);
			pthread_mutex_unlock(&tshift->lock);
		}
		else if (tshift->timeout > 0)
		{
			pthread_mutex_lock(&tshift->lock);
			AM_TIME_GetTimeSpecTimeout(tshift->timeout, &rt);
			pthread_cond_timedwait(&tshift->cond, &tshift->lock, &rt);
			pthread_mutex_unlock(&tshift->lock);
		}
	}

	AM_DEBUG(1, "Timeshift player thread exit now");
	info.status = AV_TIMESHIFT_STAT_EXIT;
	AM_EVT_Signal(0, AM_AV_EVT_PLAYER_UPDATE_INFO, (void*)&info);
	aml_timeshift_update_info(tshift, &info);

	ioctl(tshift->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
	aml_set_black_policy(blackout);
	
	return NULL;
}

static AM_ErrorCode_t aml_timeshift_fill_data(AM_AV_Device_t *dev, uint8_t *data, int size)
{
	AV_TimeshiftData_t *tshift = (AV_TimeshiftData_t *)dev->timeshift_player.drv_data;
	int now;

	if (tshift)
	{
		ssize_t ret;

		if (size > 0 && !tshift->rate)
		{
			AM_TIME_GetClock(&now);
			if (! tshift->rtime)
			{
				tshift->rtotal = 0;
				tshift->rtime = now;
			}
			else
			{
				if ((now - tshift->rtime) >= 3000)
				{
					tshift->rtotal += size;
					/*Calcaulate the rate*/
					tshift->rate = (tshift->rtotal*1000)/(now - tshift->rtime);
					if (tshift->rate && tshift->file.loop)
					{
						/*Calculate the file size*/
						tshift->file.size = tshift->rate * tshift->duration;
						pthread_cond_signal(&tshift->cond);
						AM_DEBUG(1, "@@@wirte record data %lld bytes in %d ms,so the rate is assumed to %d bps, ring file size %lld", 
							tshift->rtotal, now - tshift->rtime, tshift->rate*8, tshift->file.size);
					}
					else
					{
						tshift->rtime = 0;
					}	
				}
				else
				{
					tshift->rtotal += size;
				}
			}
		}

		ret = aml_timeshift_file_write(&tshift->file, data, size);
		if (ret != (ssize_t)size)
		{
			AM_DEBUG(1, "Write timeshift data to file failed");
			/*A write error*/
			return AM_AV_ERR_SYS;
		}
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_timeshift_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *para)
{
	AV_TimeshiftData_t *data;
	
	data = (AV_TimeshiftData_t*)dev->timeshift_player.drv_data;
	
	pthread_mutex_lock(&data->lock);
	data->cmd = cmd;
	if (cmd == AV_PLAY_FF || cmd == AV_PLAY_FB)
	{
		data->speed = (int)para;
	}
	else if (cmd == AV_PLAY_SEEK)
	{
		data->seek_pos = ((AV_FileSeekPara_t*)para)->pos;;
	}
	else if (cmd == AV_PLAY_SWITCH_AUDIO)
	{
		AV_TSPlayPara_t *audio_para = (AV_TSPlayPara_t*)para;
		if (audio_para)
		{
			int i;

			for (i=0; i<data->para.media_info.aud_cnt; i++)
			{
				if (data->para.media_info.audios[i].pid == audio_para->apid &&
					data->para.media_info.audios[i].fmt == audio_para->afmt)
				{
					data->aud_pid = audio_para->apid;
					data->aud_fmt = audio_para->afmt;
					break;
				}
			}
		}
	}
	pthread_cond_signal(&data->cond);
	pthread_mutex_unlock(&data->lock);

	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_timeshift_get_info(AM_AV_Device_t *dev, AM_AV_TimeshiftInfo_t *info)
{
	AV_TimeshiftData_t *data;
	
	data = (AV_TimeshiftData_t*)dev->timeshift_player.drv_data;
	
	pthread_mutex_lock(&data->lock);
	*info = data->info;
	pthread_mutex_unlock(&data->lock);

	return AM_SUCCESS;
}

extern unsigned long CMEM_getPhys(unsigned long virts);

/**\brief 解码JPEG数据*/
static AM_ErrorCode_t aml_decode_jpeg(AV_JPEGData_t *jpeg, const uint8_t *data, int len, int mode, void *para)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
#if !defined(ANDROID)
	AV_JPEGDecodePara_t *dec_para = (AV_JPEGDecodePara_t*)para;
	AV_JPEGDecState_t s;
	AM_Bool_t decLoop = AM_TRUE;
	int decState = 0;
	int try_count = 0;
	int decode_wait = 0;
	const uint8_t *src = data;
	int left = len;
	AM_OSD_Surface_t *surf = NULL;

#ifndef ANDROID
	jpegdec_info_t info;
#endif

	char tmp_buf[64];

	s = AV_JPEG_DEC_STAT_INFOCONFIG;
	while(decLoop)
	{
		if(jpeg->dec_fd==-1)
		{
			jpeg->dec_fd = open(JPEG_DEC_FILE, O_RDWR);
			if (jpeg->dec_fd!=-1)
			{
				s = AV_JPEG_DEC_STAT_INFOCONFIG;
			}
			else
			{
				try_count++;
				if(try_count>40)
				{
					AM_DEBUG(1, "jpegdec init timeout");
					try_count=0;
					ret = aml_init_jpeg(jpeg);
					if(ret!=AM_SUCCESS)
						break;
				}
				usleep(10000);
				continue;
			}
		}
		else
		{
			decState = ioctl(jpeg->dec_fd, JPEGDEC_IOC_STAT);
			
			if (decState & JPEGDEC_STAT_ERROR)
			{
				AM_DEBUG(1, "jpegdec JPEGDEC_STAT_ERROR");
				ret = AM_AV_ERR_DECODE;
				break;
			}

			if (decState & JPEGDEC_STAT_UNSUPPORT)
			{
				AM_DEBUG(1, "jpegdec JPEGDEC_STAT_UNSUPPORT");
				ret = AM_AV_ERR_DECODE;
				break;
			}

			if (decState & JPEGDEC_STAT_DONE)
				break;
			
			if (decState & JPEGDEC_STAT_WAIT_DATA) 
			{
				if(left > 0) 
				{
					int send = AM_MIN(left, JPEG_WRTIE_UNIT);
					int rc;
					rc = write(jpeg->vbuf_fd, src, send);
					if(rc==-1) 
					{
						AM_DEBUG(1, "write data to the jpeg decoder failed");
						ret = AM_AV_ERR_DECODE;
						break;
					}
					left -= rc;
					src  += rc;
				}
				else if(decode_wait==0)
				{
					int i, times = JPEG_WRTIE_UNIT/sizeof(tmp_buf);
					
					memset(tmp_buf, 0, sizeof(tmp_buf));
					
					for(i=0; i<times; i++)
						write(jpeg->vbuf_fd, tmp_buf, sizeof(tmp_buf));
					decode_wait++;
				}
				else
				{
					if(decode_wait>300)
					{
						AM_DEBUG(1, "jpegdec wait data error");
						ret = AM_AV_ERR_DECODE;
						break;
					}
					decode_wait++;
					usleep(10);
				}
			}
	
			switch (s)
			{
				case AV_JPEG_DEC_STAT_INFOCONFIG:
					if (decState & JPEGDEC_STAT_WAIT_INFOCONFIG) 
					{
						if(ioctl(jpeg->dec_fd, JPEGDEC_IOC_INFOCONFIG, 0)==-1)
						{
							AM_DEBUG(1, "jpegdec JPEGDEC_IOC_INFOCONFIG error");
							ret = AM_AV_ERR_DECODE;
							decLoop = AM_FALSE;
						}
						s = AV_JPEG_DEC_STAT_INFO;
					}
					break;
				case AV_JPEG_DEC_STAT_INFO:
					if (decState & JPEGDEC_STAT_INFO_READY) 
					{
						if(ioctl(jpeg->dec_fd, JPEGDEC_IOC_INFO, &info)==-1)
						{
							AM_DEBUG(1, "jpegdec JPEGDEC_IOC_INFO error");
							ret = AM_AV_ERR_DECODE;
							decLoop = AM_FALSE;
						}
						if(mode&AV_GET_JPEG_INFO)
						{
							AM_AV_JPEGInfo_t *jinfo = (AM_AV_JPEGInfo_t*)para;
							jinfo->width    = info.width;
							jinfo->height   = info.height;
							jinfo->comp_num = info.comp_num;
							decLoop = AM_FALSE;
						}
						AM_DEBUG(2, "jpegdec width:%d height:%d", info.width, info.height);
						s = AV_JPEG_DEC_STAT_DECCONFIG;
					}
					break;
				case AV_JPEG_DEC_STAT_DECCONFIG:
					if (decState & JPEGDEC_STAT_WAIT_DECCONFIG) 
					{
						jpegdec_config_t config;
						int dst_w, dst_h;
						
						switch(dec_para->para.angle)
						{
							case AM_AV_JPEG_CLKWISE_0:
							default:
								dst_w = info.width;
								dst_h = info.height;
							break;
							case AM_AV_JPEG_CLKWISE_90:
								dst_w = info.height;
								dst_h = info.width;
							break;
							case AM_AV_JPEG_CLKWISE_180:
								dst_w = info.width;
								dst_h = info.height;
							break;
							case AM_AV_JPEG_CLKWISE_270:
								dst_w = info.height;
								dst_h = info.width;
							break;
						}
						
						if(dec_para->para.width>0)
							dst_w = AM_MIN(dst_w, dec_para->para.width);
						if(dec_para->para.height>0)
							dst_h = AM_MIN(dst_h, dec_para->para.height);
						
						ret = AM_OSD_CreateSurface(AM_OSD_FMT_YUV_420, dst_w, dst_h, AM_OSD_SURFACE_FL_HW, &surf);
						if(ret!=AM_SUCCESS)
						{
							AM_DEBUG(1, "cannot create the YUV420 surface");
							decLoop = AM_FALSE;
						}
						else
						{
							config.addr_y = CMEM_getPhys((unsigned long)surf->buffer);
							config.addr_u = config.addr_y+CANVAS_ALIGN(surf->width)*surf->height;
							config.addr_v = config.addr_u+CANVAS_ALIGN(surf->width/2)*(surf->height/2);
							config.opt    = dec_para->para.option;
							config.dec_x  = 0;
							config.dec_y  = 0;
							config.dec_w  = surf->width;
							config.dec_h  = surf->height;
							config.angle  = dec_para->para.angle;
							config.canvas_width = CANVAS_ALIGN(surf->width);
						
							if(ioctl(jpeg->dec_fd, JPEGDEC_IOC_DECCONFIG, &config)==-1)
							{
								AM_DEBUG(1, "jpegdec JPEGDEC_IOC_DECCONFIG error");
								ret = AM_AV_ERR_DECODE;
								decLoop = AM_FALSE;
							}
							s = AV_JPEG_DEC_STAT_RUN;
						}
					}
				break;

				default:
					break;
			}
		}
	}
	
	if(surf)
	{
		if(ret==AM_SUCCESS)
		{
			dec_para->surface = surf;
		}
		else
		{
			AM_OSD_DestroySurface(surf);
		}
	}
#endif
	return ret;
}

static AM_ErrorCode_t aml_open(AM_AV_Device_t *dev, const AM_AV_OpenPara_t *para)
{
//#ifndef ANDROID
	char buf[32];
	int v;
	
	if(AM_FileRead(VID_AXIS_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		int left, top, right, bottom;
		
		if(sscanf(buf, "%d %d %d %d", &left, &top, &right, &bottom)==4)
		{
			dev->video_x = left;
			dev->video_y = top;
			dev->video_w = right-left;
			dev->video_h = bottom-top;
		}
	}
	if(AM_FileRead(VID_CONTRAST_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_contrast = v;
		}
	}
	if(AM_FileRead(VID_SATURATION_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_saturation = v;
		}
	}
	if(AM_FileRead(VID_BRIGHTNESS_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_brightness = v;
		}
	}
	if(AM_FileRead(VID_DISABLE_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_enable = v?AM_FALSE:AM_TRUE;
		}
	}
	if(AM_FileRead(VID_BLACKOUT_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_blackout = v?AM_TRUE:AM_FALSE;
		}
	}
	if(AM_FileRead(VID_SCREEN_MODE_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			dev->video_mode = v;
#ifndef 	CHIP_8226H
			switch(v) {
				case 0:
				case 1:
					dev->video_ratio = AM_AV_VIDEO_ASPECT_AUTO;
					dev->video_mode = v;
					break;
				case 2:
					dev->video_ratio = AM_AV_VIDEO_ASPECT_4_3;
					dev->video_mode = AM_AV_VIDEO_DISPLAY_FULL_SCREEN;
					break;					
				case 3:
					dev->video_ratio = AM_AV_VIDEO_ASPECT_16_9;
					dev->video_mode = AM_AV_VIDEO_DISPLAY_FULL_SCREEN;
					break;
			}
 #endif		
		}
	}
	if(AM_FileRead(DISP_MODE_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(!strncmp(buf, "576cvbs", 7) || !strncmp(buf, "576i", 4) || !strncmp(buf, "576i", 4))
		{
			dev->vout_w = 720;
			dev->vout_h = 576;
		}
		else if (!strncmp(buf, "480cvbs", 7) || !strncmp(buf, "480i", 4) || !strncmp(buf, "480i", 4))
		{
			dev->vout_w = 720;
			dev->vout_h = 480;
		}
		else if (!strncmp(buf, "720p", 4))
		{
			dev->vout_w = 1280;
			dev->vout_h = 720;
		}
		else if (!strncmp(buf, "1080i", 5) || !strncmp(buf, "1080p", 5))
		{
			dev->vout_w = 1920;
			dev->vout_h = 1080;
		}
	}
#ifdef CHIP_8226H	
	if(AM_FileRead(VID_ASPECT_RATIO_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if (!strncmp(buf, "auto", 4))
		{
			dev->video_ratio = AM_AV_VIDEO_ASPECT_AUTO;
		}
		else if (!strncmp(buf, "16x9", 4))
		{
			dev->video_ratio = AM_AV_VIDEO_ASPECT_16_9;
		}
		else if (!strncmp(buf, "4x3", 3))
		{
			dev->video_ratio = AM_AV_VIDEO_ASPECT_4_3;
		}
	}
	if(AM_FileRead(VID_ASPECT_MATCH_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if (!strncmp(buf, "ignore", 4))
		{
			dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_IGNORE;
		}
		else if (!strncmp(buf, "letter box", 10))
		{
			dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_LETTER_BOX;
		}
		else if (!strncmp(buf, "pan scan", 8))
		{
			dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_PAN_SCAN;
		}
		else if (!strncmp(buf, "combined", 8))
		{
			dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_COMBINED;
		}
	}
#else
	if(AM_FileRead(VID_ASPECT_MATCH_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(sscanf(buf, "%d", &v)==1)
		{
			switch(v)
			{
				case 0:
				case 1:
					dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_IGNORE;
					break;
				case 2:
					dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_PAN_SCAN;
					break;
				case 3:
					dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_LETTER_BOX;
					break;
				case 4:
					dev->video_match = AM_AV_VIDEO_ASPECT_MATCH_COMBINED;
					break;
			}
		}
	}
#endif	
//#endif

#if !defined(ADEC_API_NEW)
	adec_cmd("stop");
	return AM_SUCCESS;
#else
	audio_decode_basic_init();
	return AM_SUCCESS;
#endif
}

static AM_ErrorCode_t aml_close(AM_AV_Device_t *dev)
{
	return AM_SUCCESS;
}

#if 0
typedef struct {
    unsigned int    format;  ///< video format, such as H264, MPEG2...
    unsigned int    width;   ///< video source width
    unsigned int    height;  ///< video source height
    unsigned int    rate;    ///< video source frame duration
    unsigned int    extra;   ///< extra data information of video stream
    unsigned int    status;  ///< status of video stream
    float		    ratio;   ///< aspect ratio of video source
    void *          param;   ///< other parameters for video decoder
} dec_sysinfo_t;
#endif
dec_sysinfo_t am_sysinfo;

#define ARC_FREQ_FILE "/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq"

static void set_arc_freq(int hi)
{
	static int old_freq = 200000;
	int freq;
	char buf[32];

	if(AM_FileRead(ARC_FREQ_FILE, buf, sizeof(buf))==AM_SUCCESS){
		sscanf(buf, "%d", &freq);
		if(hi){
			old_freq = freq;
			snprintf(buf, sizeof(buf), "%d", 300000);
			AM_FileEcho(ARC_FREQ_FILE, buf);
		}else{
			snprintf(buf, sizeof(buf), "%d", old_freq);
			AM_FileEcho(ARC_FREQ_FILE, buf);
		}
	}
}

static AM_ErrorCode_t aml_start_ts_mode(AM_AV_Device_t *dev, AV_TSPlayPara_t *tp, AM_Bool_t create_thread)
{
	int fd, val;
	
	fd = (int)dev->ts_player.drv_data;
	
//	AM_AOUT_SetDriver(AOUT_DEV_NO, &adec_aout_drv, NULL);

#if defined(ANDROID) || defined(CHIP_8626X)
	/*Set tsync enable/disable*/
	if (tp->vpid >= 0x1fff || tp->apid >= 0x1fff)
	{
		AM_DEBUG(1, "Set tsync enable to 0");
		aml_set_tsync_enable(0);
	}
	else
	{
		AM_DEBUG(1, "Set tsync enable to 1");
		aml_set_tsync_enable(1);
	}
#endif

	

	if(tp->vpid && (tp->vpid<0x1fff)){
		val = tp->vfmt;
		if(ioctl(fd, AMSTREAM_IOC_VFORMAT, val)==-1)
		{
			AM_DEBUG(1, "set video format failed");
			return AM_AV_ERR_SYS;
		}
		val = tp->vpid;
		if(ioctl(fd, AMSTREAM_IOC_VID, val)==-1)
		{
			AM_DEBUG(1, "set video PID failed");
			return AM_AV_ERR_SYS;
		}
	}

	if ((tp->vfmt == VFORMAT_H264) || (tp->vfmt == VFORMAT_VC1)) {

		memset(&am_sysinfo,0,sizeof(dec_sysinfo_t));
		if(ioctl(fd, AMSTREAM_IOC_SYSINFO, (unsigned long)&am_sysinfo)==-1)
		{
			AM_DEBUG(1, "set AMSTREAM_IOC_SYSINFO");
			return AM_AV_ERR_SYS;
		}
	}	

	if(tp->apid && (tp->apid<0x1fff)){
		if((tp->afmt==AFORMAT_AC3) || (tp->afmt==AFORMAT_DTS)){
			//set_arc_freq(1);
		}

		val = tp->afmt;
		if(ioctl(fd, AMSTREAM_IOC_AFORMAT, val)==-1)
		{
			AM_DEBUG(1, "set audio format failed");
			return AM_AV_ERR_SYS;
		}
		val = tp->apid;
		if(ioctl(fd, AMSTREAM_IOC_AID, val)==-1)
		{
			AM_DEBUG(1, "set audio PID failed");
			return AM_AV_ERR_SYS;
		}
	}

	if(ioctl(fd, AMSTREAM_IOC_PORT_INIT, 0)==-1)
	{
		AM_DEBUG(1, "amport init failed");
		return AM_AV_ERR_SYS;
	}
	AM_DEBUG(1, "set ts skipbyte to 0");
	if(ioctl(fd, AMSTREAM_IOC_TS_SKIPBYTE, 0)==-1)
	{
		AM_DEBUG(1, "set ts skipbyte failed");
		return AM_AV_ERR_SYS;
	}

	AM_TIME_GetClock(&dev->ts_player.av_start_time);
	
	/*创建AV监控线程*/
	if (create_thread)
	{
		dev->ts_player.av_thread_running = AM_TRUE;
		if(pthread_create(&dev->ts_player.av_mon_thread, NULL, aml_av_monitor_thread, (void*)dev))
		{
			AM_DEBUG(1, "create the av buf monitor thread failed");
			dev->ts_player.av_thread_running = AM_FALSE;
		}
	}
	dev->ts_player.play_para = *tp;

	return AM_SUCCESS;
}

static int aml_close_ts_mode(AM_AV_Device_t *dev, AM_Bool_t destroy_thread)
{
	int fd;

	if (destroy_thread && dev->ts_player.av_thread_running) 
	{
		dev->ts_player.av_thread_running = AM_FALSE;
		pthread_cond_broadcast(&gAVMonCond);
		pthread_join(dev->ts_player.av_mon_thread, NULL);
	}
#if !defined(ADEC_API_NEW)
	
	adec_cmd("stop");
#else
	if(adec_handle)
	{
		audio_decode_stop(adec_handle);
		audio_decode_release(&adec_handle);
	}
	adec_handle = NULL;
#endif
	fd = (int)dev->ts_player.drv_data;
	if (fd != -1)
		close(fd);
	
	dev->ts_player.drv_data = (void*)-1;

	//set_arc_freq(0);
	
	return 0;
}

/**\brief 读取PTS值*/
static AM_ErrorCode_t aml_get_pts(const char *class_file,  uint32_t *pts)
{
	char buf[32];
	
	if(AM_FileRead(class_file, buf, sizeof(buf))==AM_SUCCESS)
	{
		*pts = (uint32_t)atol(buf);
		return AM_SUCCESS;
	}

	return AM_FAILURE;
}

//
#define ENABLE_CORRECR_AV_SYNC
#ifdef ENABLE_CORRECR_AV_SYNC
#define DI_BYPASS_FILE "/sys/module/di/parameters/bypass_hd"
#endif
//
/**\brief AV buffer 监控线程*/
static void* aml_av_monitor_thread(void *arg)
{
	AM_AV_Device_t *dev = (AM_AV_Device_t *)arg;
	//AM_Bool_t reset_avail = AM_FALSE;
	AM_Bool_t av_playing = AM_FALSE;
	uint32_t last_apts, last_vpts, apts, vpts;
	uint32_t last_dec_vpts, dec_vpts, dec_apts, last_dec_apts;
	int fd, pts_repeat_count, vpts_repeat_count, apts_repeat_count;
	int pts_time, last_pts_time;
	int alen, vlen;
	const int AUDIO_DATA_CHECK_PERIOD = 100;
	const int PTS_CHECK_PERIOD = 1000;
	AM_Bool_t notified = AM_FALSE;
	AM_Bool_t adec_start, vdec_start;
	struct am_io_param astatus;
	struct am_io_param vstatus;
	struct timespec rt;
	int curr_resample = 2;
	AM_Bool_t avStatus[2];
	int resample_change_ms = 0;
#ifdef ENABLE_CORRECR_AV_SYNC
        int need_skip_DI_4_HD;
        int  bypass_hd;

        need_skip_DI_4_HD = 0;
#endif
	
	last_apts = last_vpts = 0;
	last_dec_vpts = dec_vpts = 0;
	pts_repeat_count = 0;
	vpts_repeat_count = apts_repeat_count = 0;
	pts_time = last_pts_time = 0;
	alen = vlen = 0;
	adec_start = vdec_start = AM_FALSE;
	pthread_mutex_lock(&gAVMonLock);
	while (1)
	{
		if (!av_playing)
			AM_TIME_GetTimeSpecTimeout(AUDIO_DATA_CHECK_PERIOD, &rt);
		else
			AM_TIME_GetTimeSpecTimeout(PTS_CHECK_PERIOD, &rt);
		pthread_cond_timedwait(&gAVMonCond, &gAVMonLock, &rt);
		
		if (! dev->ts_player.av_thread_running)
			break;
			
		fd = (int)dev->ts_player.drv_data;
		if(!av_playing && ioctl(fd, AMSTREAM_IOC_AB_STATUS, (unsigned long)&astatus) != -1 && 
			ioctl(fd, AMSTREAM_IOC_VB_STATUS, (unsigned long)&vstatus) != -1)
		{
			int time;

			AM_TIME_GetClock(&time);

			/*当AUDIO 和 VIDEO数据达到某个长度时才开始播放, 只有音频时不进行检查直接播放*/
			if (dev->ts_player.play_para.apid < 0x1fff && 
				(dev->ts_player.play_para.vpid >= 0x1fff || 
#if defined(ENABLE_PCR_RECOVER) || defined(ENABLE_AUDIO_RESAMPLE)
				(dev->ts_player.play_para.vpid < 0x1fff && astatus.status.data_len >= AUDIO_START_LEN)
#else
				(time-dev->ts_player.av_start_time >= AUDIO_CACHE_TIME)
#endif
				))
			{ 
#ifdef ENABLE_AUDIO_RESAMPLE
				AM_FileEcho(ENABLE_RESAMPLE_FILE, "1");
				AM_FileEcho(RESAMPLE_TYPE_FILE, "2");
				curr_resample = 2;
#endif

#if !defined(ADEC_API_NEW)
				adec_cmd("start");
#else
				audio_info_t info;

				AM_DEBUG(1, "!!!!!!!!!!!!!!!!!!!Audio decode start!!!!!!!!!!!!!!\n");
				/*Set audio info*/
				memset(&info, 0, sizeof(info));
				info.valid  = 1;
				if(ioctl(fd, AMSTREAM_IOC_AUDIO_INFO, (unsigned long)&info)==-1)
				{
					AM_DEBUG(1, "set audio info failed");
				}

				{
					codec_para_t cp;
					memset(&cp, 0, sizeof(cp));
					audio_decode_init(&adec_handle, &cp);
				}
//#ifdef CHIP_8626X
				audio_set_av_sync_threshold(adec_handle, AV_SYNC_THRESHOLD);
//#endif

				audio_decode_start(adec_handle);
				AM_AOUT_SetDriver(AOUT_DEV_NO, &adec_aout_drv, NULL);
#endif
				av_playing = AM_TRUE;
				adec_start = AM_FALSE;
				vdec_start = AM_FALSE;
				last_pts_time = 0;
				vpts_repeat_count = 0;
				apts_repeat_count = 0;
				last_dec_vpts = 0;
				last_dec_apts = 0;
			}
		}
		
		if (av_playing)
		{
#ifdef ENABLE_AUDIO_RESAMPLE
			if(ioctl(fd, AMSTREAM_IOC_AB_STATUS, (unsigned long)&astatus) != -1 && 
					ioctl(fd, AMSTREAM_IOC_VB_STATUS, (unsigned long)&vstatus) != -1) {
				int ab_size = astatus.status.size;
				int a_size  = astatus.status.data_len;
				int vb_size = vstatus.status.size;
				int v_size  = vstatus.status.data_len;
				int af, vf;
				int resample;
				int apts = 0, vpts = 0, dmx_apts = 0, dmx_vpts = 0;

				if(a_size * 6 < ab_size){
					af = -1;
				}else if(a_size * 6 > ab_size * 5){
					af = 1;
				}else{
					af = 0;
				}

				if(v_size * 6 < vb_size){
					vf = -1;
				}else if(v_size * 6 > vb_size * 5){
					vf = 1;
				}else{
					vf = 0;
				}

				if(af > 0 || vf > 0){
					resample = 1;
				}else if(af < 0 || vf < 0){
					resample = 2;
				}else{
					resample = 0;
				}

				if(resample != 1){
					char buf[32], dmx_buf[32];

					if(AM_FileRead(AUDIO_PTS_FILE, buf, sizeof(buf))>=0 &&
							AM_FileRead(AUDIO_DMX_PTS_FILE, dmx_buf, sizeof(dmx_buf))>=0){
						sscanf(buf+2, "%x", &apts);
						sscanf(dmx_buf, "%d", &dmx_apts);
					}

					if(AM_FileRead(VIDEO_PTS_FILE, buf, sizeof(buf))>=0 &&
							AM_FileRead(VIDEO_DMX_PTS_FILE, dmx_buf, sizeof(dmx_buf))>=0){
						sscanf(buf+2, "%x", &vpts);
						sscanf(dmx_buf, "%d", &dmx_vpts);
					}
 #ifdef ENABLE_CORRECR_AV_SYNC   
                                        if(dev->ts_player.play_para.vpid < 0x1fff) {//gurantee it only effect for video                                    
                                         if(AM_FileRead(DI_BYPASS_FILE, buf, sizeof(buf))==AM_SUCCESS)
                                         {     
                                            if(sscanf(buf, "%d", &bypass_hd)==1)
                                            {
                                                AM_DEBUG(1, " Get DI bypass_hd status  ========= di bypass_hd is :%d",bypass_hd);
                                            }else
                                               bypass_hd = 0; 
                                         } 
                                        }
#endif

					if(((dmx_apts - apts) > 90000 * 2) || ((dmx_vpts - vpts) > 90000 * 2)){
						resample = 1;
#ifdef ENABLE_CORRECR_AV_SYNC            
                                        if(dev->ts_player.play_para.vpid < 0x1fff) {//gurantee it only effect for video                              
                                          if( ((dmx_vpts - vpts) > 90000 *4 ) && bypass_hd == 0 && v_size > 0 ) {
                                             AM_DEBUG(1, "HD near to lost AV SYNC=========set di bypass_hd to 1");	
                                             need_skip_DI_4_HD = 1;     
                                             AM_FileEcho(DI_BYPASS_FILE,"1");
                                           }
                                        }
#endif
					}
#ifdef ENABLE_CORRECR_AV_SYNC
                                       if(dev->ts_player.play_para.vpid < 0x1fff) {//gurantee it only effect for video
                                        if( need_skip_DI_4_HD ==1 && abs((dmx_apts-apts) - (dmx_vpts-vpts)) <=  27000 ){
			                    if(bypass_hd) //bypass_hd already set as bypass mode and need recover to di enable
                                            {
                                                 AM_DEBUG(1, "AV SYNC recover =========set di bypass_hd to 0");
                                                 AM_FileEcho(DI_BYPASS_FILE,"0");
                                                 need_skip_DI_4_HD = 0;
                                           }
                                        }  
                                      }
#endif
				}
				AM_DEBUG(1, "asize %d adata %d vsize %d vdata %d apts_diff %d vpts_diff %d resample %d",
					ab_size, a_size, vb_size, v_size, dmx_apts-apts, dmx_vpts-vpts, resample);

				if(resample != curr_resample){
					int set = 1;
					
					if(curr_resample){
						resample = 0;
						AM_TIME_GetClock(&resample_change_ms);
					}else{
						int now;

						AM_TIME_GetClock(&now);
						if((now - resample_change_ms) < 500){
							set = 0;
						}
					}

					if(set){
						const char *cmd;
						curr_resample = resample;
						switch(resample){
							case 1: cmd = "1"; break;
							case 2: cmd = "2"; break;
							default: cmd = "0"; break;
						}
						AM_FileEcho(ENABLE_RESAMPLE_FILE, resample?"1":"0");
						AM_FileEcho(RESAMPLE_TYPE_FILE, cmd);
					}
				}
			}
#endif /*ENABLE_AUDIO_RESAMPLE*/
			AM_TIME_GetClock(&pts_time);
			if ((pts_time - last_pts_time) >= 1000)
			{
				last_pts_time = pts_time;
				
				/* Audio 或者 Video PID 有效，需要检查是否reset av */
				if (dev->ts_player.play_para.vpid < 0x1fff || dev->ts_player.play_para.apid < 0x1fff)
				{
					dec_vpts = aml_get_pts_video();
					dec_apts = aml_get_pts_audio();
					AM_DEBUG(7, "adec_start %d, last_dec_apts %x, dec_apts %x", adec_start, last_dec_apts, dec_apts);
					/* Audio PID有效并且Audio pts有变化，认为audio 开始解码*/
					if (dev->ts_player.play_para.apid < 0x1fff && 
						!adec_start && last_dec_apts && 
						dec_apts != last_dec_apts)
					{
						AM_DEBUG(1, "@@ audio pts changed from 0x%x->0x%x @@", last_dec_apts, dec_apts);
						adec_start = AM_TRUE;
					}
					/* Video PID有效并且 Video pts有变化，认为video 开始解码*/
					if (dev->ts_player.play_para.vpid < 0x1fff && 
						!vdec_start && last_dec_vpts && 
						dec_vpts != last_dec_vpts)
					{
						AM_DEBUG(1, "@@ video pts changed from 0x%x->0x%x @@", last_dec_vpts, dec_vpts);
						vdec_start = AM_TRUE;
					}
				
					/* Audio和Video都已开始解码，或者只有单路流有效时且已开始解码, 检查是否需要reset av播放 */
					if ((dev->ts_player.play_para.vpid >= 0x1fff || vdec_start) &&
						(dev->ts_player.play_para.apid >= 0x1fff || adec_start))
					{
						if (vdec_start)
						{
							if (dec_vpts == last_dec_vpts)
							{
								vpts_repeat_count++;
								AM_DEBUG(1, "@@ Video PTS has not changed for %d times @@", vpts_repeat_count);
								if (vpts_repeat_count >= 3)
								{
									AM_DEBUG(1, "@@ Need to restart Audio & Video @@");
									aml_close_ts_mode(dev, AM_FALSE);
									aml_open_mode(dev, AV_PLAY_TS);
									aml_start_ts_mode(dev, &dev->ts_player.play_para, AM_FALSE);
									av_playing = AM_FALSE;			
						
									continue;
								}
							}
							else if (vpts_repeat_count != 0)
							{
								vpts_repeat_count = 0;
							}
						}
						
						if (adec_start)
						{
							if (dec_apts == last_dec_apts)
							{
								apts_repeat_count++;
								AM_DEBUG(1, "@@ Audio PTS has not changed for %d times @@", apts_repeat_count);
								if (apts_repeat_count >= 3)
								{
									AM_DEBUG(1, "@@ Need to restart Audio & Video @@");
									aml_close_ts_mode(dev, AM_FALSE);
									aml_open_mode(dev, AV_PLAY_TS);
									aml_start_ts_mode(dev, &dev->ts_player.play_para, AM_FALSE);
									av_playing = AM_FALSE;			
						
									continue;
								}
							}
							else if (apts_repeat_count != 0)
							{
								apts_repeat_count = 0;
							}
						}
						
					}
					
					last_dec_vpts = dec_vpts;
					last_dec_apts = dec_apts;
				}
			
				if (av_playing && aml_get_pts(AUDIO_DMX_PTS_FILE, &apts) == AM_SUCCESS && 
					aml_get_pts(VIDEO_DMX_PTS_FILE, &vpts) == AM_SUCCESS)
				{
					AM_DEBUG(6, "apts %u->%u, vpts %u->%u", last_apts, apts, last_vpts, vpts);
					if (apts == last_apts && vpts == last_vpts)
					{
						if (! notified)
						{
							pts_repeat_count++;
							AM_DEBUG(1, "Audio & Video PTS have not changed for %d times", pts_repeat_count);
							if (pts_repeat_count>= 2)
							{
								/*PTS 3秒内无变化，重新播放音视频*/
								aml_close_ts_mode(dev, AM_FALSE);
								aml_open_mode(dev, AV_PLAY_TS);
								aml_start_ts_mode(dev, &dev->ts_player.play_para, AM_FALSE);
								av_playing = AM_FALSE;

								AM_DEBUG(1, "Notify Audio & Video no data event");
								AM_DMX_GetScrambleStatus(dev->dev_no, avStatus);

								if (avStatus[0] == AM_TRUE)
								{
									AM_EVT_Signal(dev->dev_no, AM_AV_EVT_VIDEO_SCAMBLED, NULL);
								}
								else if (avStatus[1] == AM_TRUE)
								{
									AM_EVT_Signal(dev->dev_no, AM_AV_EVT_AUDIO_SCAMBLED, NULL);
								}
								else
								{
									AM_EVT_Signal(dev->dev_no, AM_AV_EVT_AV_NO_DATA, NULL);
								}
								pts_repeat_count = 0;
								notified = AM_TRUE;
							}
						}
					}
					else if (notified)
					{
						AM_DEBUG(1, "Audio or Video PTS changed, notify detect data event");
						AM_EVT_Signal(dev->dev_no, AM_AV_EVT_AV_DATA_RESUME, NULL);
						pts_repeat_count = 0;
						notified = AM_FALSE;
					}
					else if (!notified && pts_repeat_count)
					{
						/*reset the count*/
						pts_repeat_count = 0;
					}
					last_apts = apts;
					last_vpts = vpts;
				}
			}
		}
	}
	/*If notified, send resume event*/
	if (notified)
	{
		AM_DEBUG(1, "About to exit av monitor thread, notify detect data event");
		AM_EVT_Signal(dev->dev_no, AM_AV_EVT_AV_DATA_RESUME, NULL);
	}
	pthread_mutex_unlock(&gAVMonLock);

#ifdef ENABLE_AUDIO_RESAMPLE
	AM_FileEcho(ENABLE_RESAMPLE_FILE, "0");
#endif
#ifdef ENABLE_CORRECR_AV_SYNC
       if( need_skip_DI_4_HD)
       { 
         // if(bypass_hd) //bypass_hd already set as bypass mode and need recover to di enable
          {
                  AM_DEBUG(1, " recover previous setting ========set di bypass_hd to 0");
                  AM_FileEcho(DI_BYPASS_FILE,"0");
                  bypass_hd = 0;
          }

          need_skip_DI_4_HD = 0;
       }
#endif

	AM_DEBUG(1, "AV  monitor thread exit now");
	return NULL;
}

static AM_ErrorCode_t aml_open_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode)
{
	AV_DataSource_t *src;
	AV_FilePlayerData_t *data;
	AV_JPEGData_t *jpeg;
	AV_InjectData_t *inj;
	AV_TimeshiftData_t *tshift;
	int fd;
	
	switch(mode)
	{
		case AV_PLAY_VIDEO_ES:
			src = aml_create_data_source(STREAM_VBUF_FILE, dev->dev_no, AM_FALSE);
			if(!src)
			{
				AM_DEBUG(1, "cannot create data source \"/dev/amstream_vbuf\"");
				return AM_AV_ERR_CANNOT_OPEN_DEV;
			}

			fd = open(AMVIDEO_FILE, O_RDWR);
			if(fd ==-1)
			{
				AM_DEBUG(1, "cannot create data source \"/dev/amvideo\"");
				return AM_AV_ERR_CANNOT_OPEN_DEV;
			}
			ioctl(fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_I);
			src->video_fd = fd;
			dev->vid_player.drv_data = src;

		break;
		case AV_PLAY_AUDIO_ES:
			src = aml_create_data_source(STREAM_ABUF_FILE, dev->dev_no, AM_TRUE);
			if(!src)
			{
				AM_DEBUG(1, "cannot create data source \"/dev/amstream_abuf\"");
				return AM_AV_ERR_CANNOT_OPEN_DEV;
			}
			dev->aud_player.drv_data = src;
		break;
		case AV_PLAY_TS:
			fd = open(STREAM_TS_FILE, O_RDWR);
			if(fd==-1)
			{
				AM_DEBUG(1, "cannot open \"/dev/amstream_mpts\" error:%d \"%s\"", errno, strerror(errno));
				return AM_AV_ERR_CANNOT_OPEN_DEV;
			}
			dev->ts_player.drv_data = (void*)fd;
		break;
		case AV_PLAY_FILE:
			data = aml_create_fp(dev);
			if(!data)
			{
				AM_DEBUG(1, "not enough memory");
				return AM_AV_ERR_NO_MEM;
			}
			dev->file_player.drv_data = data;
		break;
		case AV_GET_JPEG_INFO:
		case AV_DECODE_JPEG:
			jpeg = aml_create_jpeg_data();
			if(!jpeg)
			{
				AM_DEBUG(1, "not enough memory");
				return AM_AV_ERR_NO_MEM;
			}
			dev->vid_player.drv_data = jpeg;
		break;
		case AV_INJECT:
			inj = aml_create_inject_data();
			if(!inj)
			{
				AM_DEBUG(1, "not enough memory");
				return AM_AV_ERR_NO_MEM;
			}
			dev->inject_player.drv_data = inj;
		break;
		case AV_TIMESHIFT:
			tshift = aml_create_timeshift_data();
			if(!tshift)
			{
				AM_DEBUG(1, "not enough memory");
				return AM_AV_ERR_NO_MEM;
			}
			tshift->dev = dev;
			dev->timeshift_player.drv_data = tshift;
		break;
		default:
		break;
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_start_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode, void *para)
{
	AM_AV_InjectPara_t *inj_p;
	AV_DataSource_t *src;
	int fd, val;
	AV_TSPlayPara_t *tp;
	AV_FilePlayerData_t *data;
	AV_FilePlayPara_t *pp;
	AV_JPEGData_t *jpeg;
	AV_InjectData_t *inj;
	AM_AV_TimeshiftPara_t *tshift_p;
	AV_TimeshiftData_t *tshift;

	switch(mode)
	{
		case AV_PLAY_VIDEO_ES:
			src = dev->vid_player.drv_data;
			val = (int)para;
			if(ioctl(src->fd, AMSTREAM_IOC_VFORMAT, val)==-1)
			{
				AM_DEBUG(1, "set video format failed");
				return AM_AV_ERR_SYS;
			}
			aml_start_data_source(src, dev->vid_player.para.data, dev->vid_player.para.len, dev->vid_player.para.times+1);
		break;
		case AV_PLAY_AUDIO_ES:
			src = dev->aud_player.drv_data;
			val = (int)para;
			if(ioctl(src->fd, AMSTREAM_IOC_AFORMAT, val)==-1)
			{
				AM_DEBUG(1, "set audio format failed");
				return AM_AV_ERR_SYS;
			}
			aml_start_data_source(src, dev->aud_player.para.data, dev->aud_player.para.len, dev->aud_player.para.times);
#ifndef ADEC_API_NEW
			adec_cmd("start");
#else
			{
				codec_para_t cp;
				memset(&cp, 0, sizeof(cp));

				audio_decode_init(&adec_handle, &cp);
			}
//#ifdef CHIP_8626X
			audio_set_av_sync_threshold(adec_handle, AV_SYNC_THRESHOLD);
//#endif

			audio_decode_start(adec_handle);
#endif
			AM_AOUT_SetDriver(AOUT_DEV_NO, &adec_aout_drv, NULL);
		break;
		case AV_PLAY_TS:
			tp = (AV_TSPlayPara_t *)para;
#if defined(ANDROID) || defined(CHIP_8626X)
#ifdef ENABLE_PCR_RECOVER
			AM_FileEcho(PCR_RECOVER_FILE, "1");
#endif
#endif
			AM_TRY(aml_start_ts_mode(dev, tp, AM_TRUE));
		break;
		case AV_PLAY_FILE:
#ifdef  MEDIA_PLAYER

			data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
			pp = (AV_FilePlayPara_t*)para;
			
			if(pp->start)
				data->media_id = MP_PlayMediaSource(pp->name, pp->loop, 0);
			else
				data->media_id = MP_AddMediaSource(pp->name);
			
			if(data->media_id==-1)
			{
				AM_DEBUG(1, "play file failed");
				return AM_AV_ERR_SYS;
			}
			
			AM_AOUT_SetDriver(AOUT_DEV_NO, &amplayer_aout_drv, (void*)data->media_id);
#elif defined PLAYER_API_NEW
			data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
			pp = (AV_FilePlayPara_t*)para;
			
			if(pp->start)
			{
				 memset((void*)&data->pctl,0,sizeof(play_control_t)); 
    
				player_register_update_callback(&data->pctl.callback_fn, aml_update_player_info_callback, PLAYER_INFO_POP_INTERVAL);

				data->pctl.file_name = strndup(pp->name,FILENAME_LENGTH_MAX);
				data->pctl.video_index = -1;
				data->pctl.audio_index = -1;
				data->pctl.hassub = 1; 
				
				/*data->pctl.t_pos = st;*/
				aml_set_tsync_enable(1);
				
				if(pp->loop)
				{
					data->pctl.loop_mode =1;
				}
				data->pctl.need_start = 1;
				data->pctl.is_variable = 1;
				
				data->media_id = player_start(&data->pctl,0);

				if(data->media_id<0)
				{
					AM_DEBUG(1, "play file failed");
					return AM_AV_ERR_SYS;
				}

				player_start_play(data->media_id);
				AM_AOUT_SetDriver(AOUT_DEV_NO, &amplayer_aout_drv, (void*)data->media_id);
			}
#endif
		break;
		case AV_GET_JPEG_INFO:
		case AV_DECODE_JPEG:
			jpeg = dev->vid_player.drv_data;
			return aml_decode_jpeg(jpeg, dev->vid_player.para.data, dev->vid_player.para.len, mode, para);
		break;
		case AV_INJECT:
			inj_p = (AM_AV_InjectPara_t*)para;
			inj = dev->inject_player.drv_data;
			if(aml_start_inject(inj, inj_p)!=AM_SUCCESS)
				return AM_AV_ERR_SYS;
		break;
		case AV_TIMESHIFT:
			tshift_p = (AM_AV_TimeshiftPara_t*)para;
			tshift = dev->timeshift_player.drv_data;
			if(aml_start_timeshift(tshift, tshift_p, AM_TRUE, AM_TRUE)!=AM_SUCCESS)
				return AM_AV_ERR_SYS;
		break;
		default:
		break;
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_close_mode(AM_AV_Device_t *dev, AV_PlayMode_t mode)
{
	AV_DataSource_t *src;
	AV_FilePlayerData_t *data;
	AV_JPEGData_t *jpeg;
	AV_InjectData_t *inj;
	AV_TimeshiftData_t *tshift;
	int fd, ret;
	
	switch(mode)
	{
		case AV_PLAY_VIDEO_ES:
			src = dev->vid_player.drv_data;
			ioctl(src->video_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
			close(src->video_fd);
			aml_destroy_data_source(src);
		break;
		case AV_PLAY_AUDIO_ES:
			src = dev->aud_player.drv_data;
#ifndef ADEC_API_NEW
			adec_cmd("stop");
#else
			audio_decode_stop(adec_handle);
			audio_decode_release(&adec_handle);
			adec_handle = NULL;
#endif
			aml_destroy_data_source(src);
		break;
		case AV_PLAY_TS:
#if defined(ANDROID) || defined(CHIP_8626X)
#ifdef ENABLE_PCR_RECOVER
			AM_FileEcho(PCR_RECOVER_FILE, "0");
#endif
#endif
		ret = aml_close_ts_mode(dev, AM_TRUE);
		break;
		case AV_PLAY_FILE:
			data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
			aml_destroy_fp(data);
			adec_cmd("stop");
		break;
		case AV_GET_JPEG_INFO:
		case AV_DECODE_JPEG:
			jpeg = dev->vid_player.drv_data;
			aml_destroy_jpeg_data(jpeg);
		break;
		case AV_INJECT:
			inj = dev->inject_player.drv_data;
			aml_destroy_inject_data(inj);
		break;
		case AV_TIMESHIFT:
			tshift = dev->timeshift_player.drv_data;
			aml_destroy_timeshift_data(tshift, AM_TRUE);
			dev->timeshift_player.drv_data = NULL;
		break;
		default:
		break;
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_ts_source(AM_AV_Device_t *dev, AM_AV_TSSource_t src)
{
	char *cmd;
	
	switch(src)
	{
		case AM_AV_TS_SRC_TS0:
			cmd = "ts0";
		break;
		case AM_AV_TS_SRC_TS1:
			cmd = "ts1";
		break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
		case AM_AV_TS_SRC_TS2:
			cmd = "ts2";
		break;
#endif
		case AM_AV_TS_SRC_HIU:
			cmd = "hiu";
		break;
		case AM_AV_TS_SRC_DMX0:
			cmd = "dmx0";
		break;
		case AM_AV_TS_SRC_DMX1:
			cmd = "dmx1";
		break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
		case AM_AV_TS_SRC_DMX2:
			cmd = "dmx2";
		break;
#endif
		default:
			AM_DEBUG(1, "illegal ts source %d", src);
			return AM_AV_ERR_NOT_SUPPORTED;
		break;
	}
	
	return AM_FileEcho("/sys/class/stb/source", cmd);
}

static AM_ErrorCode_t aml_file_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *para)
{
	AV_FilePlayerData_t *data;
	AV_FileSeekPara_t *sp;
	int rc = -1;
	
	data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
	
#ifdef  MEDIA_PLAYER
	if(data->media_id==-1)
	{
		AM_DEBUG(1, "player do not start");
		return AM_AV_ERR_SYS;
	}
	
	switch(cmd)
	{
		case AV_PLAY_START:
			rc = MP_start(data->media_id);
		break;
		case AV_PLAY_PAUSE:
			rc = MP_pause(data->media_id);
		break;
		case AV_PLAY_RESUME:
			rc = MP_resume(data->media_id);
		break;
		case AV_PLAY_FF:
			rc = MP_fastforward(data->media_id, (int)para);
		break;
		case AV_PLAY_FB:
			rc = MP_rewind(data->media_id, (int)para);
		break;
		case AV_PLAY_SEEK:
			sp = (AV_FileSeekPara_t*)para;
			rc = MP_seek(data->media_id, sp->pos, sp->start);
		break;
		default:
			AM_DEBUG(1, "illegal media player command");
			return AM_AV_ERR_NOT_SUPPORTED;
		break;
	}
	
	if(rc==-1)
	{
		AM_DEBUG(1, "player operation failed");
		return AM_AV_ERR_SYS;
	}
#elif defined PLAYER_API_NEW
	if(data->media_id<0)
	{
		AM_DEBUG(1, "player do not start");
		return AM_AV_ERR_SYS;
	}
	
	switch(cmd)
	{
		case AV_PLAY_START:
			player_start_play(data->media_id);
		break;
		case AV_PLAY_PAUSE:
			player_pause(data->media_id);
		break;
		case AV_PLAY_RESUME:
			player_resume(data->media_id);
		break;
		case AV_PLAY_FF:
			player_forward(data->media_id, (int)para);
		break;
		case AV_PLAY_FB:
			player_backward(data->media_id, (int)para);
		break;
		case AV_PLAY_SEEK:
			sp = (AV_FileSeekPara_t*)para;
			 player_timesearch(data->media_id, sp->pos);
		break;
		default:
			AM_DEBUG(1, "illegal media player command");
			return AM_AV_ERR_NOT_SUPPORTED;
		break;
	}
#endif	

	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_inject_cmd(AM_AV_Device_t *dev, AV_PlayCmd_t cmd, void *para)
{
	AV_InjectData_t *data;
	
	data = (AV_InjectData_t*)dev->inject_player.drv_data;
	
	switch(cmd)
	{
		case AV_PLAY_PAUSE:
			if(data->aud_fd!=-1)
			{
				adec_cmd("stop");
			}
		break;
		case AV_PLAY_RESUME:
			if(data->aud_fd!=-1)
			{
				adec_cmd("start");
			}
		break;
		default:
			AM_DEBUG(1, "illegal media player command");
			return AM_AV_ERR_NOT_SUPPORTED;
		break;
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_file_status(AM_AV_Device_t *dev, AM_AV_PlayStatus_t *status)
{
	AV_FilePlayerData_t *data;
	int rc;
	
	data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
	if(data->media_id==-1)
	{
		AM_DEBUG(1, "player do not start");
		return AM_AV_ERR_SYS;
	}

#ifdef  MEDIA_PLAYER
	/*Get duration*/
	pthread_mutex_lock(&data->lock);
	
	data->resp_type = AV_MP_RESP_DURATION;
	rc = MP_GetDuration(data->media_id);
	if(rc==0)
	{
		while(data->resp_type==AV_MP_RESP_DURATION)
			pthread_cond_wait(&data->cond, &data->lock);
	}
	
	status->duration = data->resp_data.duration;
	
	pthread_mutex_unlock(&data->lock);
	
	if(rc!=0)
	{
		AM_DEBUG(1, "get duration failed");
		return AM_AV_ERR_SYS;
	}
	
	/*Get position*/
	pthread_mutex_lock(&data->lock);
	
	data->resp_type = AV_MP_RESP_POSITION;
	rc = MP_GetPosition(data->media_id);
	if(rc==0)
	{
		while(data->resp_type==AV_MP_RESP_POSITION)
			pthread_cond_wait(&data->cond, &data->lock);
	}
	
	status->position = data->resp_data.position;
	
	pthread_mutex_unlock(&data->lock);
	
	if(rc!=0)
	{
		AM_DEBUG(1, "get position failed");
		return AM_AV_ERR_SYS;
	}
#elif defined PLAYER_API_NEW
	 player_info_t pinfo;
	 
	if(player_get_play_info(data->media_id,&pinfo)<0)
	{
		AM_DEBUG(1, "player_get_play_info failed");
		return AM_AV_ERR_SYS;
	}
	status->duration = pinfo.full_time;
	status->position = pinfo.current_time;
#endif	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_file_info(AM_AV_Device_t *dev, AM_AV_FileInfo_t *info)
{
	AV_FilePlayerData_t *data;
	int rc;
	
	data = (AV_FilePlayerData_t*)dev->file_player.drv_data;
	if(data->media_id==-1)
	{
		AM_DEBUG(1, "player do not start");
		return AM_AV_ERR_SYS;
	}
#ifdef  MEDIA_PLAYER	
	pthread_mutex_lock(&data->lock);
	
	data->resp_type = AV_MP_RESP_MEDIA;
	rc = MP_GetMediaInfo(dev->file_player.name, data->media_id);
	if(rc==0)
	{
		while(data->resp_type==AV_MP_RESP_MEDIA)
			pthread_cond_wait(&data->cond, &data->lock);
		
		info->size     = data->resp_data.media.size;
		info->duration = data->resp_data.media.duration;
	}
	
	pthread_mutex_unlock(&data->lock);
	
	if(rc!=0)
	{
		AM_DEBUG(1, "get media information failed");
		return AM_AV_ERR_SYS;
	}
#elif defined PLAYER_API_NEW
	 media_info_t minfo;
	 
	if(player_get_media_info(data->media_id,&minfo)<0)
	{
		AM_DEBUG(1, "player_get_media_info failed");
		return AM_AV_ERR_SYS;
	}
	info->duration = minfo.stream_info.duration;
	info->size = minfo.stream_info.file_size;
#endif	

	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_set_video_para(AM_AV_Device_t *dev, AV_VideoParaType_t para, void *val)
{
	const char *name = NULL, *cmd = "";
	char buf[32];
	AM_ErrorCode_t ret = AM_SUCCESS;
	AV_VideoWindow_t *win;
	
	switch(para)
	{
		case AV_VIDEO_PARA_WINDOW:
			name = VID_AXIS_FILE;
			win = (AV_VideoWindow_t*)val;
			snprintf(buf, sizeof(buf), "%d %d %d %d", win->x, win->y, win->x+win->w, win->y+win->h);
			cmd = buf;
		break;
		case AV_VIDEO_PARA_CONTRAST:
			name = VID_CONTRAST_FILE;
			snprintf(buf, sizeof(buf), "%d", (int)val);
			cmd = buf;
		break;
		case AV_VIDEO_PARA_SATURATION:
			name = VID_SATURATION_FILE;
			snprintf(buf, sizeof(buf), "%d", (int)val);
			cmd = buf;
		break;
		case AV_VIDEO_PARA_BRIGHTNESS:
			name = VID_BRIGHTNESS_FILE;
			snprintf(buf, sizeof(buf), "%d", (int)val);
			cmd = buf;
		break;
		case AV_VIDEO_PARA_ENABLE:
			name = VID_DISABLE_FILE;
			cmd = ((int)val)?"0":"1";
		break;
		case AV_VIDEO_PARA_BLACKOUT:
			name = VID_BLACKOUT_FILE;
			cmd = ((int)val)?"1":"0";
#if 0
#ifdef AMSTREAM_IOC_CLEAR_VBUF
			if((int)val)
			{
				int fd = open(AMVIDEO_FILE, O_RDWR);
				if(fd!=-1)
				{
					ioctl(fd, AMSTREAM_IOC_CLEAR_VBUF, 0);
					close(fd);
				}
			}
#endif
#endif	
		break;
		case AV_VIDEO_PARA_RATIO:
#ifndef 	CHIP_8226H
			name = VID_SCREEN_MODE_FILE;
			switch((int)val)
			{
				case AM_AV_VIDEO_ASPECT_AUTO:
					cmd = "0";
				break;
				case AM_AV_VIDEO_ASPECT_16_9:
					cmd = "3";
				break;
				case AM_AV_VIDEO_ASPECT_4_3:
					cmd = "2";
				break;
			}
#else			
			name = VID_ASPECT_RATIO_FILE;
			switch((int)val)
			{
				case AM_AV_VIDEO_ASPECT_AUTO:
					cmd = "auto";
				break;
				case AM_AV_VIDEO_ASPECT_16_9:
					cmd = "16x9";
				break;
				case AM_AV_VIDEO_ASPECT_4_3:
					cmd = "4x3";
				break;
			}
 #endif
		break;
		case AV_VIDEO_PARA_RATIO_MATCH:
			name = VID_ASPECT_MATCH_FILE;
#ifndef 	CHIP_8226H		
			switch((int)val)
			{
				case AM_AV_VIDEO_ASPECT_MATCH_IGNORE:
					cmd = "1";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_LETTER_BOX:
					cmd = "3";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_PAN_SCAN:
					cmd = "2";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_COMBINED:
					cmd = "4";
				break;
			}
#else			
			switch((int)val)
			{
				case AM_AV_VIDEO_ASPECT_MATCH_IGNORE:
					cmd = "ignore";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_LETTER_BOX:
					cmd = "letter box";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_PAN_SCAN:
					cmd = "pan scan";
				break;
				case AM_AV_VIDEO_ASPECT_MATCH_COMBINED:
					cmd = "combined";
				break;
			}
#endif			
		break;
		case AV_VIDEO_PARA_MODE:
			name = VID_SCREEN_MODE_FILE;
			cmd = ((AM_AV_VideoDisplayMode_t)val)?"1":"0";
		break;
		case AV_VIDEO_PARA_CLEAR_VBUF:
#if 0
#ifdef AMSTREAM_IOC_CLEAR_VBUF
			{
				int fd = open(AMVIDEO_FILE, O_RDWR);
				if(fd!=-1)
				{
					ioctl(fd, AMSTREAM_IOC_CLEAR_VBUF, 0);
					close(fd);
				}
			}
#endif
#endif
			name = VID_DISABLE_FILE;
			cmd = "2";
		break;
	}
	
	if(name)
	{
		ret = AM_FileEcho(name, cmd);
	}
	
	return ret;
}

static AM_ErrorCode_t aml_inject(AM_AV_Device_t *dev, AM_AV_InjectType_t type, uint8_t *data, int *size, int timeout)
{
	AV_InjectData_t *inj = (AV_InjectData_t*)dev->inject_player.drv_data;
	int fd, send, ret;
	
	if((inj->pkg_fmt==PFORMAT_ES) && (type==AM_AV_INJECT_AUDIO))
		fd = inj->aud_fd;
	else
		fd = inj->vid_fd;
	
	if(fd==-1)
	{
		AM_DEBUG(1, "device is not openned");
		return AM_AV_ERR_NOT_ALLOCATED;
	}
	
	if(timeout>=0)
	{
		struct pollfd pfd;
		
		pfd.fd = fd;
		pfd.events = POLLOUT;
		
		ret = poll(&pfd, 1, timeout);
		if(ret!=1)
			return AM_AV_ERR_TIMEOUT;
	}
	
	send = *size;
	if(send)
	{
		ret = write(fd, data, send);
		if((ret==-1) && (errno!=EAGAIN))
		{
			AM_DEBUG(1, "inject data failed errno:%d msg:%s", errno, strerror(errno));
			return AM_AV_ERR_SYS;
		}
		else if((ret==-1) && (errno==EAGAIN))
		{
			ret = 0;
		}
	}
	else
		ret = 0;
	
	*size = ret;
	
	return AM_SUCCESS;
}

#ifdef AMSTREAM_IOC_GET_VBUF
extern AM_ErrorCode_t ge2d_blit_yuv(struct vbuf_info *info, AM_OSD_Surface_t *surf);
#endif

static AM_ErrorCode_t aml_video_frame(AM_AV_Device_t *dev, const AM_AV_SurfacePara_t *para, AM_OSD_Surface_t **s)
{
#ifdef AMSTREAM_IOC_GET_VBUF
	int fd = -1, rc;
	struct vbuf_info info;
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_OSD_PixelFormatType_t type;
	AM_OSD_Surface_t *surf = NULL;
	int w, h, append;
	
	fd = open(AMVIDEO_FILE, O_RDWR);
	if(fd==-1)
	{
		AM_DEBUG(1, "cannot open \"%s\"", AMVIDEO_FILE);
		ret = AM_AV_ERR_SYS;
		goto end;
	}
	
	rc = ioctl(fd, AMSTREAM_IOC_GET_VBUF, &info);
	if(rc==-1)
	{
		AM_DEBUG(1, "AMSTREAM_IOC_GET_VBUF_INFO failed");
		ret = AM_AV_ERR_SYS;
		goto end;
	}
	
	type = AM_OSD_FMT_COLOR_RGB_888;
	w = info.width;
	h = info.height;
	
	ret = AM_OSD_CreateSurface(type, w, h, AM_OSD_SURFACE_FL_HW, &surf);
	if(ret!=AM_SUCCESS)
		goto end;
	
	ret = ge2d_blit_yuv(&info, surf);
	if(ret!=AM_SUCCESS)
		goto end;
	
	*s = surf;
	return AM_SUCCESS;
	
end:
	if(surf)
		AM_OSD_DestroySurface(surf);
	if(fd!=-1)
		close(fd);
	return ret;
#else
	return AM_AV_ERR_NOT_SUPPORTED;
#endif
}

static AM_ErrorCode_t aml_get_astatus(AM_AV_Device_t *dev, AM_AV_AudioStatus_t *para)
{
	struct am_io_param astatus;
	int fd, rc;
	char buf[32];

	pthread_mutex_lock(&gAVMonLock);
	fd = get_amstream(dev);
	if(fd==-1)
	{
		AM_DEBUG(1, "cannot get status in this mode");
		goto get_fail;
	}
	
	rc = ioctl(fd, AMSTREAM_IOC_ADECSTAT, (int)&astatus);
	if(rc==-1)
	{
		AM_DEBUG(1, "AMSTREAM_IOC_ADECSTAT failed");
		goto get_fail;
	}
	
	para->channels    = astatus.astatus.channels;
	para->sample_rate = astatus.astatus.sample_rate;
	para->resolution  = astatus.astatus.resolution;
	para->frames      = 1;
	para->aud_fmt     = -1;
	
	if(AM_FileRead(ASTREAM_FORMAT_FILE, buf, sizeof(buf))==AM_SUCCESS)
	{
		if(!strncmp(buf, "amadec_mpeg", 11))
			para->aud_fmt = AFORMAT_MPEG;
		else if(!strncmp(buf, "amadec_pcm_s16le", 16))
			para->aud_fmt = AFORMAT_PCM_S16LE;
		else if(!strncmp(buf, "amadec_aac", 10))
			para->aud_fmt = AFORMAT_AAC;
		else if(!strncmp(buf, "amadec_ac3", 10))
			para->aud_fmt = AFORMAT_AC3;
		else if(!strncmp(buf, "amadec_alaw", 11))
			para->aud_fmt = AFORMAT_ALAW;
		else if(!strncmp(buf, "amadec_mulaw", 12))
			para->aud_fmt = AFORMAT_MULAW;
		else if(!strncmp(buf, "amadec_dts", 10))
			para->aud_fmt = AFORMAT_MULAW;
		else if(!strncmp(buf, "amadec_pcm_s16be", 16))
			para->aud_fmt = AFORMAT_PCM_S16BE;
		else if(!strncmp(buf, "amadec_flac", 11))
			para->aud_fmt = AFORMAT_FLAC;
		else if(!strncmp(buf, "amadec_cook", 11))
			para->aud_fmt = AFORMAT_COOK;
		else if(!strncmp(buf, "amadec_pcm_u8", 13))
			para->aud_fmt = AFORMAT_PCM_U8;
		else if(!strncmp(buf, "amadec_adpcm", 12))
			para->aud_fmt = AFORMAT_ADPCM;
		else if(!strncmp(buf, "amadec_amr", 10))
			para->aud_fmt = AFORMAT_AMR;
		else if(!strncmp(buf, "amadec_raac", 11))
			para->aud_fmt = AFORMAT_RAAC;
		else if(!strncmp(buf, "amadec_wma", 10))
			para->aud_fmt = AFORMAT_WMA;
	}
	
	rc = ioctl(fd, AMSTREAM_IOC_AB_STATUS, (int)&astatus);
	if(rc==-1)
	{
		AM_DEBUG(1, "AMSTREAM_IOC_AB_STATUS failed");
		goto get_fail;	
	}
	
	para->ab_size = astatus.status.size;
	para->ab_data = astatus.status.data_len;
	para->ab_free = astatus.status.free_len;
	
	pthread_mutex_unlock(&gAVMonLock);
	return AM_SUCCESS;

get_fail:
	pthread_mutex_unlock(&gAVMonLock);
	return AM_FAILURE;
}

static AM_ErrorCode_t aml_get_vstatus(AM_AV_Device_t *dev, AM_AV_VideoStatus_t *para)
{
	struct am_io_param vstatus;
	char buf[32];
	int fd, rc;

	pthread_mutex_lock(&gAVMonLock);
	fd = get_amstream(dev);
	if(fd==-1)
	{
		AM_DEBUG(1, "cannot get status in this mode");
		goto get_fail;	
	}
	
	rc = ioctl(fd, AMSTREAM_IOC_VDECSTAT, &vstatus);

	if(rc==-1)
	{
		AM_DEBUG(1, "AMSTREAM_IOC_VDECSTAT failed");
		goto get_fail;	
	}
	
	para->vid_fmt   = dev->ts_player.vid_fmt;
	para->src_w     = vstatus.vstatus.width;
	para->src_h     = vstatus.vstatus.height;
	para->fps       = vstatus.vstatus.fps;
	para->frames    = 1;
	para->interlaced  = 1;

	if(AM_FileRead("/sys/class/video/frame_format", buf, sizeof(buf))>=0){
		char *ptr = strstr(buf, "interlace");
		if(ptr){
			para->interlaced = 1;
		}else{
			para->interlaced = 0;
		}
	}
	
	rc = ioctl(fd, AMSTREAM_IOC_VB_STATUS, (int)&vstatus);
	if(rc==-1)
	{
		AM_DEBUG(1, "AMSTREAM_IOC_VB_STATUS failed");
		goto get_fail;	
	}
	
	para->vb_size = vstatus.status.size;
	para->vb_data = vstatus.status.data_len;
	para->vb_free = vstatus.status.free_len;

	pthread_mutex_unlock(&gAVMonLock);
	return AM_SUCCESS;

get_fail:
	pthread_mutex_unlock(&gAVMonLock);
	return AM_FAILURE;
}

static AM_ErrorCode_t aml_set_ppmgr_3dcmd(int cmd)
{
	int ppmgr_fd;
	int arg = -1, ret;
	
	ppmgr_fd = open(PPMGR_FILE, O_RDWR);
	if (ppmgr_fd < 0) 
	{
		AM_DEBUG(0, "Open /dev/ppmgr error(%s)!\n", strerror(errno));
		return AM_AV_ERR_SYS;
	}

    switch (cmd) 
    {
    case AM_AV_PPMGR_MODE3D_DISABLE:
        arg = MODE_3D_DISABLE;
        AM_DEBUG(1,"3D fucntion (0: Disalbe!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_AUTO:
        arg = MODE_3D_ENABLE|MODE_AUTO;
        AM_DEBUG(1,"3D fucntion (1: Auto!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_2D_TO_3D:
        arg = MODE_3D_ENABLE|MODE_2D_TO_3D;
        AM_DEBUG(1,"3D fucntion (2: 2D->3D!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_LR:
        arg = MODE_3D_ENABLE|MODE_LR;
        AM_DEBUG(1,"3D fucntion (3: L/R!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_BT:
        arg = MODE_3D_ENABLE|MODE_BT;
        AM_DEBUG(1,"3D fucntion (4: B/T!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_OFF_LR_SWITCH:
        arg = MODE_3D_ENABLE|MODE_LR;
        AM_DEBUG(1,"3D fucntion (5: LR SWITCH OFF!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_ON_LR_SWITCH:
        arg = MODE_3D_ENABLE|MODE_LR_SWITCH;
        AM_DEBUG(1,"3D fucntion (6: LR SWITCH!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_FIELD_DEPTH:
        arg = MODE_3D_ENABLE|MODE_FIELD_DEPTH;
        AM_DEBUG(1,"3D function (7: FIELD_DEPTH!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_OFF_3D_TO_2D:
        arg = MODE_3D_ENABLE|MODE_LR;
        AM_DEBUG(1,"3D fucntion (8: 3D_TO_2D_TURN_OFF!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_L_3D_TO_2D:
        arg = MODE_3D_ENABLE|MODE_3D_TO_2D_L;
        AM_DEBUG(1,"3D function (9: 3D_TO_2D_L!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_R_3D_TO_2D:
        arg = MODE_3D_ENABLE|MODE_3D_TO_2D_R;
        AM_DEBUG(1,"3D function (10: 3D_TO_2D_R!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_OFF_LR_SWITCH_BT:
        arg = MODE_3D_ENABLE|MODE_BT|BT_FORMAT_INDICATOR;
        AM_DEBUG(1,"3D function (11: BT SWITCH OFF!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_ON_LR_SWITCH_BT:
        arg = MODE_3D_ENABLE|MODE_LR_SWITCH|BT_FORMAT_INDICATOR;
        AM_DEBUG(1,"3D fucntion (12: BT SWITCH!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_OFF_3D_TO_2D_BT:
        arg = MODE_3D_ENABLE|MODE_BT;
        AM_DEBUG(1,"3D fucntion (13: 3D_TO_2D_TURN_OFF_BT!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_L_3D_TO_2D_BT:
        arg = MODE_3D_ENABLE|MODE_3D_TO_2D_L|BT_FORMAT_INDICATOR;
        AM_DEBUG(1,"3D function (14: 3D TO 2D L BT!)\n");
        break;
    case AM_AV_PPMGR_MODE3D_R_3D_TO_2D_BT:
        arg = MODE_3D_ENABLE|MODE_3D_TO_2D_R|BT_FORMAT_INDICATOR;
        AM_DEBUG(1,"3D function (15: 3D TO 2D R BT!)\n");
        break;
    default:
    	AM_DEBUG(1, "Unkown set 3D cmd %d", cmd);
    	arg = -1;
    	break;
    }
    
    if (arg != -1)
    {
    	if (ioctl(ppmgr_fd, PPMGR_IOC_ENABLE_PP, arg) == -1)
    	{
    		AM_DEBUG(1, "Set 3D function failed: %s", strerror(errno));
    		close(ppmgr_fd);
    		return AM_AV_ERR_SYS;
    	}
    }

	close(ppmgr_fd);
	
    return AM_SUCCESS;
}

static int
get_osd_prop(const char *mode, const char *p, const char *defv)
{
	char n[32];
	char v[32];
	int r;

	snprintf(n, sizeof(n), "ubootenv.var.%soutput%s", mode, p);
	property_get(n,v,defv);
	sscanf(v, "%d", &r);

	return r;
}

static void
get_osd_rect(const char *mode, int *x, int *y, int *w, int *h)
{
	const char *m = mode?mode:"720p";
	char defw[16], defh[16];
	int r;

	r = get_osd_prop(m, "x", "0");
	*x = r;
	r = get_osd_prop(m, "y", "0");
	*y = r;
	if(!strncmp(m, "480", 3)){
		snprintf(defw, sizeof(defw), "%d", 720);
		snprintf(defh, sizeof(defh), "%d", 480);
	}else if(!strncmp(m, "576", 3)){
		snprintf(defw, sizeof(defw), "%d", 720);
		snprintf(defh, sizeof(defh), "%d", 576);
	}else if(!strncmp(m, "720", 3)){
		snprintf(defw, sizeof(defw), "%d", 1280);
		snprintf(defh, sizeof(defh), "%d", 720);
	}else if(!strncmp(m, "1080", 4)){
		snprintf(defw, sizeof(defw), "%d", 1920);
		snprintf(defh, sizeof(defh), "%d", 1080);
	}


	r = get_osd_prop(m, "width", defw);
	*w = r;
	r = get_osd_prop(m, "height", defh);
	*h = r;
}

static AM_ErrorCode_t
aml_set_vpath(AM_AV_Device_t *dev)
{
	AM_ErrorCode_t ret;
	int times = 10;

	AM_DEBUG(1, "set video path fs:%d di:%d ppmgr:%d", dev->vpath_fs, dev->vpath_di, dev->vpath_ppmgr);
	AM_FileEcho("/sys/class/deinterlace/di0/config", "disable");

		
	do{
		ret = AM_FileEcho("/sys/class/vfm/map", "rm default");
		if(ret!=AM_SUCCESS){
			usleep(10000);
		}
	}while(ret!=AM_SUCCESS && times--);

	
	if(dev->vpath_fs==AM_AV_FREE_SCALE_ENABLE){
		char mode[16];
		char ppr[32];
		AM_Bool_t blank = AM_TRUE;
#ifdef ANDROID
		{
			int x, y, w, h;

			AM_FileRead("/sys/class/display/mode", mode, sizeof(mode));
			if(!strncmp(mode, "480i", 4)){
				get_osd_rect("480i", &x, &y, &w, &h);
			}else if(!strncmp(mode, "480p", 4)){
				get_osd_rect("480p", &x, &y, &w, &h);
			}else if(!strncmp(mode, "480cvbs", 7)){
				get_osd_rect("480cvbs", &x, &y, &w, &h);				
			}else if(!strncmp(mode, "576i", 4)){
				get_osd_rect("576i", &x, &y, &w, &h);
			}else if(!strncmp(mode, "576p", 4)){
				get_osd_rect("576p", &x, &y, &w, &h);
			}else if(!strncmp(mode, "576cvbs", 7)){
				get_osd_rect("576cvbs", &x, &y, &w, &h);					
			}else if(!strncmp(mode, "720p", 4)){
				get_osd_rect("720p", &x, &y, &w, &h);
				blank = AM_FALSE;
			}else if(!strncmp(mode, "1080i", 5)){
				get_osd_rect("1080i", &x, &y, &w, &h);
			}else if(!strncmp(mode, "1080p", 5)){
				get_osd_rect("1080p", &x, &y, &w, &h);			
			}else{
				get_osd_rect(NULL, &x, &y, &w, &h);
			}

			snprintf(ppr, sizeof(ppr), "%d %d %d %d 0", x, y, x+w, y+h);
		}
#endif
		AM_FileRead("/sys/class/graphics/fb0/request2XScale", mode, sizeof(mode));
		if (blank && !strncmp(mode, "2", 1)) {
			blank = AM_FALSE;
		}
		if(blank){
			AM_FileEcho("/sys/class/graphics/fb0/blank", "1");
		}
		AM_FileEcho("/sys/class/graphics/fb0/free_scale", "1");
		AM_FileEcho("/sys/class/graphics/fb1/free_scale", "1");
#ifdef ANDROID
		{
			AM_FileEcho("/sys/class/graphics/fb0/request2XScale", "2");
			AM_FileEcho("/sys/class/graphics/fb1/scale", "0");
			AM_FileEcho("/sys/module/amvdec_h264/parameters/dec_control", "0");
			AM_FileEcho("/sys/module/amvdec_mpeg12/parameters/dec_control", "0");
			AM_FileEcho("/sys/class/ppmgr/ppscaler","1");
			AM_FileEcho("/sys/class/ppmgr/ppscaler_rect", ppr);
			AM_FileEcho("/sys/module/amvideo/parameters/smooth_sync_enable", "0");
			usleep(200*1000);
		}
#endif
	}else{
		AM_Bool_t blank = AM_TRUE;
		char m1080scale[8];
		char mode[16];
		char verstr[32];
		char *reqcmd, *osd1axis, *osd1cmd;
		int version;
		AM_Bool_t scale=AM_TRUE;

#ifdef ANDROID
		{
			property_get("ro.platform.has.1080scale",m1080scale,"fail");
			if(!strncmp(m1080scale, "fail", 4)){
				scale = AM_FALSE;
			}

			AM_FileRead("/sys/class/display/mode", mode, sizeof(mode));

			if(strncmp(m1080scale, "2", 1) && (strncmp(m1080scale, "1", 1) || (strncmp(mode, "1080i", 5) && strncmp(mode, "1080p", 5) && strncmp(mode, "720p", 4)))){
				scale = AM_FALSE;
			}

			AM_FileRead("/sys/class/graphics/fb0/request2XScale", verstr, sizeof(verstr));
			if(!strncmp(mode, "480i", 4) || !strncmp(mode, "480p", 4) || !strncmp(mode, "480cvbs", 7)){
				reqcmd   = "16 720 480";
				osd1axis = "1280 720 720 480";
				osd1cmd  = "0x10001";
			}else if(!strncmp(mode, "576i", 4) || !strncmp(mode, "576p", 4) || !strncmp(mode, "576cvbs", 7)){
				reqcmd   = "16 720 576";
				osd1axis = "1280 720 720 576";
				osd1cmd  = "0x10001";
			}else if(!strncmp(mode, "1080i", 5) || !strncmp(mode, "1080p", 5)){
				reqcmd   = "8";
				osd1axis = "1280 720 1920 1080";
				osd1cmd  = "0x10001";
			}else{
				reqcmd   = "2";
				osd1axis = NULL;
				osd1cmd  = "0";
				blank    = AM_FALSE;
			}
			
			if (blank && !strncmp(verstr, reqcmd, strlen(reqcmd))) {
				blank = AM_FALSE;
			}

			property_get("ro.build.version.sdk",verstr,"10");
			if(sscanf(verstr, "%d", &version)==1){
				if(version < 15){
					blank = AM_FALSE;
				}
			}
		}
#endif
		if(blank && scale){
			AM_FileEcho("/sys/class/graphics/fb0/blank", "1");
		}
		AM_FileEcho("/sys/class/graphics/fb0/free_scale", "0");
		AM_FileEcho("/sys/class/graphics/fb1/free_scale", "0");

#ifdef ANDROID
		{
			if(scale){
				AM_FileEcho("/sys/class/graphics/fb0/request2XScale", reqcmd);
				if(osd1axis){
					AM_FileEcho("/sys/class/graphics/fb1/scale_axis", osd1axis);
				}
				AM_FileEcho("/sys/class/graphics/fb1/scale", osd1cmd);
			}

			AM_FileEcho("/sys/module/amvdec_h264/parameters/dec_control", "3");
			AM_FileEcho("/sys/module/amvdec_mpeg12/parameters/dec_control", "62");
			AM_FileEcho("/sys/module/di/parameters/bypass_hd","1");
			AM_FileEcho("/sys/class/ppmgr/ppscaler","0");
			AM_FileEcho("/sys/class/ppmgr/ppscaler_rect","0 0 0 0 1");
			AM_FileEcho("/sys/class/video/axis", "0 0 0 0");
			AM_FileEcho("/sys/module/amvideo/parameters/smooth_sync_enable", AV_SMOOTH_SYNC_VAL);
		}
#endif
	}

	if(dev->vpath_ppmgr!=AM_AV_PPMGR_DISABLE){
		if (dev->vpath_ppmgr == AM_AV_PPMGR_MODE3D_2D_TO_3D){
			AM_FileEcho("/sys/class/vfm/map", "add default decoder deinterlace d2d3 amvideo");
			AM_FileEcho("/sys/class/d2d3/d2d3/debug", "enable");
		}else{
			AM_FileEcho("/sys/class/vfm/map", "add default decoder ppmgr amvideo");
			if (dev->vpath_ppmgr!=AM_AV_PPMGR_ENABLE) {
				/*Set 3D mode*/
				AM_TRY(aml_set_ppmgr_3dcmd(dev->vpath_ppmgr));
			}
		}
		
	}else if(dev->vpath_di==AM_AV_DEINTERLACE_ENABLE){
		AM_FileEcho("/sys/class/vfm/map", "add default decoder deinterlace amvideo");
		AM_FileEcho("/sys/class/deinterlace/di0/config", "enable");
	}else{
		AM_FileEcho("/sys/class/vfm/map", "add default decoder amvideo");
	}
	
	return AM_SUCCESS;
}

/**\brief 切换TS播放的音频*/
static AM_ErrorCode_t aml_switch_ts_audio(AM_AV_Device_t *dev, uint16_t apid, AM_AV_AFormat_t afmt)
{
	int fd;
	
	fd = (int)dev->ts_player.drv_data;
	
	if (fd < 0)
	{
		AM_DEBUG(1, "ts_player fd < 0, error!");
		return AM_AV_ERR_SYS;
	}
	
	/*Stop Audio first*/
	if (dev->ts_player.av_thread_running) 
	{
		dev->ts_player.av_thread_running = AM_FALSE;
		pthread_cond_broadcast(&gAVMonCond);
		pthread_join(dev->ts_player.av_mon_thread, NULL);
	}
#if !defined(ADEC_API_NEW)
	adec_cmd("stop");
#else
	if(adec_handle)
	{
		audio_decode_stop(adec_handle);
		audio_decode_release(&adec_handle);
	}
	adec_handle = NULL;
#endif
	
	/*Set Audio PID & fmt*/
	if(apid && (apid<0x1fff))
	{
		if(ioctl(fd, AMSTREAM_IOC_AFORMAT, (int)afmt)==-1)
		{
			AM_DEBUG(1, "set audio format failed");
			return AM_AV_ERR_SYS;
		}
		if(ioctl(fd, AMSTREAM_IOC_AID, (int)apid)==-1)
		{
			AM_DEBUG(1, "set audio PID failed");
			return AM_AV_ERR_SYS;
		}
	}
	
	/*reset audio*/
	if(ioctl(fd, AMSTREAM_IOC_AUDIO_RESET, 0)==-1)
	{
		AM_DEBUG(1, "audio reset failed");
		return AM_AV_ERR_SYS;
	}
	
	AM_TIME_GetClock(&dev->ts_player.av_start_time);
	
	/*Start Audio*/
	dev->ts_player.av_thread_running = AM_TRUE;
	if(pthread_create(&dev->ts_player.av_mon_thread, NULL, aml_av_monitor_thread, (void*)dev))
	{
		AM_DEBUG(1, "create the av buf monitor thread failed");
		dev->ts_player.av_thread_running = AM_FALSE;
	}
		
	dev->ts_player.play_para.apid = apid;
	dev->ts_player.play_para.afmt = afmt;
	
	
	return AM_SUCCESS;
}


