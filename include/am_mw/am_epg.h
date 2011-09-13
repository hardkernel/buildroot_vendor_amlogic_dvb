/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_epg.h
 * \brief EPG监控模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-11-04: create the document
 ***************************************************************************/

#ifndef _AM_EPG_H
#define _AM_EPG_H

#include <am_types.h>
#include <am_mem.h>
#include <am_fend.h>
#include <am_si.h>
#include <am_db.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/**\brief 将16位的MJD值转换为秒*/
#define AM_EPG_MJD2SEC(m) (((m)-40587)*86400)

/**\brief 将时，分，秒的BCD值转换为秒*/
#define AM_EPG_BCD2SEC(h,m,s)\
		((3600 * ((10*(((h) & 0xF0)>>4)) + ((h) & 0xF))) + \
		(60 * ((10*(((m) & 0xF0)>>4)) + ((m) & 0xF))) + \
		((10*(((s) & 0xF0)>>4)) + ((s) & 0xF)))

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief EPG模块错误代码*/
enum AM_EPG_ErrorCode
{
	AM_EPG_ERROR_BASE=AM_ERROR_BASE(AM_MOD_EPG),
	AM_EPG_ERR_INVALID_PARAM,			/**< 参数无效*/
	AM_EPG_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_EPG_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_EPG_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_EPG_ERR_CANNOT_CREATE_SI,		/**< 无法创建SI解析器*/
	AM_EPG_ERR_SUBSCRIBE_EVENT_FAILED,	/**< 预约EPG事件失败*/
	AM_EPG_ERR_END
};

/**\brief EPG监控模式定义*/
enum AM_EPG_Mode
{
	AM_EPG_SCAN_PAT = 0x01,	/**< 监控PAT*/
	AM_EPG_SCAN_PMT = 0x02,	/**< 监控PMT*/
	AM_EPG_SCAN_CAT = 0x04,	/**< 监控CAT*/
	AM_EPG_SCAN_SDT = 0x08,	/**< 监控SDT*/
	AM_EPG_SCAN_NIT = 0x10,	/**< 监控NIT*/
	AM_EPG_SCAN_TDT = 0x20,	/**< 监控TDT*/
	AM_EPG_SCAN_EIT_PF_ACT 		= 0x40,	/**< 监控当前频点eit pf*/
	AM_EPG_SCAN_EIT_PF_OTH 		= 0x80,	/**< 监控其他频点eit pf*/
	AM_EPG_SCAN_EIT_SCHE_ACT	= 0x100,	/**< 监控当前频点eit schedule*/
	AM_EPG_SCAN_EIT_SCHE_OTH 	= 0x200,	/**< 监控其他频点eit schedule*/
	
	/*For ATSC*/
	AM_EPG_SCAN_MGT	= 0x400,	/**< 监控MGT*/
	AM_EPG_SCAN_VCT	= 0x800,	/**< 监控VCT*/
	AM_EPG_SCAN_STT	= 0x1000,	/**< 监控STT*/
	AM_EPG_SCAN_RRT	= 0x2000,	/**< 监控RRT*/
	
	/*Composed mode*/
	AM_EPG_SCAN_EIT_PF_ALL		= AM_EPG_SCAN_EIT_PF_ACT | AM_EPG_SCAN_EIT_PF_OTH,/**< 监控所有eit pf*/
	AM_EPG_SCAN_EIT_SCHE_ALL 	= AM_EPG_SCAN_EIT_SCHE_ACT | AM_EPG_SCAN_EIT_SCHE_OTH,/**< 监控所有eit schedule*/
	AM_EPG_SCAN_EIT_ALL 		= AM_EPG_SCAN_EIT_PF_ALL | AM_EPG_SCAN_EIT_SCHE_ALL, /**< 监控所有eit*/
	AM_EPG_SCAN_ALL				= AM_EPG_SCAN_PAT|AM_EPG_SCAN_PMT|AM_EPG_SCAN_CAT|AM_EPG_SCAN_SDT|AM_EPG_SCAN_NIT|AM_EPG_SCAN_TDT|AM_EPG_SCAN_EIT_ALL|
									AM_EPG_SCAN_MGT|AM_EPG_SCAN_VCT|AM_EPG_SCAN_STT|AM_EPG_SCAN_RRT,
};

/**\brief EPG监控事件类型*/
enum AM_EPG_EventType
{
	AM_EPG_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_EPG),
	AM_EPG_EVT_NEW_PAT,    /**< 发现新版本PAT，参数为dvbpsi_pat_t*/
	AM_EPG_EVT_NEW_PMT,    /**< 发现新版本PMT，参数为dvbpsi_pmt_t*/
	AM_EPG_EVT_NEW_CAT,    /**< 发现新版本CAT，参数为dvbpsi_cat_t*/
	AM_EPG_EVT_NEW_SDT,    /**< 发现新版本SDT，参数为dvbpsi_sdt_t*/
	AM_EPG_EVT_NEW_NIT,    /**< 发现新版本NIT，参数为dvbpsi_nit_t*/
	AM_EPG_EVT_NEW_TDT,    /**< 发现新版本NIT，参数为dvbpsi_tot_t*/
	AM_EPG_EVT_NEW_EIT,	   /**< 接收到新的EIT单个section，参数为dvbpsi_eit_t*/
	AM_EPG_EVT_NEW_STT,		/**< 发新新版本STT， 参数为stt_section_info_t*/
	AM_EPG_EVT_EIT_DONE,   /**< 所有EIT接收完毕*/
	AM_EPG_EVT_EIT_UPDATE,	/**< 通知接收到新的EIT数据，用于上层实现更新*/
	AM_EPG_EVT_NEW_SUB_PLAY,	/**< 提前通知有将开始的预约节目要播放,参数为db_evt_id*/
	AM_EPG_EVT_SUB_PLAY_START,	/**< 预约节目即将切换播放,参数为db_evt_id*/
	AM_EPG_EVT_END
};

/**\brief 模式操作*/
enum AM_EPG_ModeOp
{
	AM_EPG_MODE_OP_ADD, 	 /**< 增加一个或几个监控模式*/
	AM_EPG_MODE_OP_REMOVE,	 /**< 取消一个或几个监控模式*/
};

/**\brief EPG创建参数*/
typedef struct
{
	int fend_dev;	/**< 前端设备号*/
	int dmx_dev;	/**< DMX设备号*/
	int source;	/*< 源标识*/
	sqlite3	*hdb;	/**< 数据库句柄，可用于预约播放查询等数据库操作*/
}AM_EPG_CreatePara_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/
 
/**\brief 在指定源上创建一个EPG监控
 * \param [in] para 创建参数
 * \param [out] handle 返回句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_Create(AM_EPG_CreatePara_t *para, int *handle);

/**\brief 销毀一个EPG监控
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_Destroy(int handle);

/**\brief 设置EPG监控模式
 * \param handle 句柄
 * \param op	修改操作，见AM_EPG_ModeOp
 * \param mode 监控模式，见AM_EPG_Mode
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_ChangeMode(int handle, int op, int mode);

/**\brief 设置当前监控的service，监控其PMT和EIT actual pf
 * \param handle 句柄
 * \param service_id	需要监控的service的service_id
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_MonitorService(int handle, uint16_t service_id);

/**\brief 设置EPG PF 自动更新间隔
 * \param handle 句柄
 * \param distance 检查间隔,ms单位
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_SetEITPFCheckDistance(int handle, int distance);

/**\brief 设置EPG Schedule 自动更新间隔
 * \param handle 句柄
 * \param distance 检查间隔,ms单位, 为0时将一直更新
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_SetEITScheCheckDistance(int handle, int distance);


/**\brief 字符编码转换
 * \param [in] in_code 需要转换的字符数据
 * \param in_len 需要转换的字符数据长度
 * \param [out] out_code 转换后的字符数据
 * \param out_len 输出字符缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_ConvertCode(char *in_code,int in_len,char *out_code,int out_len);

/**\brief 获得当前UTC时间
 * \param [out] utc_time UTC时间,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_GetUTCTime(int *utc_time);

/**\brief 计算本地时间
 * \param utc_time UTC时间，单位为秒
 * \param [out] local_time 本地时间,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_UTCToLocal(int utc_time, int *local_time);

/**\brief 计算UTC时间
 * \param local_time 本地时间,单位为秒
 * \param [out] utc_time UTC时间，单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_LocalToUTC(int local_time, int *utc_time);

/**\brief 设置时区偏移值
 * \param offset 偏移值,单位为秒
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_SetLocalOffset(int offset);

/**\brief 预约一个EPG事件，用于预约播放
 * \param handle EPG句柄
 * \param db_evt_id 事件的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_SubscribeEvent(int handle, int db_evt_id);

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_SetUserData(int handle, void *user_data);

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_epg.h)
 */
extern AM_ErrorCode_t AM_EPG_GetUserData(int handle, void **user_data);

#ifdef __cplusplus
}
#endif

#endif

