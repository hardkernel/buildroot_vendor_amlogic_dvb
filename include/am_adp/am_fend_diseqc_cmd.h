/***************************************************************************
 *  Copyright C 2012 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief Diseqc 协议命令
 *
 * \author jiang zhongming <zhongming.jiang@amlogic.com>
 * \date 2012-03-20: create the document
 ***************************************************************************/

#ifndef _AM_FEND_DISEQC_CMD_H
#define _AM_FEND_DISEQC_CMD_H

#include "am_types.h"
#include "am_evt.h"
#include <linux/dvb/frontend.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief Diseqc协议命令模块错误代码*/
enum AM_FEND_DISEQCCMD_ErrorCode
{
	AM_FEND_DISEQCCMD_ERROR_BASE=AM_ERROR_BASE(AM_MOD_FEND_DISEQCCMD),
	AM_FEND_DISEQCCMD_ERR_END
};

/****************************************************************************
 * Event type definitions
 ****************************************************************************/


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief DVB-S/S2前端switch输出*/
typedef enum AM_FEND_SWITCHINPUT
{
	AM_FEND_SWITCH1INPUTA,
	AM_FEND_SWITCH2INPUTA, 
	AM_FEND_SWITCH3INPUTA,
	AM_FEND_SWITCH4INPUTA,
	AM_FEND_SWITCH1INPUTB,
	AM_FEND_SWITCH2INPUTB,
	AM_FEND_SWITCH3INPUTB,
	AM_FEND_SWITCH4INPUTB	
}AM_FEND_Switchinput_t; 

/**\brief DVB-S/S2前端极性*/
typedef enum AM_FEND_POLARISATION
{
	AM_FEND_POLARISATION_H,
	AM_FEND_POLARISATION_V,
	AM_FEND_POLARISATION_NOSET
}AM_FEND_Polarisation_t; 

/**\brief DVB-S/S2前端本振频率*/
typedef enum AM_FEND_LOCALOSCILLATORFREQ
{
	AM_FEND_LOCALOSCILLATORFREQ_L,
	AM_FEND_LOCALOSCILLATORFREQ_H,
	AM_FEND_LOCALOSCILLATORFREQ_NOSET
}AM_FEND_Localoscollatorfreq_t;

/**\brief 22Khz参数,match AM_SEC_22khz_Signal*/ 
typedef enum {	AM_FEND_ON=0, AM_FEND_OFF=1 }AM_FEND_22khz_Signal; // 22 Khz

/**\brief 电压参数,match AM_SEC_Voltage_Mode*/ 
typedef enum {	AM_FEND_13V=0, AM_FEND_18V=1 }AM_FEND_Voltage_Mode; // 13/18 V

/**\brief DVB-S/S2前端本振频率表LOCAL OSCILLATOR FREQ TABLE*/
typedef enum AM_FEND_LOT
{
	AM_FEND_LOT_STANDARD_NONE = 0x00,
	AM_FEND_LOT_STANDARD_UNKNOWN,
	AM_FEND_LOT_STANDARD_9750MHZ,
	AM_FEND_LOT_STANDARD_10000MHZ,
	AM_FEND_LOT_STANDARD_10600MHZ,
	AM_FEND_LOT_STANDARD_10750MHZ,
	AM_FEND_LOT_STANDARD_11000MHZ,
	AM_FEND_LOT_STANDARD_11250MHZ,	
	AM_FEND_LOT_STANDARD_11475MHZ,
	AM_FEND_LOT_STANDARD_20250MHZ,
	AM_FEND_LOT_STANDARD_5150MHZ,	
	AM_FEND_LOT_STANDARD_1585MHZ,
	AM_FEND_LOT_STANDARD_13850MHZ,
	AM_FEND_LOT_WIDEBAND_NONE = 0x10,
	AM_FEND_LOT_WIDEBAND_10000MHZ,
	AM_FEND_LOT_WIDEBAND_10200MHZ,
	AM_FEND_LOT_WIDEBAND_13250MHZ,
	AM_FEND_LOT_WIDEBAND_13450MHZ
}AM_FEND_LOT_t;
                
/**\brief DVB-S/S2前端RF类型*/
typedef enum AM_FEND_RFType
{
	AM_FEND_RFTYPE_STANDARD,
	AM_FEND_RFTYPE_WIDEBAND
}AM_FEND_RFType_t;
                

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 重置Diseqc微控制器(Diseqc1.0 M*R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_ResetDiseqcMicro(int dev_no);

/**\brief 待机外围的Switch设备(Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_StandbySwitch(int dev_no);

/**\brief 供电外围的Switch设备(Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_PoweronSwitch(int dev_no);

/**\brief 选择低本振频率(Low Local Oscillator Freq)(Diseqc1.0 R)
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetLo(int dev_no);

/**\brief 选择垂直极化(polarisation)(Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetVR(int dev_no); 

/**\brief 选择卫星A (Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetSatellitePositionA(int dev_no); 

/**\brief 选择Switch Option A (Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetSwitchOptionA(int dev_no);

/**\brief 选择高本振频率(High Local Oscillator Freq)(Diseqc1.0 R)
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetHi(int dev_no);

/**\brief 选择水平极化(polarisation)(Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetHL(int dev_no);

/**\brief 选择卫星B (Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetSatellitePositionB(int dev_no); 

/**\brief 选择Switch Option B (Diseqc1.0 R)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetSwitchOptionB(int dev_no); 

/**\brief 选择Switch input (Diseqc1.1 R)
 * \param dev_no 前端设备号  
 * \param switch_input switch输入
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetSwitchInput(int dev_no, AM_FEND_Switchinput_t switch_input);

/**\brief 选择LNB1-LNB4\极性\本振频率 (Diseqc1.0 M) 
 * \param dev_no 前端设备号
 * \param lnbport LNB1-LNB4对应LNBPort取值AA=0, AB=1, BA=2, BB=3, SENDNO=4
 * \param polarisation 垂直水平极性 
 * \param local_oscillator_freq 高低本振频率   
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetLNBPort4(int dev_no, int lnbport, 
																AM_FEND_Polarisation_t polarisation, 
																AM_FEND_Localoscollatorfreq_t local_oscillator_freq);
                                                                 
/**\brief 选择LNB1-LNB16 (Diseqc1.1 M) 
 * \param dev_no 前端设备号
 * \param lnbport LNB1-LNB16对应LNBPort取值0xF0 .. 0xFF
 * \param polarisation 垂直水平极性 
 * \param local_oscillator_freq 高低本振频率 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetLNBPort16(int dev_no, int lnbport,
																AM_FEND_Polarisation_t polarisation, 
																AM_FEND_Localoscollatorfreq_t local_oscillator_freq);
                                                                  
/**\brief 设置channel频率 (Diseqc1.1 M) 
 * \param dev_no 前端设备号
 * \param Freq unit KHZ 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetChannelFreq(int dev_no, int freq);

/**\brief 停止定位器(positioner)移动 (Diseqc1.2 M)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetPositionerHalt(int dev_no);

/**\brief 允许定位器(positioner)限制 (Diseqc1.2 M)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_EnablePositionerLimit(int dev_no);
																  
/**\brief 禁止定位器(positioner)限制 (Diseqc1.2 M)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_DisablePositionerLimit(int dev_no);

/**\brief 允许定位器(positioner)限制并存储方向东的限制 (Diseqc1.2 M)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetPositionerELimit(int dev_no);

/**\brief 允许定位器(positioner)限制并存储方向西的限制 (Diseqc1.2 M)
 * \param dev_no 前端设备号 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetPositionerWLimit(int dev_no);

/**\brief 定位器(positioner)向东移动 (Diseqc1.2 M) 
 * \param dev_no 前端设备号
 * \param unit 00 continuously 01-7F(单位second, e.g 01-one second 02-two second) 80-FF (单位step，e.g FF-one step FE-two step)
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_PositionerGoE(int dev_no, unsigned char unit);

/**\brief 定位器(positioner)向西移动 (Diseqc1.2 M) 
 * \param dev_no 前端设备号
 * \param unit 00 continuously 01-7F(单位second, e.g 01-one second 02-two second) 80-FF (单位step，e.g FF-one step FE-two step)  
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_PositionerGoW(int dev_no, unsigned char unit);

/**\brief 存储定位信息 (Diseqc1.2 M) 
 * \param dev_no 前端设备号
 * \param position 位置 (e.g 1,2...) 0 参考位置，实际不存储，可当做Enable limit命令使用
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_StorePosition(int dev_no, unsigned char position);

/**\brief 定位器(positioner)定位到之前存储的定位信息位置 (Diseqc1.2 M) 
 * \param dev_no 前端设备号
 * \param position 位置 (e.g 1,2...) 0 参考位置
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_GotoPositioner(int dev_no, unsigned char position); 

/**\brief 定位器(positioner)根据经纬度定位到卫星 (gotoxx Diseqc extention) 
 * \param dev_no 前端设备号
 * \param local_longitude 本地经度
 * \param local_latitude 本地纬度
 * \param satellite_longitude 卫星经度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_GotoxxAngularPositioner(int dev_no, double local_longitude, double local_latitude, double satellite_longitude);

/**\brief 定位器(positioner)根据经纬度定位到卫星 (USALS(another name Diseqc1.3) Diseqc extention) 
 * \param dev_no 前端设备号
 * \param local_longitude 本地经度
 * \param local_latitude 本地纬度
 * \param satellite_longitude 卫星经度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_GotoAngularPositioner(int dev_no, double local_longitude, double local_latitude, double satellite_longitude);
                                                                       
/**\brief 设置ODU channel (Diseqc extention) 
 * \param dev_no 前端设备号
 * \param ub_number user band number(0-7)
 * \param inputbank_number input bank number (0-7)
 * \param transponder_freq unit KHZ
 * \param oscillator_freq unit KHZ
 * \param ub_freq unit KHZ    
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetODUChannel(int dev_no, unsigned char ub_number, unsigned char inputbank_number,
																int transponder_freq, int oscillator_freq, int ub_freq);
       
/**\brief 关闭ODU UB (Diseqc extention) 
 * \param dev_no 前端设备号
 * \param ub_number user band number(0-7)  
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetODUPowerOff(int dev_no, unsigned char ub_number); 

/**\brief 打开所有ODU UB (Diseqc extention)
 * \param dev_no 前端设备号  
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetODUUbxSignalOn(int dev_no);
                                                                 
/**\brief 设置ODU (Diseqc extention) 
 * \param dev_no 前端设备号
 * \param ub_number user band number(0-7)   
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetODUConfig(int dev_no, unsigned char ub_number, 
																unsigned char satellite_position_count,
																unsigned char input_bank_count,
																AM_FEND_RFType_t rf_type,
																unsigned char ub_count);
                                                                  
/**\brief 设置ODU 本振(oscillator)频率 (Diseqc extention) 
 * \param dev_no 前端设备号
 * \param ub_number user band number(0-7)    
 * \param lot 本振频率表选择
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern AM_ErrorCode_t AM_FEND_Diseqccmd_SetODULoFreq(int dev_no, unsigned char ub_number, AM_FEND_LOT_t lot);                                                                  
                                                                               

/**\brief 根据经纬度生成到卫星方位角 (USALS(another name Diseqc1.3) Diseqc extention) 
 * \param dev_no 前端设备号
 * \param local_longitude 本地经度
 * \param local_latitude 本地纬度
 * \param satellite_longitude 卫星经度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_diseqc_cmd.h)
 */
extern int AM_ProduceAngularPositioner(int dev_no, double local_longitude, double local_latitude, double satellite_longitude);
																		
#ifdef __cplusplus
}
#endif

#endif
