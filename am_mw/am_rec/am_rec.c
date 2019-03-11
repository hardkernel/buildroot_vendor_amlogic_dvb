#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file am_rec.c
 * \brief 录像管理模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-03-30: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 2
#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
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
#include "am_misc.h"

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

#define DVB_STB_ASYNCFIFO_FLUSHSIZE_FILE "/sys/class/stb/asyncfifo0_flush_size"

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 获取录像文件长度*/
static inline long long am_rec_get_file_size(AM_REC_Recorder_t *rec)
{
	long long size = 0;
	struct stat statbuff;  
		
	if(stat(rec->rec_file_name, &statbuff) >= 0)
	{  
		size = statbuff.st_size;
	}
	
	return size;
}

static void am_rec_packet_data(int fd, int pid, const uint8_t *data, int size, int appearence)
{
	uint8_t buf[188];
	int left, cc = 0;
	int data_start, data_len;
	AM_Bool_t payload_start = (pid == 0) ? AM_TRUE : AM_FALSE;

	buf[0] = 0x47;
	buf[1] = ((uint8_t)(pid>>8)&0x1f);
	buf[2] = pid&0xff;
		
	while (appearence > 0)
	{
		left = size;
		while (left > 0)
		{
			if (left == size && payload_start)
			{
				buf[1] |= 0x40;
				buf[4] = 0x0; /* pointer_field */
				data_start = 5;
			}
			else
			{
				buf[1] &= ~0x40;
				data_start = 4;
			}
			
			buf[3] = 0x10 + (cc%0x10);
			data_len = 188 - data_start;
			
			if (left >= data_len)
			{
				memcpy(buf+data_start, &data[size-left], data_len);
				left -= data_len;
			}
			else
			{
				memset(buf+data_start, 0xff, data_len);
				memcpy(buf+data_start, &data[size-left], left);
				left = 0;
			}
			
			cc++;	

			/* write this ts packet to file */
			write(fd, buf, sizeof(buf));
		}

		appearence--;
	}
}

static AM_ErrorCode_t am_rec_insert_pat(AM_REC_Recorder_t *rec)
{
	dvbpsi_psi_section_t *psec;
	dvbpsi_pat_t pat;
	dvbpsi_pat_program_t program;

	program = rec->rec_para.program;
	program.p_next = NULL;

	pat.b_current_next = 1;
	pat.i_table_id = 0;
	pat.i_ts_id = 0x1;
	pat.i_version = 0;
	pat.p_first_program = &program;
	pat.p_next = NULL;
	
	psec = dvbpsi_GenPATSections(&pat, 1);
	if (psec == NULL)
	{
		AM_DEBUG(1, "Cannot insert PAT to file!");
		return AM_REC_ERROR_BASE;
	}

	am_rec_packet_data(rec->rec_fd, 0, psec->p_data, psec->i_length + 3, 10);

	dvbpsi_DeletePSISections(psec);

	return AM_SUCCESS;
}

static AM_ErrorCode_t am_rec_insert_file_header(AM_REC_Recorder_t *rec)
{
    uint8_t buf[188];
	uint8_t stream[sizeof(AM_REC_MediaInfo_t)];
    uint16_t header_pid = 0x1234;
	int pos = 0, i, left, npacket;
	AM_REC_MediaInfo_t *pp = &rec->rec_para.media_info; 

#define WRITE_INT(_i)\
	AM_MACRO_BEGIN\
		stream[pos++] = (uint8_t)(((_i)&0xff000000) >> 24);\
		stream[pos++] = (uint8_t)(((_i)&0x00ff0000) >> 16);\
		stream[pos++] = (uint8_t)(((_i)&0x0000ff00) >> 8);\
		stream[pos++] = (uint8_t)((_i)&0x000000ff);\
	AM_MACRO_END

	WRITE_INT(0);
	memcpy(stream+pos, pp->program_name, sizeof(pp->program_name));
	pos += sizeof(pp->program_name);
	WRITE_INT(pp->vid_pid);
	WRITE_INT(pp->vid_fmt);

	if (pp->aud_cnt > (int)AM_ARRAY_SIZE(pp->audios))
		pp->aud_cnt = AM_ARRAY_SIZE(pp->audios);
	WRITE_INT(pp->aud_cnt);
	for (i=0; i<pp->aud_cnt; i++)
	{
		WRITE_INT(pp->audios[i].pid);
		WRITE_INT(pp->audios[i].fmt);
		memcpy(stream+pos, pp->audios[i].lang, sizeof(pp->audios[i].lang));
		pos += sizeof(pp->audios[i].lang);
	}
	
	if (pp->sub_cnt > (int)AM_ARRAY_SIZE(pp->subtitles))
		pp->sub_cnt = AM_ARRAY_SIZE(pp->subtitles);
	WRITE_INT(pp->sub_cnt);
	for (i=0; i<pp->sub_cnt; i++)
	{
		WRITE_INT(pp->subtitles[i].pid);
		WRITE_INT(pp->subtitles[i].type);
		WRITE_INT(pp->subtitles[i].composition_page);
		WRITE_INT(pp->subtitles[i].ancillary_page);
		WRITE_INT(pp->subtitles[i].magzine_no);
		WRITE_INT(pp->subtitles[i].page_no);
		memcpy(stream+pos, pp->subtitles[i].lang, sizeof(pp->subtitles[i].lang));
		pos += sizeof(pp->subtitles[i].lang);
	}
	
	if (pp->ttx_cnt > (int)AM_ARRAY_SIZE(pp->teletexts))
		pp->ttx_cnt = AM_ARRAY_SIZE(pp->teletexts);
	WRITE_INT(pp->ttx_cnt);
	for (i=0; i<pp->ttx_cnt; i++)
	{
		WRITE_INT(pp->teletexts[i].pid);
		WRITE_INT(pp->teletexts[i].magzine_no);
		WRITE_INT(pp->teletexts[i].page_no);
		memcpy(stream+pos, pp->teletexts[i].lang, sizeof(pp->teletexts[i].lang));
		pos += sizeof(pp->teletexts[i].lang);
	}
	
    /* In order to be compatible with libplayer, we pack it as a ts packet */
	lseek64(rec->rec_fd, 0, SEEK_SET);
	am_rec_packet_data(rec->rec_fd, header_pid, stream, pos, 1);
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t am_rec_update_record_time(AM_REC_Recorder_t *rec)
{
	int now, rec_time;
	off64_t cur_pos;
	uint8_t buf[4];

	if (rec->rec_start_time != 0)
	{
		cur_pos = lseek64(rec->rec_fd, 0, SEEK_CUR);
		
		AM_TIME_GetClock(&now);
		rec_time = (now - rec->rec_start_time)/1000;
		lseek64(rec->rec_fd, 4, SEEK_SET);

		buf[0] = (uint8_t)(((rec_time)&0xff000000) >> 24);
		buf[1] = (uint8_t)(((rec_time)&0x00ff0000) >> 16);
		buf[2] = (uint8_t)(((rec_time)&0x0000ff00) >> 8);
		buf[3] = (uint8_t)((rec_time)&0x000000ff);

		write(rec->rec_fd, buf, sizeof(buf));

		lseek64(rec->rec_fd, cur_pos, SEEK_SET);
	}	

	return AM_SUCCESS;
}

static AM_ErrorCode_t am_rec_gen_next_file_name(AM_REC_Recorder_t *rec, const char *prefix, const char *suffix)
{	
	struct stat st;
	
	do
	{
		if (rec->rec_file_index <= 1)
		{
			snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
				"%s/%s.%s", rec->create_para.store_dir, prefix, suffix);
		}
		else
		{
			snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
				"%s/%s-%d.%s", rec->create_para.store_dir, prefix, 
				rec->rec_file_index, suffix);
		}
		
		if (!stat(rec->rec_file_name, &st))
		{
			AM_DEBUG(1, "PVR file '%s' already exist.", rec->rec_file_name);
			rec->rec_file_index++;
			
			continue;
		}
		else if (errno == ENOENT)
		{
			AM_DEBUG(1, "PVR file is assigned to '%s'", rec->rec_file_name);
			break;
		}
		else
		{
			AM_DEBUG(1, "Searching PVR files failed, error: %s", strerror(errno));
			if (errno == EACCES)
				return AM_REC_ERR_CANNOT_ACCESS_FILE;
			
			return AM_REC_ERR_CANNOT_OPEN_FILE;
		}	

		if (rec->rec_file_index == INT_MAX)
		{
			/* Is this possible? */
			AM_DEBUG(1, "Too many sub-files!");
			return AM_REC_ERR_CANNOT_OPEN_FILE;
		}
		
		rec->rec_file_index++;
	}while(1);

	return AM_SUCCESS;
}

static AM_ErrorCode_t am_rec_auto_switch_file(AM_REC_Recorder_t *rec)
{
	if (rec->rec_fd >= 0)
	{
		close(rec->rec_fd);
		rec->rec_fd = -1;
	}

	/* try to create the new record sub-file */
	AM_TRY(am_rec_gen_next_file_name(rec, rec->rec_para.prefix_name, rec->rec_para.suffix_name));

	rec->rec_fd = open(rec->rec_file_name, O_TRUNC|O_CREAT|O_WRONLY, 0666);
	if (rec->rec_fd == -1)
	{
		AM_DEBUG(1, "Cannot open new file '%s', record aborted.", rec->rec_file_name);
		return AM_REC_ERR_CANNOT_OPEN_FILE;
	}

	am_rec_insert_file_header(rec);

	am_rec_insert_pat(rec);

	return AM_SUCCESS;
}

static AM_ErrorCode_t am_rec_start_dvr(AM_REC_Recorder_t *rec)
{
	int i;
	AM_DVR_StartRecPara_t spara;
	AM_REC_MediaInfo_t *minfo = &rec->rec_para.media_info;

#define ADD_PID(_pid)\
	AM_MACRO_BEGIN\
		if ((_pid) < 0x1fff && spara.pid_count < AM_DVR_MAX_PID_COUNT)\
			spara.pids[spara.pid_count++] = (_pid);\
	AM_MACRO_END

	spara.pid_count = 0;
	ADD_PID(minfo->vid_pid);
	for (i=0; i<minfo->aud_cnt; i++)
	{
		ADD_PID(minfo->audios[i].pid);
	}
	for (i=0; i<minfo->sub_cnt; i++)
	{
		ADD_PID(minfo->subtitles[i].pid);
	}
	for (i=0; i<minfo->ttx_cnt; i++)
	{
		ADD_PID(minfo->teletexts[i].pid);
	}

	ADD_PID(rec->rec_para.program.i_pid);
	
	return AM_DVR_StartRecord(rec->create_para.dvr_dev, &spara);
}

/**\brief 写录像数据到文件*/
static int am_rec_data_write(AM_REC_Recorder_t *rec, uint8_t *buf, int size)
{
	int ret;
	int left = size;
	uint8_t *p = buf;

	while (left > 0)
	{
		ret = write(rec->rec_fd, p, left);
		if (ret == -1)
		{
			if (errno == EFBIG)
			{
				AM_DEBUG(1, "EFBIG detected, automatically write to a new file!");
				
				if (am_rec_auto_switch_file(rec) != AM_SUCCESS)
					break;
			}
			else if (errno != EINTR)
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

/**\brief 录像线程*/
static void *am_rec_record_thread(void* arg)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)arg;
	int cnt, err = 0, check_time, update_time=0;
	int retry_count = 0;
	uint8_t buf[256*1024];
	AM_DVR_OpenPara_t para;
	AM_DVR_StartRecPara_t spara;
	AM_REC_RecEndPara_t epara;
	const int UPDATE_TIME_PERIOD = 1000;
	const int stat_flag = rec->stat_flag;
	
	memset(&epara, 0, sizeof(epara));
	epara.hrec = rec;
	/*设置DVR设备参数*/
	memset(&para, 0, sizeof(para));
	if (AM_DVR_Open(rec->create_para.dvr_dev, &para) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Open DVR%d failed", rec->create_para.dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_file;
	}

	AM_DVR_SetSource(rec->create_para.dvr_dev, rec->create_para.async_fifo_id);
	
	if (am_rec_start_dvr(rec) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Start DVR%d failed", rec->create_para.dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_dvr;
	}

	AM_EVT_Signal((long)rec, AM_REC_EVT_RECORD_START, NULL);

	/*从DVR设备读取数据并存入文件*/
	while (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		if (rec->rec_start_time != 0)
		{
			AM_TIME_GetClock(&check_time);
			if (rec->rec_para.total_time > 0 && 
				(check_time-rec->rec_start_time) >= (rec->rec_para.total_time*1000))
			{
				AM_DEBUG(1, "Reach record end time, now will stop recording...");
				am_rec_update_record_time(rec);
				break;
			}
			
			if (update_time == 0 || (check_time-update_time) >= UPDATE_TIME_PERIOD)
			{
				am_rec_update_record_time(rec);
				update_time = check_time;
			}
		}
		retry_count = 5;
		cnt = AM_DVR_Read(rec->create_para.dvr_dev, buf, sizeof(buf), 50);
		if (cnt <= 0)
		{
			while (retry_count--) {
				if  (rec->stat_flag & REC_STAT_FL_RECORDING)
					usleep(10*1000);
				else
					break;
			}
			if  (rec->stat_flag & REC_STAT_FL_RECORDING) {
				AM_DEBUG(1, "No data available from DVR%d", rec->create_para.dvr_dev);
				continue;
			} else
				break;
		}

		if (rec->stat_flag & REC_STAT_FL_PAUSED)
			continue;

		if (rec->rec_para.is_timeshift)
		{
			if (rec->tfile_flag & REC_TFILE_FLAG_AUTO_CREATE)
			{
				if (AM_TFile_Write(rec->tfile, buf, cnt) != cnt)
				{
					err = AM_REC_ERR_CANNOT_WRITE_FILE;
					break;
				}
			}
			else
			{
				if (AM_AV_TimeshiftFillData(0, buf, cnt) != AM_SUCCESS)
				{
					err = AM_REC_ERR_CANNOT_WRITE_FILE;
					break;
				}
			}
		}
		else
		{
			if (am_rec_data_write(rec, buf, cnt) != cnt)
			{
				err = AM_REC_ERR_CANNOT_WRITE_FILE;
				break;
			}
			else if (! rec->rec_start_time)
			{
				/*已有数据写入文件，记录开始时间*/
				AM_TIME_GetClock(&rec->rec_start_time);
			}
		}
	}

close_dvr:
	AM_DVR_StopRecord(rec->create_para.dvr_dev);
	AM_DVR_Close(rec->create_para.dvr_dev);
close_file:
	if (rec->rec_fd != -1)
	{
		close(rec->rec_fd);
		rec->rec_fd = -1;
	}
	AM_DEBUG(1, "ready to stop, %d %p", rec->tfile_flag, rec->tfile);
	if (!(rec->tfile_flag & REC_TFILE_FLAG_DETACH) && rec->tfile)
	{
	    AM_DEBUG(1, "TFile Close");
		AM_TFile_Close(rec->tfile);
	}

	/* Maybe there is no space left in disk, we need to remove this empty file */
	if (err == AM_REC_ERR_CANNOT_WRITE_FILE &&
		am_rec_get_file_size(rec) <= 0)
	{
		AM_DEBUG(1, "unliking empty file: %s", rec->rec_file_name);
		unlink(rec->rec_file_name);
	}

	if (! rec->rec_para.is_timeshift)
	{
		int duration = 0;
	
		if (rec->rec_start_time != 0)
		{
			AM_TIME_GetClock(&check_time);
			duration = check_time - rec->rec_start_time;
		}
		duration /= 1000;
		epara.total_size = am_rec_get_file_size(rec);
		epara.total_time = duration;
		
		AM_DEBUG(1, "Record end , duration %d:%02d:%02d", duration/3600, (duration%3600)/60, duration%60);
	}

	epara.error_code = err;

	if ((stat_flag & REC_STAT_FL_RECORDING))
	{
		/*通知录像结束*/
		AM_EVT_Signal((long)rec, AM_REC_EVT_RECORD_END, (void*)&epara);
		rec->stat_flag &= ~REC_STAT_FL_RECORDING;
		rec->stat_flag &= ~REC_STAT_FL_PAUSED;
	}
	
	return NULL;
}


/**\brief 取录像参数*/
static int am_rec_fill_rec_param(AM_REC_Recorder_t *rec, AM_REC_RecPara_t *start_para)
{
	snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
		"%s", rec->create_para.store_dir);	
	/*尝试创建录像文件夹*/
	if (mkdir(rec->rec_file_name, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "Cannot create record store directory '%s', error: %s.", 
			rec->rec_file_name, strerror(errno));	
		if (errno == EACCES)
			return AM_REC_ERR_CANNOT_ACCESS_FILE;
			
		return AM_REC_ERR_CANNOT_OPEN_FILE;
	}
		
	/*设置输出文件名*/
	if (! start_para->is_timeshift)
	{
		rec->rec_file_index = 1;
		return am_rec_gen_next_file_name(rec, start_para->prefix_name, start_para->suffix_name);
	}
	else
	{
		snprintf(rec->rec_file_name, sizeof(rec->rec_file_name),
			"%s/%s.%s", rec->create_para.store_dir, start_para->prefix_name, start_para->suffix_name);
	}
	
	return 0;
}

/**\brief 开始录像*/
static int am_rec_start_record(AM_REC_Recorder_t *rec, AM_REC_RecPara_t *start_para)
{
	int rc, ret = 0;
	
	if (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		AM_DEBUG(1, "Already recording now, cannot start");
		ret = AM_REC_ERR_BUSY;
		goto start_end;
	}
	/*取录像相关参数*/
	if ((ret = am_rec_fill_rec_param(rec, start_para)) != 0)
		goto start_end;
		
	/*打开录像文件*/
	rec->rec_fd = -1;
	rec->rec_para = *start_para;

	if (! rec->rec_para.is_timeshift)
	{
		rec->rec_fd = open(rec->rec_file_name, O_TRUNC|O_CREAT|O_WRONLY, 0666);
		if (rec->rec_fd == -1)
		{
			AM_DEBUG(1, "Cannot open record file '%s', cannot start", rec->rec_file_name);
			ret = AM_REC_ERR_CANNOT_OPEN_FILE;
			goto start_end;
		}

		char buf[64];
		snprintf(buf, sizeof(buf), "%d", 32*1024);

		AM_FileEcho(DVB_STB_ASYNCFIFO_FLUSHSIZE_FILE, buf);

		am_rec_insert_file_header(rec);
		am_rec_insert_pat(rec);
	}
	else if ((rec->tfile_flag & REC_TFILE_FLAG_AUTO_CREATE) && !rec->tfile)
	{
		//timeshifting use tfile
		ret = AM_TFile_Open(&rec->tfile, rec->rec_file_name, AM_TRUE, rec->rec_para.total_time, rec->rec_para.total_size);
		if (ret != AM_SUCCESS)
		{
			AM_DEBUG(1, "Cannot open timeshifting file '%s', cannot start", rec->rec_file_name);
			goto start_end;
		}
		AM_DEBUG(1, "create Tfile %p", rec->tfile);
	}

	rec->rec_start_time = 0;
	rec->stat_flag |= REC_STAT_FL_RECORDING;
	rc = pthread_create(&rec->rec_thread, NULL, am_rec_record_thread, (void*)rec);
	if (rc)
	{
		AM_DEBUG(0, "Create record thread failed: %s", strerror(rc));
		ret = AM_REC_ERR_CANNOT_CREATE_THREAD;
		goto start_end;
	}

start_end:
	AM_DEBUG(1, "start record return %d", ret);
	if (ret != 0)
	{
		rec->stat_flag &= ~REC_STAT_FL_RECORDING;
		if (rec->rec_fd != -1)
			close(rec->rec_fd);
		rec->rec_fd = -1;
		rec->rec_start_time = 0;
		if (ret != AM_REC_ERR_BUSY)
		{
			AM_REC_RecEndPara_t epara;
			epara.hrec = rec;
			epara.error_code = ret;
			epara.total_size = 0;
			epara.total_time = 0;
			AM_EVT_Signal((long)rec, AM_REC_EVT_RECORD_END, (void*)&epara);
		}
	}

	return ret;
}

/**\brief 停止录像*/
static int am_rec_stop_record(AM_REC_Recorder_t *rec)
{
	rec->stat_flag &= ~REC_STAT_FL_PAUSED;

	if (! (rec->stat_flag & REC_STAT_FL_RECORDING))
		return 0;

	rec->stat_flag &= ~REC_STAT_FL_RECORDING;
	/*As in record thread will call AM_EVT_Signal, wo must unlock it*/
	pthread_mutex_unlock(&rec->lock);
	pthread_join(rec->rec_thread, NULL);
	pthread_mutex_lock(&rec->lock);
	rec->rec_start_time = 0;
	
	return 0;
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
AM_ErrorCode_t AM_REC_Create(AM_REC_CreatePara_t *para, AM_REC_Handle_t *handle)
{
	AM_REC_Recorder_t *rec;
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
	rec->create_para = *para;

	/*尝试创建输出文件夹*/
	if (mkdir(para->store_dir, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "**Waring:Cannot create store directory '%s'**.", para->store_dir);	
	}

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&rec->lock, &mta);
	pthread_mutexattr_destroy(&mta);

	*handle = rec;

	return AM_SUCCESS;
}

/**\brief 销毁一个录像管理器
 * param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_Destroy(AM_REC_Handle_t handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	pthread_t thread;

	assert(rec);

	AM_REC_StopRecord(handle);
	pthread_mutex_destroy(&rec->lock);
	free(rec);

	return AM_SUCCESS;
}

/**\brief 开始录像
 * \param handle 录像管理器句柄
 * \param [in] start_para 录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StartRecord(AM_REC_Handle_t handle, AM_REC_RecPara_t *start_para)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec && start_para);
	pthread_mutex_lock(&rec->lock);
	ret = am_rec_start_record(rec, start_para);
	pthread_mutex_unlock(&rec->lock);

	return ret;
}

/**\brief 停止录像
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StopRecord(AM_REC_Handle_t handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec);

	pthread_mutex_lock(&rec->lock);
	ret = am_rec_stop_record(rec);
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

AM_ErrorCode_t AM_REC_PauseRecord(AM_REC_Handle_t handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec);

	pthread_mutex_lock(&rec->lock);
	rec->stat_flag |= REC_STAT_FL_PAUSED;
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

AM_ErrorCode_t AM_REC_ResumeRecord(AM_REC_Handle_t handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec);

	pthread_mutex_lock(&rec->lock);
	rec->stat_flag &= ~REC_STAT_FL_PAUSED;
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SetUserData(AM_REC_Handle_t handle, void *user_data)
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
AM_ErrorCode_t AM_REC_GetUserData(AM_REC_Handle_t handle, void **user_data)
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
AM_ErrorCode_t AM_REC_SetRecordPath(AM_REC_Handle_t handle, const char *path)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && path);
	
	pthread_mutex_lock(&rec->lock);
	AM_DEBUG(1, "Set record store path to: '%s'", path);
	strncpy(rec->create_para.store_dir, path, sizeof(rec->create_para.store_dir)-1);
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

/**\brief 获取当前录像信息
 * \param handle 录像管理器句柄
 * \param [out] info 当前录像信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetRecordInfo(AM_REC_Handle_t handle, AM_REC_RecInfo_t *info)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && info);

	memset(info, 0, sizeof(AM_REC_RecInfo_t));
	pthread_mutex_lock(&rec->lock);
	if ((rec->stat_flag & REC_STAT_FL_RECORDING)&&rec->rec_start_time!=0)
	{
		int now;
		
		strncpy(info->file_path, rec->rec_file_name, sizeof(info->file_path));
		info->file_size = am_rec_get_file_size(rec);
		AM_TIME_GetClock(&now);
		info->cur_rec_time = (now - rec->rec_start_time)/1000;
		info->create_para = rec->create_para;
		info->record_para = rec->rec_para;
	}
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

AM_ErrorCode_t AM_REC_SetTFile(AM_REC_Handle_t handle, AM_TFile_t tfile, int flag)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec && tfile);
	pthread_mutex_lock(&rec->lock);
	rec->tfile = tfile;
	rec->tfile_flag = flag;
	pthread_mutex_unlock(&rec->lock);

	return ret;
}

AM_ErrorCode_t AM_REC_GetTFile(AM_REC_Handle_t handle, AM_TFile_t *tfile, int *flag)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(rec && tfile);
	pthread_mutex_lock(&rec->lock);
	*tfile = rec->tfile;
	if (flag)
		*flag = rec->tfile_flag;
	pthread_mutex_unlock(&rec->lock);

	return ret;
}

