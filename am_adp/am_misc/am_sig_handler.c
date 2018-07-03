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
 * \brief pthread 信号注册
 *
 * \author Yan Yan <Yan.Yan@amlogic.com>
 * \date 2018-04-03: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <pthread.h>
#include <signal.h>

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void sig_handler(int signo)
{
	pthread_t tid =pthread_self();
	AM_DEBUG(AM_DEBUG_LEVEL, "signal handler, tid %ld, signo %d", tid, signo);
}

static void register_sig_handler()
{
	struct sigaction action = {0};
	action.sa_flags = 0;
	action.sa_handler = sig_handler;
	sigaction(SIGALRM, &action, NULL);
}

void AM_SigHandlerInit()
{
	pthread_once(&once, register_sig_handler);
}

