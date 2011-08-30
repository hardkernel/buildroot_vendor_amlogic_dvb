/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief AMLogic adaptor layer全局锁
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-05: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 0

#include <am_debug.h>
#include <am_mem.h>
#include "../am_adp_internal.h"

/****************************************************************************
 * Data definitions
 ***************************************************************************/

pthread_mutex_t am_gAdpLock = PTHREAD_MUTEX_INITIALIZER;

