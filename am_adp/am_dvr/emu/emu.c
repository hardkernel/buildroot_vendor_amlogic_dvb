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
 * \brief  emu dvr 驱动
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-12-13: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 2

#include <am_debug.h>
#include "../am_dvr_internal.h"

/****************************************************************************
 * Type definitions
 ***************************************************************************/


/****************************************************************************
 * Static data definitions
 ***************************************************************************/

static AM_ErrorCode_t emu_open(AM_DVR_Device_t *dev, const AM_DVR_OpenPara_t *para);
static AM_ErrorCode_t emu_close(AM_DVR_Device_t *dev);
static AM_ErrorCode_t emu_set_buf_size(AM_DVR_Device_t *dev, int size);
static AM_ErrorCode_t emu_poll(AM_DVR_Device_t *dev, int timeout);
static AM_ErrorCode_t emu_read(AM_DVR_Device_t *dev, uint8_t *buf, int *size);

const AM_DVR_Driver_t emu_dvr_drv = {
.open  			= emu_open,
.close 			= emu_close,
.set_buf_size   = emu_set_buf_size,
.poll           = emu_poll,
.read           = emu_read,
};


/****************************************************************************
 * Static functions
 ***************************************************************************/

static AM_ErrorCode_t emu_open(AM_DVR_Device_t *dev, const AM_DVR_OpenPara_t *para)
{
	return AM_DVR_ERR_NOT_SUPPORTED;
}

static AM_ErrorCode_t emu_close(AM_DVR_Device_t *dev)
{
	return AM_DVR_ERR_NOT_SUPPORTED;
}

static AM_ErrorCode_t emu_set_buf_size(AM_DVR_Device_t *dev, int size)
{
	return AM_DVR_ERR_NOT_SUPPORTED;
}

static AM_ErrorCode_t emu_poll(AM_DVR_Device_t *dev, int timeout)
{
	return AM_DVR_ERR_NOT_SUPPORTED;
}

static AM_ErrorCode_t emu_read(AM_DVR_Device_t *dev, uint8_t *buf, int *size)
{
	return AM_DVR_ERR_NOT_SUPPORTED;
}

