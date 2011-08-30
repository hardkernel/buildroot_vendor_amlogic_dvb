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
	int				fend_dev;
	int				av_dev;
	int				dvr_dev;
	int				fifo_id;
	int				evt;
	int				stat_flag;
	int				rec_fd;
	int				rec_start_time;
	int				rec_end_time;
	int				tshift_play_check_time;
	int				rec_check_time;
	int				rec_end_check_time;
	int				speed;
	int				seek_pos;
	int				rec_db_id;
	int				timeshift_duration;
	int				timeshift_dmx_dev;
	int				rec_duration;
	AM_AV_VFormat_t	vfmt;
	AM_AV_AFormat_t	afmt1;
	AM_AV_AFormat_t	afmt2;
	AM_Bool_t		seek_start;
	pthread_t		thread;
	pthread_t		rec_thread;
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	sqlite3			*hdb;
	AM_REC_Pid_t		rec_pids;
	AM_REC_RecPara_t	rec_para;
	char			rec_file_name[256];
	char			store_dir[256];
	void			*user_data;
}AM_REC_Recorder_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

