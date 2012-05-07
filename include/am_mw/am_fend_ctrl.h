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
typedef enum { DISEQC_NONE=0, V1_0=1, V1_1=2, V1_2=3, SMATV=4 }AM_SEC_Diseqc_Mode;	// DiSEqC Mode
typedef enum { NO=0, A=1, B=2 }AM_SEC_Toneburst_Param;

typedef struct AM_SEC_DVBSatelliteDiseqcParameters
{
	unsigned char m_committed_cmd;
	AM_SEC_Diseqc_Mode m_diseqc_mode;
	AM_SEC_Toneburst_Param m_toneburst_param;

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
}AM_SEC_DVBSatelliteDiseqcParameters_t;

typedef enum {	HILO=0, ON=1, OFF=2	}AM_SEC_22khz_Signal; // 22 Khz
typedef enum {	HV=0, _14V=1, _18V=2, _0V=3, HV_13=4 }AM_SEC_Voltage_Mode; // 14/18 V

typedef struct AM_SEC_DVBSatelliteSwitchParameters
{
	AM_SEC_Voltage_Mode m_voltage_mode;
	AM_SEC_22khz_Signal m_22khz_signal;
	unsigned char m_rotorPosNum; // 0 is disable.. then use gotoxx
}AM_SEC_DVBSatelliteSwitchParameters_t;

typedef enum { NORTH, SOUTH, EAST, WEST }AM_SEC_Rotor_Direction;
enum { FAST, SLOW };

typedef struct AM_SEC_DVBSatelliteRotorInputpowerParameters
{
	AM_Bool_t m_use;	// can we use rotor inputpower to detect rotor running state ?
	unsigned char m_delta;	// delta between running and stopped rotor
	unsigned int m_turning_speed; // SLOW, FAST, or fast turning epoch
}AM_SEC_DVBSatelliteRotorInputpowerParameters_t;

typedef struct AM_SEC_DVBSatelliteRotorGotoxxParameters
{
	AM_SEC_Rotor_Direction m_lo_direction;	// EAST, WEST
	AM_SEC_Rotor_Direction m_la_direction;	// NORT, SOUTH
	double m_longitude;	// longitude for gotoXX? function
	double m_latitude;	// latitude for gotoXX? function
}AM_SEC_DVBSatelliteRotorGotoxxParameters_t;

typedef struct AM_SEC_DVBSatelliteRotorParameters
{
	AM_SEC_DVBSatelliteRotorInputpowerParameters_t m_inputpower_parameters;

	AM_SEC_DVBSatelliteRotorGotoxxParameters_t m_gotoxx_parameters;
}AM_SEC_DVBSatelliteRotorParameters_t;

typedef enum { RELAIS_OFF=0, RELAIS_ON }AM_SEC_12V_Relais_State;


typedef struct AM_SEC_DVBSatelliteLNBParameters
{
	AM_SEC_12V_Relais_State m_12V_relais_state;	// 12V relais output on/off

	int m_slot_mask; // useable by slot ( 1 | 2 | 4...)

	unsigned int m_lof_hi,	// for 2 band universal lnb 10600 Mhz (high band offset frequency)
				m_lof_lo,	// for 2 band universal lnb  9750 Mhz (low band offset frequency)
				m_lof_threshold;	// for 2 band universal lnb 11750 Mhz (band switch frequency)

	AM_Bool_t m_increased_voltage; // use increased voltage ( 14/18V )

	AM_SEC_DVBSatelliteSwitchParameters_t m_cursat_parameters;
	AM_SEC_DVBSatelliteDiseqcParameters_t m_diseqc_parameters;
	AM_SEC_DVBSatelliteRotorParameters_t m_rotor_parameters;

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
}AM_SEC_DVBSatelliteLNBParameters_t;

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

typedef struct AM_SEC_DVBSatelliteEquipmentControl
{
	AM_SEC_DVBSatelliteLNBParameters_t m_lnbs; // i think its enough
	int m_rotorMoving;
	AM_Bool_t m_canMeasureInputPower;

	AM_SEC_Cmd_Param_t m_params[MAX_PARAMS];
}AM_SEC_DVBSatelliteEquipmentControl_t;


typedef struct AM_FENDCTRL_DVBFrontendParametersSatellite
{
	enum {
		Polarisation_Horizontal, Polarisation_Vertical, Polarisation_CircularLeft, Polarisation_CircularRight
	};

	AM_Bool_t no_rotor_command_on_tune;
	int polarisation, orbital_position;

	struct dvb_frontend_parameters para;
}AM_FENDCTRL_DVBFrontendParametersSatellite_t;

typedef struct AM_FENDCTRL_DVBFrontendParametersCable
{
	struct dvb_frontend_parameters para;
}AM_FENDCTRL_DVBFrontendParametersCable_t;

typedef struct AM_FENDCTRL_DVBFrontendParametersTerrestrial
{
	struct dvb_frontend_parameters para;
}AM_FENDCTRL_DVBFrontendParametersTerrestrial_t;

typedef struct AM_FENDCTRL_DVBFrontendParametersATSC
{
	struct dvb_frontend_parameters para;
}AM_FENDCTRL_DVBFrontendParametersATSC_t;

typedef enum {DVBS = 0, DVBC = 1, DVBT = 2, ATSC = 3}AM_FENDCTRL_Fendtype;

typedef struct AM_FENDCTRL_DVBFrontendParameters{
	union
	{
		AM_FENDCTRL_DVBFrontendParametersSatellite_t sat;
		AM_FENDCTRL_DVBFrontendParametersCable_t cable;
		AM_FENDCTRL_DVBFrontendParametersTerrestrial_t terrestrial;
		AM_FENDCTRL_DVBFrontendParametersATSC_t atsc;
	};	
	AM_FENDCTRL_Fendtype m_type;
}AM_FENDCTRL_DVBFrontendParameters_t;

/* sec control interface */
extern AM_ErrorCode_t AM_SEC_SetSetting(AM_SEC_DVBSatelliteEquipmentControl_t *setting);
extern AM_ErrorCode_t AM_SEC_GetSetting(AM_SEC_DVBSatelliteEquipmentControl_t *setting);

/* frontend control interface */
extern AM_ErrorCode_t AM_FENDCTRL_SetPara(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para);
extern AM_ErrorCode_t AM_FENDCTRL_Lock(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para, fe_status_t *status);

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

