/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_rec.c
 * \brief 录像管理模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-03-30: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 2

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>
#include <am_debug.h>
#include <am_mem.h>
#include <am_time.h>
#include <am_util.h>
#include <am_av.h>
#include <am_db.h>
#include <am_epg.h>
#include <am_rec.h>
#include "am_rec_internal.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/**\brief 定期检查录像时间的间隔*/
#define REC_RECORD_CHECK_TIME	(10*1000)
/**\brief 开始Timeshifting录像后，检查开始播放的间隔*/
#define REC_TSHIFT_PLAY_CHECK_TIME	(1000)
/**\brief 开始Timeshifting录像后，开始播放前需要缓冲的数据长度*/
#define REC_TIMESHIFT_PLAY_BUF_SIZE	(1024*1024)
/**\brief 定时录像即将开始时，提前通知用户的间隔*/
#define REC_NOTIFY_RECORD_TIME	(60 * 1000)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 等待管理线程处理完所有事件*/
static void am_rec_wait_events_done(AM_REC_Recorder_t *rec)
{
	/*同一线程下允许嵌套调用*/
	if (rec->thread == pthread_self())
		return;
	
	while (rec->evt != REC_EVT_NONE)
	{
		//AM_DEBUG(0, "wait_events_done start");
		pthread_cond_wait(&rec->cond, &rec->lock);
		//AM_DEBUG(0, "wait_events_done end");
	}
}

/**\brief 写录像数据到文件*/
static int am_rec_data_write(int fd, uint8_t *buf, int size)
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
				AM_DEBUG(0, "Write record data failed: %s", strerror(errno));
				break;
			}
			ret = 0;
		}
		
		left -= ret;
		p += ret;
	}

	return (size - left);
}

/**\brief 更新录像状态到数据库*/
static void am_rec_db_update_record_status(sqlite3 *hdb, int db_rec_id, int status)
{
	char sql[128];

	snprintf(sql, sizeof(sql), "update rec_table set status=%d where db_id=%d",status, db_rec_id);

	sqlite3_exec(hdb, sql, NULL, NULL, NULL);
}

/**\brief 更新录像持续时间到数据库*/
static void am_rec_db_update_record_duration(sqlite3 *hdb, int db_rec_id, int duration)
{
	char sql[128];

	snprintf(sql, sizeof(sql), "update rec_table set duration=%d where db_id=%d",duration, db_rec_id);

	sqlite3_exec(hdb, sql, NULL, NULL, NULL);
}

/**\brief 更新录像文件名到数据库*/
static void am_rec_db_update_record_file_name(sqlite3 *hdb, int db_rec_id, const char *name)
{
	char sql[128];

	snprintf(sql, sizeof(sql), "update rec_table set file_name='%s' where db_id=%d", name, db_rec_id);

	sqlite3_exec(hdb, sql, NULL, NULL, NULL);
}

/**\brief 录像线程*/
static void *am_rec_record_thread(void* arg)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)arg;
	int cnt, err = 0;
	uint8_t buf[256*1024];
	AM_DVR_OpenPara_t para;
	AM_DVR_StartRecPara_t spara;
	int tshift_ready = 0, start_time;

	/*设置DVR设备参数*/
	memset(&para, 0, sizeof(para));
	if (AM_DVR_Open(rec->dvr_dev, &para) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Open DVR%d failed", rec->dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_file;
	}

	AM_DVR_SetSource(rec->dvr_dev, rec->fifo_id);
	
	spara.pid_count = rec->rec_pids.pid_count;
	memcpy(spara.pids, rec->rec_pids.pids, sizeof(spara.pids));
	if (AM_DVR_StartRecord(rec->dvr_dev, &spara) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Start DVR%d failed", rec->dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_dvr;
	}

	AM_TIME_GetClock(&start_time);
	
	rec->tshift_play_check_time = 0;

	/*从DVR设备读取数据并存入文件*/
	while (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		/*if (rec->rec_fd == -1)
			break;*/
		cnt = AM_DVR_Read(rec->dvr_dev, buf, sizeof(buf), 1000);
		if (cnt <= 0)
		{
			AM_DEBUG(1, "No data available from DVR%d", rec->dvr_dev);
			usleep(200*1000);
			continue;
		}
		if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
		{
			if (AM_AV_TimeshiftFillData(rec->av_dev, buf, cnt) != AM_SUCCESS)
			{
				err = AM_REC_ERR_CANNOT_WRITE_FILE;
				break;
			}
		}
		else
		{
			if (am_rec_data_write(rec->rec_fd, buf, cnt) == 0)
			{
				err = AM_REC_ERR_CANNOT_WRITE_FILE;
				break;
			}
			else if (! rec->rec_start_time)
			{
				/*已有数据写入文件，记录开始时间*/
				AM_TIME_GetClock(&rec->rec_start_time);
				/*重新设置结束检查时间*/
				if (rec->rec_end_check_time)
					AM_TIME_GetClock(&rec->rec_end_check_time);
			}
		}
		
	/*	if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING) && rec->rec_start_time  && ! tshift_ready)
		{
			int cur;

			AM_TIME_GetClock(&cur);
			if ((cur - start_time) >= 3000)
			{
				AM_DEBUG(1, "Timeshifting ready to play");
				tshift_ready = 1;
				rec->tshift_play_check_time = 0;
				AM_EVT_Signal((int)rec, AM_REC_EVT_TIMESHIFTING_READY, NULL);
			}
		}*/
	}

close_dvr:
	AM_DVR_StopRecord(rec->dvr_dev);
	AM_DVR_Close(rec->dvr_dev);
close_file:
	if (rec->rec_fd != -1)
	{
		close(rec->rec_fd);
		rec->rec_fd = -1;
	}

	/*设置定时录像完成标志，更新录像持续时间*/
	if (!(rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
	{
		int duration = 0;
		
		am_rec_db_update_record_status(rec->hdb, rec->rec_db_id, AM_REC_STAT_COMPLETE);
		if (rec->rec_start_time)
		{
			AM_TIME_GetClock(&rec->rec_end_time);
			duration = rec->rec_end_time - rec->rec_start_time;
			
		}
		duration /= 1000;
		am_rec_db_update_record_duration(rec->hdb, rec->rec_db_id, duration);
		
		AM_DEBUG(1, "Record end , duration %d:%02d:%02d", duration/3600, (duration%3600)/60, duration%60);
	}

	if (err != 0)
	{
		/*通知错误*/
		AM_EVT_Signal((int)rec, AM_REC_EVT_NOTIFY_ERROR, (void*)err);
	}

	/*通知录像结束*/
	AM_EVT_Signal((int)rec, AM_REC_EVT_RECORD_END, (void*)NULL);
	
	return NULL;
}

/**\brief 增加一个录像到数据库*/
static int am_rec_db_add_record(AM_REC_Recorder_t *rec, int start, int duration, AM_REC_RecPara_t *para)
{
	int now, row, db_rec_id, end, db_srv_id = -1, db_ts_id = -1, db_evt_id = -1;
	int pid_cnt = 0, i;
	int pids[AM_DVR_MAX_PID_COUNT];
	int avfmts[3];
	char sql[256];
	char srv_name[64];
	char evt_name[64];
	char recpara[256];
	char fmts[32];
	
	srv_name[0] = 0;
	evt_name[0] = 0;
	memset(avfmts, 0, sizeof(avfmts));
	if (para->type == AM_REC_TYPE_EVENT)
	{
		db_evt_id = para->u.event.db_evt_id;
		/*取出事件的时间信息*/
		row = 1;
		snprintf(sql, sizeof(sql), "select db_ts_id,db_srv_id,name,start,end from evt_table where db_id=%d", para->u.event.db_evt_id);
		if (AM_DB_Select(rec->hdb, sql, &row, "%d,%d,%s:64,%d,%d",
		 &db_ts_id, &db_srv_id, evt_name, &start, &end) == AM_SUCCESS && row > 0)
		{
			duration = end - start;
			if (start == 0 || end == 0 || duration <= 0)
			{
				AM_DEBUG(0, "Cannot get start time and duration from dbase for evt '%s'", evt_name);
				return AM_REC_ERR_INVALID_PARAM;
			}
		}
		else
		{
			AM_DEBUG(0, "Cannot get event info for db_id = %d", para->u.event.db_evt_id);
			return AM_REC_ERR_INVALID_PARAM;
		}
		/*取事件对应频道名称*/
		row = 1;
		snprintf(sql, sizeof(sql), "select name,vid_pid,aud1_pid, vid_fmt,aud1_fmt from srv_table where db_id=%d", db_srv_id);
		if (AM_DB_Select(rec->hdb, sql, &row, "%s:64,%d,%d,%d,%d",srv_name, &pids[0], &pids[1],&avfmts[0],&avfmts[1]) != AM_SUCCESS || row< 1)
		{
			AM_DEBUG(0, "Cannot get srv info for db_id = %d", db_srv_id);
			return AM_REC_ERR_INVALID_PARAM;
		}
		pid_cnt = 2;
	}
	else if (para->type == AM_REC_TYPE_PID)
	{
		strcpy(srv_name, "PID Record");
		pid_cnt =para->u.pid.pid_count;
		if (pid_cnt <= 0)
		{
			AM_DEBUG(0, "Invalid pid count %d", pid_cnt);
			return AM_REC_ERR_INVALID_PARAM;
		}
		if (pid_cnt > AM_DVR_MAX_PID_COUNT)
			pid_cnt = AM_DVR_MAX_PID_COUNT;
		memcpy(pids,para->u.pid.pids, pid_cnt*sizeof(int));
	}
	else if (para->type == AM_REC_TYPE_SERVICE)
	{
		db_srv_id = para->u.service.db_srv_id;
		row = 1;
		snprintf(sql, sizeof(sql), "select db_ts_id,name,vid_pid,aud1_pid, vid_fmt,aud1_fmt from srv_table where db_id=%d", db_srv_id);
		if (AM_DB_Select(rec->hdb, sql, &row, "%d,%s:64,%d,%d,%d,%d",
		&db_ts_id, srv_name, &pids[0], &pids[1],&avfmts[0],&avfmts[1]) != AM_SUCCESS || row< 1)
		{
			AM_DEBUG(0, "Cannot get srv info for db_id = %d", db_srv_id);
			return AM_REC_ERR_INVALID_PARAM;
		}
		pid_cnt = 2;
	}
	else if (para->type == AM_REC_TYPE_CHAN_NUM)
	{
		row = 1;
		snprintf(sql, sizeof(sql), "select db_ts_id,name,vid_pid,aud1_pid, vid_fmt,aud1_fmt from srv_table where chan_num=%d", para->u.chan_num.num);
		if (AM_DB_Select(rec->hdb, sql, &row, "%d,%s:64,%d,%d,%d,%d",
		&db_ts_id, srv_name, &pids[0], &pids[1],&avfmts[0],&avfmts[1]) != AM_SUCCESS || row< 1)
		{
			AM_DEBUG(0, "Cannot get srv info for chan_num = %d", para->u.chan_num.num);
			return AM_REC_ERR_INVALID_PARAM;
		}
		pid_cnt = 2;
	}

	/*生成录像PID信息*/
	snprintf(recpara, sizeof(recpara), "%d", pid_cnt);
	for (i=0; i<pid_cnt; i++)
	{
		snprintf(recpara, sizeof(recpara), "%s %d", recpara, pids[i]);
	}
	
	/*添加到数据库*/
	snprintf(sql, sizeof(sql), 
		"insert into rec_table(fend_no,dvr_no, db_ts_id, db_srv_id, db_evt_id, srv_name, evt_name, pids, start, duration, status,file_name,vid_fmt,aud1_fmt,aud2_fmt) values(%d,%d,%d,%d,%d,'%s','%s','%s',%d,%d,%d,'',%d,%d,%d)",
		rec->fend_dev, rec->dvr_dev, db_ts_id, db_srv_id, db_evt_id, srv_name, evt_name, recpara, start, duration, AM_REC_STAT_NOT_START,avfmts[0],avfmts[1],avfmts[2]);
	if (sqlite3_exec(rec->hdb, sql, NULL, NULL, NULL) != SQLITE_OK)
		return AM_REC_ERR_SQLITE;

	/*如果时事件录像，则更新事件预约状态*/
	if (db_evt_id != -1)
	{
		snprintf(sql, sizeof(sql), "update evt_table set sub_flag=2,sub_status=0 where db_id=%d", db_evt_id);
		sqlite3_exec(rec->hdb, sql, NULL, NULL, NULL);
	}
		
	return AM_SUCCESS;
}

/**\brief 取录像参数*/
static int am_rec_fill_rec_param(AM_REC_Recorder_t *rec)
{
	int db_rec_id, row, duration = 0;
	char sql[200];

	/*获取PID信息*/
	rec->rec_pids.pid_count = 0;
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		if (rec->rec_para.type == AM_REC_TYPE_EVENT)
		{
			AM_DEBUG(0, "Timeshifting dose not support the event type");
			return AM_REC_ERR_INVALID_PARAM;
		}
		else if (rec->rec_para.type == AM_REC_TYPE_PID)
		{
			/*直接获取PID*/
			rec->rec_pids.pid_count = rec->rec_para.u.pid.pid_count;
			if (rec->rec_para.u.pid.pid_count <= 0)
			{
				AM_DEBUG(0, "Invalid pid count %d", rec->rec_para.u.pid.pid_count);
				return AM_REC_ERR_INVALID_PARAM;
			}
			if (rec->rec_pids.pid_count > AM_DVR_MAX_PID_COUNT)
				rec->rec_pids.pid_count = AM_DVR_MAX_PID_COUNT;
			memcpy(rec->rec_pids.pids, rec->rec_para.u.pid.pids, rec->rec_pids.pid_count*sizeof(int));
		}
		else if (rec->rec_para.type == AM_REC_TYPE_SERVICE)
		{
			/*从srv_table查询音视频PID*/
			row = 1;
			snprintf(sql, sizeof(sql), "select vid_pid,aud1_pid,vid_fmt,aud1_fmt from srv_table where db_id=%d", rec->rec_para.u.service.db_srv_id);
			if (AM_DB_Select(rec->hdb, sql, &row, "%d,%d,%d,%d",&rec->rec_pids.pids[0], &rec->rec_pids.pids[1],&rec->vfmt, &rec->afmt1) != AM_SUCCESS || row< 1)
			{
				AM_DEBUG(0, "Cannot get srv info for db_id = %d", rec->rec_para.u.service.db_srv_id);
				return AM_REC_ERR_INVALID_PARAM;
			}
			rec->rec_pids.pid_count = 2;
		}
		else if (rec->rec_para.type == AM_REC_TYPE_CHAN_NUM)
		{
			row = 1;
			snprintf(sql, sizeof(sql), "select vid_pid,aud1_pid,vid_fmt,aud1_fmt from srv_table where chan_num=%d", rec->rec_para.u.chan_num.num);
			if (AM_DB_Select(rec->hdb, sql, &row, "%d,%d,%d,%d", &rec->rec_pids.pids[0], &rec->rec_pids.pids[1],&rec->vfmt, &rec->afmt1) != AM_SUCCESS || row< 1)
			{
				AM_DEBUG(0, "Cannot get srv info for chan_num = %d", rec->rec_para.u.chan_num.num);
				return AM_REC_ERR_INVALID_PARAM;
			}
			rec->rec_pids.pid_count = 2;
		}
	}
	else if (rec->rec_db_id != -1)
	{
		int i;
		char pid_para[256];
		char str[256];
		char str1[256];
		
		row = 1;
		snprintf(sql, sizeof(sql), "select duration,pids from rec_table where db_id=%d", rec->rec_db_id);
		if (AM_DB_Select(rec->hdb, sql, &row, "%d,%s:256", &duration, pid_para) != AM_SUCCESS || row< 1)
		{
			AM_DEBUG(0, "Cannot get record info for db_id= %d", rec->rec_db_id);
			return AM_REC_ERR_INVALID_PARAM;
		}
		/*取出PID信息*/
		sscanf(pid_para, "%d", &rec->rec_pids.pid_count);
		if (rec->rec_pids.pid_count > AM_DVR_MAX_PID_COUNT)
			rec->rec_pids.pid_count = AM_DVR_MAX_PID_COUNT;
		strcpy(str, "%*d");
		
		for (i=0; i<rec->rec_pids.pid_count; i++)
		{
			snprintf(str1, sizeof(str1), "%s %%d", str);
			sscanf(pid_para, str1, &rec->rec_pids.pids[i]);
			strcat(str, " %*d");
		}

	}
	if (rec->rec_pids.pid_count <= 0)
		return AM_REC_ERR_INVALID_PARAM;

	snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), "%s/DVBRecordFiles", rec->store_dir);
	/*尝试创建录像文件夹*/
	if (mkdir(rec->rec_file_name, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "Cannot create record store directory '%s', error: %s.", rec->rec_file_name, strerror(errno));	
		return AM_REC_ERR_CANNOT_OPEN_FILE;
	}
		
	/*设置输出文件名*/
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), "%s/DVBRecordFiles/REC_TimeShifting%d.ts", rec->store_dir, rec->av_dev);
		//snprintf(rec->rec_file_name, sizeof(rec->rec_file_name),"/mnt/sda/sda1/REC_TimeShifting0.ts");
		rec->rec_end_check_time = 0;
	}
	else
	{
		int now;
		time_t tnow;
		struct stat st;
		struct tm stim;

		/*新建一个不同名的文件名*/
		AM_EPG_GetUTCTime(&now);
		tnow = (time_t)now;
		gmtime_r(&tnow, &stim);
		srand(now);
		do
		{
			now = rand();
			snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), "%s/DVBRecordFiles/REC_%04d%02d%02d_%d.ts", 
				rec->store_dir, stim.tm_year + 1900, stim.tm_mon+1, stim.tm_mday, now);
			if (stat(rec->rec_file_name, &st) && errno == ENOENT)
				break;
		}while(1);

		if (rec->rec_db_id != -1)
		{
			AM_DEBUG(0, "Assign record file name to : %s", rec->rec_file_name);
			am_rec_db_update_record_file_name(rec->hdb, rec->rec_db_id, rec->rec_file_name + strlen(rec->store_dir) + 1);
		}
	
		/*设置定时录像结束检查时间*/
		if (duration > 0)
		{
			AM_TIME_GetClock(&rec->rec_end_check_time);
			//rec->rec_end_check_time += duration*1000;
			rec->rec_duration = duration*1000;
		}
		else
		{
			/*即时录像时不检查结束时间*/
			rec->rec_end_check_time = 0;
		}
	}
	
	return 0;
}

/**\brief 开始录像*/
static int am_rec_start_record(AM_REC_Recorder_t *rec)
{
	int rc, ret = 0;
	
	if (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		AM_DEBUG(1, "Already recording now, cannot start");
		ret = AM_REC_ERR_BUSY;
		goto start_end;
	}
	/*取录像相关参数*/
	if ((ret = am_rec_fill_rec_param(rec)) != 0)
		goto start_end;
		
	/*打开录像文件*/
	if (! (rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
	{
		rec->rec_fd = open(rec->rec_file_name, O_TRUNC|O_CREAT|O_WRONLY, 0666);
		if (rec->rec_fd == -1)
		{
			AM_DEBUG(1, "Cannot open record file '%s', cannot start", rec->rec_file_name);
			ret = AM_REC_ERR_CANNOT_OPEN_FILE;
			goto start_end;
		}
	}
	else
	{
		rec->rec_fd = -1;
	}
	rec->rec_start_time = 0;
	rec->rec_end_time = 0;
	rec->stat_flag |= REC_STAT_FL_RECORDING;
		
	rc = pthread_create(&rec->rec_thread, NULL, am_rec_record_thread, (void*)rec);
	if (rc)
	{
		AM_DEBUG(0, "%s", strerror(rc));
		rec->stat_flag &= ~REC_STAT_FL_RECORDING;
		close(rec->rec_fd);
		rec->rec_fd = -1;
		rec->rec_end_check_time = 0;
		rec->rec_db_id = -1;
		ret = AM_REC_ERR_CANNOT_CREATE_THREAD;
		goto start_end;
	}

start_end:
	if (ret != 0)
	{
		/*通知录像错误*/
		AM_EVT_Signal((int)rec, AM_REC_EVT_NOTIFY_ERROR, (void*)ret);
		if (ret != AM_REC_ERR_BUSY)
		{
			if (!(rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
			{
				am_rec_db_update_record_status(rec->hdb, rec->rec_db_id, AM_REC_STAT_COMPLETE);
				am_rec_db_update_record_duration(rec->hdb, rec->rec_db_id, 0);
				rec->rec_end_check_time = 0;
				rec->rec_db_id = -1;
			}
			AM_EVT_Signal((int)rec, AM_REC_EVT_RECORD_END, (void*)NULL);
		}
	}
	
	return ret;
}

/**\brief 停止录像*/
static int am_rec_stop_record(AM_REC_Recorder_t *rec)
{
	if (! (rec->stat_flag & REC_STAT_FL_RECORDING))
		return 0;

	rec->stat_flag &= ~REC_STAT_FL_RECORDING;
	/*As in record thread will call AM_EVT_Signal, wo must unlock it*/
	pthread_mutex_unlock(&rec->lock);
	pthread_join(rec->rec_thread, NULL);
	pthread_mutex_lock(&rec->lock);
	rec->rec_end_check_time = 0;
	rec->rec_db_id = -1;
	
	return 0;
}

/**\brief 检查是否开始Timeshifting播放*/
static void am_rec_check_timeshift_play(AM_REC_Recorder_t *rec)
{
	struct stat st;

	return;
	if (! (rec->stat_flag & REC_STAT_FL_TIMESHIFTING) ||
		rec->rec_fd == -1)
	{
		AM_DEBUG(1, "Timeshifting status error");
		rec->tshift_play_check_time = 0;
		return;
	}

	if (! fstat(rec->rec_fd, &st) && st.st_size >= REC_TIMESHIFT_PLAY_BUF_SIZE)
	{
		AM_DEBUG(1, "Timeshifting ready to play");
		rec->tshift_play_check_time = 0;
		AM_EVT_Signal((int)rec, AM_REC_EVT_TIMESHIFTING_READY, NULL);
	}
	else
	{
		/*设置进行下次检查*/
		AM_TIME_GetClock(&rec->tshift_play_check_time);
	}
}


/**\brief 检查是否有定时录像需要开始*/
static void am_rec_check_records(AM_REC_Recorder_t *rec)
{
	int now, row, db_rec_id, start;
	int expired[32];/*Note: 每轮查询最多处理的过期录像数*/
	char sql[512];

	AM_EPG_GetUTCTime(&now);

	/*查找是否当前正在进行的录像已被用户删除*/
	if (rec->stat_flag & REC_STAT_FL_RECORDING && rec->rec_db_id != -1)
	{
		snprintf(sql, sizeof(sql), "select start from rec_table where db_id=%d", rec->rec_db_id);
		row = 1;
		if (AM_DB_Select(rec->hdb, sql, &row, "%d", &start) == AM_SUCCESS && row <= 0)
		{
			AM_DEBUG(0, "***Warning: current recording not found in the database, now will stop the record***");
			am_rec_stop_record(rec);
		}
	}

	/*从rec_table中查找接下来即将开始的录像*/
	snprintf(sql, sizeof(sql), "select rec_table.db_id from rec_table left join srv_table on srv_table.db_id = rec_table.db_srv_id \
						where skip=0 and lock=0 and start>%d and start<=%d and status=%d and dvr_no=%d order by start limit 1",
						now, now+REC_NOTIFY_RECORD_TIME/1000, AM_REC_STAT_NOT_START, rec->dvr_dev);
	row = 1;
	if (AM_DB_Select(rec->hdb, sql, &row, "%d", &db_rec_id) == AM_SUCCESS && row > 0)
	{
		am_rec_db_update_record_status(rec->hdb, db_rec_id, AM_REC_STAT_WAIT_START);
		/*通知用户即将开始的录像*/
		/*由上层控制录像冲突*/
		/*if (rec->stat_flag & REC_STAT_FL_RECORDING)
		{
			AM_EVT_Signal((int)rec, AM_REC_EVT_NEW_RECORD_CONFLICT, (void*)db_rec_id);
		}
		else*/
		{
			AM_EVT_Signal((int)rec, AM_REC_EVT_NEW_RECORD, (void*)db_rec_id);
		}
	}

	/*从rec_table中查找立即开始的录像 )*/
	snprintf(sql, sizeof(sql), "select rec_table.db_id from rec_table left join srv_table on srv_table.db_id = rec_table.db_srv_id \
							   where skip=0 and lock=0 and start<=%d and start>=%d and (duration<0 or ((duration>0) and ((start+duration)>%d))) and \
						status<=%d and dvr_no=%d order by start limit 1", 
						now,  now-REC_RECORD_CHECK_TIME/1000, now, AM_REC_STAT_WAIT_START, rec->dvr_dev);
	row = 1;
	if (AM_DB_Select(rec->hdb, sql, &row, "%d", &db_rec_id) == AM_SUCCESS && row > 0)
	{
		am_rec_db_update_record_status(rec->hdb, db_rec_id, AM_REC_STAT_RECORDING);

		if (rec->stat_flag & REC_STAT_FL_RECORDING)
		{
			AM_DEBUG(0, "***Warning: A new record start now but now is already recording, start the new one***");
			am_rec_stop_record(rec);
		}
		if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
		{
			rec->stat_flag &= ~REC_STAT_FL_TIMESHIFTING;
			AM_AV_StopTimeshift(rec->av_dev);
		}
		
		/*通知用户录像现在开始*/
		AM_EVT_Signal((int)rec, AM_REC_EVT_RECORD_START, (void*)db_rec_id);
		
		/*立即开始录像*/
		rec->rec_db_id = db_rec_id;
		am_rec_start_record(rec);
		
	}

	/*从rec_table中查找过期的录像*/
	snprintf(sql, sizeof(sql), "select db_id from rec_table where (start<%d or ((duration>0) and ((start+duration)<%d))) and \
						status<=%d and dvr_no=%d", 
						now-REC_RECORD_CHECK_TIME/1000, now, AM_REC_STAT_WAIT_START, rec->dvr_dev);
	row = AM_ARRAY_SIZE(expired);
	if (AM_DB_Select(rec->hdb, sql, &row, "%d", expired) == AM_SUCCESS && row > 0)
	{
		int i;
		for (i=0; i<row; i++)
		{
			/*删除该记录*/
			AM_DEBUG(1, "@@Record %d expired.",  expired[i]);
			snprintf(sql, sizeof(sql), "delete from rec_table where db_id=%d",expired[i]);
			sqlite3_exec(rec->hdb, sql, NULL, NULL, NULL);
		}
	}
	
	/*设置进行下次检查*/
	AM_TIME_GetClock(&rec->rec_check_time);
}

/**\brief 计算最近的一次超时时间*/
static int am_rec_get_next_timeout_ms(AM_REC_Recorder_t *rec)
{
	int min = 0, now;

#define CHECK_EVENT(check, dis, func)\
	AM_MACRO_BEGIN\
		if (check) {\
			if ((now- (check+dis)) >= 0){\
				func(rec);\
				if (min == 0) \
					min = dis;\
				else\
					min = AM_MIN(min, dis);\
			} else if (min == 0){\
				min = (check+dis) - now;\
			} else {\
				AM_DEBUG(0, "min %d, check %d, dis %d, now %d, end %d, end-now %d", min, check, dis, now, check+dis, (check+dis) - now);\
				min = AM_MIN(min, (check+dis) - now);\
			}\
		}\
	AM_MACRO_END

	AM_TIME_GetClock(&now);

	/*定期检查是否有已标志的事件需要开始录像*/
	CHECK_EVENT(rec->rec_check_time, REC_RECORD_CHECK_TIME, am_rec_check_records);

	/*检查正在录像的事件是否结束时间已到*/
	CHECK_EVENT(rec->rec_end_check_time, rec->rec_duration, am_rec_stop_record);

	/*检查是否进行Timeshifting播放*/
	CHECK_EVENT(rec->tshift_play_check_time, REC_TSHIFT_PLAY_CHECK_TIME, am_rec_check_timeshift_play);

	AM_DEBUG(2, "REC next timeout is %d ms", min);

	return min;
}

/**\brief 录像管理线程*/
static void *am_rec_thread(void *arg)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)arg;
	AM_Bool_t go = AM_TRUE;
	int timeout, ret, evt;
	struct timespec rt;

	AM_TIME_GetClock(&rec->rec_check_time);
	
	pthread_mutex_lock(&rec->lock);
	
	/*Singal the waiting operations*/
	rec->evt = REC_EVT_NONE;
	pthread_cond_broadcast(&rec->cond);
	
	while (go)
	{
		timeout = am_rec_get_next_timeout_ms(rec);

		ret = 0;
		if (! timeout) 
		{
			ret = pthread_cond_wait(&rec->cond, &rec->lock);	
		}
		else
		{
			AM_TIME_GetTimeSpecTimeout(timeout, &rt);
			ret = pthread_cond_timedwait(&rec->cond, &rec->lock, &rt);
		}

		if (ret != ETIMEDOUT && rec->evt != REC_EVT_NONE)
		{
handle_event:
			evt = rec->evt;	
			switch (evt)
			{
				case REC_EVT_START_RECORD:
					am_rec_start_record(rec);
					break;
				case REC_EVT_STOP_RECORD:
					am_rec_stop_record(rec);
					break;
				case REC_EVT_START_TIMESHIFTING:
					if (! (rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						rec->stat_flag |= REC_STAT_FL_TIMESHIFTING;
						if (am_rec_start_record(rec) != 0)
						{
							rec->stat_flag &= ~REC_STAT_FL_TIMESHIFTING;
							AM_DEBUG(0, "Start timeshifting failed, cannot start record");
						}
						else
						{
							AM_AV_TimeshiftPara_t tpara;

							tpara.aud_fmt = rec->afmt1;
							tpara.vid_fmt = rec->vfmt;
							tpara.vid_id = (rec->rec_pids.pids[0]>=8191)?-1:rec->rec_pids.pids[0];
							tpara.aud_id = (rec->rec_pids.pids[1]>=8191)?-1:rec->rec_pids.pids[1];
							tpara.duration = rec->timeshift_duration;
							tpara.file_path = rec->rec_file_name;
							tpara.playback_only = AM_FALSE;
							tpara.dmx_id = rec->timeshift_dmx_dev;
							AM_AV_StartTimeshift(rec->av_dev, &tpara);
						}
					}
					break;
				case REC_EVT_PLAY_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_PlayTimeshift(rec->av_dev);
					}
					break;
				case REC_EVT_STOP_TIMESHIFTING:
					if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
					{
						AM_DEBUG(1, "Stop recording...");
						am_rec_stop_record(rec);
						AM_DEBUG(1, "Stop recording OK!");
						rec->stat_flag &= ~REC_STAT_FL_TIMESHIFTING;
						AM_AV_StopTimeshift(rec->av_dev);
					}
					break;
				case REC_EVT_PAUSE_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_PauseTimeshift(rec->av_dev);
					}
					break;
				case REC_EVT_RESUME_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_ResumeTimeshift(rec->av_dev);
					}
					break;
				case REC_EVT_FF_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_FastForwardTimeshift(rec->av_dev, rec->speed);
					}
					break;
				case REC_EVT_FB_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_FastBackwardTimeshift(rec->av_dev, rec->speed);
					}
					break;
				case REC_EVT_SEEK_TIMESHIFTING:
					if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING))
					{
						AM_AV_SeekTimeshift(rec->av_dev, rec->seek_pos, rec->seek_start);
					}
					break;
				case REC_EVT_QUIT:
					AM_DEBUG(0, "Rec thread will eixt...");
					go = AM_FALSE;
					break;
				default:
					break;
			}
			if (evt != rec->evt)
				goto handle_event;
				
			rec->evt = REC_EVT_NONE;
			pthread_cond_broadcast(&rec->cond);
		}
	}
	am_rec_stop_record(rec);
	pthread_mutex_unlock(&rec->lock);

	pthread_mutex_destroy(&rec->lock);
	pthread_cond_destroy(&rec->cond);

	AM_DEBUG(0, "Rec thread exit ok.");
	
	return NULL;
}

/****************************************************************************
 * API functions
 ***************************************************************************/
/**\brief 创建一个录像管理器
 * \param [in] para 创建参数
 * \param [out] handle 返回录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_Create(AM_REC_CreatePara_t *para, int *handle)
{
	AM_REC_Recorder_t *rec;
	int rc;
	pthread_mutexattr_t mta;
	
	assert(para && handle);

	*handle = 0;
	rec = (AM_REC_Recorder_t*)malloc(sizeof(AM_REC_Recorder_t));
	if (! rec)
	{
		AM_DEBUG(0, "no enough memory");
		return AM_REC_ERR_NO_MEM;
	}
	memset(rec, 0, sizeof(AM_REC_Recorder_t));
	rec->fend_dev = para->fend_dev;
	rec->hdb = para->hdb;
	rec->dvr_dev = para->dvr_dev;
	rec->fifo_id = para->async_fifo_id;
	rec->timeshift_duration = 10*60;
	strncpy(rec->store_dir, para->store_dir, sizeof(rec->store_dir)-1);

	/*初始化evt*/
	rec->evt = -1;
	rec->rec_db_id = -1;
	
	/*尝试创建输出文件夹*/
	if (mkdir(para->store_dir, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "**Waring:Cannot create store directory '%s'**.", para->store_dir);	
		/*Just give a warning message, do not return error*/
		/*free(rec);
		return AM_REC_ERR_INVALID_PARAM;*/
	}

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&rec->lock, &mta);
	pthread_cond_init(&rec->cond, NULL);
	pthread_mutexattr_destroy(&mta);
	
	rc = pthread_create(&rec->thread, NULL, am_rec_thread, (void*)rec);
	if (rc)
	{
		AM_DEBUG(0, "%s", strerror(rc));
		pthread_mutex_destroy(&rec->lock);
		pthread_cond_destroy(&rec->cond);
		free(rec);
		return AM_REC_ERR_CANNOT_CREATE_THREAD;
	}

	*handle = (int)rec;

	return AM_SUCCESS;
}

/**\brief 销毁一个录像管理器
 * param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_Destroy(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	pthread_t thread;
	
	assert(rec);

	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	rec->evt = REC_EVT_QUIT;
	thread = rec->thread;
	pthread_mutex_unlock(&rec->lock);
	pthread_cond_signal(&rec->cond);
	
	if (thread != pthread_self())
		pthread_join(thread, NULL);

	return AM_SUCCESS;
}

/**\brief 开始录像
 * \param handle 录像管理器句柄
 * \param db_rec_id 录像的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StartRecord(int handle, int db_rec_id)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(rec);
	if (db_rec_id < 0)
	{
		AM_DEBUG(0,"Invalid db_rec_id %d", db_rec_id);
		return AM_REC_ERR_INVALID_PARAM;
	}
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		AM_DEBUG(0, "Already recording, stop it first for new record");
		ret = AM_REC_ERR_BUSY;
	}
	else
	{
		rec->evt = REC_EVT_START_RECORD;
		rec->rec_db_id = db_rec_id;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	

	return ret;
}

/**\brief 停止录像
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StopRecord(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		rec->evt = REC_EVT_STOP_RECORD;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 增加一个定时录像
 * \param handle 录像管理器句柄
 * \param [in] para 定时录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_AddTimeRecord(int handle, AM_REC_TimeRecPara_t *para)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	int now;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(rec && para);
	if (para->rec_para.type > AM_REC_TYPE_PID)
	{
		AM_DEBUG(0,"Invalid record type %d", para->rec_para.type);
		return AM_REC_ERR_INVALID_PARAM;
	}
	AM_EPG_GetUTCTime(&now);
	if ((para->rec_para.type != AM_REC_TYPE_EVENT) && (
	((para->duration > 0) && ((para->start_time + para->duration) <= now)) 
	|| para->duration == 0))
	{
		AM_DEBUG(0,"Invalid start_time+duration=%d, now %d", para->start_time + para->duration, now);
		return AM_REC_ERR_INVALID_PARAM;
	}
	if (para->start_time < now)
		para->start_time = now;
		
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);

	/*添加一个录像到数据库*/
	ret = am_rec_db_add_record(rec, para->start_time, para->duration, &para->rec_para);
	if (ret == AM_SUCCESS)
	{
		/*强制进行一次录像检查*/
		AM_TIME_GetClock(&rec->rec_check_time);
		rec->rec_check_time -= REC_RECORD_CHECK_TIME;
		pthread_cond_signal(&rec->cond);
	}
	
	pthread_mutex_unlock(&rec->lock);
	
	return ret;
}

/**\brief 开始TimeShifting
 * \param handle 录像管理器句柄
 * \param [in] para 录像参数, para->type不能为AM_REC_TYPE_EVENT
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StartTimeShifting(int handle, AM_REC_TimeShiftingPara_t *para)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_DEBUG(0,"AM_REC_StartTimeShifting");
	assert(rec && para);
	if (para->rec_para.type > AM_REC_TYPE_PID || para->rec_para.type == AM_REC_TYPE_EVENT)
	{
		AM_DEBUG(0,"Invalid record type %d", para->rec_para.type);
		return AM_REC_ERR_INVALID_PARAM;
	}
	if (para->duration <= 0)
	{
		AM_DEBUG(0,"Invalid Timeshift duration %d", para->duration);
		return AM_REC_ERR_INVALID_PARAM;
	}

	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING || rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		AM_DEBUG(0, "Already in recording or timeshifting play, stop it first for new play");
		ret = AM_REC_ERR_BUSY;
	}
	else
	{
		rec->evt = REC_EVT_START_TIMESHIFTING;
		rec->av_dev = para->av_dev;
		rec->timeshift_duration = para->duration;
		rec->timeshift_dmx_dev = para->playback_dmx_dev;
		rec->rec_para = para->rec_para;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	

	return ret;
}

/**\brief 播放TimeShifting
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_PlayTimeShifting(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_PLAY_TIMESHIFTING;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 停止TimeShifting
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StopTimeShifting(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_STOP_TIMESHIFTING;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 暂停TimeShifting播放
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_PauseTimeShifting(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_PAUSE_TIMESHIFTING;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 恢复TimeShifting播放
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_ResumeTimeShifting(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_RESUME_TIMESHIFTING;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief TimeShifting快速向前播放
 * \param handle 录像管理器句柄
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_FastForwardTimeShifting(int handle, int speed)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_FF_TIMESHIFTING;
		rec->speed = speed;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief TimeShifting快速向后播放
 * \param handle 录像管理器句柄
  * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_FastBackwardTimeShifting(int handle, int speed)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_FB_TIMESHIFTING;
		rec->speed = speed;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief TimeShifting设定当前播放位置
 * \param handle 录像管理器句柄
 * \param pos 播放位置
 * \param start 是否开始播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SeekTimeShifting(int handle, int pos, AM_Bool_t start)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	am_rec_wait_events_done(rec);
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		rec->evt = REC_EVT_SEEK_TIMESHIFTING;
		rec->seek_pos = pos;
		rec->seek_start = start;
		pthread_cond_signal(&rec->cond);
	}
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 获取TimeShifting播放信息
 * \param handle 录像管理器句柄
  * \param [out] info 播放信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetTimeShiftingInfo(int handle, AM_AV_PlayStatus_t *info)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(info);

	memset(info, 0, sizeof(AM_AV_PlayStatus_t));
	if (rec)
	{
		pthread_mutex_lock(&rec->lock);
		if ((rec->stat_flag & REC_STAT_FL_TIMESHIFTING) && ! rec->tshift_play_check_time)
		{
			AM_AV_GetPlayStatus(rec->av_dev, info);
		}
		pthread_mutex_unlock(&rec->lock);
	}

	return AM_SUCCESS;
}

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SetUserData(int handle, void *user_data)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	if (rec)
	{
		pthread_mutex_lock(&rec->lock);
		rec->user_data = user_data;
		pthread_mutex_unlock(&rec->lock);
	}

	return AM_SUCCESS;
}

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetUserData(int handle, void **user_data)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(user_data);
	
	if (rec)
	{
		pthread_mutex_lock(&rec->lock);
		*user_data = rec->user_data;
		pthread_mutex_unlock(&rec->lock);
	}

	return AM_SUCCESS;
}

/**\brief 设置录像保存路径
 * \param handle Scan句柄
 * \param [in] path 路径
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SetRecordPath(int handle, const char *path)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && path);
	
	pthread_mutex_lock(&rec->lock);
	AM_DEBUG(1, "Set record store path to: '%s'", path);
	strncpy(rec->store_dir, path, sizeof(rec->store_dir)-1);
	pthread_mutex_unlock(&rec->lock);


	return AM_SUCCESS;
}

/**\brief 取当前录像时长
 * \param handle Scan句柄
 * \param [in] duration 秒为单位
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetRecordDuration(int handle, int *duration)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && duration);

	*duration = 0;
	pthread_mutex_lock(&rec->lock);
	if ((rec->stat_flag & REC_STAT_FL_RECORDING)&&rec->rec_start_time!=0)
	{
		int now;
		AM_TIME_GetClock(&now);
		*duration = (now - rec->rec_start_time)/1000;
	}
	pthread_mutex_unlock(&rec->lock);


	return AM_SUCCESS;
}

