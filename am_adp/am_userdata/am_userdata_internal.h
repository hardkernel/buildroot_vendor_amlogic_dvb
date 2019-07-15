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
 * \brief USER DATA module
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2013-3-13: create the document
 ***************************************************************************/

#ifndef _AM_USERDATA_INTERNAL_H
#define _AM_USERDATA_INTERNAL_H

#include <am_userdata.h>
#include <am_util.h>
#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define USERDATA_BUF_SIZE (5*1024)

/****************************************************************************
 * Type definitions
 ***************************************************************************/
 
/**\brief USERDATA设备*/
typedef struct AM_USERDATA_Device AM_USERDATA_Device_t;


typedef struct
{
	uint8_t           *data;
	ssize_t           size;
	ssize_t           pread;
	ssize_t           pwrite;
	int               error;
	
	pthread_cond_t	  cond;
}AM_USERDATA_RingBuffer_t;

/**\brief USERDATA设备驱动*/
typedef struct
{
	AM_ErrorCode_t (*open)(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para);
	AM_ErrorCode_t (*close)(AM_USERDATA_Device_t *dev);
	AM_ErrorCode_t (*set_mode)(AM_USERDATA_Device_t *dev, int mode);
	AM_ErrorCode_t (*get_mode)(AM_USERDATA_Device_t *dev, int *mode);
} AM_USERDATA_Driver_t;

/**\brief USERDATA设备*/
struct AM_USERDATA_Device
{
	int                 dev_no;  /**< 设备号*/
	const AM_USERDATA_Driver_t *drv;  /**< 设备驱动*/
	void               *drv_data;/**< 驱动私有数据*/
	int                 open_cnt;   /**< 设备打开计数*/
	pthread_mutex_t     lock;    /**< 设备保护互斥体*/
	AM_USERDATA_RingBuffer_t pkg_buf;

	int (*write_package)(AM_USERDATA_Device_t *dev, const uint8_t *buf, size_t size);

};



/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

