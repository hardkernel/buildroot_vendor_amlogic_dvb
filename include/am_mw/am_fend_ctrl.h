/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_fend_ctrl.h
 * \brief Frontend控制模块头文件
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2012-05-06: create the document
 ***************************************************************************/

#ifndef _AM_FEND_CTRL_H
#define _AM_FEND_CTRL_H

#include <am_types.h>
#include <am_mem.h>
#include <am_fend.h>
#include <am_fend_diseqc_cmd.h>


#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define guard_offset_min -8000
#define guard_offset_max 8000
#define guard_offset_step 8000
#define MAX_SATCR 8
#define MAX_LNBNUM 32

/****************************************************************************
 * Type definitions
 ***************************************************************************/
enum { AA=0, AB=1, BA=2, BB=3, SENDNO=4 /* and 0xF0 .. 0xFF*/  };	// DiSEqC Parameter
typedef enum { DISEQC_NONE=0, V1_0=1, V1_1=2, V1_2=3, SMATV=4 }t_diseqc_mode;	// DiSEqC Mode
typedef enum { NO=0, A=1, B=2 }t_toneburst_param;

typedef struct eDVBSatelliteDiseqcParameters
{
	unsigned char m_committed_cmd;
	t_diseqc_mode m_diseqc_mode;
	t_toneburst_param m_toneburst_param;

	unsigned char m_repeats;	// for cascaded switches
	AM_Bool_t m_use_fast;	// send no DiSEqC on H/V or Lo/Hi change
	AM_Bool_t m_seq_repeat;	// send the complete DiSEqC Sequence twice...
	unsigned char m_command_order;
	/* 	diseqc 1.0)
			0) commited, toneburst
			1) toneburst, committed
		diseqc > 1.0)
			2) committed, uncommitted, toneburst
			3) toneburst, committed, uncommitted
			4) uncommitted, committed, toneburst
			5) toneburst, uncommitted, committed */
	unsigned char m_uncommitted_cmd;	// state of the 4 uncommitted switches..
}eDVBSatelliteDiseqcParameters_t;

typedef enum {	HILO=0, ON=1, OFF=2	}t_22khz_signal; // 22 Khz
typedef enum {	HV=0, _14V=1, _18V=2, _0V=3, HV_13=4 }t_voltage_mode; // 14/18 V

typedef struct eDVBSatelliteSwitchParameters
{
	t_voltage_mode m_voltage_mode;
	t_22khz_signal m_22khz_signal;
	unsigned char m_rotorPosNum; // 0 is disable.. then use gotoxx
}eDVBSatelliteSwitchParameters_t;

typedef enum { NORTH, SOUTH, EAST, WEST }AM_SEC_Rotor_Direction;
enum { FAST, SLOW };

typedef struct eDVBSatelliteRotorInputpowerParameters
{
	AM_Bool_t m_use;	// can we use rotor inputpower to detect rotor running state ?
	unsigned char m_delta;	// delta between running and stopped rotor
	unsigned int m_turning_speed; // SLOW, FAST, or fast turning epoch
}eDVBSatelliteRotorInputpowerParameters_t;

typedef struct eDVBSatelliteRotorGotoxxParameters
{
	AM_SEC_Rotor_Direction m_lo_direction;	// EAST, WEST
	AM_SEC_Rotor_Direction m_la_direction;	// NORT, SOUTH
	double m_longitude;	// longitude for gotoXX? function
	double m_latitude;	// latitude for gotoXX? function
}eDVBSatelliteRotorGotoxxParameters_t;

typedef struct eDVBSatelliteRotorParameters
{
	eDVBSatelliteRotorInputpowerParameters_t m_inputpower_parameters;

	eDVBSatelliteRotorGotoxxParameters_t m_gotoxx_parameters;
}eDVBSatelliteRotorParameters_t;

typedef enum { RELAIS_OFF=0, RELAIS_ON }t_12V_relais_state;

typedef struct eDVBSatelliteSwitchParametersList
{
	eDVBSatelliteSwitchParameters_t satswitch[100];
	eDVBSatelliteSwitchParameters_t *cur_satswitch;
}eDVBSatelliteSwitchParametersList_t;

typedef struct eDVBSatelliteLNBParameters
{
	t_12V_relais_state m_12V_relais_state;	// 12V relais output on/off

	int m_slot_mask; // useable by slot ( 1 | 2 | 4...)

	unsigned int m_lof_hi,	// for 2 band universal lnb 10600 Mhz (high band offset frequency)
				m_lof_lo,	// for 2 band universal lnb  9750 Mhz (low band offset frequency)
				m_lof_threshold;	// for 2 band universal lnb 11750 Mhz (band switch frequency)

	AM_Bool_t m_increased_voltage; // use increased voltage ( 14/18V )

	eDVBSatelliteSwitchParametersList_t m_satellites;
	eDVBSatelliteDiseqcParameters_t m_diseqc_parameters;
	eDVBSatelliteRotorParameters_t m_rotor_parameters;

	int m_prio; // to override automatic tuner management ... -1 is Auto

	int SatCR_positions;
	int SatCR_idx;
	unsigned int SatCRvco;
	unsigned int UnicableTuningWord;
	unsigned int UnicableConfigWord;
	int old_frequency;
	int old_polarisation;
	int old_orbital_position;
	int guard_offset_old;
	int guard_offset;
	int LNBNum;
}eDVBSatelliteLNBParameters_t;

typedef enum{
	DELAY_AFTER_CONT_TONE_DISABLE_BEFORE_DISEQC=0,  // delay after continuous tone disable before diseqc command
	DELAY_AFTER_FINAL_CONT_TONE_CHANGE, // delay after continuous tone change before tune
	DELAY_AFTER_FINAL_VOLTAGE_CHANGE, // delay after voltage change at end of complete sequence
	DELAY_BETWEEN_DISEQC_REPEATS, // delay between repeated diseqc commands
	DELAY_AFTER_LAST_DISEQC_CMD, // delay after last diseqc command
	DELAY_AFTER_TONEBURST, // delay after toneburst
	DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_SWITCH_CMDS, // delay after enable voltage before transmit toneburst/diseqc
	DELAY_BETWEEN_SWITCH_AND_MOTOR_CMD, // delay after transmit toneburst / diseqc and before transmit motor command
	DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MEASURE_IDLE_INPUTPOWER, // delay after voltage change before measure idle input power
	DELAY_AFTER_ENABLE_VOLTAGE_BEFORE_MOTOR_CMD, // delay after enable voltage before transmit motor command
	DELAY_AFTER_MOTOR_STOP_CMD, // delay after transmit motor stop
	DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_MOTOR_CMD, // delay after voltage change before transmit motor command
	DELAY_BEFORE_SEQUENCE_REPEAT, // delay before the complete sequence is repeated (when enabled)
	MOTOR_COMMAND_RETRIES, // max transmit tries of rotor command when the rotor dont start turning (with power measurement)
	MOTOR_RUNNING_TIMEOUT, // max motor running time before timeout
	DELAY_AFTER_VOLTAGE_CHANGE_BEFORE_SWITCH_CMDS, // delay after change voltage before transmit toneburst/diseqc
	DELAY_AFTER_DISEQC_RESET_CMD,
	DELAY_AFTER_DISEQC_PERIPHERIAL_POWERON_CMD,
	MAX_PARAMS
}AM_SEC_Cmd_Param_t;

typedef struct eDVBSatelliteEquipmentControl
{
	eDVBSatelliteLNBParameters_t m_lnbs[144]; // i think its enough
	int m_lnbidx; // current index for set parameters
	eDVBSatelliteSwitchParameters_t m_curSat;
	int m_rotorMoving;
	int m_not_linked_slot_mask;
	AM_Bool_t m_canMeasureInputPower;

	AM_SEC_Cmd_Param_t m_params[MAX_PARAMS];
}eDVBSatelliteEquipmentControl_t;


typedef struct eDVBFrontendParametersSatellite
{
	enum {
		Polarisation_Horizontal, Polarisation_Vertical, Polarisation_CircularLeft, Polarisation_CircularRight
	};

	AM_Bool_t no_rotor_command_on_tune;
	int polarisation, orbital_position;

	struct dvb_frontend_parameters para;
}eDVBFrontendParametersSatellite_t;

typedef struct eDVBFrontendParametersCable
{
	struct dvb_frontend_parameters para;
}eDVBFrontendParametersCable_t;

typedef struct eDVBFrontendParametersTerrestrial
{
	struct dvb_frontend_parameters para;
}eDVBFrontendParametersTerrestrial_t;

typedef struct eDVBFrontendParametersATSC
{
	struct dvb_frontend_parameters para;
}eDVBFrontendParametersATSC_t;

typedef struct eDVBFrontendParameters{
	union
	{
		eDVBFrontendParametersSatellite_t sat;
		eDVBFrontendParametersCable_t cable;
		eDVBFrontendParametersTerrestrial_t terrestrial;
		eDVBFrontendParametersATSC_t atsc;
	};	
	int m_type;
}eDVBFrontendParameters_t;

extern AM_ErrorCode_t AM_SEC_Init(void);
extern AM_ErrorCode_t AM_SEC_DeInit(void);

/* LNB Specific Parameters */
extern AM_ErrorCode_t AM_SEC_AddLNB(void);
extern AM_ErrorCode_t AM_SEC_SetLNBSlotMask(int slotmask);
extern AM_ErrorCode_t AM_SEC_SetLNBLOFL(int lofl);
extern AM_ErrorCode_t AM_SEC_SetLNBLOFH(int lofh);
extern AM_ErrorCode_t AM_SEC_SetLNBThreshold(int threshold);
extern AM_ErrorCode_t AM_SEC_SetLNBIncreasedVoltage(AM_Bool_t onoff);
extern AM_ErrorCode_t AM_SEC_SetLNBPrio(int prio);
extern AM_ErrorCode_t AM_SEC_SetLNBNum(int LNBNum);
/* DiSEqC Specific Parameters */
extern AM_ErrorCode_t AM_SEC_SetDiSEqCMode(t_diseqc_mode diseqcmode);
extern AM_ErrorCode_t AM_SEC_SetToneburst(t_toneburst_param toneburst);
extern AM_ErrorCode_t AM_SEC_SetRepeats(int repeats);
extern AM_ErrorCode_t AM_SEC_SetCommittedCommand(int command);
extern AM_ErrorCode_t AM_SEC_SetUncommittedCommand(int command);
extern AM_ErrorCode_t AM_SEC_SetCommandOrder(int order);
extern AM_ErrorCode_t AM_SEC_SetFastDiSEqC(AM_Bool_t onoff);
extern AM_ErrorCode_t AM_SEC_SetSeqRepeat(AM_Bool_t onoff); // send the complete switch sequence twice (without rotor command)
/* Rotor Specific Parameters */
extern AM_ErrorCode_t AM_SEC_SetLongitude(float longitude);
extern AM_ErrorCode_t AM_SEC_SetLatitude(float latitude);
extern AM_ErrorCode_t AM_SEC_SetLoDirection(AM_SEC_Rotor_Direction direction);
extern AM_ErrorCode_t AM_SEC_SetLaDirection(AM_SEC_Rotor_Direction direction);
extern AM_ErrorCode_t AM_SEC_SetUseInputpower(AM_Bool_t onoff);
extern AM_ErrorCode_t AM_SEC_SetInputpowerDelta(int delta);  // delta between running and stopped rotor
extern AM_ErrorCode_t AM_SEC_SetRotorTurningSpeed(int speed);  // set turning speed..
/* Unicable Specific Parameters */
extern AM_ErrorCode_t AM_SEC_SetLNBSatCR(int SatCR_idx);
extern AM_ErrorCode_t AM_SEC_SetLNBSatCRvco(int SatCRvco);
extern AM_ErrorCode_t AM_SEC_SetLNBSatCRpositions(int SatCR_positions);
extern AM_ErrorCode_t AM_SEC_GetLNBSatCR(void);
extern AM_ErrorCode_t AM_SEC_GetLNBSatCRvco(void);
extern AM_ErrorCode_t AM_SEC_GetLNBSatCRpositions(void);
/* Satellite Specific Parameters */
extern AM_ErrorCode_t AM_SEC_AddSatellite(int orbital_position);
extern AM_ErrorCode_t AM_SEC_SetVoltageMode(t_voltage_mode mode);
extern AM_ErrorCode_t AM_SEC_SetToneMode(t_22khz_signal mode);
extern AM_ErrorCode_t AM_SEC_SetRotorPosNum(int rotor_pos_num);

extern AM_Bool_t AM_SEC_IsRotorMoving(void);

/* frontend control interface */
extern AM_ErrorCode_t AM_FENDCTRL_SetPara(int dev_no, const eDVBFrontendParameters_t *para);
extern AM_ErrorCode_t AM_FENDCTRL_Lock(int dev_no, const eDVBFrontendParameters_t *para, fe_status_t *status);



/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

