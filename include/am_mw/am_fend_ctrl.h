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

#include <linux/dvb/frontend.h>

#include <am_types.h>
#include <am_mem.h>
#include <am_fend.h>
#include <am_fend_diseqc_cmd.h>

#include <semaphore.h>

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
 * Error code definitions
 ****************************************************************************/
 
/**\brief 前端控制模块错误代码*/
enum AM_FENDCTRL_ErrorCode
{
	AM_FENDCTRL_ERROR_BASE=AM_ERROR_BASE(AM_MOD_FENDCTRL),
	AM_FENDCTRL_ERR_CANNOT_CREATE_THREAD,	/**< 无法创建线程*/
	AM_FENDCTRL_ERR_END
};  

/****************************************************************************
 * Event type definitions
 ****************************************************************************/

/**\brief 前端控制模块事件类型*/
enum AM_FENDCTRL_EventType
{
	AM_FENDCTRL_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_FENDCTRL),
	AM_FENDCTRL_EVT_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/ 

/**\brief DVB-S前端控制模块设置参数*/ 
typedef struct AM_FENDCTRL_DVBFrontendParametersSatellite
{
	struct dvb_frontend_parameters para; /**< 前端控制模块设置参数*/

	AM_Bool_t no_rotor_command_on_tune;  /**< 锁频时是否有马达*/
	AM_FEND_Polarisation_t polarisation; /**< LNB极性*/
}AM_FENDCTRL_DVBFrontendParametersSatellite_t;

/**\brief DVB-C前端控制模块设置参数*/ 
typedef struct AM_FENDCTRL_DVBFrontendParametersCable
{
	struct dvb_frontend_parameters para; /**< 前端控制模块设置参数*/
}AM_FENDCTRL_DVBFrontendParametersCable_t;

/**\brief DVB-T前端控制模块设置参数*/
typedef struct AM_FENDCTRL_DVBFrontendParametersTerrestrial
{
	struct dvb_frontend_parameters para; /**< 前端控制模块设置参数*/
}AM_FENDCTRL_DVBFrontendParametersTerrestrial_t;

/**\brief ATSC前端控制模块设置参数*/
typedef struct AM_FENDCTRL_DVBFrontendParametersATSC
{
	struct dvb_frontend_parameters para; /**< 前端控制模块设置参数*/
}AM_FENDCTRL_DVBFrontendParametersATSC_t;

/**\brief ATSC前端控制模块设置参数*/
typedef struct AM_FENDCTRL_DVBFrontendParametersAnalog
{
	struct dvb_frontend_parameters para; /**< 前端控制模块设置参数*/
}AM_FENDCTRL_DVBFrontendParametersAnalog_t;

/**\brief 前端控制模块设置参数*/
typedef struct AM_FENDCTRL_DVBFrontendParameters{
	union
	{
		AM_FENDCTRL_DVBFrontendParametersSatellite_t sat;
		AM_FENDCTRL_DVBFrontendParametersCable_t cable;
		AM_FENDCTRL_DVBFrontendParametersTerrestrial_t terrestrial; 
		AM_FENDCTRL_DVBFrontendParametersATSC_t atsc;
		AM_FENDCTRL_DVBFrontendParametersAnalog_t analog;
	};	
	int m_type; /**< 前端控制模块解调模式*/
}AM_FENDCTRL_DVBFrontendParameters_t;
  
/**\brief DiSEqC参数*/
enum { AA=0, AB=1, BA=2, BB=3, SENDNO=4 /* and 0xF0 .. 0xFF*/  };	// DiSEqC Parameter

/**\brief DiSEqC模式*/
typedef enum { DISEQC_NONE=0, V1_0=1, V1_1=2, V1_2=3, V1_3=4, SMATV=5 }AM_SEC_Diseqc_Mode;	// DiSEqC Mode

/**\brief Toneburst参数*/
typedef enum { NO=0, A=1, B=2 }AM_SEC_Toneburst_Param;

/**\brief 卫星设备（diseqc）控制参数*/ 
typedef struct AM_SEC_DVBSatelliteDiseqcParameters
{
	unsigned char m_committed_cmd;                /**< committed命令*/
	AM_SEC_Diseqc_Mode m_diseqc_mode;             /**< DiSEqC模式*/
	AM_SEC_Toneburst_Param m_toneburst_param;     /**< Toneburst参数*/

	unsigned char m_repeats;	// for cascaded switches
	AM_Bool_t m_use_fast;	// send no DiSEqC on H/V or Lo/Hi change
	AM_Bool_t m_seq_repeat;	// send the complete DiSEqC Sequence twice...
	unsigned char m_command_order;					/**< low 4 bits for diseqc 1.0, high 4 bits for diseqc > 1.0*/
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

/**\brief 22Khz参数*/ 
typedef enum {	ON=0, OFF=1, HILO=2}AM_SEC_22khz_Signal; // 22 Khz

/**\brief 电压参数*/ 
typedef enum {	_14V=0, _18V=1, _0V=2, HV=3, HV_13=4 }AM_SEC_Voltage_Mode; // 14/18 V

/**\brief 卫星设备（switch）控制参数*/ 
typedef struct AM_SEC_DVBSatelliteSwitchParameters
{
	AM_SEC_Voltage_Mode m_voltage_mode;		/**< 电压参数*/ 
	AM_SEC_22khz_Signal m_22khz_signal;		/**< 22Khz参数*/ 
	unsigned char m_rotorPosNum;			/**< 马达位置索引 0 is disable.. then use gotoxx*/ 
}AM_SEC_DVBSatelliteSwitchParameters_t;

/**\brief 马达速度属性*/ 
enum { FAST, SLOW };

/**\brief 马达供电参数*/ 
typedef struct AM_SEC_DVBSatelliteRotorInputpowerParameters
{
	AM_Bool_t m_use;	// can we use rotor inputpower to detect rotor running state ?
	unsigned char m_delta;	// delta between running and stopped rotor
	unsigned int m_turning_speed; // SLOW, FAST, or fast turning epoch
}AM_SEC_DVBSatelliteRotorInputpowerParameters_t;

/**\brief 马达定位参数*/ 
typedef struct AM_SEC_DVBSatelliteRotorGotoxxParameters
{
	double m_longitude;	// longitude for gotoXX? function
	double m_latitude;	// latitude for gotoXX? function
	int m_sat_longitude;	// longitude for gotoXX? function of satellite unit-0.1 degree
}AM_SEC_DVBSatelliteRotorGotoxxParameters_t;

/**\brief 卫星设备马达控制参数*/ 
typedef struct AM_SEC_DVBSatelliteRotorParameters
{
	AM_SEC_DVBSatelliteRotorInputpowerParameters_t m_inputpower_parameters; /**< 马达供电参数*/
	AM_SEC_DVBSatelliteRotorGotoxxParameters_t m_gotoxx_parameters;         /**< 马达定位参数*/
	AM_Bool_t m_reset_rotor_status_cache;								/**< 马达缓存参数重置*/
	unsigned int m_rotor_move_unit;  										/**< 马达转动调整单位 00 continuously 01-7F(单位second, e.g 01-one second 02-two second) 80-FF (单位step，e.g FF-one step FE-two step)*/
}AM_SEC_DVBSatelliteRotorParameters_t;

typedef enum { RELAIS_OFF=0, RELAIS_ON }AM_SEC_12V_Relais_State;

/**\brief DVB-S前端控制模块盲扫设置参数*/ 
typedef struct AM_FENDCTRL_DVBFrontendParametersBlindSatellite
{
	AM_FEND_Polarisation_t polarisation;               /**< DVB-S前端控制模块盲扫极性*/ 
	AM_FEND_Localoscollatorfreq_t ocaloscollatorfreq;  /**< DVB-S前端控制模块盲扫工作本振*/
}AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t;

/**\brief LNB参数*/ 
typedef struct AM_SEC_DVBSatelliteLNBParameters
{
	AM_SEC_12V_Relais_State m_12V_relais_state;	// 12V relais output on/off

	unsigned int m_lof_hi,	// for 2 band universal lnb 10600 Mhz (high band offset frequency)
				m_lof_lo,	// for 2 band universal lnb  9750 Mhz (low band offset frequency)
				m_lof_threshold;	// for 2 band universal lnb 11750 Mhz (band switch frequency)

	AM_Bool_t m_increased_voltage; // use increased voltage ( 14/18V )

	AM_SEC_DVBSatelliteSwitchParameters_t m_cursat_parameters; /**< 卫星设备（switch）控制参数*/
	AM_SEC_DVBSatelliteDiseqcParameters_t m_diseqc_parameters; /**< Diseqc控制参数*/
	AM_SEC_DVBSatelliteRotorParameters_t m_rotor_parameters;   /**< 卫星设备马达控制参数*/

	int m_prio; // to override automatic tuner management ... -1 is Auto

	int SatCR_positions;                /**< unicable*/
	int SatCR_idx;                      /**< unicable cannel索引*/
	unsigned int SatCRvco;              /**< unicable cannel下行频率*/
	unsigned int UnicableTuningWord;    /**< unicable转折命令*/
	unsigned int UnicableConfigWord;    /**< unicable配置命令*/
	int old_frequency;                  /**< 旧频率*/
	int old_polarisation;               /**< 旧极性*/
	int old_orbital_position;           /**< 旧卫星轨道位置*/
	int guard_offset_old;               /**< 旧保护偏移*/
	int guard_offset;                   /**< 保护偏移*/
	int LNBNum;                         /**< LNB1 LNB2*/

	AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t b_para; /**< 盲扫设置参数*/

	AM_Bool_t sec_blind_flag; /**< 盲扫模式标识*/

	pthread_mutex_t    lock; /**< 卫星设备控制LNB数据保护互斥体*/	
}AM_SEC_DVBSatelliteLNBParameters_t;

/**\brief 卫星设备控制命令延迟*/
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
	SEC_CMD_MAX_PARAMS
}AM_SEC_Cmd_Param_t;

/**\brief 卫星设备控制命令*/
typedef enum{
	TYPE_SEC_PREFORTUNERDEMODDEV = 0,	/**< 卫星设备控制为Tuner与Demod设备工作准备*/
	TYPE_SEC_LNBSSWITCHCFGVALID = 31,	/**< 卫星设备LNB与Switch配置生效*/
	TYPE_SEC_POSITIONERSTOP,				/**< diseqc马达停止转动*/
	TYPE_SEC_POSITIONERDISABLELIMIT,		/**< diseqc马达禁止限制*/
	TYPE_SEC_POSITIONEREASTLIMIT,		/**< diseqc马达东向限制设置*/
	TYPE_SEC_POSITIONERWESTLIMIT,		/**< diseqc马达西向限制设置*/
	TYPE_SEC_POSITIONEREAST,				/**< diseqc马达东向转动*/
	TYPE_SEC_POSITIONERWEST,				/**< diseqc马达西向转动*/
	TYPE_SEC_POSITIONERSTORE,			/**< diseqc马达存储位置*/
	TYPE_SEC_POSITIONERGOTO,				/**< diseqc马达转动到指定位置*/
	TYPE_SEC_POSITIONERGOTOX				/**< diseqc马达转动根据本地经纬度以及卫星经度*/
}AM_SEC_Cmd_t;

/**\brief 异步卫星设备控制信息*/
typedef struct AM_SEC_AsyncInfo
{
	int                dev_no;        /**< 设备号*/ 
	AM_Bool_t          short_circuit; /**< Frontend短路状态*/
	AM_Bool_t          sec_monitor_enable_thread; /**< 异步卫星设备监控线程是否运行*/
	pthread_t          sec_monitor_thread;        /**< 异步卫星设备监控线程*/	
	AM_Bool_t          enable_thread; /**< 异步卫星设备控制线程是否运行*/
	pthread_t          thread;        /**< 异步卫星设备控制线程*/
	pthread_mutex_t    lock;          /**< 异步卫星设备控制数据保护互斥体*/
	pthread_cond_t     cond;          /**< 异步卫星设备控制控制条件变量*/
	AM_Bool_t          preparerunning;/**< 异步卫星设备控制运行状态*/
	AM_Bool_t          prepareexitnotify; /**< 异步卫星设备控制运行退出通知*/
	sem_t sem_running; /**< 异步卫星设备控制运行通知*/
	AM_FENDCTRL_DVBFrontendParametersBlindSatellite_t *sat_b_para; /**< 盲扫参数*/
	AM_FENDCTRL_DVBFrontendParametersSatellite_t sat_para; /**< 锁频参数*/
	AM_Bool_t sat_para_valid; /**< 锁频参数是否有效*/
	fe_status_t *sat_status; /**< 锁频状态*/
	unsigned int sat_tunetimeout; /**< 锁频超时*/
}AM_SEC_AsyncInfo_t;

/**\brief 卫星设备控制参数*/ 
typedef struct AM_SEC_DVBSatelliteEquipmentControl
{
	AM_SEC_DVBSatelliteLNBParameters_t m_lnbs; /**< LNB参数*/
	AM_Bool_t m_canMeasureInputPower; /**< 供电状态*/
	AM_SEC_Cmd_Param_t m_params[SEC_CMD_MAX_PARAMS]; /**< 命令延迟参数*/

	AM_SEC_Cmd_t sec_cmd;								/**< 卫星设备控制命令*/

	AM_SEC_AsyncInfo_t m_sec_asyncinfo; /**< 卫星设备控制异步运行信息*/
}AM_SEC_DVBSatelliteEquipmentControl_t;


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/* sec control interface */ 

/**\brief 设定卫星设备控制参数
 * \param dev_no 前端设备号
 * \param[in] para 卫星设备控制参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_SetSetting(int dev_no, const AM_SEC_DVBSatelliteEquipmentControl_t *para); 

/**\brief 获取卫星设备控制参数
 * \param dev_no 前端设备号
 * \param[out] para 卫星设备控制参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_GetSetting(int dev_no, AM_SEC_DVBSatelliteEquipmentControl_t *para);

/**\brief 马达缓存参数重置
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_ResetRotorStatusCache(int dev_no);

/**\brief 准备盲扫卫星设备控制
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_PrepareBlindScan(int dev_no);

/**\brief 中频转换传输频率
 * \param dev_no 前端设备号
 * \param centre_freq unit KHZ
 * \param[out] tp_freq unit KHZ
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_FreqConvert(int dev_no, unsigned int centre_freq, unsigned int *tp_freq);

/**\brief 过了无效传输频率
 * \param dev_no 前端设备号
 * \param tp_freq unit KHZ
 * \return
 *   - AM_TRUE 过滤
 *   - AM_FALSE 不过滤
 */
extern AM_Bool_t AM_SEC_FilterInvalidTp(int dev_no, unsigned int tp_freq);

/**\brief 执行卫星设备控制
 * \param dev_no 前端设备号
 * \param para 前端设备参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_SEC_ExecSecCmd(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para); 

extern AM_ErrorCode_t AM_SEC_DumpSetting(void);

/* frontend control interface */ 

/**\brief 设定前端参数
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_FENDCTRL_SetPara(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para); 

/**\brief 设定前端设备参数，并等待参数设定完成
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \param[out] status 返回前端设备状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend_ctrl.h)
 */
extern AM_ErrorCode_t AM_FENDCTRL_Lock(int dev_no, const AM_FENDCTRL_DVBFrontendParameters_t *para, fe_status_t *status);


#ifdef __cplusplus
}
#endif

#endif

