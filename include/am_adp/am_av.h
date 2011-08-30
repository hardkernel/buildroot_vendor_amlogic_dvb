/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 音视频解码驱动
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-06: create the document
 ***************************************************************************/

#ifndef _AM_AV_H
#define _AM_AV_H

#include "am_types.h"
#include "am_osd.h"
#include "am_evt.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief 音视频解码模块错误代码*/
enum AM_AV_ErrorCode
{
	AM_AV_ERROR_BASE=AM_ERROR_BASE(AM_MOD_AV),
	AM_AV_ERR_INVALID_DEV_NO,          /**< 设备号无效*/
	AM_AV_ERR_BUSY,                    /**< 设备已经被打开*/
	AM_AV_ERR_ILLEGAL_OP,              /**< 无效的操作*/
	AM_AV_ERR_INVAL_ARG,               /**< 无效的参数*/
	AM_AV_ERR_NOT_ALLOCATED,           /**< 设备没有分配*/
	AM_AV_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_AV_ERR_CANNOT_OPEN_DEV,         /**< 无法打开设备*/
	AM_AV_ERR_CANNOT_OPEN_FILE,        /**< 无法打开文件*/
	AM_AV_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_AV_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_AV_ERR_TIMEOUT,                 /**< 等待设备数据超时*/
	AM_AV_ERR_SYS,                     /**< 系统操作错误*/
	AM_AV_ERR_DECODE,                  /**< 解码出错*/
	AM_AV_ERR_END
};

/****************************************************************************
 * Event type definitions
 ****************************************************************************/

/**\brief 音视频模块事件类型*/
enum AM_AV_EventType
{
	AM_AV_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_AV),
	AM_AV_EVT_PLAYER_STATE_CHANGED,    /**< 文件播放器状态发生改变,参数为AM_AV_MPState_t*/
	AM_AV_EVT_PLAYER_SPEED_CHANGED,    /**< 文件播放速度变化,参数为速度值(0为正常播放，<0表示回退播放，>0表示快速向前播放)*/
	AM_AV_EVT_PLAYER_TIME_CHANGED,     /**< 文件播放当前时间变化，参数为当前时间*/
	AM_AV_EVT_PLAYER_UPDATE_INFO,	/**< 更新当前文件播放信息*/
	AM_AV_EVT_VIDEO_WINDOW_CHANGED,    /**< 视频窗口发生变化,参数为新的窗口大小(AM_AV_VideoWindow_t)*/
	AM_AV_EVT_VIDEO_CONTRAST_CHANGED,  /**< 视频对比度变化，参数为新对比度值(int 0~100)*/
	AM_AV_EVT_VIDEO_SATURATION_CHANGED,/**< 视频饱和度变化，参数为新饱和度值(int 0~100)*/
	AM_AV_EVT_VIDEO_BRIGHTNESS_CHANGED,/**< 视频亮度变化，参数为新亮度值(int 0~100)*/
	AM_AV_EVT_VIDEO_ENABLED,           /**< 视频层显示*/
	AM_AV_EVT_VIDEO_DISABLED,          /**< 视频曾隐藏*/
	AM_AV_EVT_VIDEO_ASPECT_RATIO_CHANGED, /**< 视频长宽比变化，参数为新的长宽比(AM_AV_VideoAspectRatio_t)*/
	AM_AV_EVT_VIDEO_DISPLAY_MODE_CHANGED, /**< 视频显示模式变化，参数为新的显示模式(AM_AV_VideoDisplayMode_t)*/
	AM_AV_EVT_AV_NO_DATA,		/**< 播放过程中检测到Audio PTS 和 Video PTS 一段时间未变化，通知无数据*/
	AM_AV_EVT_AV_DATA_RESUME,	/**< 在AM_AV_EVT_AV_NO_DATA后，检测到Audio or Video PTS有变化，通知有数据*/
	AM_AV_EVT_END
};


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 文件播放器状态*/
typedef enum
{
	AM_AV_MP_STATE_UNKNOWN = 0,        /**< 未知状态*/
	AM_AV_MP_STATE_INITING,            /**< 正在进行初始化*/
	AM_AV_MP_STATE_NORMALERROR,        /**< 发生错误*/
	AM_AV_MP_STATE_FATALERROR,         /**< 发生致命错误*/
	AM_AV_MP_STATE_PARSERED,           /**< 文件信息分析完毕*/
	AM_AV_MP_STATE_STARTED,            /**< 开始播放*/
	AM_AV_MP_STATE_PLAYING,            /**< 正在播放*/
	AM_AV_MP_STATE_PAUSED,             /**< 暂停播放*/
	AM_AV_MP_STATE_CONNECTING,         /**< 正在连接服务器*/
	AM_AV_MP_STATE_CONNECTDONE,        /**< 服务器连接完毕*/
	AM_AV_MP_STATE_BUFFERING,          /**< 正在缓存数据*/
	AM_AV_MP_STATE_BUFFERINGDONE,      /**< 数据缓存结束*/
	AM_AV_MP_STATE_SEARCHING,          /**< 正在设定播放位置*/
	AM_AV_MP_STATE_TRICKPLAY,          /**< 正在进行trick mode播放*/
	AM_AV_MP_STATE_MESSAGE_EOD,        /**< 消息结束*/
	AM_AV_MP_STATE_MESSAGE_BOD,        /**< 消息开始*/
	AM_AV_MP_STATE_TRICKTOPLAY,        /**< 从trick mode切换到play模式*/
	AM_AV_MP_STATE_FINISHED,           /**< 播放完毕*/
	AM_AV_MP_STATE_STOPED              /**< 播放器停止*/
} AM_AV_MPState_t;

/**\brief 视频窗口*/
typedef struct
{
	int x;                               /**< 窗口左上角X坐标*/
	int y;                               /**< 窗口左上角Y坐标*/
	int w;                               /**< 窗口宽度*/
	int h;                               /**< 窗口高度*/
} AM_AV_VideoWindow_t;

/**\brief 音视频播放的TS流输入源*/
typedef enum
{
	AM_AV_TS_SRC_TS0,                    /**< TS输入0*/
	AM_AV_TS_SRC_TS1,                    /**< TS输入1*/
	AM_AV_TS_SRC_TS2,                    /**< TS输入1*/	
	AM_AV_TS_SRC_HIU                     /**< HIU 接口*/
} AM_AV_TSSource_t;

/**\brief 音频压缩格式*/
typedef enum
{
	AFORMAT_MPEG   = 0,
	AFORMAT_PCM_S16LE = 1,
	AFORMAT_AAC   = 2,
	AFORMAT_AC3   =3,
	AFORMAT_ALAW = 4,
	AFORMAT_MULAW = 5,
	AFORMAT_DTS = 6,
	AFORMAT_PCM_S16BE = 7,
	AFORMAT_FLAC = 8,
	AFORMAT_COOK = 9,
	AFORMAT_PCM_U8 = 10,
	AFORMAT_ADPCM = 11,
	AFORMAT_AMR  = 12,
	AFORMAT_RAAC  = 13,
	AFORMAT_MAX    = 14
} AM_AV_AFormat_t;

/**\brief 视频压缩格式*/
typedef enum
{
	VFORMAT_MPEG12 = 0,
	VFORMAT_MPEG4,
	VFORMAT_H264,
	VFORMAT_MJPEG,
	VFORMAT_REAL,
	VFORMAT_JPEG,
	VFORMAT_VC1,
	VFORMAT_MAX
} AM_AV_VFormat_t;

/**\brief 数据封装格式*/
typedef enum
{
	PFORMAT_ES = 0,
	PFORMAT_PS,
	PFORMAT_TS,
	PFORMAT_REAL
} AM_AV_PFormat_t;

/**\brief 视频长宽比*/
typedef enum
{
	AM_AV_VIDEO_ASPECT_AUTO,      /**< 自动设定长宽比*/
	AM_AV_VIDEO_ASPECT_4_3,       /**< 4:3*/
	AM_AV_VIDEO_ASPECT_16_9       /**< 16:9*/
} AM_AV_VideoAspectRatio_t;

/**\brief 视频长宽比匹配模式*/
typedef enum
{
	AM_AV_VIDEO_ASPECT_MATCH_IGNORE,     /**< 忽略长宽比变化*/
	AM_AV_VIDEO_ASPECT_MATCH_LETTER_BOX, /**< letter box匹配模式*/
	AM_AV_VIDEO_ASPECT_MATCH_PAN_SCAN,   /**< pan scan匹配模式*/
	AM_AV_VIDEO_ASPECT_MATCH_COMBINED    /**< 混合letter box/pan scan匹配模式*/
} AM_AV_VideoAspectMatchMode_t;

/**\brief 视频屏幕显示模式*/
typedef enum
{
	AM_AV_VIDEO_DISPLAY_NORMAL,     /**< 普通显示*/
	AM_AV_VIDEO_DISPLAY_FULL_SCREEN /**< 全屏显示*/
} AM_AV_VideoDisplayMode_t;

/**\brief 音视频解码设备开启参数*/
typedef struct
{
	int      vout_dev_no;         /**< 音视频播放设备对应的视频输出设备ID*/
} AM_AV_OpenPara_t;

/**\brief 媒体文件信息*/
typedef struct
{
	uint64_t size;                /**< 文件大小*/ 
	int      duration;            /**< 总播放时长*/
} AM_AV_FileInfo_t;

/**\brief 文件播放状态*/
typedef struct
{
	int      duration;            /**< 总播放时长*/
	int      position;            /**< 当前播放时间*/
} AM_AV_PlayStatus_t;

/**\brief JPEG文件属性*/
typedef struct
{
	int      width;               /**< JPEG宽度*/
	int      height;              /**< JPEG高度*/
	int      comp_num;            /**< 颜色数*/
} AM_AV_JPEGInfo_t;

/**\brief JPEG旋转角度*/
typedef enum
{
	AM_AV_JPEG_CLKWISE_0    = 0,  /**< 不旋转*/
	AM_AV_JPEG_CLKWISE_90   = 1,  /**< 顺时针旋转90度*/
	AM_AV_JPEG_CLKWISE_180  = 2,  /**< 顺时针旋转180度*/
	AM_AV_JPEG_CLKWISE_270  = 3   /**< 顺时针旋转270度*/
} AM_AV_JPEGAngle_t;

/**\brief JPEG 解码选项*/
typedef enum
{
	AM_AV_JPEG_OPT_THUMBNAIL_ONLY     = 1, /**< 按缩略图方式解码*/
	AM_AV_JPEG_OPT_THUMBNAIL_PREFERED = 2, /**< 按缩略图方式解码*/
	AM_AV_JPEG_OPT_FULLRANGE          = 4  /**< 全图解码*/
} AM_AV_JPEGOption_t;

/**\brief 取得显示图形表面的参数（用于取得解码JPEG和视频Frame图像）*/
typedef struct
{
	int    width;                 /**< 图形宽度,<=0表示使用原始大小*/
	int    height;                /**< 图形高度,<=0表示使用原始大小*/
	AM_AV_JPEGAngle_t  angle;     /**< JPEG图片的旋转角度*/
	AM_AV_JPEGOption_t option;    /**< JPEG解码选项*/
} AM_AV_SurfacePara_t;

/**\brief 注入数据类型*/
typedef enum
{
	AM_AV_INJECT_AUDIO,           /**< 音频数据*/
	AM_AV_INJECT_VIDEO,           /**< 视频数据*/
	AM_AV_INJECT_MULTIPLEX        /**< 复用数据*/
} AM_AV_InjectType_t;

/**\brief 音视频注入参数*/
typedef struct
{
	AM_AV_AFormat_t  aud_fmt;     /**< 音频格式*/
	AM_AV_VFormat_t  vid_fmt;     /**< 视频格式*/
	AM_AV_PFormat_t  pkg_fmt;     /**< 数据封装格式*/
	int              aud_id;      /**< 音频ID, -1表示没有音频*/
	int              vid_id;      /**< 视频ID, -1表示没有视频*/
	int              channel;     /**< 音频通道数目(播放PCM音频时使用)*/
	int              sample_rate; /**< 音频采样率(播放PCM音频时使用)*/
	int              data_width;  /**< 音频采样数据位数(播放PCM音频时使用)*/
} AM_AV_InjectPara_t;

typedef struct
{
	AM_AV_VFormat_t  vid_fmt;     /**< 视频格式*/
	int              SourceWidth;         
	int              SourceHight;                 
	int              FrameRate;                   
	char             bInterlaced;                  
	int              FrameCount;
	int              vb_size;
	int              vb_data;
	int              vb_free;
}AM_AV_VideoStatus_t;

typedef struct
{
	AM_AV_AFormat_t  	aud_fmt;     /**< 音频格式*/
	int              	SampleRate;  
	int     			BitWidth;    
	int 				Channels;    
	unsigned int    	FrameCount;
	int              ab_size;
	int              ab_data;
	int              ab_free;
}AM_AV_AudioStatus_t;

/**\brief Timeshift播放参数*/
typedef struct
{
	int				 dmx_id;	  /**< 用于回放的dmx*/
	AM_AV_AFormat_t  aud_fmt;     /**< 音频格式*/
	AM_AV_VFormat_t  vid_fmt;     /**< 视频格式*/
	int              aud_id;      /**< 音频ID, -1表示没有音频*/
	int              vid_id;      /**< 视频ID, -1表示没有视频*/
	int				 duration;	  /**< 时移时长,秒为单位*/
	const char		*file_path;		/**< 存储文件全路径*/
	AM_Bool_t		playback_only;	/**< 仅回放,不做时移，相当于普通的文件播放*/
} AM_AV_TimeshiftPara_t;

/**\brief 播放器状态信息*/
typedef struct player_info
{
	char *name;
	int last_sta;
	int status;		   /*stop,pause	*/
	int full_time;	   /*Seconds	*/
	int current_time;  /*Seconds	*/
	int current_ms;	/*ms*/
	int last_time;		
	int error_no;  
	int start_time;
	int pts_video;
	int pts_pcrscr;
	int current_pts;
	long curtime_old_time;    
	unsigned int video_error_cnt;
	unsigned int audio_error_cnt;
	float		    audio_bufferlevel;
	float		    video_bufferlevel;
}player_info_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 打开音视频解码设备
 * \param dev_no 音视频设备号
 * \param[in] para 音视频设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_Open(int dev_no, const AM_AV_OpenPara_t *para);

/**\brief 关闭音视频解码设备
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_Close(int dev_no);

/**\brief 设定TS流的输入源
 * \param dev_no 音视频设备号
 * \param src TS输入源
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetTSSource(int dev_no, AM_AV_TSSource_t src);

/**\brief 开始解码TS流
 * \param dev_no 音视频设备号
 * \param vpid 视频流PID
 * \param apid 音频流PID
 * \param vfmt 视频压缩格式
 * \param afmt 音频压缩格式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartTS(int dev_no, uint16_t vpid, uint16_t apid, AM_AV_VFormat_t vfmt, AM_AV_AFormat_t afmt);

/**\brief 停止TS流解码
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopTS(int dev_no);

/**\brief 开始TS视频流解码
 * \param dev_no 音视频设备号
 * \param vpid 视频PID
 * \param vfmt 视频格式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartTSVideo(int dev_no, uint16_t vpid, AM_AV_VFormat_t vfmt);

/**\brief 停止TS视频流解码
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopTSVideo(int dev_no);

/**\brief 开始TS音频流解码
 * \param dev_no 音视频设备号
 * \param apid 音频PID
 * \param afmt 音频格式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartTSAudio(int dev_no, uint16_t apid, AM_AV_AFormat_t afmt);

/**\brief 停止TS音频流解码
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopTSAudio(int dev_no);

/**\brief 开始播放文件
 * \param dev_no 音视频设备号
 * \param[in] fname 媒体文件名
 * \param loop 是否循环播放
 * \param pos 播放的起始位置
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartFile(int dev_no, const char *fname, AM_Bool_t loop, int pos);

/**\brief 切换到文件播放模式但不开始播放
 * \param dev_no 音视频设备号
 * \param[in] fname 媒体文件名
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_AddFile(int dev_no, const char *fname);

/**\brief 开始播放已经添加的文件，在AM_AV_AddFile后调用此函数开始播放
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartCurrFile(int dev_no);

/**\brief 停止播放文件
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopFile(int dev_no);

/**\brief 暂停文件播放
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_PauseFile(int dev_no);

/**\brief 恢复文件播放
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_ResumeFile(int dev_no);

/**\brief 设定当前文件播放位置
 * \param dev_no 音视频设备号
 * \param pos 播放位置
 * \param start 是否开始播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SeekFile(int dev_no, int pos, AM_Bool_t start);

/**\brief 快速向前播放
 * \param dev_no 音视频设备号
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_FastForwardFile(int dev_no, int speed);

/**\brief 快速向后播放
 * \param dev_no 音视频设备号
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_FastBackwardFile(int dev_no, int speed);

/**\brief 取得当前媒体文件信息
 * \param dev_no 音视频设备号
 * \param[out] info 返回媒体文件信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetCurrFileInfo(int dev_no, AM_AV_FileInfo_t *info);

/**\brief 取得媒体文件播放状态
 * \param dev_no 音视频设备号
 * \param[out] status 返回媒体文件播放状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetPlayStatus(int dev_no, AM_AV_PlayStatus_t *status);

/**\brief 取得JPEG图片文件属性
 * \param dev_no 音视频设备号
 * \param[in] fname 图片文件名
 * \param[out] info 文件属性
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetJPEGInfo(int dev_no, const char *fname, AM_AV_JPEGInfo_t *info);

/**\brief 取得JPEG图片数据属性
 * \param dev_no 音视频设备号
 * \param[in] data 图片数据缓冲区
 * \param len 数据缓冲区大小
 * \param[out] info 文件属性
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetJPEGDataInfo(int dev_no, const uint8_t *data, int len, AM_AV_JPEGInfo_t *info);

/**\brief 解码JPEG图片文件
 * \param dev_no 音视频设备号
 * \param[in] fname 图片文件名
 * \param[in] para 输出JPEG的参数
 * \param[out] surf 返回JPEG图片的surface
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_DecodeJPEG(int dev_no, const char *fname, const AM_AV_SurfacePara_t *para, AM_OSD_Surface_t **surf);

/**\brief 解码JPEG图片数据
 * \param dev_no 音视频设备号
 * \param[in] data 图片数据缓冲区
 * \param len 数据缓冲区大小
 * \param[in] para 输出JPEG的参数
 * \param[out] surf 返回JPEG图片的surface
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_DacodeJPEGData(int dev_no, const uint8_t *data, int len, const AM_AV_SurfacePara_t *para, AM_OSD_Surface_t **surf);

/**\brief 解码视频基础流文件
 * \param dev_no 音视频设备号
 * \param format 视频压缩格式
 * \param[in] fname 视频文件名
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartVideoES(int dev_no, AM_AV_VFormat_t format, const char *fname);

/**\brief 解码视频基础流数据
 * \param dev_no 音视频设备号
 * \param format 视频压缩格式
 * \param[in] data 视频数据缓冲区
 * \param len 数据缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartVideoESData(int dev_no, AM_AV_VFormat_t format, const uint8_t *data, int len);

/**\brief 停止解码视频基础流
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopVideoES(int dev_no);

/**\brief 解码音频基础流文件
 * \param dev_no 音视频设备号
 * \param format 音频压缩格式
 * \param[in] fname 视频文件名
 * \param times 播放次数，<=0表示一直播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartAudioES(int dev_no, AM_AV_AFormat_t format, const char *fname, int times);

/**\brief 解码音频基础流数据
 * \param dev_no 音视频设备号
 * \param format 音频压缩格式
 * \param[in] data 音频数据缓冲区
 * \param len 数据缓冲区大小
 * \param times 播放次数，<=0表示一直播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartAudioESData(int dev_no, AM_AV_AFormat_t format, const uint8_t *data, int len, int times);

/**\brief 停止解码音频基础流
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopAudioES(int dev_no);

/**\brief 开始进入数据注入模式
 * \param dev_no 音视频设备号
 * \param[in] para 注入参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartInject(int dev_no, const AM_AV_InjectPara_t *para);

/**\brief 进入数据注入模式后，向音视频设备注入数据
 * \param dev_no 音视频设备号
 * \param type 注入数据类型
 * \param[in] data 注入数据的缓冲区
 * \param[in,out] size 传入要注入数据的长度，返回实际注入数据的长度
 * \param timeout 等待设备的超时时间，以毫秒为单位，<0表示一直等待
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_InjectData(int dev_no, AM_AV_InjectType_t type, uint8_t *data, int *size, int timeout);

/**\brief 暂停数据注入
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_PauseInject(int dev_no);

/**\brief 恢复注入
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_ResumeInject(int dev_no);

/**\brief 进入数据注入模式后，改变播放的视频
 * \param dev_no 音视频设备号
 * \param vid 视频流ID
 * \param vfmt 视频格式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetInjectVideo(int dev_no, int vid, AM_AV_VFormat_t vfmt);

/**\brief 进入数据注入模式后，改变播放的音频
 * \param dev_no 音视频设备号
 * \param aid 音频流ID
 * \param afmt 音频格式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetInjectAudio(int dev_no, int aid, AM_AV_AFormat_t afmt);

/**\brief 停止数据注入模式
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopInject(int dev_no);

/**\brief 设定视频窗口
 * \param dev_no 音视频设备号
 * \param x 窗口左上顶点x坐标
 * \param y 窗口左上顶点y坐标
 * \param w 窗口宽度
 * \param h 窗口高度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoWindow(int dev_no, int x, int y, int w, int h);

/**\brief 返回视频窗口
 * \param dev_no 音视频设备号
 * \param[out] x 返回窗口左上顶点x坐标
 * \param[out] y 返回窗口左上顶点y坐标
 * \param[out] w 返回窗口宽度
 * \param[out] h 返回窗口高度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoWindow(int dev_no, int *x, int *y, int *w, int *h);

/**\brief 设定视频对比度(0~100)
 * \param dev_no 音视频设备号
 * \param val 视频对比度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoContrast(int dev_no, int val);

/**\brief 取得当前视频对比度
 * \param dev_no 音视频设备号
 * \param[out] val 返回当前视频对比度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoContrast(int dev_no, int *val);

/**\brief 设定视频饱和度(0~100)
 * \param dev_no 音视频设备号
 * \param val 视频饱和度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoSaturation(int dev_no, int val);

/**\brief 取得当前视频饱和度
 * \param dev_no 音视频设备号
 * \param[out] val 返回当前视频饱和度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoSaturation(int dev_no, int *val);

/**\brief 设定视频亮度(0~100)
 * \param dev_no 音视频设备号
 * \param val 视频亮度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoBrightness(int dev_no, int val);

/**\brief 取得当前视频亮度
 * \param dev_no 音视频设备号
 * \param[out] val 返回当前视频亮度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoBrightness(int dev_no, int *val);

/**\brief 显示视频层
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_EnableVideo(int dev_no);

/**\brief 隐藏视频层
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_DisableVideo(int dev_no);

/**\brief 设定视频长宽比
 * \param dev_no 音视频设备号
 * \param ratio 长宽比
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoAspectRatio(int dev_no, AM_AV_VideoAspectRatio_t ratio);

/**\brief 取得视频长宽比
 * \param dev_no 音视频设备号
 * \param[out] ratio 长宽比
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoAspectRatio(int dev_no, AM_AV_VideoAspectRatio_t *ratio);

/**\brief 设定视频长宽比匹配模式
 * \param dev_no 音视频设备号
 * \param mode 长宽比匹配模式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoAspectMatchMode(int dev_no, AM_AV_VideoAspectMatchMode_t mode);

/**\brief 取得视频长宽比匹配模式
 * \param dev_no 音视频设备号
 * \param[out] mode 长宽比匹配模式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoAspectMatchMode(int dev_no, AM_AV_VideoAspectMatchMode_t *mode);

/**\brief 设定视频停止时自动隐藏视频层
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_EnableVideoBlackout(int dev_no);

/**\brief 视频停止时不隐藏视频层
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_DisableVideoBlackout(int dev_no);

/**\brief 设定视频显示模式
 * \param dev_no 音视频设备号
 * \param mode 视频显示模式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SetVideoDisplayMode(int dev_no, AM_AV_VideoDisplayMode_t mode);

/**\brief 取得当前视频显示模式
 * \param dev_no 音视频设备号
 * \param mode 返回当前视频显示模式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoDisplayMode(int dev_no, AM_AV_VideoDisplayMode_t *mode);

/**\brief 清除视频缓冲区
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_ClearVideoBuffer(int dev_no);

/**\brief 取得当前视频显示祯图形(此操作只有视频停止后才可进行)
 * \param dev_no 音视频设备号
 * \param[in] para 创建图像表面的参数
 * \param[out] s 返回图像
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_GetVideoFrame(int dev_no, const AM_AV_SurfacePara_t *para, AM_OSD_Surface_t **s);

extern AM_ErrorCode_t AM_AV_GetVideoStatus(int dev_no,AM_AV_VideoStatus_t *status);

extern AM_ErrorCode_t AM_AV_GetAudioStatus(int dev_no,AM_AV_AudioStatus_t *status);


/**\brief 开始进入Timeshift模式
 * \param dev_no 音视频设备号
 * \param[in] para Timeshift参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StartTimeshift(int dev_no, const AM_AV_TimeshiftPara_t *para);

/**\brief 进入Timeshift模式后，写数据到timeshift文件
 * \param dev_no 音视频设备号
 * \param[in] data 注入数据的缓冲区
 * \param[in,out] size 传入要注入数据的长度，返回实际注入数据的长度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_TimeshiftFillData(int dev_no, uint8_t *data, int size);

/**\brief 停止Timeshift模式
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_StopTimeshift(int dev_no);

/**\brief 开始播放Timeshift
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_PlayTimeshift(int dev_no);

/**\brief 暂停Timeshift
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_PauseTimeshift(int dev_no);

/**\brief 恢复Timeshift播放
 * \param dev_no 音视频设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_ResumeTimeshift(int dev_no);

/**\brief 设定当前Timeshift播放位置
 * \param dev_no 音视频设备号
 * \param pos 播放位置
 * \param start 是否开始播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_SeekTimeshift(int dev_no, int pos, AM_Bool_t start);

/**\brief 快速向前播放
 * \param dev_no 音视频设备号
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_FastForwardTimeshift(int dev_no, int speed);

/**\brief 快速向后播放
 * \param dev_no 音视频设备号
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_av.h)
 */
extern AM_ErrorCode_t AM_AV_FastBackwardTimeshift(int dev_no, int speed);

#ifdef __cplusplus
}
#endif

#endif

