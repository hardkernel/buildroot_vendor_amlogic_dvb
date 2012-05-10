/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_sec_internal.h
 * \brief SEC卫星设备控制模块内部头文件
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2012-05-06: create the document
 ***************************************************************************/

#ifndef _AM_SEC_INTERNAL_H
#define _AM_SEC_INTERNAL_H

#include "list.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define MAX_DISEQC_LENGTH  16

/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef struct eDVBDiseqcCommand
{
	int len;
	unsigned char data[MAX_DISEQC_LENGTH];
}eDVBDiseqcCommand_t;

enum AM_SEC_CMD_MODE{ modeStatic, modeDynamic };
enum AM_SEC_CMD{
	NONE, SLEEP, SET_VOLTAGE, SET_TONE, GOTO,
	SEND_DISEQC, SEND_TONEBURST, SET_FRONTEND,
	SET_TIMEOUT, IF_TIMEOUT_GOTO, 
	IF_VOLTAGE_GOTO, IF_NOT_VOLTAGE_GOTO,
	SET_POWER_LIMITING_MODE,
	SET_ROTOR_DISEQC_RETRYS, IF_NO_MORE_ROTOR_DISEQC_RETRYS_GOTO,
	MEASURE_IDLE_INPUTPOWER, MEASURE_RUNNING_INPUTPOWER,
	IF_MEASURE_IDLE_WAS_NOT_OK_GOTO, IF_INPUTPOWER_DELTA_GOTO,
	UPDATE_CURRENT_ROTORPARAMS, INVALIDATE_CURRENT_ROTORPARMS,
	UPDATE_CURRENT_SWITCHPARMS, INVALIDATE_CURRENT_SWITCHPARMS,
	IF_ROTORPOS_VALID_GOTO,
	IF_TUNER_LOCKED_GOTO,
	IF_TONE_GOTO, IF_NOT_TONE_GOTO,
	START_TUNE_TIMEOUT,
	SET_ROTOR_MOVING,
	SET_ROTOR_STOPPED,
	DELAYED_CLOSE_FRONTEND
};

typedef struct rotor
{
	union {
		int deltaA;   // difference in mA between running and stopped rotor
		int lastSignal;
	};
	int okcount;  // counter
	int steps;    // goto steps
	int direction;
}rotor_t;

typedef struct pair
{
	union
	{
		int voltage;
		int tone;
		int val;
	};
	int steps;
}pair_t;

typedef struct eSecCommand
{
	/* Use kernel list is not portable,  I suggest add a module in future*/
	struct list_head head;

	int cmd;

	union
	{
		int val;
		int steps;
		int timeout;
		int voltage;
		int tone;
		int toneburst;
		int msec;
		int mode;
		rotor_t measure;
		eDVBDiseqcCommand_t diseqc;
		pair_t compare;
	};
}eSecCommand_t;


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/
void AM_SEC_SetCommandString(eDVBDiseqcCommand_t *diseqc_cmd, const char *str);

#ifdef __cplusplus
}
#endif

#endif

