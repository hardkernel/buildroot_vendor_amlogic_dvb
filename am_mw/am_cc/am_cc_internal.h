/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_cc_internal.h
 * \brief CC模块内部数据
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-12-27: create the document
 ***************************************************************************/

#ifndef _AM_CC_INTERNAL_H
#define _AM_CC_INTERNAL_H

#include <pthread.h>
#include <libzvbi.h>
#include <dtvcc.h>

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

enum
{
	AM_CC_EVT_SET_USER_OPTIONS,
};

enum
{
	FLASH_NONE,
	FLASH_SHOW,
	FLASH_HIDE
};

typedef struct AM_CC_Decoder AM_CC_Decoder_t;


/**\brief ClosedCaption数据*/
struct AM_CC_Decoder
{
	int evt;
	int vbi_pgno;
	int flash_stat;
	int timeout;
	AM_Bool_t running;
	AM_Bool_t render_flag;
	pthread_t render_thread;
	pthread_t data_thread;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	struct tvcc_decoder decoder;

	AM_CC_CreatePara_t cpara;
	AM_CC_StartPara_t spara;
};


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

