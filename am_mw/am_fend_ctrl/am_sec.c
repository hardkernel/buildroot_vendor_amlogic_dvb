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

#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <pthread.h>

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

#define M_CENTRE_START_FREQ (950000)
#define M_CENTRE_END_FREQ (2150000)

#define M_C_TP_START_FREQ (3700000)
#define M_C_TP_END_FREQ (4200000)

#define M_KU_TP_START_FREQ (10750000)
#define M_KU_TP_END_FREQ (12750000)
	
/****************************************************************************
 * Static data
 ***************************************************************************/
static AM_Bool_t sec_init_flag = AM_FALSE;

static AM_Bool_t sec_blind_flag = AM_FALSE;

static AM_SEC_DVBSatelliteEquipmentControl_t sec_control;

static struct list_head sec_command_list;
static struct list_head sec_command_cur;

static long am_sec_fend_data[NUM_DATA_ENTRIES];

static int am_sec_timeoutcount; // needed for timeout

/****************************************************************************
 * Static functions
 ***************************************************************************/

static AM_ErrorCode_t AM_SEC_Prepare(int dev_no, const AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t *b_para,
										const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para, fe_status_t *status, unsigned int tunetimeout);

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

static AM_Bool_t AM_SEC_If_Tone_Goto(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data = -1;
	
	if(sec_cmd->cmd == IF_TONE_GOTO)
	{
		AM_SEC_GetFendData(CUR_TONE, &data);
	
		AM_DEBUG(1, "AM_SEC_If_Tone_Goto CUR_TONE:%ld need set tone:%d \n", data, sec_cmd->compare.tone);
		
		if ( sec_cmd->compare.tone == data)
			return AM_TRUE;
	}

	return AM_FALSE;
}

static void AM_SEC_Set_Tone(int dev_no, eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data = -1;

	data = sec_cmd->tone;
	
	if(sec_cmd->cmd == SET_TONE)
	{
		AM_DEBUG(1, "AM_SEC_Set_Tone %ld\n", data);
		
		AM_SEC_SetFendData(CUR_TONE, data);
		AM_FEND_SetTone(dev_no, data);
	}

	return;
}

static AM_Bool_t AM_SEC_If_Voltage_Goto(eSecCommand_t *sec_cmd, AM_Bool_t increased)
{
	assert(sec_cmd);
	long data1 = -1, data2 = -1;
	
	if(sec_cmd->cmd == IF_VOLTAGE_GOTO)
	{
		AM_SEC_GetFendData(CUR_VOLTAGE, &data1);
		AM_SEC_GetFendData(CUR_VOLTAGE_INC, &data2);
		
		AM_DEBUG(1, "AM_SEC_If_Voltage_Goto CUR_VOLTAGE:%ld CUR_VOLTAGE_INC:%ld need set voltage:%d need increased:%d\n", data1, data2, sec_cmd->compare.voltage, increased);
		
		if(sec_cmd->compare.voltage == data1)
		{
			if(sec_cmd->compare.voltage != SEC_VOLTAGE_OFF)
			{
				if(increased == data2)
					return AM_TRUE;
			}
			else
			{
				return AM_TRUE;
			}
		}
	}

	return AM_FALSE;
}

static AM_Bool_t AM_SEC_If_Not_Voltage_Goto(eSecCommand_t *sec_cmd, AM_Bool_t increased)
{
	assert(sec_cmd);
	long data1 = -1, data2 = -1;
	
	if(sec_cmd->cmd == IF_NOT_VOLTAGE_GOTO)
	{
		AM_SEC_GetFendData(CUR_VOLTAGE, &data1);
		AM_SEC_GetFendData(CUR_VOLTAGE_INC, &data2);
		
		AM_DEBUG(1, "AM_SEC_If_Not_Voltage_Goto CUR_VOLTAGE:%ld CUR_VOLTAGE_INC:%ld need set voltage:%d need increased:%d\n", data1, data2, sec_cmd->compare.voltage, increased);

		if  (sec_cmd->compare.voltage != data1)
		{
			if(sec_cmd->compare.voltage == SEC_VOLTAGE_OFF)
			{
				return AM_TRUE;
			}
			else
			{			
				if(increased != data2)
					return AM_TRUE;			
			}
		}
	}

	return AM_FALSE;
}

static void AM_SEC_Set_Voltage(int dev_no, eSecCommand_t *sec_cmd, AM_Bool_t increased)
{
	assert(sec_cmd);
	long data = -1;
	
	data = sec_cmd->voltage;
	
	if(sec_cmd->cmd == SET_VOLTAGE)
	{
		if(data == SEC_VOLTAGE_OFF)
		{
			AM_SEC_SetFendData(CSW, -1);
			AM_SEC_SetFendData(UCSW, -1);
			AM_SEC_SetFendData(TONEBURST, -1);		
		}

		AM_DEBUG(1, "AM_SEC_Set_Voltage %ld\n", data);

		AM_SEC_SetFendData(CUR_VOLTAGE_INC, increased);
		AM_FEND_EnableHighLnbVoltage(dev_no, increased);
		AM_SEC_SetFendData(CUR_VOLTAGE, data);
		AM_FEND_SetVoltage(dev_no, data);
	}

	return;
}

static void AM_SEC_Set_Invalid_Cur_SwitchPara(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if(sec_cmd->cmd == INVALIDATE_CURRENT_SWITCHPARMS)
	{
		AM_SEC_SetFendData(CSW, -1);
		AM_SEC_SetFendData(UCSW, -1);
		AM_SEC_SetFendData(TONEBURST, -1);
	}

	return;
}

static void AM_SEC_Set_Update_Cur_SwitchPara(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data1 = -1, data2 = -1, data3 = -1;
	
	if(sec_cmd->cmd == UPDATE_CURRENT_SWITCHPARMS)
	{

		AM_SEC_GetFendData(NEW_CSW, &data1);
		AM_SEC_GetFendData(NEW_UCSW, &data2);
		AM_SEC_GetFendData(NEW_TONEBURST, &data3);	

		AM_SEC_SetFendData(CSW, data1);
		AM_SEC_SetFendData(UCSW, data2);
		AM_SEC_SetFendData(TONEBURST, data3);		
	}

	return;
}

static void AM_SEC_Set_Toneburst(int dev_no, eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data = -1;
	
	data = sec_cmd->toneburst;
	
	if(sec_cmd->cmd == SEND_TONEBURST)
	{
		AM_DEBUG(1, "AM_SEC_Set_Toneburst %ld\n", data);
		
		if(data == A)
		{
			AM_FEND_DiseqcSendBurst(dev_no, SEC_MINI_A);
		}else if(data == B)
		{
			AM_FEND_DiseqcSendBurst(dev_no, SEC_MINI_B);
		}
	}
	
	return;
}

static void AM_SEC_Set_Diseqc(int dev_no, eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	eDVBDiseqcCommand_t diseqc;
	
	diseqc = sec_cmd->diseqc;
	
	if(sec_cmd->cmd == SEND_DISEQC)
	{	
		struct dvb_diseqc_master_cmd cmd;
		memset(&cmd, 0, sizeof(struct dvb_diseqc_master_cmd));
		
		memcpy(cmd.msg, diseqc.data, diseqc.len);
		cmd.msg_len = diseqc.len;

		AM_DEBUG(1, "AM_SEC_Set_Diseqc:\n");
		int i = 0;
		for (i = 0; i < diseqc.len; i++)
		{
			AM_DEBUG(1, "%02x", diseqc.data[i]);
		}
		AM_DEBUG(1, "\n");
		
		AM_FEND_DiseqcSendMasterCmd(dev_no, &cmd);	
	}
	
	return;
}

static AM_Bool_t AM_SEC_If_Rotorpos_Valid_Goto(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data1 = -1, data2 = -1;
	
	if(sec_cmd->cmd == IF_ROTORPOS_VALID_GOTO)
	{
		AM_SEC_GetFendData(ROTOR_CMD, &data1);
		AM_SEC_GetFendData(ROTOR_POS, &data2);
		
		if((data1 != -1) && (data2 != -1))
		{
			return AM_TRUE;
		}
	}

	return AM_FALSE;
}

static void AM_SEC_Set_Invalid_Cur_RotorPara(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if(sec_cmd->cmd == INVALIDATE_CURRENT_ROTORPARMS)
	{
		AM_SEC_SetFendData(ROTOR_CMD, -1);
		AM_SEC_SetFendData(ROTOR_POS, -1);
	}

	return;
}

static void AM_SEC_Set_Update_Cur_RotorPara(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	long data1 = -1, data2 = -1;
	
	if(sec_cmd->cmd == UPDATE_CURRENT_ROTORPARAMS)
	{

		AM_SEC_GetFendData(NEW_ROTOR_CMD, &data1);
		AM_SEC_GetFendData(NEW_ROTOR_POS, &data2);

		AM_SEC_SetFendData(ROTOR_CMD, data1);
		AM_SEC_SetFendData(ROTOR_POS, data2);
	}

	return;
}

static AM_Bool_t AM_SEC_If_Tuner_Locked_Goto(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	rotor_t cmd = sec_cmd->measure;
	AM_ErrorCode_t ret = AM_SUCCESS;
	fe_status_t status;

	
	if(sec_cmd->cmd == IF_TUNER_LOCKED_GOTO)
	{
		--am_sec_timeoutcount;
		ret = AM_FEND_GetStatus(cmd.dev_no, &status);
		if(status & FE_HAS_LOCK)
			return AM_TRUE;
	}

	return AM_FALSE;
}

static void AM_SEC_Set_RotorMoving(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if(sec_cmd->cmd == SET_ROTOR_MOVING)
	{
		sec_control.m_rotorMoving = AM_TRUE;
	}

	return;
}

static void AM_SEC_Set_RotorStoped(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if(sec_cmd->cmd == SET_ROTOR_STOPPED)
	{
		sec_control.m_rotorMoving = AM_FALSE;
	}

	return;
}

static AM_Bool_t AM_SEC_Need_Turn_Fast(int turn_speed)
{
	if (turn_speed == FAST)
		return AM_TRUE;
	else if (turn_speed != SLOW)
	{
		/*no use*/
		return AM_TRUE;
	}
	return AM_FALSE;
}

static void AM_SEC_Set_Timeout(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if(sec_cmd->cmd == SET_TIMEOUT)
	{
		am_sec_timeoutcount = sec_cmd->val;
		AM_DEBUG(1, "AM_SEC_Set_Timeout: %d\n", am_sec_timeoutcount);
	}

	return;
}

static AM_Bool_t AM_SEC_If_Timeout_Goto(eSecCommand_t *sec_cmd)
{
	assert(sec_cmd);
	
	if((sec_cmd->cmd == IF_TIMEOUT_GOTO) && (!am_sec_timeoutcount))
	{
		return AM_TRUE;
	}

	return AM_FALSE;
}

static AM_ErrorCode_t AM_Sec_SetAsyncInfo(int dev_no, const AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t *b_para,
												const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para, fe_status_t *status, unsigned int tunetimeout)
{
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);
	AM_ErrorCode_t ret = AM_SUCCESS;
		
	pthread_mutex_lock(&p_sec_asyncinfo->lock);

	if(p_sec_asyncinfo->preparerunning)
	{
		if((b_para == NULL) && (status == NULL) && (tunetimeout == 0))/*setpara mode*/
		{
			p_sec_asyncinfo->prepareexitnotify = AM_TRUE;
		}
	
		if(p_sec_asyncinfo->enable_thread && (p_sec_asyncinfo->thread!=pthread_self()))
		{
			/*等待prepare函数执行完*/
			while(p_sec_asyncinfo->preparerunning)
			{
				pthread_cond_wait(&p_sec_asyncinfo->cond, &p_sec_asyncinfo->lock);
			}
		}
	}

	p_sec_asyncinfo->dev_no = dev_no;
	p_sec_asyncinfo->sat_b_para = (void *)b_para;
	p_sec_asyncinfo->sat_para = (void *)para;
	p_sec_asyncinfo->sat_status = (void *)status;
	p_sec_asyncinfo->sat_tunetimeout = tunetimeout;
	
	pthread_mutex_unlock(&p_sec_asyncinfo->lock);
	
	return ret;
}


static AM_ErrorCode_t AM_Sec_AsyncCheck(void)
{
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);
	AM_ErrorCode_t ret = AM_FAILURE;

	pthread_mutex_lock(&p_sec_asyncinfo->lock);
	if(p_sec_asyncinfo->prepareexitnotify)
	{
		ret = AM_SUCCESS;
	}
	pthread_mutex_unlock(&p_sec_asyncinfo->lock);		
	
	return ret;
}

static AM_ErrorCode_t AM_Sec_AsyncSet(void)
{
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);
	AM_ErrorCode_t ret = AM_SUCCESS;
	int sem_value = 0;

	sem_getvalue(&p_sec_asyncinfo->sem_running, &sem_value);
	if(sem_value == 0)
	{
		sem_post(&p_sec_asyncinfo->sem_running);
	}
	
	return ret;
}

static AM_ErrorCode_t AM_Sec_AsyncWait(void)
{
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);
	AM_ErrorCode_t ret = AM_SUCCESS;
	int rc;

	rc = sem_wait(&p_sec_asyncinfo->sem_running);
	if(rc)
	{
		ret = AM_FENDCTRL_ERROR_BASE;
	}
	
	return ret;
}

static void* AM_Sec_AsyncThread(void *arg)
{
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = (AM_SEC_AsyncInfo_t *)arg;
	AM_ErrorCode_t ret = AM_FAILURE;
	
	while(p_sec_asyncinfo->enable_thread)
	{
		//wait
		ret = AM_Sec_AsyncWait();
		if(ret != AM_SUCCESS)
			continue;
	
		if(p_sec_asyncinfo->enable_thread)
		{
			pthread_mutex_lock(&p_sec_asyncinfo->lock);
			p_sec_asyncinfo->prepareexitnotify = AM_FALSE;
			p_sec_asyncinfo->preparerunning = AM_TRUE;
			pthread_mutex_unlock(&p_sec_asyncinfo->lock);

			ret = AM_SEC_Prepare(p_sec_asyncinfo->dev_no, p_sec_asyncinfo->sat_b_para,
									p_sec_asyncinfo->sat_para, p_sec_asyncinfo->sat_status, p_sec_asyncinfo->sat_tunetimeout);
		
			pthread_mutex_lock(&p_sec_asyncinfo->lock);
			p_sec_asyncinfo->prepareexitnotify = AM_FALSE;			
			p_sec_asyncinfo->preparerunning = AM_FALSE;
			pthread_mutex_unlock(&p_sec_asyncinfo->lock);
			pthread_cond_broadcast(&p_sec_asyncinfo->cond);
		}
	}
	
	return NULL;
}

static int AM_SEC_CanBlindScanOrTune(int dev_no, const AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t *b_para,
											const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para)
{	
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

	if(para != NULL)
	{
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
	}
	else if(b_para != NULL)
	{
		if ( b_para->ocaloscollatorfreq & AM_FEND_LOCALOSCILLATORFREQ_H)
			band |= 1;
		if (!(b_para->polarisation & AM_FEND_POLARISATION_V))
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

	AM_DEBUG(1, "ret1 %d\n", ret);

	if (!is_unicable)
	{
		// compare tuner data
		if ( (csw != linked_csw) ||
			( diseqc && (ucsw != linked_ucsw || toneburst != linked_toneburst) ) )
		{
			//ret = 0;
		}
		else
			ret += 15;
		AM_DEBUG(1, "ret2 %d\n", ret);
	}

	if ((para != NULL) && ret && !is_unicable)
	{
		int lof = para->para.frequency > lnb_param.m_lof_threshold ?
			lnb_param.m_lof_hi : lnb_param.m_lof_lo;
		int tuner_freq = abs(para->para.frequency - lof);
		if (tuner_freq < 900000 || tuner_freq > 2200000)
			ret = 0;
	}

	/* rotor */
	
	AM_DEBUG(1, "ret %d, score old %d\n", ret, score);

	score = ret;

	AM_DEBUG(1, "final score %d\n", score);
	
	return score;
}

#define M_AM_SEC_ASYNCCHECK()\
	AM_MACRO_BEGIN\
		if((b_para == NULL) && (status == NULL) && (tunetimeout == 0))\
		{\
			AM_DEBUG(1, "M_AM_SEC_ASYNCCHECK \n");\
			if(AM_Sec_AsyncCheck() == AM_SUCCESS)\
			{\
				return ret;\
			};\
		}\
	AM_MACRO_END

#define M_AM_SEC_ASYNCCHECK_ROTORSTOP()\
	AM_MACRO_BEGIN\
		if((b_para == NULL) && (status == NULL) && (tunetimeout == 0))\
		{\
			AM_DEBUG(1, "M_AM_SEC_ASYNCCHECK_ROTORSTOP \n");\
			if(AM_Sec_AsyncCheck() == AM_SUCCESS)\
			{\
				AM_FEND_SetActionCallback(dev_no, AM_TRUE);\
				AM_DEBUG(1, "AM_FEND_SetActionCallback enable\n");\			
				AM_FEND_Diseqccmd_SetPositionerHalt(dev_no);\
				usleep(15 * 1000);\
				return ret;\
			};\
		}\
	AM_MACRO_END			


static AM_ErrorCode_t AM_SEC_Prepare(int dev_no, const AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t *b_para,
										const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para, fe_status_t *status, unsigned int tunetimeout)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	eSecCommand_t sec_cmd;

	struct dvb_frontend_parameters convert_para;

	AM_DEBUG(1, "AM_SEC_Prepare enter %d %p %p %p %d\n", dev_no, b_para, para, status, tunetimeout);

	if(para != NULL)
	{
		AM_DEBUG(1, "lock tp freq %d\n", para->para.frequency);
		memcpy(&convert_para, &(para->para), sizeof(struct dvb_frontend_parameters));
	}

	//AM_SEC_DumpSetting();
	
	if(AM_SEC_CanBlindScanOrTune(dev_no, b_para, para))
	{
		M_AM_SEC_ASYNCCHECK();
	
		AM_SEC_DVBSatelliteLNBParameters_t lnb_param = sec_control.m_lnbs;
		AM_SEC_DVBSatelliteDiseqcParameters_t di_param = lnb_param.m_diseqc_parameters;
		AM_SEC_DVBSatelliteRotorParameters_t rotor_param = lnb_param.m_rotor_parameters;
		AM_SEC_DVBSatelliteSwitchParameters_t sw_param = lnb_param.m_cursat_parameters;
		
		AM_Bool_t doSetFrontend = AM_TRUE;
		AM_Bool_t doSetVoltageToneFrontend = AM_TRUE;
		AM_Bool_t forceChanged = AM_FALSE;
		AM_Bool_t needDiSEqCReset = AM_FALSE;
		long band=0,
			voltage = SEC_VOLTAGE_OFF,
			tone = SEC_TONE_OFF,
			csw = di_param.m_committed_cmd,
			ucsw = di_param.m_uncommitted_cmd,
			toneburst = di_param.m_toneburst_param,
			lastcsw = -1,
			lastucsw = -1,
			lastToneburst = -1,
			lastRotorCmd = -1,
			curRotorPos = -1,
			satposDependPtr = -1;
		AM_SEC_Diseqc_Mode diseqc_mode = di_param.m_diseqc_mode;
		AM_SEC_Voltage_Mode voltage_mode = sw_param.m_voltage_mode;
		AM_Bool_t diseqc13V = voltage_mode == HV_13;
		AM_Bool_t is_unicable = lnb_param.SatCR_idx != -1;

		AM_DEBUG(1, "lnb_param.SatCR_idx %d\n", lnb_param.SatCR_idx);

		AM_Bool_t useGotoXX = AM_FALSE;
		int RotorCmd=-1;
		int send_mask = 0;

		lnb_param.guard_offset = 0; //HACK

		AM_SEC_SetFendData(SATCR, lnb_param.SatCR_idx);

		if (diseqc13V)
			voltage_mode = HV;

		AM_SEC_GetFendData(CSW, &lastcsw);
		AM_SEC_GetFendData(UCSW, &lastucsw);
		AM_SEC_GetFendData(TONEBURST, &lastToneburst);
		AM_SEC_GetFendData(ROTOR_CMD, &lastRotorCmd);
		AM_SEC_GetFendData(ROTOR_POS, &curRotorPos);

		if (lastcsw == lastucsw && lastToneburst == lastucsw && lastucsw == -1)
			needDiSEqCReset = AM_TRUE;

		if((b_para == NULL) && (para != NULL))
		{
			AM_DEBUG(1, "Tune Prepare\n");
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

			int lof = (band&1)?lnb_param.m_lof_hi:lnb_param.m_lof_lo;

			AM_DEBUG(1, "is_unicable %d\n", is_unicable);
			if(!is_unicable)
			{
				// calc Frequency
				int local= abs(para->para.frequency
					- lof);
				convert_para.frequency = ((((local * 2) / 125) + 1) / 2) * 125;
				AM_DEBUG(1, " tp:%d lof:%d\n", para->para.frequency, lof);
				AM_DEBUG(1, " local:%d freq:%d\n", local, convert_para.frequency);
				AM_SEC_SetFendData(FREQ_OFFSET, para->para.frequency - convert_para.frequency);

				/* Dishpro bandstacking HACK */
				if (lnb_param.m_lof_threshold == 1000)
					voltage = SEC_VOLTAGE_18;
				else if ( voltage_mode == _14V
					|| ( para->polarisation & AM_FEND_POLARISATION_V
						&& voltage_mode == HV )  )
					voltage = SEC_VOLTAGE_13;
				else if ( voltage_mode == _18V
					|| ( !(para->polarisation & AM_FEND_POLARISATION_V)
						&& voltage_mode == HV )  )
					voltage = SEC_VOLTAGE_18;
				if ( (sw_param.m_22khz_signal == ON)
					|| ( sw_param.m_22khz_signal == HILO && (band&1) ) )
					tone = SEC_TONE_ON;
				else if ( (sw_param.m_22khz_signal == OFF)
					|| ( sw_param.m_22khz_signal == HILO && !(band&1) ) )
					tone = SEC_TONE_OFF;
			}
			else
			{
				int tmp1 = abs(para->para.frequency
						-lof)
						+ lnb_param.SatCRvco
						- 1400000
						+ lnb_param.guard_offset;
				AM_DEBUG(1, "[prepare] UnicableTuningWord %#04x\n",lnb_param.UnicableTuningWord);
				int tmp2 = ((((tmp1 * 2) / 4000) + 1) / 2) * 4000;
				convert_para.frequency = lnb_param.SatCRvco - (tmp1-tmp2) + lnb_param.guard_offset;
				lnb_param.UnicableTuningWord = ((tmp2 / 4000) 
						| ((band & 1) ? 0x400 : 0)			//HighLow
						| ((band & 2) ? 0x800 : 0)			//VertHor
						| ((lnb_param.LNBNum & 1) ? 0 : 0x1000)			//Umschaltung LNB1 LNB2
						| (lnb_param.SatCR_idx << 13));		//Adresse des SatCR
				AM_DEBUG(1, "[prepare] UnicableTuningWord %#04x\n",lnb_param.UnicableTuningWord);
				AM_DEBUG(1, "[prepare] guard_offset %d\n",lnb_param.guard_offset);
				AM_SEC_SetFendData(FREQ_OFFSET, (lnb_param.UnicableTuningWord & 0x3FF) *4000 + 1400000 + lof - (2 * (lnb_param.SatCRvco - (tmp1-tmp2))) );
				voltage = SEC_VOLTAGE_13;
			}
		}else if(b_para != NULL)
		{
			AM_DEBUG(1, "Blind Prepare\n");
			if ( b_para->ocaloscollatorfreq & AM_FEND_LOCALOSCILLATORFREQ_H)
				band |= 1;
			if (!(b_para->polarisation & AM_FEND_POLARISATION_V))
				band |= 2;
			
			if ( voltage_mode == _14V
				|| ( b_para->polarisation & AM_FEND_POLARISATION_V
					&& voltage_mode == HV )  )
				voltage = SEC_VOLTAGE_13;
			else if ( voltage_mode == _18V
				|| ( !(b_para->polarisation & AM_FEND_POLARISATION_V)
					&& voltage_mode == HV )  )
				voltage = SEC_VOLTAGE_18;
			if ( (sw_param.m_22khz_signal == ON)
				|| ( sw_param.m_22khz_signal == HILO && (band&1) ) )
				tone = SEC_TONE_ON;
			else if ( (sw_param.m_22khz_signal == OFF)
				|| ( sw_param.m_22khz_signal == HILO && !(band&1) ) )
				tone = SEC_TONE_OFF;			
		}

		M_AM_SEC_ASYNCCHECK();

		AM_DEBUG(1, "band:%ld voltage:%ld tone:%ld\n", band, voltage, tone);
		AM_DEBUG(1, "diseqc_mode:%d\n", diseqc_mode);
		
		if (diseqc_mode >= V1_0)
		{
			if ( di_param.m_committed_cmd < SENDNO )
				csw = 0xF0 | (csw << 2);

			if (di_param.m_committed_cmd <= SENDNO)
				csw |= band;

			AM_Bool_t send_csw =
				(di_param.m_committed_cmd != SENDNO);
			AM_Bool_t changed_csw = send_csw && (forceChanged || csw != lastcsw);

			AM_Bool_t send_ucsw =
				(di_param.m_uncommitted_cmd && diseqc_mode > V1_0);
			AM_Bool_t changed_ucsw = send_ucsw && (forceChanged || ucsw != lastucsw);

			AM_Bool_t send_burst =
				(di_param.m_toneburst_param != NO);
			AM_Bool_t changed_burst = send_burst && (forceChanged || toneburst != lastToneburst);

			AM_DEBUG(1, "send_csw:%d changed_csw:%d send_ucsw:%d changed_ucsw:%d send_burst:%d changed_burst:%d\n", 
							send_csw, changed_csw, send_ucsw, changed_ucsw, send_burst, changed_burst);

			/* send_mask
				1 must send csw
				2 must send ucsw
				4 send toneburst first
				8 send toneburst at end */
			if (changed_burst) // toneburst first and toneburst changed
			{
				if (di_param.m_command_order&1)
				{
					send_mask |= 4;
					if ( send_csw )
						send_mask |= 1;
					if ( send_ucsw )
						send_mask |= 2;
				}
				else
					send_mask |= 8;
			}
			if (changed_ucsw)
			{
				send_mask |= 2;
				if ((di_param.m_command_order&4) && send_csw)
					send_mask |= 1;
				if (di_param.m_command_order==4 && send_burst)
					send_mask |= 8;
			}
			if (changed_csw)
			{
				if ( di_param.m_use_fast
					&& di_param.m_committed_cmd < SENDNO
					&& (lastcsw & 0xF0)
					&& ((csw / 4) == (lastcsw / 4)) )
					AM_DEBUG(1, "dont send committed cmd (fast diseqc)\n");
				else
				{
					send_mask |= 1;
					if (!(di_param.m_command_order&4) && send_ucsw)
						send_mask |= 2;
					if (!(di_param.m_command_order&1) && send_burst)
						send_mask |= 8;
				}
			}

#if 1
			AM_DEBUG(1, "sendmask: \n");
			int i = 3;
			for (i = 3; i >= 0; --i)
				if ( send_mask & (1<<i) )
					AM_DEBUG(1, "1");
				else
					AM_DEBUG(1, "0");
			AM_DEBUG(1, "\n");
#endif

			if ( diseqc_mode == V1_2 )
			{
				if (sw_param.m_rotorPosNum) // we have stored rotor pos?
					RotorCmd=sw_param.m_rotorPosNum;
				else  // we must calc gotoxx cmd
				{
					AM_DEBUG(1, "useGotoXX\n");
					useGotoXX = AM_TRUE;
					
					/*as flag, judge rotor cmd status*/
					double	SatLon = abs(rotor_param.m_gotoxx_parameters.m_sat_longitude)/10.00,
							SiteLat = rotor_param.m_gotoxx_parameters.m_latitude,
							SiteLon = rotor_param.m_gotoxx_parameters.m_longitude;

					if(rotor_param.m_gotoxx_parameters.m_sat_longitude < 0)
						SatLon = -SatLon;	


					RotorCmd = AM_ProduceAngularPositioner(dev_no, SiteLon, SiteLat, SatLon);
				}
			}

			M_AM_SEC_ASYNCCHECK();

			if ( send_mask )
			{
				int diseqc_repeats = diseqc_mode > V1_0 ? di_param.m_repeats : 0;
				int vlt = SEC_VOLTAGE_OFF;
				pair_t compare;
				compare.steps = +3;
				compare.tone = SEC_TONE_OFF;
				AM_SEC_SetSecCommandByCompare( &sec_cmd, IF_TONE_GOTO, compare );
				if(!AM_SEC_If_Tone_Goto(&sec_cmd))
				{
					AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, SEC_TONE_OFF );
					AM_SEC_Set_Tone(dev_no, &sec_cmd);
					usleep(sec_control.m_params[DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC] * 1000);
				}

				if (diseqc13V)
					vlt = SEC_VOLTAGE_13;
				else if ( RotorCmd != -1 && RotorCmd != lastRotorCmd )
				{
					if (rotor_param.m_inputpower_parameters.m_use && !is_unicable)
						vlt = SEC_VOLTAGE_18;  // in input power mode set 18V for measure input power
					else
						vlt = SEC_VOLTAGE_13;  // in normal mode start turning with 13V
				}
				else
					vlt = voltage;

				// check if voltage is already correct..
				compare.voltage = vlt;
				compare.steps = +7;
				AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
				if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
				{
					// check if voltage is disabled
					compare.voltage = SEC_VOLTAGE_OFF;
					compare.steps = +4;
					AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
					if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
					{
						// voltage is changed... use DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS
						AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, vlt );
						AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);
						usleep(sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS] * 1000);
						/*GOTO, +3*/
					}
					else
					{
						// voltage was disabled.. use DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS
						AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, vlt );
						AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);
						usleep(sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS] * 1000);
					}
				}

				AM_SEC_SetSecCommand( &sec_cmd, INVALIDATE_CURRENT_SWITCHPARMS );
				AM_SEC_Set_Invalid_Cur_SwitchPara(&sec_cmd);
				if (needDiSEqCReset)
				{
					AM_FEND_Diseqccmd_ResetDiseqcMicro(dev_no);
					usleep(sec_control.m_params[DELAY_AFTER_DISEQC_RESET_CMD] * 1000);
					
					// diseqc peripherial powersupply on
					AM_FEND_Diseqccmd_PoweronSwitch(dev_no);
					usleep(sec_control.m_params[DELAY_AFTER_DISEQC_PERIPHERIAL_POWERON_CMD] * 1000);
				}

				int seq_repeat = 0;
				for (seq_repeat = 0; seq_repeat < (di_param.m_seq_repeat?2:1); ++seq_repeat)
				{
					if ( send_mask & 4 )
					{
						AM_SEC_SetSecCommandByVal( &sec_cmd, SEND_TONEBURST, di_param.m_toneburst_param );
						AM_SEC_Set_Toneburst(dev_no, & sec_cmd);
						usleep(sec_control.m_params[DELAY_AFTER_TONEBURST] * 1000);
					}

					int loops=0;

					if ( send_mask & 1 )
						++loops;
					if ( send_mask & 2 )
						++loops;

					AM_DEBUG(1, "loops:%d\n", loops);

					loops <<= diseqc_repeats;

					int i = 0;
					for ( i = 0; i < loops;)  // fill commands...
					{
						eDVBDiseqcCommand_t diseqc;
						memset(diseqc.data, 0, MAX_DISEQC_LENGTH);
						diseqc.len = 4;
						diseqc.data[0] = i ? 0xE1 : 0xE0;
						diseqc.data[1] = 0x10;
						if ( (send_mask & 2) && (di_param.m_command_order & 4) )
						{
							diseqc.data[2] = 0x39;
							diseqc.data[3] = ucsw;
						}
						else if ( send_mask & 1 )
						{
							diseqc.data[2] = 0x38;
							diseqc.data[3] = csw;
						}
						else  // no committed command confed.. so send uncommitted..
						{
							diseqc.data[2] = 0x39;
							diseqc.data[3] = ucsw;
						}
						AM_SEC_SetSecCommandByDiseqc(&sec_cmd, SEND_DISEQC, diseqc);
						AM_SEC_Set_Diseqc(dev_no, &sec_cmd);

						i++;
						if ( i < loops )
						{
							int cmd=0;
							if (diseqc.data[2] == 0x38 && (send_mask & 2))
								cmd=0x39;
							else if (diseqc.data[2] == 0x39 && (send_mask & 1))
								cmd=0x38;
							int tmp = sec_control.m_params[DELAY_BETWEEN_DISEQC_REPEATS] * 1000;
							if (cmd)
							{
								int delay = diseqc_repeats ? (tmp - 54) / 2 : tmp;  // standard says 100msek between two repeated commands
								usleep(delay);
								diseqc.data[2]=cmd;
								diseqc.data[3]=(cmd==0x38) ? csw : ucsw;
								AM_SEC_SetSecCommandByDiseqc(&sec_cmd, SEND_DISEQC, diseqc);
								AM_SEC_Set_Diseqc(dev_no, &sec_cmd);
								++i;
								if ( i < loops )
									usleep(delay);
								else
									usleep(sec_control.m_params[DELAY_AFTER_LAST_DISEQC_CMD] * 1000);
							}
							else  // delay 120msek when no command is in repeat gap
								usleep(tmp);
						}
						else
							usleep(sec_control.m_params[DELAY_AFTER_LAST_DISEQC_CMD] * 1000);
					}

					if ( send_mask & 8 )  // toneburst at end of sequence
					{
						AM_SEC_SetSecCommandByVal( &sec_cmd, SEND_TONEBURST, di_param.m_toneburst_param );
						AM_SEC_Set_Toneburst(dev_no, & sec_cmd);						
						usleep(sec_control.m_params[DELAY_AFTER_TONEBURST] * 1000);
					}

					if (di_param.m_seq_repeat && seq_repeat == 0)
						usleep(sec_control.m_params[DELAY_BEFORE_SEQUENCE_REPEAT] * 1000);
				}
			}
		}
		else
		{
			AM_SEC_SetSecCommand( &sec_cmd, INVALIDATE_CURRENT_SWITCHPARMS );
			AM_SEC_Set_Invalid_Cur_SwitchPara(&sec_cmd);
			csw = band;
		}

		AM_SEC_SetFendData(NEW_CSW, csw);
		AM_SEC_SetFendData(NEW_UCSW, ucsw);
		AM_SEC_SetFendData(NEW_TONEBURST, di_param.m_toneburst_param);

		M_AM_SEC_ASYNCCHECK();

		if(is_unicable)
		{
			// check if voltage is disabled
			pair_t compare;
			compare.steps = +3;
			compare.voltage = SEC_VOLTAGE_OFF;
			AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_NOT_VOLTAGE_GOTO, compare );
			if(!AM_SEC_If_Not_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
			{
				AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_13 );
				AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);	
				usleep(sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS] * 1000);
			}

			AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_18 );
			AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);			
			AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, SEC_TONE_OFF );
			AM_SEC_Set_Tone(dev_no, &sec_cmd);
			usleep(sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS] * 1000);  // wait 20 ms after voltage change

			eDVBDiseqcCommand_t diseqc;
			memset(diseqc.data, 0, MAX_DISEQC_LENGTH);
			diseqc.len = 5;
			diseqc.data[0] = 0xE0;
			diseqc.data[1] = 0x10;
			diseqc.data[2] = 0x5A;
			diseqc.data[3] = lnb_param.UnicableTuningWord >> 8;
			diseqc.data[4] = lnb_param.UnicableTuningWord;

			AM_SEC_SetSecCommandByDiseqc(&sec_cmd, SEND_DISEQC, diseqc);
			AM_SEC_Set_Diseqc(dev_no, &sec_cmd);
			usleep(sec_control.m_params[DELAY_AFTER_LAST_DISEQC_CMD] * 1000);
			AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_13 );
			AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);	
			if ( RotorCmd != -1 && RotorCmd != lastRotorCmd && !rotor_param.m_inputpower_parameters.m_use)
				usleep(sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD] * 1000);  // wait 150msec after voltage change

			M_AM_SEC_ASYNCCHECK();
	
		}


		AM_DEBUG(1, "RotorCmd %02x, lastRotorCmd %02lx\n", RotorCmd, lastRotorCmd);
		if ( RotorCmd != -1 && RotorCmd != lastRotorCmd )
		{
			AM_Bool_t no_need_sendrotorstop_and_recheck_vol = AM_FALSE;
			int mrt = sec_control.m_params[MOTOR_RUNNING_TIMEOUT]; // in seconds!
			pair_t compare;
			if (!send_mask && !is_unicable)
			{
				compare.steps = +3;
				compare.tone = SEC_TONE_OFF;
				AM_SEC_SetSecCommandByCompare( &sec_cmd, IF_TONE_GOTO, compare );
				if(!AM_SEC_If_Tone_Goto(&sec_cmd))
				{
					AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, SEC_TONE_OFF );
					AM_SEC_Set_Tone(dev_no, &sec_cmd);
					usleep(sec_control.m_params[DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC] * 1000); 
				}
				
				compare.voltage = SEC_VOLTAGE_OFF;
				compare.steps = +4;
				AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_NOT_VOLTAGE_GOTO, compare );
				// the next is a check if voltage is switched off.. then we first set a voltage :)
				// else we set voltage after all diseqc stuff..
				if(!AM_SEC_If_Not_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
				{
					if (rotor_param.m_inputpower_parameters.m_use)
					{
						AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_18 );
						AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage); // set 18V for measure input power
					}
					else
					{
						AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_13 );
						AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);	 // in normal mode start turning with 13V
					}

					usleep(sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_MOTOR_CMD] * 1000);  // wait 750ms when voltage was disabled
				}
				no_need_sendrotorstop_and_recheck_vol = AM_TRUE;  // no need to send stop rotor cmd and recheck voltage
			}
			else
				usleep(sec_control.m_params[DELAY_BETWEEN_SWITCH_AND_MOTOR_CMD] * 1000);  // wait 700ms when diseqc changed

			M_AM_SEC_ASYNCCHECK();

			AM_SEC_SetSecCommandByVal( &sec_cmd, IF_ROTORPOS_VALID_GOTO, +5 );
			if(AM_SEC_If_Rotorpos_Valid_Goto(&sec_cmd))
			{
				AM_FEND_Diseqccmd_SetPositionerHalt(dev_no);
				usleep(50 * 1000);
				AM_FEND_Diseqccmd_SetPositionerHalt(dev_no);
				// wait 150msec after send rotor stop cmd
				usleep(sec_control.m_params[DELAY_AFTER_MOTOR_STOP_CMD] * 1000);

				M_AM_SEC_ASYNCCHECK();
			}

		// use measure rotor input power to detect motor state
			if ( rotor_param.m_inputpower_parameters.m_use)
			{			
				AM_Bool_t turn_fast = AM_SEC_Need_Turn_Fast(rotor_param.m_inputpower_parameters.m_turning_speed) && !is_unicable;
				rotor_t cmd;
				pair_t compare;
				if (turn_fast)
					compare.voltage = SEC_VOLTAGE_18;
				else
					compare.voltage = SEC_VOLTAGE_13;
				compare.steps = +3;
				/*no use*/
			}
		// use normal motor turning mode
			else
			{
				if (curRotorPos != -1)
				{
					AM_DEBUG(1, "calc cur = %d m_sat_longitude sub timout = %d \n", curRotorPos, rotor_param.m_gotoxx_parameters.m_sat_longitude );
					
					mrt = abs(curRotorPos - rotor_param.m_gotoxx_parameters.m_sat_longitude);
					if (mrt > 1800)
						mrt = 3600 - mrt;
					if (mrt % 10)
						mrt += 10; // round a little bit
					mrt *= 2000;  // (we assume a very slow rotor with just 0.5 degree per second here)
					mrt /= 10000;
					mrt += 3; // a little bit overhead
				}
				doSetVoltageToneFrontend=AM_FALSE;
				doSetFrontend=AM_FALSE;
				rotor_t cmd;
				pair_t compare;
				compare.voltage = SEC_VOLTAGE_13;
				compare.steps = +3;
				AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
				if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
				{
					AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, compare.voltage );
					AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);
					usleep(sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD] * 1000);  // wait 150msec after voltage change
				}

				AM_SEC_SetSecCommand( &sec_cmd, INVALIDATE_CURRENT_ROTORPARMS );
				AM_SEC_Set_Invalid_Cur_RotorPara(&sec_cmd);
				AM_SEC_SetSecCommand( &sec_cmd, SET_ROTOR_MOVING); 
				AM_SEC_Set_RotorStoped(&sec_cmd);

				M_AM_SEC_ASYNCCHECK();
				
				if ( useGotoXX )
				{

					double	SatLon = abs(rotor_param.m_gotoxx_parameters.m_sat_longitude)/10.00,
							SiteLat = rotor_param.m_gotoxx_parameters.m_latitude,
							SiteLon = rotor_param.m_gotoxx_parameters.m_longitude;

					if(rotor_param.m_gotoxx_parameters.m_sat_longitude < 0)
						SatLon = -SatLon;						

					AM_DEBUG(1, "degrees = %d \n", rotor_param.m_gotoxx_parameters.m_sat_longitude );
					AM_DEBUG(1, "siteLatitude = %f, siteLongitude = %f, degrees = %f \n", SiteLat, SiteLon, SatLon );

					AM_FEND_Diseqccmd_GotoAngularPositioner(dev_no, SiteLon, SiteLat, SatLon);			
				}
				else
				{
					AM_FEND_Diseqccmd_GotoPositioner(dev_no, RotorCmd); // goto stored sat position
				}
				usleep( 1000 * 1000 ); // sleep one second before change voltage or tone

				compare.voltage = voltage;
				compare.steps = +3;
				AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
				if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))// correct final voltage?
				{
					usleep( 2000 * 1000 );  // wait 2 second before set high voltage
					AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, voltage );
					AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);					
				}

				compare.tone = tone;
				AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_TONE_GOTO, compare );
				if(!AM_SEC_If_Tone_Goto(&sec_cmd))				
				{
					AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, tone );
					AM_SEC_Set_Tone(dev_no, &sec_cmd);				
					usleep(sec_control.m_params[DELAY_AFTER_FINAL_CONT_TONE_CHANGE] * 1000);
				}
				if((b_para == NULL) && (para != NULL))
				{
					AM_FEND_SetActionCallback(dev_no, AM_FALSE);
					AM_DEBUG(1, "AM_FEND_SetActionCallback disable\n");

					AM_FEND_SetPara(dev_no, &(convert_para));
				}

				cmd.direction=1;  // check for running rotor
				cmd.deltaA=0;
				cmd.steps = +3;
				cmd.okcount=0;
				cmd.dev_no = dev_no;

				AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TIMEOUT, mrt*4 ); // mrt is in seconds... our SLEEP time is 250ms.. so * 4
				AM_SEC_Set_Timeout(&sec_cmd);

				AM_DEBUG(1, "set rotor timeout to %d seconds start\n", mrt);

				while(1)
				{
					usleep( 250 * 1000 );  // 250msec delay

					M_AM_SEC_ASYNCCHECK_ROTORSTOP();

					AM_SEC_SetSecCommandByMeasure( &sec_cmd, IF_TUNER_LOCKED_GOTO, cmd); 
					if(AM_SEC_If_Tuner_Locked_Goto(&sec_cmd))
					{
						if((b_para == NULL) && (para != NULL))
						{
							AM_FEND_SetActionCallback(dev_no, AM_TRUE);
							AM_DEBUG(1, "AM_FEND_SetActionCallback enable\n");
						
							if(status != NULL)
							{
								ret = AM_FEND_Lock(dev_no, &(convert_para), status);
							}	
							else
							{
								ret = AM_FEND_SetPara(dev_no, &(convert_para));
							}						
						}
						AM_DEBUG(1, "set rotor lock out\n");
						break;
					}

					AM_SEC_SetSecCommand( &sec_cmd, IF_TIMEOUT_GOTO); 
					if(AM_SEC_If_Timeout_Goto(&sec_cmd))
					{
						if((b_para == NULL) && (para != NULL))
						{
							AM_FEND_SetActionCallback(dev_no, AM_TRUE);
							AM_DEBUG(1, "AM_FEND_SetActionCallback enable\n");
						
							/*tune timeout?*/
							if(status != NULL)
							{
								ret = AM_FEND_Lock(dev_no, &(convert_para), status);
							}	
							else
							{
								ret = AM_FEND_SetPara(dev_no, &(convert_para));
							}	
						}

						AM_DEBUG(1, "set rotor unlock out\n");
						break;
					}					
				}

				AM_SEC_SetSecCommand( &sec_cmd, UPDATE_CURRENT_ROTORPARAMS); 
				AM_SEC_Set_Update_Cur_RotorPara(&sec_cmd);
				AM_SEC_SetSecCommand( &sec_cmd, SET_ROTOR_STOPPED); 
				AM_SEC_Set_RotorStoped(&sec_cmd);

				AM_DEBUG(1, "set rotor timeout to %d seconds end\n", mrt);
			}

			M_AM_SEC_ASYNCCHECK();
			
			AM_SEC_SetFendData(NEW_ROTOR_CMD, RotorCmd);
			AM_SEC_SetFendData(NEW_ROTOR_POS, rotor_param.m_gotoxx_parameters.m_sat_longitude);			
		}

		M_AM_SEC_ASYNCCHECK();

		if (doSetVoltageToneFrontend && !is_unicable)
		{
			pair_t compare;
			compare.voltage = voltage;
			compare.steps = +3;
			AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
			if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))// voltage already correct ?	
			{
				AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, voltage );
				AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);	
				usleep(sec_control.m_params[DELAY_AFTER_FINAL_VOLTAGE_CHANGE] * 1000);
			}
			
			compare.tone = tone;
			AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_TONE_GOTO, compare );
			if(!AM_SEC_If_Tone_Goto(&sec_cmd))
			{
				AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, tone );
				AM_SEC_Set_Tone(dev_no, &sec_cmd);	
				usleep(sec_control.m_params[DELAY_AFTER_FINAL_CONT_TONE_CHANGE] * 1000);
			} 
		}

		AM_SEC_SetSecCommand( &sec_cmd, UPDATE_CURRENT_SWITCHPARMS);
		AM_SEC_Set_Update_Cur_SwitchPara(&sec_cmd);

		if (doSetFrontend)
		{
			/*tune timeout?*/			
			AM_DEBUG(1, "lock centre freq %d\n", convert_para.frequency);

			if((b_para == NULL) && (para != NULL))
			{
				if(status != NULL)
				{
					ret = AM_FEND_Lock(dev_no, &(convert_para), status);
				}
				else
				{
					ret = AM_FEND_SetPara(dev_no, &(convert_para));
				}
			}
		}

		return ret;

	}
	
	return AM_FENDCTRL_ERR_END;
}

static AM_ErrorCode_t AM_SEC_Init(void)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	int rc;

	int num = 0; 
	for ( num = 0; num < NUM_DATA_ENTRIES; num++)
	{
		am_sec_fend_data[num] = -1;
	}

	memset(&sec_control, 0, sizeof(AM_SEC_DVBSatelliteEquipmentControl_t));

	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);

	pthread_mutex_init(&p_sec_asyncinfo->lock, NULL);
	pthread_cond_init(&p_sec_asyncinfo->cond, NULL);
	
	p_sec_asyncinfo->dev_no = -1;
	p_sec_asyncinfo->enable_thread = AM_TRUE;
	sem_init(&p_sec_asyncinfo->sem_running, 1, 0);

	p_sec_asyncinfo->prepareexitnotify = AM_FALSE;
	p_sec_asyncinfo->preparerunning = AM_FALSE;	
	
	rc = pthread_create(&p_sec_asyncinfo->thread, NULL, AM_Sec_AsyncThread, p_sec_asyncinfo);
	if(rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));
		
		pthread_mutex_destroy(&p_sec_asyncinfo->lock);
		pthread_cond_destroy(&p_sec_asyncinfo->cond);
		
		ret = AM_FENDCTRL_ERR_CANNOT_CREATE_THREAD;
	}	
	
	return ret;
}

static AM_ErrorCode_t AM_SEC_DeInit(void)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_SEC_AsyncInfo_t *p_sec_asyncinfo = &(sec_control.m_sec_asyncinfo);
	
	/*Stop the thread*/
	p_sec_asyncinfo->enable_thread = AM_FALSE;
	pthread_join(p_sec_asyncinfo->thread, NULL);
	
	pthread_mutex_destroy(&p_sec_asyncinfo->lock);
	pthread_cond_destroy(&p_sec_asyncinfo->cond);

	sem_destroy(&p_sec_asyncinfo->sem_running);

	return ret;
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

	if(sec_init_flag == AM_FALSE)
	{
		sec_init_flag = AM_TRUE;
		AM_SEC_Init();
	}	

	//memset(&sec_control, 0, sizeof(AM_SEC_DVBSatelliteEquipmentControl_t));
	
	/* LNB Specific Parameters */
	sec_control.m_lnbs.m_lof_lo = para->m_lnbs.m_lof_lo;
	sec_control.m_lnbs.m_lof_hi = para->m_lnbs.m_lof_hi;
	sec_control.m_lnbs.m_lof_threshold = para->m_lnbs.m_lof_threshold;
	sec_control.m_lnbs.m_increased_voltage = para->m_lnbs.m_increased_voltage;
	sec_control.m_lnbs.m_prio = para->m_lnbs.m_prio;

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
	sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_sat_longitude =
		para->m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_sat_longitude;	

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
	sec_control.m_lnbs.LNBNum = para->m_lnbs.LNBNum;

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

	//AM_SEC_DumpSetting();

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

/**\brief 准备盲扫卫星设备控制
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_SEC_PrepareBlindScan(int dev_no)
{	
	AM_ErrorCode_t ret = AM_SUCCESS;

	sec_blind_flag = AM_TRUE;

	AM_DEBUG(1, "%s", "AM_SEC_PrepareBlindScan async\n");
	ret = AM_Sec_SetAsyncInfo(dev_no, &(sec_control.m_lnbs.b_para), NULL, NULL, 0);
	AM_Sec_AsyncSet();
	AM_DEBUG(1, "%s", "AM_SEC_PrepareBlindScan async ok\n");

#if 0
	ret = AM_SEC_Prepare(dev_no, &(sec_control.m_lnbs.b_para), NULL, NULL, 0);
#endif

	return ret;
}

/**\brief 中频转换传输频率
 * \param centre_freq unit KHZ
 * \param[out] tp_freq unit KHZ
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
AM_ErrorCode_t AM_SEC_FreqConvert(int dev_no, unsigned int centre_freq, unsigned int *tp_freq)
{
	assert(tp_freq);

	AM_ErrorCode_t ret = AM_FENDCTRL_ERROR_BASE;
	AM_FEND_Localoscollatorfreq_t cur_ocaloscollatorfreq = AM_FEND_LOCALOSCILLATORFREQ_H;

	if(sec_blind_flag == AM_FALSE)
	{
		long data = -1;
	
		AM_SEC_GetFendData(CUR_TONE, &data);
		if(data == SEC_TONE_ON)
		{
			cur_ocaloscollatorfreq = AM_FEND_LOCALOSCILLATORFREQ_H;
		}
		else if(data == SEC_TONE_OFF)
		{
			cur_ocaloscollatorfreq = AM_FEND_LOCALOSCILLATORFREQ_L;
		}
	}
	else
	{
		cur_ocaloscollatorfreq = sec_control.m_lnbs.b_para.ocaloscollatorfreq;
	}

	if(cur_ocaloscollatorfreq == AM_FEND_LOCALOSCILLATORFREQ_H)
	{
		if((sec_control.m_lnbs.m_lof_hi >= (M_KU_TP_START_FREQ - M_CENTRE_END_FREQ)) 
			&& (sec_control.m_lnbs.m_lof_hi <= (M_KU_TP_END_FREQ - M_CENTRE_START_FREQ)))
		{
			*tp_freq = sec_control.m_lnbs.m_lof_hi + centre_freq;
			ret = AM_SUCCESS;
		}
		else if((sec_control.m_lnbs.m_lof_hi >= (M_C_TP_START_FREQ + M_CENTRE_START_FREQ)) 
			&& (sec_control.m_lnbs.m_lof_hi <= (M_C_TP_END_FREQ + M_CENTRE_END_FREQ)))
		{
			*tp_freq = sec_control.m_lnbs.m_lof_hi - centre_freq;
			ret = AM_SUCCESS;
		}
	}
	else if(cur_ocaloscollatorfreq == AM_FEND_LOCALOSCILLATORFREQ_L)
	{
		if((sec_control.m_lnbs.m_lof_lo >= (M_KU_TP_START_FREQ - M_CENTRE_END_FREQ)) 
			&& (sec_control.m_lnbs.m_lof_lo <= (M_KU_TP_END_FREQ - M_CENTRE_START_FREQ)))
		{
			*tp_freq = sec_control.m_lnbs.m_lof_lo + centre_freq;
			ret = AM_SUCCESS;
		}
		else if((sec_control.m_lnbs.m_lof_lo >= (M_C_TP_START_FREQ + M_CENTRE_START_FREQ)) 
			&& (sec_control.m_lnbs.m_lof_lo <= (M_C_TP_END_FREQ + M_CENTRE_END_FREQ)))
		{
			*tp_freq = sec_control.m_lnbs.m_lof_lo - centre_freq;
			ret = AM_SUCCESS;
		}	
	}
	
	return ret;
}

AM_ErrorCode_t AM_SEC_PrepareTune(int dev_no, const AM_FENDCTRL_DVBFrontendParametersSatellite_t *para, fe_status_t *status, unsigned int tunetimeout)
{
	assert(para);
	assert(status);
	
	AM_ErrorCode_t ret = AM_SUCCESS;

	sec_blind_flag = AM_FALSE;

	AM_DEBUG(1, "%s", "AM_SEC_PrepareTune async\n");
	ret = AM_Sec_SetAsyncInfo(dev_no, NULL, para, status, tunetimeout);
	AM_Sec_AsyncSet();
	AM_DEBUG(1, "%s", "AM_SEC_PrepareTune async ok\n");

#if 0
	if(status == NULL)
	{
		AM_DEBUG(1, "%s", "AM_SEC_PrepareTune async\n");
		ret = AM_Sec_SetAsyncInfo(dev_no, para);
		AM_Sec_AsyncSet();
		AM_DEBUG(1, "%s", "AM_SEC_PrepareTune async ok\n");
	}
	else
	{
		AM_DEBUG(1, "%s", "AM_SEC_PrepareTune sync\n");
		ret = AM_SEC_Prepare(dev_no, NULL, para, status, tunetimeout);
	}
#endif

	return ret;
}

AM_ErrorCode_t AM_SEC_PrepareTurnOffSatCR(int dev_no, int satcr)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	eSecCommand_t sec_cmd;
	AM_SEC_DVBSatelliteLNBParameters_t lnb_param = sec_control.m_lnbs;
	
	// check if voltage is disabled
	pair_t compare;
	compare.steps = +8;	//only close frontend
	compare.voltage = SEC_VOLTAGE_OFF;

	AM_SEC_SetSecCommandByCompare(&sec_cmd, IF_VOLTAGE_GOTO, compare );
	if(!AM_SEC_If_Voltage_Goto(&sec_cmd, lnb_param.m_increased_voltage))
	{
		AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_13 );
		AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);
		usleep(sec_control.m_params[DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS] * 1000);

		AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_18 );
		AM_SEC_Set_Voltage(dev_no, &sec_cmd, AM_TRUE);
		AM_SEC_SetSecCommandByVal( &sec_cmd, SET_TONE, SEC_TONE_OFF );
		AM_SEC_Set_Tone(dev_no, &sec_cmd);			
		usleep(sec_control.m_params[DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS] * 1000);

		AM_FEND_Diseqccmd_SetODUPowerOff(dev_no, satcr); 
		usleep(sec_control.m_params[DELAY_AFTER_LAST_DISEQC_CMD] * 1000);
		AM_SEC_SetSecCommandByVal( &sec_cmd, SET_VOLTAGE, SEC_VOLTAGE_13 );
		AM_SEC_Set_Voltage(dev_no, &sec_cmd, lnb_param.m_increased_voltage);		
	}

	return ret;
}

AM_Bool_t AM_SEC_IsRotorMoving(void)
{
	return sec_control.m_rotorMoving;
}

void AM_SEC_SetCommandString(int dev_no, const char *str)
{
	assert(str);

	eSecCommand_t sec_cmd;
	eDVBDiseqcCommand_t diseqc_cmd;
	
	if (!str)
		return;
	diseqc_cmd.len=0;
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
			diseqc_cmd.data[i/2] = val;
		}
		else
			val = c << 4;
	}
	diseqc_cmd.len = slen/2;

	AM_SEC_SetSecCommandByDiseqc(&sec_cmd, SEND_DISEQC, diseqc_cmd);
	AM_SEC_Set_Diseqc(dev_no, &sec_cmd);

	return;
}

AM_ErrorCode_t AM_SEC_DumpSetting(void)
{	
	AM_ErrorCode_t ret = AM_SUCCESS;

	AM_DEBUG(1, "AM_SEC_DumpSetting Start\n");
	AM_DEBUG(1, "-----------------------\n");
	
	/* LNB Specific Parameters */
	AM_DEBUG(1, "LNB Specific Parameters:\n");
	AM_DEBUG(1, "m_lof_lo %d\n", sec_control.m_lnbs.m_lof_lo);
	AM_DEBUG(1, "m_lof_hi %d\n", sec_control.m_lnbs.m_lof_hi);
	AM_DEBUG(1, "m_lof_threshold %d\n", sec_control.m_lnbs.m_lof_threshold);
	AM_DEBUG(1, "m_increased_voltage %d\n", sec_control.m_lnbs.m_increased_voltage);
	AM_DEBUG(1, "m_prio %d\n", sec_control.m_lnbs.m_prio);

	/* DiSEqC Specific Parameters */
	AM_DEBUG(1, "DiSEqC Specific Parameters:\n");
	AM_DEBUG(1, "m_diseqc_mode %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_diseqc_mode);
	AM_DEBUG(1, "m_toneburst_param %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_toneburst_param);
	AM_DEBUG(1, "m_repeats %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_repeats);
	AM_DEBUG(1, "m_committed_cmd %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_committed_cmd);
	AM_DEBUG(1, "m_uncommitted_cmd %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_uncommitted_cmd);
	AM_DEBUG(1, "m_command_order %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_command_order);
	AM_DEBUG(1, "m_use_fast %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_use_fast);
	AM_DEBUG(1, "m_seq_repeat %d\n", sec_control.m_lnbs.m_diseqc_parameters.m_seq_repeat);
	
	/* Rotor Specific Parameters */
	AM_DEBUG(1, "Rotor Specific Parameters m_rotor_parameters:\n");
	AM_DEBUG(1, "m_longitude %f\n", sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_longitude);
	AM_DEBUG(1, "m_latitude %f\n", sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_latitude);
	AM_DEBUG(1, "m_sat_longitude %d\n", sec_control.m_lnbs.m_rotor_parameters.m_gotoxx_parameters.m_sat_longitude);

	AM_DEBUG(1, "Rotor Specific Parameters m_inputpower_parameters:\n");
	AM_DEBUG(1, "m_use %d\n", sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_use);
	AM_DEBUG(1, "m_delta %d\n", sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_delta);
	AM_DEBUG(1, "m_turning_speed %d\n", sec_control.m_lnbs.m_rotor_parameters.m_inputpower_parameters.m_turning_speed);
	
	/* Unicable Specific Parameters */
	AM_DEBUG(1, "Unicable Specific Parameters:\n");
	AM_DEBUG(1, "SatCR_idx %d\n", sec_control.m_lnbs.SatCR_idx);
	AM_DEBUG(1, "SatCRvco %d\n", sec_control.m_lnbs.SatCRvco);
	AM_DEBUG(1, "SatCR_positions %d\n", sec_control.m_lnbs.SatCR_positions);
	AM_DEBUG(1, "LNBNum %d\n", sec_control.m_lnbs.LNBNum);
	
	/* Satellite Specific Parameters */
	AM_DEBUG(1, "Satellite Specific Parameters:\n");
	AM_DEBUG(1, "m_voltage_mode %d\n", sec_control.m_lnbs.m_cursat_parameters.m_voltage_mode);
	AM_DEBUG(1, "m_22khz_signal %d\n", sec_control.m_lnbs.m_cursat_parameters.m_22khz_signal);
	AM_DEBUG(1, "m_rotorPosNum %d\n", sec_control.m_lnbs.m_cursat_parameters.m_rotorPosNum);

	AM_DEBUG(1, "-----------------------\n");
	AM_DEBUG(1, "AM_SEC_DumpSetting End\n");
	
	return ret;
}

