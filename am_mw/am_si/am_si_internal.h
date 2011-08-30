/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_si_internal.h
 * \brief SI Decoder 模块内部头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-15: create the document
 ***************************************************************************/

#ifndef _AM_SI_INTERNAL_H
#define _AM_SI_INTERNAL_H

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

/**\brief SI Decoder 数据*/
typedef struct
{
	void 		*prv_data;	/**< 私有数据,用于句柄检查*/
	AM_Bool_t   allocated;	/**< 是否已经分配*/
}SI_Decoder_t;


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/
 

#ifdef __cplusplus
}
#endif

#endif

