/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_rec_internal.h
 * \brief 录像管理模块内部头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-03-30: create the document
 ***************************************************************************/

#ifndef _AM_REC_INTERNAL_H
#define _AM_REC_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 状态定义*/
enum
{
	REC_STAT_FL_RECORDING = 0x1,		/**< 录像*/
	REC_STAT_FL_TIMESHIFTING = 0x2,	/**< Timshifting*/
};

/**\brief 内部事件定义*/
enum
{
	REC_EVT_NONE,
	REC_EVT_START_RECORD,
	REC_EVT_STOP_RECORD,
	REC_EVT_START_TIMESHIFTING,
	REC_EVT_PLAY_TIMESHIFTING,
	REC_EVT_STOP_TIMESHIFTING,
	REC_EVT_PAUSE_TIMESHIFTING,
	REC_EVT_RESUME_TIMESHIFTING,
	REC_EVT_FF_TIMESHIFTING,
	REC_EVT_FB_TIMESHIFTING,
	REC_EVT_SEEK_TIMESHIFTING,
	REC_EVT_QUIT
};

/**\brief 录像管理数据*/
typedef struct
{
	AM_REC_CreatePara_t	create_para;
	AM_REC_RecPara_t	rec_para;
	pthread_t		rec_thread;
	pthread_mutex_t	lock;
	int				stat_flag;
	int				rec_fd;
	int				rec_start_time;
	char			rec_file_name[256];
	void			*user_data;
}AM_REC_Recorder_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif
