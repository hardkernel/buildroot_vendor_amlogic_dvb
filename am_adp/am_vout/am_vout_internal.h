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
 * \brief 视频输出模块内部头文件
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-13: create the document
 ***************************************************************************/

#ifndef _AM_VOUT_INTERNAL_H
#define _AM_VOUT_INTERNAL_H

#include <am_vout.h>
#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define VOUT_FL_RUN_CB   1

/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef struct AM_VOUT_Driver AM_VOUT_Driver_t;
typedef struct AM_VOUT_Device AM_VOUT_Device_t;

/**\brief 视频输出驱动*/
struct AM_VOUT_Driver
{
	AM_ErrorCode_t (*open)(AM_VOUT_Device_t *dev, const AM_VOUT_OpenPara_t *para);
	AM_ErrorCode_t (*set_format)(AM_VOUT_Device_t *dev, AM_VOUT_Format_t fmt);
	AM_ErrorCode_t (*enable)(AM_VOUT_Device_t *dev, AM_Bool_t enable);
	AM_ErrorCode_t (*close)(AM_VOUT_Device_t *dev);
};

/**\brief 视频输出设备*/
struct AM_VOUT_Device
{
	int                      dev_no;   /**< 设备号*/
	const AM_VOUT_Driver_t  *drv;      /**< 设备驱动*/
	void                    *drv_data; /**< 驱动私有数据*/
	AM_VOUT_Format_t         format;   /**< 视频输出格式*/
	pthread_mutex_t          lock;     /**< 设备数据保护互斥体*/
	AM_Bool_t                openned;  /**< 设备是否已经打开*/
	AM_Bool_t                enable;   /**< 是否输出视频信号*/
};

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

