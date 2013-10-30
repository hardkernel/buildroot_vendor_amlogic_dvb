/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_scan.h
 * \brief 搜索模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-27: create the document
 ***************************************************************************/

#ifndef _AM_SCAN_H
#define _AM_SCAN_H

#include <am_types.h>
#include <am_mem.h>
#include <am_fend.h>
#include <am_fend_diseqc_cmd.h>
#include <am_fend_ctrl.h>
#include <am_si.h>
#include <am_db.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

//virtual define 
#define TUNER_COLOR_AUTO    ((tuner_std_id)0x02000000)


/****************************************************************************
 * Type definitions
 ***************************************************************************/
/**\brief Scan模块错误代码*/
enum AM_SCAN_ErrorCode
{
	AM_SCAN_ERROR_BASE=AM_ERROR_BASE(AM_MOD_SCAN),
	AM_SCAN_ERR_INVALID_PARAM,			 /**< 参数无效*/
	AM_SCAN_ERR_INVALID_HANDLE,          /**< 句柄无效*/
	AM_SCAN_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_SCAN_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_SCAN_ERR_CANNOT_LOCK,             /**< 前端无法锁定*/
	AM_SCAN_ERR_CANNOT_GET_NIT,			 /**< NIT接收失败*/
	AM_SCAN_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_SCAN_ERR_END
};

/**\brief 搜索进度事件类型，为AM_SCAN_Progress_t的evt,以下指出的参数为
 * AM_SCAN_Progress_t的data
 */
enum AM_SCAN_ProgressEvt
{
	AM_SCAN_PROGRESS_SCAN_BEGIN,	/**< 搜索开始，参数为搜索句柄*/
	AM_SCAN_PROGRESS_SCAN_END,		/**< 搜索结束，参数为搜索结果代码*/
	AM_SCAN_PROGRESS_NIT_BEGIN,		/**< 当为自动搜索时，开始搜索NIT表*/
	AM_SCAN_PROGRESS_NIT_END,		/**< 当为自动搜索时，NIT表搜索完毕*/
	AM_SCAN_PROGRESS_TS_BEGIN,		/**< 开始搜索一个TS，参数为AM_SCAN_TSProgress_t给出的进度信息*/
	AM_SCAN_PROGRESS_TS_END,		/**< 搜索一个TS完毕*/
	AM_SCAN_PROGRESS_PAT_DONE,		/**< 当前TS的PAT表搜索完毕，参数为dvbpsi_pat_t*/
	AM_SCAN_PROGRESS_PMT_DONE,		/**< 当前TS的所有PMT表搜索完毕，参数为dvbpsi_pmt_t*/
	AM_SCAN_PROGRESS_CAT_DONE,		/**< 当前TS的CAT表搜索完毕，参数为dvbpsi_cat_t*/
	AM_SCAN_PROGRESS_SDT_DONE,		/**< 当前TS的SDT表搜索完毕，参数为dvbpsi_sdt_t*/
	AM_SCAN_PROGRESS_MGT_DONE,		/**< 当前TS的MGT表搜索完毕，参数为mgt_section_info_t*/
	AM_SCAN_PROGRESS_VCT_DONE,		/**< 当前TS的VCT表搜索完毕，参数为vct_section_info_t*/
	AM_SCAN_PROGRESS_STORE_BEGIN,	/**< 开始存储*/
	AM_SCAN_PROGRESS_STORE_END,		/**< 存储完毕*/
	AM_SCAN_PROGRESS_BLIND_SCAN,	/**< 卫星盲扫搜索进度，参数为AM_SCAN_BlindScanProgress_t给出的进度信息*/
	AM_SCAN_PROGRESS_ATV_TUNING,	/**< ATV tuning a new frequency*/
	AM_SCAN_PROGRESS_NEW_PROGRAM,	/**< Searched a new program which will be stored*/
	AM_SCAN_PROGRESS_DVBT2_PLP_BEGIN,	/**< Start a DVB-T2 data PLP, parameter is AM_SCAN_PLPProgress_t*/
	AM_SCAN_PROGRESS_DVBT2_PLP_END,	/**< DVB-T2 PLP search end, parameter is AM_SCAN_PLPProgress_t*/
};

/**\brief 搜索事件类型*/
enum AM_SCAN_EventType
{
	AM_SCAN_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_SCAN),
	AM_SCAN_EVT_PROGRESS,	/**< 搜索进度事件，参数为AM_SCAN_Progress_t*/
	AM_SCAN_EVT_SIGNAL,		/**< 当前搜索频点的信号信息， 参数为AM_SCAN_SignalInfo_t*/
	AM_SCAN_EVT_END
};

/**\ service type定义*/
enum AM_SCAN_ServiceType
{
	AM_SCAN_SRV_UNKNOWN	= 0,	/**< 未知类型*/
	AM_SCAN_SRV_DTV		= 1,	/**< 数字电视类型*/
	AM_SCAN_SRV_DRADIO	= 2,	/**< 数字广播类型*/
	AM_SCAN_SRV_ATV		= 3,	/**< 模拟电视类型*/	
};

/**\brief DTV标准定义*/
typedef enum 
{
	AM_SCAN_DTV_STD_DVB		= 0x00,	/**< DVB标准*/
	AM_SCAN_DTV_STD_ATSC	= 0x01,	/**< ATSC标准*/
	AM_SCAN_DTV_STD_ISDB	= 0x02,	/**< ISDB标准*/
	AM_SCAN_DTV_STD_MAX,
}AM_SCAN_DTVStandard_t;

/**\brief TV搜索模式定义*/
enum AM_SCAN_Mode
{
	AM_SCAN_MODE_ATV_DTV,	/**< 首先搜索所有ATV，然后搜索所有DTV*/
	AM_SCAN_MODE_DTV_ATV,	/**< 首先搜索所有DTV，然后搜索所有ATV*/
	AM_SCAN_MODE_ADTV,		/**< A/DTV使用一个频率表，在一次搜索中完成逐个频率的A/DTV搜索*/
};

/**\brief DTV搜索模式定义*/
enum AM_SCAN_DTVMode
{
	AM_SCAN_DTVMODE_AUTO 			= 0x01,	/**< 自动搜索*/
	AM_SCAN_DTVMODE_MANUAL 			= 0x02,	/**< 手动搜索*/
	AM_SCAN_DTVMODE_ALLBAND 		= 0x03, /**< 全频段搜索*/
	AM_SCAN_DTVMODE_SAT_BLIND		= 0x04,	/**< 卫星盲扫*/
	AM_SCAN_DTVMODE_NONE			= 0x07,	/**< 将不进行DTV搜索*/
	/* OR option(s)*/
	AM_SCAN_DTVMODE_SEARCHBAT		= 0x08, /**< 是否搜索BAT表*/
	AM_SCAN_DTVMODE_SAT_UNICABLE	= 0x10,	/**< 卫星Unicable模式*/
	AM_SCAN_DTVMODE_FTA				= 0x20,	/**< Only scan free programs*/
	AM_SCAN_DTVMODE_NOTV			= 0x40, /**< Donot store tv programs*/
	AM_SCAN_DTVMODE_NORADIO			= 0x80, /**< Donot store radio programs*/
};

/**\brief ATV搜索模式定义*/
enum AM_SCAN_ATVMode
{
	AM_SCAN_ATVMODE_AUTO	= 0x01,	/**< 自动搜索*/
	AM_SCAN_ATVMODE_MANUAL	= 0x02,	/**< 手动搜索*/
	AM_SCAN_ATVMODE_FREQ	= 0x03,	/**< 按指定频率表逐个搜索*/
	AM_SCAN_ATVMODE_NONE	= 0x07,	/**< 将不进行ATV搜索*/
};

/**\brief 搜索结果代码*/
enum AM_SCAN_ResultCode
{
	AM_SCAN_RESULT_OK,			/**< 正常结束*/
	AM_SCAN_RESULT_UNLOCKED,	/**< 未锁定任何频点*/
};

/**\brief TS信号类型*/
typedef enum 
{
	AM_SCAN_TS_DIGITAL,
	AM_SCAN_TS_ANALOG
}AM_SCAN_TSType_t;

/**\brief DTV频道排序方法*/
typedef enum
{
	AM_SCAN_SORT_BY_FREQ_SRV_ID,	/**< 按照频率大小排序,同频率下按service_id排序*/
	AM_SCAN_SORT_BY_SCAN_ORDER,		/**< 按照搜索先后顺序排序*/
	AM_SCAN_SORT_BY_LCN,			/**< 按照LCN排序*/
	AM_SCAN_SORT_BY_HD_SD,
}AM_SCAN_DTVSortMethod_t;

/**\brief 频点进度数据*/
typedef struct
{
	int								index;		/**< 当前搜索频点索引*/
	int								total;		/**< 总共需要搜索的频点个数*/
	AM_FENDCTRL_DVBFrontendParameters_t	fend_para;	/**< 当前搜索的频点信息*/
}AM_SCAN_TSProgress_t;

/**\brief DVB-T2 PLP progress data*/
typedef struct
{
	int				index;	/**< current searching plp index, start from 0*/
	int				total;	/**< total plps in current TS*/
	struct AM_SCAN_TS_s	*ts;	/**< current searching TS*/
}AM_SCAN_PLPProgress_t;

/**\brief 搜索进度数据*/
typedef struct 
{
	int		evt;	/**< 事件类型，见AM_SCAN_ProgressEvt*/
	void	*data;  /**< 事件数据，见AM_SCAN_ProgressEvt描述*/
}AM_SCAN_Progress_t;

/**\brief DTV 卫星盲扫进度数据*/
typedef struct
{
	int	progress;	/**< 盲扫总进度，0-100*/
	int	polar;		/**< 当前盲扫的极化方式设置，见AM_FEND_Polarisation_t，-1表示未知当前设置的极化方式*/
	int lo;			/**< 当前盲扫的本振频率，见AM_FEND_Localoscollatorfreq_t， -1表示未知当前的本振*/
	int freq;		/**< 当前搜索的频点*/
	int new_tp_cnt;	/**< 本次通知搜索到的新TP个数, <=0表示本次无新搜索到的TP*/
	struct dvb_frontend_parameters	*new_tps;	/**< 本次通知搜索到的新TP信息*/
}AM_SCAN_DTVBlindScanProgress_t;

/**\breif New program progress data*/
typedef struct
{
	int service_id;
	int service_type;	/**< See AM_SCAN_ServiceType*/
	AM_Bool_t scrambled;
	char name[AM_DB_MAX_SRV_NAME_LEN + 1];
}AM_SCAN_ProgramProgress_t;

/**\brief 当前搜索的频点信号信息*/
typedef struct
{
	AM_Bool_t locked;
	int snr;
	int ber;
	int strength;
	int frequency;
}AM_SCAN_DTVSignalInfo_t;

/**\brief 卫星配置参数*/
typedef struct
{
	char									sat_name[64];	/**< 卫星名称，用于唯一标识一个卫星*/
	int										lnb_num;		/**< LNB number*/
	int										motor_num;		/**< Motor number*/
	AM_SEC_DVBSatelliteEquipmentControl_t	sec;			/**< 卫星配置参数*/
}AM_SCAN_DTVSatellitePara_t;

/**\brief 搜索结果的单个TS数据*/
typedef struct AM_SCAN_TS_s
{
	AM_SCAN_TSType_t type;		/**< 标识 数字/模拟*/
	union
	{
		struct
		{
			int snr;		/**< SNR*/
			int ber;		/**< BER*/
			int strength;	/**< Strength*/
			AM_FENDCTRL_DVBFrontendParameters_t fend_para;	/**< 频点信息*/
			dvbpsi_nit_t *nits;		/**< 搜索到的NIT表，为每个频点单独保存各自的NIT表，用于LCN等应用*/
			dvbpsi_pat_t *pats;		/**< 搜索到的PAT表*/
			dvbpsi_cat_t *cats;		/**< 搜索到的CAT表*/
			dvbpsi_pmt_t *pmts;		/**< 搜索到的PMT表*/
			dvbpsi_sdt_t *sdts;		/**< 搜索到的SDT表*/
			mgt_section_info_t *mgts;		/**< 搜索到的MGT表*/
			vct_section_info_t *vcts;		/**< 搜索到的VCT表*/

			int				dvbt2_data_plp_num;	/**< DVB-T2 DATA PLP count*/
			struct
			{
				uint8_t					id;			/**< the data plp id*/
				dvbpsi_pat_t				*pats;		/**< the pats in this data PLP*/
				dvbpsi_pmt_t				*pmts;		/**< the pmts in this data PLP*/
				dvbpsi_sdt_t 				*sdts;		/**< the sdt actuals in this data PLP*/
			}dvbt2_data_plps[256];
		}digital;
		
		struct
		{
			int freq;		/**< 频率*/
			int std;		/**< tuner std*/
		}analog;
	};
	
	int tp_index; /**< 位于频率表中的位置*/
	struct AM_SCAN_TS_s *p_next;	/**< 指向下一个TS*/
}AM_SCAN_TS_t;

/**\brief ATV搜索参数定义*/
typedef struct
{
	int mode;		/**< ATV搜索模式,见AM_SCAN_ATVMode*/
	int channel_id;	/**< Used for manual scan*/
	int direction;	/**< Manual模式时设置*/
	int afe_dev_id;			/**< AFE设备号*/
	int default_std;		/**< tuner std*/	
	int afc_range;			/**< AFC range in Hz*/
	int afc_unlocked_step;	/**< AFC unlocked step frequency in Hz*/
	int cvbs_unlocked_step;	/**< CVBS unlocked step frequency in Hz*/
	int cvbs_locked_step;	/**< CVBS locked step frequency in Hz*/
	int fe_cnt;		/**< 前端参数个数*/
	AM_FENDCTRL_DVBFrontendParameters_t *fe_paras;	/**< 前端参数列表，当mode!=AM_SCAN_ATVMODE_FREQ时，前3个参数分别
														为min_freq, max_freq, start_freq*/
}AM_SCAN_ATVCreatePara_t;

/**\brief DTV搜索参数定义*/
typedef struct
{
	int mode;						/**< DTV搜索模式*/
	int dmx_dev_id;  				/**< demux设备号*/
	int source;      				/**< 前端调制模式*/
	AM_SCAN_DTVStandard_t standard;    /**< 搜索标准，DVB/ATSC*/
	AM_SCAN_DTVSatellitePara_t sat_para;	/**< 卫星参数配置,只有当source位Satellite时有效*/
 	AM_SCAN_DTVSortMethod_t sort_method;	/**< 频道排序方法*/
	AM_Bool_t resort_all;					/**< 重新排列数据库中的所有service*/
	int fe_cnt;								/**< 前端参数个数*/
	AM_FENDCTRL_DVBFrontendParameters_t *fe_paras;/**< 前端参数列表，自动搜索时对应主频点列表，手动搜索时为单个频点，
 														全频段搜索时可自定义频点列表或留空使用默认值*/
}AM_SCAN_DTVCreatePara_t;

typedef struct AM_SCAN_CreatePara_s AM_SCAN_CreatePara_t;

/**\brief 节目搜索结果数据结构*/
typedef struct
{
	struct AM_SCAN_CreatePara_s *start_para;/**< AM_SCAN_Create()传入的参数*/
	dvbpsi_nit_t *nits;				/**< 搜索到的NIT表*/
	dvbpsi_bat_t *bats;				/**< 搜索到的BAT表*/
	vct_channel_info_t	*vcs;		/**<ATSC C virtual channels*/
	AM_SCAN_TS_t *tses;				/**< 所有TS列表*/
	void *reserved;                 /**< reserved data*/
}AM_SCAN_Result_t;

/**\brief 存储回调函数
 * result 保存的搜索结果
 */
typedef void (*AM_SCAN_StoreCb) (AM_SCAN_Result_t *result);

/**\brief 搜索创建参数*/
struct AM_SCAN_CreatePara_s
{
	int fend_dev_id; 					/**< 前端设备号*/
	int mode;							/**< TV 搜索模式，见AM_SCAN_Mode*/
	AM_SCAN_StoreCb store_cb;			/**< 搜索完成时存储回调函数*/
	sqlite3 *hdb;						/**< 数据库句柄*/
	char default_text_lang[4];			/**< */
	char text_langs[128];				/**< */
	AM_SCAN_ATVCreatePara_t atv_para;	/**< ATV 搜索参数*/
	AM_SCAN_DTVCreatePara_t dtv_para;	/**< DTV 搜索参数*/
};


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/
 
 /**\brief 创建节目搜索
 * \param [in] para 创建参数
 * \param [out] handle 返回SCAN句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
extern AM_ErrorCode_t AM_SCAN_Create(AM_SCAN_CreatePara_t *para, int *handle);

/**\brief 销毀节目搜索
 * \param handle Scan句柄
 * \param store 是否存储
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
extern AM_ErrorCode_t AM_SCAN_Destroy(int handle, AM_Bool_t store);

/**\brief 启动节目搜索
 * \param handle Scan句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
extern AM_ErrorCode_t AM_SCAN_Start(int handle);

/**\brief 设置用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
extern AM_ErrorCode_t AM_SCAN_SetUserData(int handle, void *user_data);

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_scan.h)
 */
extern AM_ErrorCode_t AM_SCAN_GetUserData(int handle, void **user_data);


#ifdef __cplusplus
}
#endif

#endif

