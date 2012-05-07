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

#include <string.h>
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

void AM_SEC_SetSecCommand( eSecCommand_t *sec_cmd, int cmd )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	
	return;
}

void AM_SEC_SetSecCommandByVal( eSecCommand_t *sec_cmd, int cmd, int val )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->val = val;
	
	return;
}

void AM_SEC_SetSecCommandByDiseqc( eSecCommand_t *sec_cmd, int cmd, eDVBDiseqcCommand_t diseqc )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->diseqc = diseqc;
	
	return;
}

void AM_SEC_SetSecCommandByMeasure( eSecCommand_t *sec_cmd, int cmd, rotor_t measure )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->measure = measure;
	
	return;
}

void AM_SEC_SetSecCommandByCompare( eSecCommand_t *sec_cmd, int cmd, pair_t compare )
{
	assert(sec_cmd);
	
	sec_cmd->cmd = cmd;
	sec_cmd->compare = compare;
	
	return;
}

