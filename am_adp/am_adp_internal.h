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
 * \brief AMLogic adaptor layer 内部头文件
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-05: create the document
 ***************************************************************************/

#ifndef _AM_ADP_INTERNAL_H
#define _AM_ADP_INTERNAL_H

#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Data definitions
 ***************************************************************************/

/**\brief ADP全局锁*/
extern pthread_mutex_t am_gAdpLock;

#ifdef __cplusplus
}
#endif

#endif

