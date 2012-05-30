/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_fend_ctrl.c
 * \brief Frontend控制模块
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2012-05-06: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 3

#include <am_debug.h>

#include "am_fend.h"

#include "am_fend_ctrl_internal.h"
#include "am_sec_internal.h"
#include "am_fend_ctrl.h"

#include <assert.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

	
/****************************************************************************
 * Static data
 ***************************************************************************/


/****************************************************************************
 * Static functions
 ***************************************************************************/


/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 设定前端参数
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_FENDCTRL_SetPara(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para)
{
	assert(para);
	
	AM_ErrorCode_t ret = AM_SUCCESS;

	switch(para->m_type)
	{
		case AM_FEND_DEMOD_DVBS:
			{
				/* process sec */
				ret = AM_SEC_PrepareTune(dev_no, &(para->sat), NULL, 0);
				break;
			}
		case AM_FEND_DEMOD_DVBC:
			ret = AM_FEND_SetPara(dev_no, &(para->cable.para));
			break;
		case AM_FEND_DEMOD_DVBT:
			ret = AM_FEND_SetPara(dev_no, &(para->terrestrial.para));
			break;
		case AM_FEND_DEMOD_ATSC:
			ret = AM_FEND_SetPara(dev_no, &(para->atsc.para));
			break;	
		default:
			break;
	}

	return ret;
}

/**\brief 设定前端设备参数，并等待参数设定完成
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \param[out] status 返回前端设备状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_FENDCTRL_Lock(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para, fe_status_t *status)
{
	assert(para);
	assert(status);
	
	AM_ErrorCode_t ret = AM_SUCCESS;

	switch(para->m_type)
	{
		case AM_FEND_DEMOD_DVBS:
			{
				/* process sec */
				ret = AM_SEC_PrepareTune(dev_no, &(para->sat), status, 5000);
			}
			break;
		case AM_FEND_DEMOD_DVBC:
			AM_FEND_Lock(dev_no, &(para->cable.para), status);
			break;
		case AM_FEND_DEMOD_DVBT:
			AM_FEND_Lock(dev_no, &(para->terrestrial.para), status);
			break;
		case AM_FEND_DEMOD_ATSC:
			AM_FEND_Lock(dev_no, &(para->atsc.para), status);
			break;	
		default:
			break;
	}

	return ret;
}


