/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_sec.c
 * \brief SEC卫星设备控制模块模块
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2012-05-06: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 3

#include <am_debug.h>

#include "am_sec_internal.h"
#include "am_fend_ctrl.h"

#include "am_fend.h"
#include "am_fend_diseqc_cmd.h"

#include <string.h>
#include <assert.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define M_DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC (25)
#define M_DELAY_AFTER_FINAL_CONT_TONE_CHANGE (10)
#define M_DELAY_AFTER_FINAL_VOLTAGE_CHANGE (10)
#define M_DELAY_BETWEEN_DISEQC_REPEATS (120)
#define M_DELAY_AFTER_LAST_DISEQC_CMD (50)
#define M_DELAY_AFTER_TONEBURST (50)
#define M_DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS (200)
#define M_DELAY_BETWEEN_SWITCH_AND_MOTOR_CMD (700)
#define M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MEASURE_IDLE_INPUTPOWER (500)
#define M_DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_MOTOR_CMD (900)
#define M_DELAY_AFTER_MOTOR_STOP_CMD (500)
#define M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD (500)
#define M_DELAY_BEFORE_SEQUENCE_REPEAT (70)
#define M_MOTOR_COMMAND_RETRIES (1)
#define M_MOTOR_RUNNING_TIMEOUT (360)
#define M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS (50)
#define M_DELAY_AFTER_DISEQC_RESET_CMD (50)
#define M_DELAY_AFTER_DISEQC_PERIPHERIAL_POWERON_CMD (150)
	
/****************************************************************************
 * Static data
 ***************************************************************************/
static AM_SEC_DVBSatelliteEquipmentControl_t sec_control;

static struct list_head sec_command_list;
static struct list_head sec_command_cur;

static long am_sec_fend_data[NUM_DATA_ENTRIES];

/****************************************************************************
 * Static functions
 ***************************************************************************/
static void AM_SEC_SetSecCommand( eSecCommand_t *sec_cmd, int cmd )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	
	return;
}

static void AM_SEC_SetSecCommandByVal( eSecCommand_t *sec_cmd, int cmd, int val )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->val = val;
	
	return;
}

static void AM_SEC_SetSecCommandByDiseqc( eSecCommand_t *sec_cmd, int cmd, eDVBDiseqcCommand_t diseqc )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->diseqc = diseqc;
	
	return;
}

static void AM_SEC_SetSecCommandByMeasure( eSecCommand_t *sec_cmd, int cmd, rotor_t measure )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->measure = measure;
	
	return;
}

static void AM_SEC_SetSecCommandByCompare( eSecCommand_t *sec_cmd, int cmd, pair_t compare )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->compare = compare;
	
	return;
}

static void AM_SEC_SecCommandListInit(void)
{
	LIST_HEAD(sec_command_list);
}

static void AM_SEC_SecCommandListPushFront(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);

	list_add(&sec_cmd->head, &sec_command_list);

	return;
}

static void AM_SEC_SecCommandListPushBack(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);

	list_add_tail(&sec_cmd->head, &sec_command_list);

	return;
}

static void AM_SEC_SecCommandListClear(void)
{
}

static struct list_head *AM_SEC_SecCommandListCurrent(void)
{
	return &sec_command_cur;
}

static struct list_head *AM_SEC_SecCommandListBegin(void)
{
	return &sec_command_list;
}

static struct list_head *AM_SEC_SecCommandListEnd(void)
{
	return &sec_command_list;
}

static int AM_SEC_GetFendData(AM_SEC_FEND_DATA num, long *data)
{
	assert(data);
	
	if ( num < NUM_DATA_ENTRIES )
	{
		*data = am_sec_fend_data[num];
		return 0;
	}
	return -1;
}

static int AM_SEC_SetFendData(AM_SEC_FEND_DATA num, long val)
{
	if ( num < NUM_DATA_ENTRIES )
	{
		am_sec_fend_data[num] = val;
		return 0;
	}
	return -1;
}


static int AM_SEC_CanTune(int dev_no, const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para)
{
	assert(para);
	
	int score=0;
	long linked_csw=-1, linked_ucsw=-1, linked_toneburst=-1, rotor_pos=-1;

	AM_SEC_GetFendData(CSW, &linked_csw);
	AM_SEC_GetFendData(UCSW, &linked_ucsw);
	AM_SEC_GetFendData(TONEBURST, &linked_toneburst);
	AM_SEC_GetFendData(ROTOR_POS, &rotor_pos);

	AM_Bool_t rotor = AM_FALSE;
	AM_SEC_DVBSatelliteLNBParameters_t lnb_param = sec_control.m_lnbs;
	AM_Bool_t is_unicable = lnb_param.SatCR_idx != -1;
	AM_Bool_t is_unicable_position_switch = lnb_param.SatCR_positions > 1;

	int ret = 0;
	AM_SEC_DVBSatelliteDiseqcParameters_t di_param = lnb_param.m_diseqc_parameters;

	AM_Bool_t diseqc=AM_FALSE;
	long band=0,
		csw = di_param.m_committed_cmd,
		ucsw = di_param.m_uncommitted_cmd,
		toneburst = di_param.m_toneburst_param;

	/* Dishpro bandstacking HACK */
	if (lnb_param.m_lof_threshold == 1000)
	{
		if (!(para->polarisation & AM_FEND_POLARISATION_V))
		{
			band |= 1;
		}
		band |= 2; /* voltage always 18V for Dishpro */
	}
	else
	{
		if ( para->para.frequency > lnb_param.m_lof_threshold )
			band |= 1;
		if (!(para->polarisation & AM_FEND_POLARISATION_V))
			band |= 2;
	}

	if (di_param.m_diseqc_mode >= V1_0)
	{
		diseqc=AM_TRUE;
		if ( di_param.m_committed_cmd < SENDNO )
			csw = 0xF0 | (csw << 2);

		if (di_param.m_committed_cmd <= SENDNO)
			csw |= band;

		if ( di_param.m_diseqc_mode == V1_2 )  // ROTOR
			rotor = AM_TRUE;

		ret = 10000;
	}
	else
	{
		csw = band;
		ret = 15000;
	}

	AM_DEBUG(1, "ret1 %d", ret);

	if (!is_unicable)
	{
		// compare tuner data
		if ( (csw != linked_csw) ||
			( diseqc && (ucsw != linked_ucsw || toneburst != linked_toneburst) ) )
		{
			ret = 0;
		}
		else
			ret += 15;
		AM_DEBUG(1, "ret2 %d", ret);
	}

	if (ret && !is_unicable)
	{
		int lof = para->para.frequency > lnb_param.m_lof_threshold ?
			lnb_param.m_lof_hi : lnb_param.m_lof_lo;
		int tuner_freq = abs(para->para.frequency - lof);
		if (tuner_freq < 900000 || tuner_freq > 2200000)
			ret = 0;
	}

	/* rotor */
	
	AM_DEBUG(1, "ret %d, score old %d", ret, score);

	score = ret;

	AM_DEBUG(1, "final score %d", score);
	
	return score;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 设定卫星设备控制参数
 * \param dev_no 前端设备号
 * \param[in] para 卫星设备控制参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_SEC_SetSetting(int dev_no, const AM_SEC_DVBSatelliteEquipmentControl_t *para)
{
	assert(para);
	
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	/* LNB Specific Parameters */
	sec_control.m_lnbs.m_lof_lo = para->m_lnbs.m_lof_lo;
	sec_control.m_lnbs.m_lof_hi = para->m_lnbs.m_lof_hi;
	sec_control.m_lnbs.m_lof_threshold = para->m_lnbs.m_lof_threshold;
	sec_control.m_lnbs.m_increased_voltage = para->m_lnbs.m_increased_voltage;
	sec_control.m_lnbs.m_prio = para->m_lnbs.m_prio;
	sec_control.m_lnbs.LNBNum = para->m_lnbs.LNBNum;

	/* DiSEqC Specific Parameters */
	sec_control.m_lnbs.m_diseqc_parameters.m_diseqc_mode = para->m_lnbs.m_diseqc_parameters.m_diseqc_mode;
	sec_control.m_lnbs.m_diseqc_parameters.m_toneburst_param = para->m_lnbs.m_diseqc_parameters.m_toneburst_param;
	sec_control.m_lnbs.m_diseqc_parameters.m_repeats = para->m_lnbs.m_diseqc_parameters.m_repeats;
	sec_control.m_lnbs.m_diseqc_parameters.m_committed_cmd = para->m_lnbs.m_diseqc_parameters.m_committed_cmd;
	sec_control.m_lnbs.m_diseqc_parameters.m_uncommitted_cmd = para->m_lnbs.m_diseqc_parameters.m_uncommitted_cmd;
	sec_control.m_lnbs.m_diseqc_parameters.m_command_order = para->m_lnbs.m_diseqc_parameters.m_command_order;	
	sec_control.m_lnbs.m_diseqc_parameters.m_use_fast = para->m_lnbs.m_diseqc_parameters.m_use_fast;
	sec_control.m_lnbs.m_diseqc_parameters.m_seq_repeat = para->m_lnbs.m_diseqc_parameters.m_seq_repeat;	
	
	/* Rotor Specific Parameters */
	sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_longitude =
		para->m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_longitude;	
	sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_latitude =
		para->m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_latitude;
	sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_lo_direction =
		para->m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_lo_direction;	
	sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_la_direction =
		para->m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_la_direction;

	sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_use = 
		para->m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_use;
	sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_delta = 
		para->m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_delta;
	sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_turning_speed = 
		para->m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_turning_speed;
	
	/* Unicable Specific Parameters */
	sec_control.m_lnbs.SatCR_idx = para->m_lnbs.SatCR_idx;
	sec_control.m_lnbs.SatCRvco = para->m_lnbs.SatCRvco;
	sec_control.m_lnbs.SatCR_positions = para->m_lnbs.SatCR_positions;

	/* Satellite Specific Parameters */
	sec_control.m_lnbs.m_cursat_parameters.m_voltage_mode = para->m_lnbs.m_cursat_parameters.m_voltage_mode;
	sec_control.m_lnbs.m_cursat_parameters.m_22khz_signal = para->m_lnbs.m_cursat_parameters.m_22khz_signal;
	sec_control.m_lnbs.m_cursat_parameters.m_rotorPosNum = para->m_lnbs.m_cursat_parameters.m_rotorPosNum;

	/* for the moment, this value is default setting */
	sec_control.m_params[DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC] = M_DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC;
	sec_control.m_params[DELAY_AFTER_FINAL_CONT_TONE_CHANGE] = M_DELAY_AFTER_FINAL_CONT_TONE_CHANGE;
	sec_control.m_params[DELAY_AFTER_FINAL_VOLTAGE_CHANGE] = M_DELAY_AFTER_FINAL_VOLTAGE_CHANGE;
	sec_control.m_params[DELAY_BETWEEN_DISEQC_REPEATS] = M_DELAY_BETWEEN_DISEQC_REPEATS;
	sec_control.m_params[DELAY_AFTER_LAST_DISEQC_CMD] = M_DELAY_AFTER_LAST_DISEQC_CMD;
	sec_control.m_params[DELAY_AFTER_TONEBURST] = M_DELAY_AFTER_TONEBURST;
	sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS] = M_DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS;
	sec_control.m_params[DELAY_BETWEEN_SWITCH_AND_MOTOR_CMD] = M_DELAY_BETWEEN_SWITCH_AND_MOTOR_CMD;
	sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MEASURE_IDLE_INPUTPOWER] = M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MEASURE_IDLE_INPUTPOWER;
	sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_MOTOR_CMD] = M_DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_MOTOR_CMD;
	sec_control.m_params[DELAY_AFTER_MOTOR_STOP_CMD] = M_DELAY_AFTER_MOTOR_STOP_CMD;
	sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD] = M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD;
	sec_control.m_params[DELAY_BEFORE_SEQUENCE_REPEAT] = M_DELAY_BEFORE_SEQUENCE_REPEAT;
	sec_control.m_params[MOTOR_COMMAND_RETRIES] = M_MOTOR_COMMAND_RETRIES;
	sec_control.m_params[MOTOR_RUNNING_TIMEOUT] = M_MOTOR_RUNNING_TIMEOUT;
	sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS] = M_DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS;
	sec_control.m_params[DELAY_AFTER_DISEQC_RESET_CMD] = M_DELAY_AFTER_DISEQC_RESET_CMD;
	sec_control.m_params[DELAY_AFTER_DISEQC_PERIPHERIAL_POWERON_CMD] = M_DELAY_AFTER_DISEQC_PERIPHERIAL_POWERON_CMD;

	return ret;
}

/**\brief 获取卫星设备控制参数
 * \param dev_no 前端设备号
 * \param[out] para 卫星设备控制参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_SEC_GetSetting(int dev_no, AM_SEC_DVBSatelliteEquipmentControl_t *para)
{
	assert(para);
	
	AM_ErrorCode_t ret = AM_SUCCESS;

	memcpy(para, &sec_control, sizeof(AM_SEC_DVBSatelliteEquipmentControl_t));
	
	return ret;
}

AM_ErrorCode_t AM_SEC_Prepare(int dev_no, const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para, unsigned int tunetimeout)
{
	assert(para);
	
	AM_ErrorCode_t ret = AM_SUCCESS;

	if(AM_SEC_CanTune(dev_no, para))
	{


	}
			
	return ret;
}

AM_ErrorCode_t AM_SEC_PrepareTurnOffSatCR(int dev_no, int satcr)
{
	AM_ErrorCode_t ret = AM_SUCCESS;

	return ret;
}

void AM_SEC_SetCommandString(eDVBDiseqcCommand_t *diseqc_cmd, const char *str)
{
	assert(diseqc_cmd);
	assert(str);
	
	if (!str)
		return;
	diseqc_cmd->len=0;
	int slen = strlen(str);
	if (slen % 2)
	{
		AM_DEBUG(1, "%s", "invalid diseqc command string length (not 2 byte aligned)");
		return;
	}
	if (slen > MAX_DISEQC_LENGTH*2)
	{
		AM_DEBUG(1, "%s", "invalid diseqc command string length (string is to long)");
		return;
	}
	unsigned char val=0;
	int i=0; 
	for (i=0; i < slen; ++i)
	{
		unsigned char c = str[i];
		switch(c)
		{
			case '0' ... '9': c-=48; break;
			case 'a' ... 'f': c-=87; break;
			case 'A' ... 'F': c-=55; break;
			default:
				AM_DEBUG(1, "%s", "invalid character in hex string..ignore complete diseqc command !");
				return;
		}
		if ( i % 2 )
		{
			val |= c;
			diseqc_cmd->data[i/2] = val;
		}
		else
			val = c << 4;
	}
	diseqc_cmd->len = slen/2;

	return;
}


