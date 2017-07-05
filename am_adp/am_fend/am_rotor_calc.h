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
 * \brief motor方位角计算库内部头文件
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2010-05-22: create the document
 ***************************************************************************/

#ifndef __AM_ROTOR_CALC_H__
#define __AM_ROTOR_CALC_H__

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

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

double AM_CalcSatHourangle( double SatLon, double SiteLat, double SiteLon );

#ifdef __cplusplus
}
#endif

#endif
