/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
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

