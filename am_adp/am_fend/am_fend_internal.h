/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief DVB前端设备内部头文件
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-07: create the document
 ***************************************************************************/

#ifndef _AM_FEND_INTERNAL_H
#define _AM_FEND_INTERNAL_H

#include <am_fend.h>
#include <am_thread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define FEND_FL_RUN_CB        (1)
#define FEND_FL_LOCK          (2)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 前端设备*/
typedef struct AM_FEND_Device AM_FEND_Device_t;

/**\brief 前端设备驱动*/
typedef struct
{
	AM_ErrorCode_t (*open) (AM_FEND_Device_t *dev, const AM_FEND_OpenPara_t *para);
	AM_ErrorCode_t (*set_mode) (AM_FEND_Device_t *dev, int mode);
	AM_ErrorCode_t (*get_info) (AM_FEND_Device_t *dev, struct dvb_frontend_info *info);
	AM_ErrorCode_t (*get_ts) (AM_FEND_Device_t *dev, AM_DMX_Source_t *src);
	AM_ErrorCode_t (*set_para) (AM_FEND_Device_t *dev, const struct dvb_frontend_parameters *para);
	AM_ErrorCode_t (*get_para) (AM_FEND_Device_t *dev, struct dvb_frontend_parameters *para);
	AM_ErrorCode_t (*get_status) (AM_FEND_Device_t *dev, fe_status_t *status);
	AM_ErrorCode_t (*get_snr) (AM_FEND_Device_t *dev, int *snr);
	AM_ErrorCode_t (*get_ber) (AM_FEND_Device_t *dev, int *ber);
	AM_ErrorCode_t (*get_strength) (AM_FEND_Device_t *dev, int *strength);
	AM_ErrorCode_t (*wait_event) (AM_FEND_Device_t *dev, struct dvb_frontend_event *evt, int timeout);
	AM_ErrorCode_t (*set_delay) (AM_FEND_Device_t *dev, int delay);
	AM_ErrorCode_t (*diseqc_reset_overload)(AM_FEND_Device_t *dev);
	AM_ErrorCode_t (*diseqc_send_master_cmd)(AM_FEND_Device_t *dev, struct dvb_diseqc_master_cmd* cmd);
	AM_ErrorCode_t (*diseqc_recv_slave_reply)(AM_FEND_Device_t *dev, struct dvb_diseqc_slave_reply* reply);
	AM_ErrorCode_t (*diseqc_send_burst)(AM_FEND_Device_t *dev, fe_sec_mini_cmd_t minicmd);
	AM_ErrorCode_t (*set_tone)(AM_FEND_Device_t *dev, fe_sec_tone_mode_t tone);
	AM_ErrorCode_t (*set_voltage)(AM_FEND_Device_t *dev, fe_sec_voltage_t voltage);
	AM_ErrorCode_t (*enable_high_lnb_voltage)(AM_FEND_Device_t *dev, long arg);	
	AM_ErrorCode_t (*close) (AM_FEND_Device_t *dev);
	AM_ErrorCode_t (*set_prop) (AM_FEND_Device_t *dev, const struct dtv_properties *prop);
	AM_ErrorCode_t (*get_prop) (AM_FEND_Device_t *dev, struct dtv_properties *prop);
	/*dvbsx support*/
	AM_ErrorCode_t (*blindscan_scan)(AM_FEND_Device_t *dev, struct dvbsx_blindscanpara *pbspara);
	AM_ErrorCode_t (*blindscan_getscanstatus)(AM_FEND_Device_t *dev, struct dvbsx_blindscaninfo *pbsinfo);
	AM_ErrorCode_t (*blindscan_cancel)(AM_FEND_Device_t *dev);
	AM_ErrorCode_t (*blindscan_readchannelinfo)(AM_FEND_Device_t *dev, struct dvb_frontend_parameters *pchannel);
} AM_FEND_Driver_t;

/**\brief Defines the status of blind scan process.*/
enum AM_FEND_DVBSx_BlindScanAPI_Status
{	
	DVBSx_BS_Status_Init = 0,							/**< = 0 Indicates that the blind scan process is initializing the parameters.*/
	DVBSx_BS_Status_Start = 1,							/**< = 1 Indicates that the blind scan process is starting to scan.*/
	DVBSx_BS_Status_Wait = 2,							/**< = 2 Indicates that the blind scan process is waiting for the completion of scanning.*/
	DVBSx_BS_Status_Adjust = 3,							/**< = 3 Indicates that the blind scan process is reading the channel info which have scanned out.*/
	DVBSx_BS_Status_User_Process = 4,					/**< = 4 Indicates that the blind scan process is in custom code. Customer can add the callback function in this stage such as adding TP information to TP list or lock the TP for parsing PSI.*/
	DVBSx_BS_Status_Cancel = 5,							/**< = 5 Indicates that the blind scan process is cancelled or the blind scan have completed.*/
	DVBSx_BS_Status_Exit = 6,							/**< = 6 Indicates that the blind scan process have ended.*/
	DVBSx_BS_Status_WaitExit = 7						/**< = 7 Indicates that the blind scan process wait user exit.*/
};

/**\brief Defines the blind scan mode.*/
enum AM_FEND_DVBSx_BlindScanAPI_Mode
{
	DVBSx_BS_Fast_Mode = 0,								/**< = 0 Indicates that the blind scan frequency step is automatic settings.*/
	DVBSx_BS_Slow_Mode = 1								/**< = 1 Indicates that the blind scan frequency step can be setting by user. The default value is 10MHz.*/
};

/**\brief Stores the blind scan configuration parameters.*/
struct AM_FEND_DVBSx_BlindScanAPI_Setting
{
	unsigned short  m_uiScan_Min_Symbolrate_MHz;				/**< The minimum symbol rate to be scanned in units of MHz. The minimum value is 1000 kHz.*/
	unsigned short  m_uiScan_Max_Symbolrate_MHz;			/**< The maximum symbol rate to be scanned in units of MHz. The maximum value is 45000 kHz. */
	unsigned short  m_uiScan_Start_Freq_MHz;					/**< The start scan frequency in units of MHz. The minimum value depends on the tuner specification.*/
	unsigned short  m_uiScan_Stop_Freq_MHz;					/**< The stop scan frequency in units of MHz. The maximum value depends on the tuner specification.*/
	unsigned short  m_uiScan_Next_Freq_100KHz;				/**< The start frequency of the next scan in units of 100kHz.*/
	unsigned short  m_uiScan_Progress_Per;					/**< The percentage completion of the blind scan process. A value of 100 indicates that the blind scan is finished.*/
	unsigned short  m_uiScan_Bind_No;							/**< The number of completion of the blind scan procedure.*/
	unsigned short  m_uiTuner_MaxLPF_100kHz;				/**< The max low pass filter bandwidth of the tuner.*/
	unsigned short  m_uiScan_Center_Freq_Step_100KHz;		/**< The blind scan frequency step. The value is only valid when BS_Mode set to DVBSx_BS_Slow_Mode and would be ignored when BS_Mode set to DVBSx_BS_Fast_Mode.*/
	enum AM_FEND_DVBSx_BlindScanAPI_Mode BS_Mode;			/**< The blind scan mode. \sa ::AM_FEND_DVBSx_BlindScanAPI_Mode.*/
	unsigned short  m_uiScaning;								/**< whether in blindscan progress.*/
	unsigned short  m_uiChannelCount;							/**< The number of channels detected thus far by the blind scan operation.  The Availink device can store up to 120 detected channels.*/
	struct dvb_frontend_parameters channels[128];				/**< Stores the channel information that all scan out results.*/
	struct dvb_frontend_parameters channels_Temp[16];			/**< Stores the channel information temporarily that scan out results by the blind scan procedure.*/
	struct dvbsx_blindscanpara	bsPara;							/**< Stores the blind scan parameters each blind scan procedure.*/
	struct dvbsx_blindscaninfo	bsInfo;							/**< Stores the blind scan status information each blind scan procedure.*/
	fe_spectral_inversion_t m_eSpectrumMode;					/**< Defines the device spectrum polarity setting.  \sa ::fe_spectral_inversion_t.*/
};

/**\brief 前端设备*/
struct AM_FEND_Device
{
	int                dev_no;        /**< 设备号*/
	const AM_FEND_Driver_t  *drv;     /**< 设备驱动*/
	void              *drv_data;      /**< 驱动私有数据*/
	AM_Bool_t          openned;       /**< 设备是否已经打开*/
	AM_Bool_t          enable_thread; /**< 状态监控线程是否运行*/
	pthread_t          thread;        /**< 状态监控线程*/
	pthread_mutex_t    lock;          /**< 设备数据保护互斥体*/
	pthread_cond_t     cond;          /**< 状态监控线程控制条件变量*/
	int                flags;         /**< 状态监控线程标志*/
	AM_FEND_Callback_t cb;            /**< 状态监控回调函数*/
	int                curr_mode;     /**< 当前解调模式*/
	void              *user_data;     /**< 回调函数参数*/
	AM_Bool_t         enable_cb;      /**< 允许或者禁止状态监控回调函数*/
	//struct dvb_frontend_info info;    /**< 前端设备信息*/ 
	
	AM_Bool_t          enable_blindscan_thread; /**< 状扫处理线程是否运行*/
	pthread_t          blindscan_thread;        /**< 盲扫处理线程*/
	AM_FEND_BlindCallback_t blindscan_cb;		/**< 盲扫更新回调函数*/
	void              *blindscan_cb_user_data;		/**< 盲扫更新回调函数参数*/
	struct AM_FEND_DVBSx_BlindScanAPI_Setting bs_setting;	/**< 盲扫设置*/	
};

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

