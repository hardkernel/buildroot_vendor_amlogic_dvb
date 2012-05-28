/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
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
