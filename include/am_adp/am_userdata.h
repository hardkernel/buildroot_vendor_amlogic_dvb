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
/**\file am_userdata.h
 * \brief MPEG user data device module
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2013-3-13: create the document
 ***************************************************************************/

#ifndef _AM_USERDATA_H
#define _AM_USERDATA_H

#include <unistd.h>
#include <sys/types.h>
#include <am_types.h>

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

/**\brief Error code of the user data module*/
enum AM_USERDATA_ErrorCode
{
	AM_USERDATA_ERROR_BASE=AM_ERROR_BASE(AM_MOD_USERDATA),
	AM_USERDATA_ERR_INVALID_ARG,             /**< Invalid argument*/
	AM_USERDATA_ERR_INVALID_DEV_NO,          /**< Invalid device number*/
	AM_USERDATA_ERR_BUSY,                    /**< The device is busy*/
	AM_USERDATA_ERR_CANNOT_OPEN_DEV,         /**< Cannot open the device*/
	AM_USERDATA_ERR_NOT_SUPPORTED,           /**< Not supported*/
	AM_USERDATA_ERR_NO_MEM,                  /**< Not enough memory*/
	AM_USERDATA_ERR_TIMEOUT,                 /**< Timeout*/
	AM_USERDATA_ERR_SYS,                     /**< System error*/
	AM_USERDATA_ERR_END
};


/**\brief MPEG user data device open parameters*/
typedef struct
{
	int    foo;	
} AM_USERDATA_OpenPara_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief Open the MPEG user data device
 * \param dev_no Device number
 * \param[in] para Device open parameters
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern AM_ErrorCode_t AM_USERDATA_Open(int dev_no, const AM_USERDATA_OpenPara_t *para);

/**\brief Close the MPEG user data device
 * \param dev_no Device number
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern AM_ErrorCode_t AM_USERDATA_Close(int dev_no);

/**\brief Read MPEG user data from the device
 * \param dev_no Device number
 * \param[out] buf Output buffer to store the user data
 * \param size	Buffer length in bytes
 * \param timeout_ms Timeout time in milliseconds
 * \return Read data length in bytes
 */
extern int AM_USERDATA_Read(int dev_no, uint8_t *buf, int size, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif

