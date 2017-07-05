#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Description:
 */
/**\file
 * \brief 时钟、时间相关函数
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-05-21: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_time.h>
#include <am_mem.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#ifdef ANDROID
//#undef CLOCK_REALTIME
//#define CLOCK_REALTIME	CLOCK_MONOTONIC
#endif

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 得到开机到当前系统运行的时间，单位为毫秒
 * \param[out] clock 返回系统运行时间
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetClock(int *clock)
{
	struct timespec ts;
	int ms;
	
	assert(clock);
	
	clock_gettime(CLOCK_REALTIME, &ts);
	ms = ts.tv_sec*1000+ts.tv_nsec/1000000;
	*clock = ms;
	
	return AM_SUCCESS;
}

/**\brief 得到开机到当前系统运行的时间，格式为struct timespec
 * \param[out] ts 返回当前timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetTimeSpec(struct timespec *ts)
{
	assert(ts);
	
	clock_gettime(CLOCK_REALTIME, ts);
	
	return AM_SUCCESS;
}

/**\brief 得到若干毫秒后的timespec值
 * 此函数主要用于pthread_cond_timedwait, sem_timedwait等函数计算超时时间。
 * \param timeout 以毫秒为单位的超时时间
 * \param[out] ts 返回timespec值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_time.h)
 */
AM_ErrorCode_t AM_TIME_GetTimeSpecTimeout(int timeout, struct timespec *ts)
{
	struct timespec ots;
	int left, diff;
	
	assert(ts);
	
	clock_gettime(CLOCK_REALTIME, &ots);
	
	ts->tv_sec  = ots.tv_sec + timeout/1000;
	ts->tv_nsec = ots.tv_nsec;
	
	left = timeout % 1000;
	left *= 1000000;
	diff = 1000000000-ots.tv_nsec;

	if(diff<=left)
	{
		ts->tv_sec++;
		ts->tv_nsec = left-diff;
	}
	else
	{
		ts->tv_nsec += left;
	}

	return AM_SUCCESS;
}

