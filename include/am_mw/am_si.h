/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_si.h
 * \brief SI Decoder 模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-14: create the document
 ***************************************************************************/

#ifndef _AM_SI_H
#define _AM_SI_H

#include "am_types.h"
#include "libdvbsi/descriptor.h"
#include "libdvbsi/dvbpsi.h"
#include "libdvbsi/psi.h"
#include "libdvbsi/demux.h"
#include "libdvbsi/tables/cat.h"
#include "libdvbsi/tables/pat.h"
#include "libdvbsi/tables/pmt.h"
#include "libdvbsi/tables/nit.h"
#include "libdvbsi/tables/sdt.h"
#include "libdvbsi/tables/eit.h"
#include "libdvbsi/tables/tot.h"
#include "libdvbsi/tables/bat.h"
#include "libdvbsi/descriptors/dr.h"
#include "atsc/atsc_mgt.h"
#include "atsc/atsc_tvct.h"
#include "atsc/atsc_cvct.h"
#include "atsc/atsc_rrt.h"
#include "atsc/atsc_stt.h"


#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/*PID定义*/
#define AM_SI_PID_PAT	(0x0)
#define AM_SI_PID_CAT	(0x1)
#define AM_SI_PID_NIT	(0x10)
#define AM_SI_PID_SDT	(0x11)
#define AM_SI_PID_BAT	(0x11)
#define AM_SI_PID_EIT	(0x12)
#define AM_SI_PID_TDT	(0x14)
#define AM_SI_PID_TOT	(0x14)
/*ATSC PSIP base pid*/
#define AM_SI_ATSC_BASE_PID	ATSC_BASE_PID

/*Table ID定义*/
#define AM_SI_TID_PAT			(0x0)
#define AM_SI_TID_CAT			(0x1)
#define AM_SI_TID_PMT			(0x2)
#define AM_SI_TID_NIT_ACT		(0x40)
#define AM_SI_TID_NIT_OTH		(0X41)
#define AM_SI_TID_SDT_ACT		(0x42)
#define AM_SI_TID_SDT_OTH		(0x46)
#define AM_SI_TID_BAT			(0x4A)
#define AM_SI_TID_EIT_PF_ACT	(0x4E)
#define AM_SI_TID_EIT_PF_OTH	(0x4F)
#define AM_SI_TID_EIT_SCHE_ACT	(0x50) /* 0x50 - 0x5f*/
#define AM_SI_TID_EIT_SCHE_OTH	(0x60) /* 0x60 - 0x6f */
#define AM_SI_TID_TDT			(0x70)
#define AM_SI_TID_TOT			(0x73)

/* atsc table */
#define AM_SI_TID_PSIP_MGT			ATSC_PSIP_MGT_TID
#define AM_SI_TID_PSIP_TVCT			ATSC_PSIP_TVCT_TID
#define AM_SI_TID_PSIP_CVCT			ATSC_PSIP_CVCT_TID
#define AM_SI_TID_PSIP_RRT			ATSC_PSIP_RRT_TID
#define AM_SI_TID_PSIP_EIT			ATSC_PSIP_EIT_TID
#define AM_SI_TID_PSIP_ETT			ATSC_PSIP_ETT_TID
#define AM_SI_TID_PSIP_STT			ATSC_PSIP_STT_TID
#define AM_SI_TID_PSIP_DCCT			ATSC_PSIP_DCCT_TID
#define AM_SI_TID_PSIP_DCCSCT		ATSC_PSIP_DCCSCT_TID

/*descriptor tag 定义*/

/*ISO/IEC 13818-1*/
#define AM_SI_DESCR_VIDEO_STREAM			(0x02)
#define AM_SI_DESCR_AUDIO_STREAM			(0x03)
#define AM_SI_DESCR_HIERARCHY				(0x04)
#define AM_SI_DESCR_REGISTRATION			(0x05)
#define AM_SI_DESCR_DS_ALIGNMENT			(0x06)
#define AM_SI_DESCR_TARGET_BG_GRID			(0x07)
#define AM_SI_DESCR_VIDEO_WINDOW			(0x08)
#define AM_SI_DESCR_CA						(0x09)
#define AM_SI_DESCR_ISO639					(0x0a)
#define AM_SI_DESCR_SYSTEM_CLOCK			(0x0b)
#define AM_SI_DESCR_MULX_BUF_UTILIZATION	(0x0c)
#define AM_SI_DESCR_COPYRIGHT				(0x0d)
#define AM_SI_DESCR_MAX_BITRATE				(0x0e)
#define AM_SI_DESCR_PRIVATE_DATA_INDICATOR	(0x0f)

/*EN 300 468*/
#define AM_SI_DESCR_NETWORK_NAME			(0x40)
#define AM_SI_DESCR_SERVICE_LIST			(0x41)
#define AM_SI_DESCR_STUFFING				(0x42)
#define AM_SI_DESCR_SATELLITE_DELIVERY		(0x43)
#define AM_SI_DESCR_CABLE_DELIVERY			(0x44)
#define AM_SI_DESCR_VBI_DATA				(0x45)
#define AM_SI_DESCR_VBI_TELETEXT			(0x46)
#define AM_SI_DESCR_BOUQUET_NAME			(0x47)
#define AM_SI_DESCR_SERVICE					(0x48)
#define AM_SI_DESCR_LINKAGE					(0x4A)
#define AM_SI_DESCR_NVOD_REFERENCE			(0x4B)
#define AM_SI_DESCR_TIME_SHIFTED_SERVICE	(0x4C)
#define AM_SI_DESCR_SHORT_EVENT				(0x4D)
#define AM_SI_DESCR_EXTENDED_EVENT			(0x4E)
#define AM_SI_DESCR_TIME_SHIFTED_EVENT		(0x4F)
#define AM_SI_DESCR_COMPONENT				(0x50)
#define AM_SI_DESCR_MOSAIC					(0x51)
#define AM_SI_DESCR_STREAM_IDENTIFIER		(0x52)
#define AM_SI_DESCR_CA_IDENTIFIER			(0x53)
#define AM_SI_DESCR_CONTENT					(0x54)
#define AM_SI_DESCR_PARENTAL_RATING			(0x55)
#define AM_SI_DESCR_TELETEXT				(0x56)
#define AM_SI_DESCR_TELPHONE				(0x57)
#define AM_SI_DESCR_LOCAL_TIME_OFFSET		(0x58)
#define AM_SI_DESCR_SUBTITLING				(0x59)
#define AM_SI_DESCR_TERRESTRIAL_DELIVERY		(0x5A)
#define AM_SI_DESCR_MULTI_NETWORK_NAME		(0x5B)
#define AM_SI_DESCR_MULTI_BOUQUET_NAME		(0x5C)
#define AM_SI_DESCR_MULTI_SERVICE_NAME		(0x5D)
#define AM_SI_DESCR_MULTI_COMPONENT			(0x5E)
#define AM_SI_DESCR_DATA_BROADCAST			(0x64)
#define AM_SI_DESCR_DATA_BROADCAST_ID		(0x66)
#define AM_SI_DESCR_PDC						(0x69)
#define AM_SI_DESCR_AC3						(0x6A)
#define AM_SI_DESCR_ENHANCED_AC3			(0x7A)
#define AM_SI_DESCR_DTS						(0x7B)
#define AM_SI_DESCR_AAC						(0x7C)

/**\brief 遍历SI提供的链表开始*/
#define AM_SI_LIST_BEGIN(l, v) for ((v)=(l); (v)!=NULL; (v)=(v)->p_next){

/**\brief 遍历SI提供的链表结束*/
#define AM_SI_LIST_END() }

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief SI模块错误代码*/
enum AM_SI_ErrorCode
{
	AM_SI_ERROR_BASE=AM_ERROR_BASE(AM_MOD_SI),
	AM_SI_ERR_INVALID_HANDLE,          /**< 句柄无效*/
	AM_SI_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_SI_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_SI_ERR_INVALID_SECTION_DATA,		   /**< section数据错误*/
	AM_SI_ERR_END
};

/**\brief section头定义*/
typedef struct 
{
	uint8_t		table_id;			/**< table_id*/
	uint8_t		syntax_indicator;	/**< section_syntax_indicator*/
	uint8_t		private_indicator;	/**< private_indicator*/
	uint16_t	length;				/**< section_length*/
	uint16_t	extension;			/**< table_id_extension*/
									/**< transport_stream_id for a
                                             PAT section */
    uint8_t		version;			/**< version_number*/
    uint8_t		cur_next_indicator;	/**< current_next_indicator*/
    uint8_t		sec_num;			/**< section_number*/
    uint8_t		last_sec_num;		/**< last_section_number*/
}AM_SI_SectionHeader_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 创建一个SI解析器
 * \param [out] handle 返回SI解析句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
extern AM_ErrorCode_t AM_SI_Create(int *handle);

/**\brief 销毀一个SI解析器
 * \param handle SI解析句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
extern AM_ErrorCode_t AM_SI_Destroy(int handle);

/**\brief 解析一个section,并返回解析数据
 * 支持的表(相应返回结构):CAT(dvbpsi_cat_t) PAT(dvbpsi_pat_t) PMT(dvbpsi_pmt_t) 
 * SDT(dvbpsi_sdt_t) EIT(dvbpsi_eit_t) TOT(dvbpsi_tot_t) NIT(dvbpsi_nit_t).
 *
 * e.g.解析一个PAT section:
 * 	dvbpsi_pat_t *pat_sec;
 * 	AM_SI_DecodeSection(hSI, AM_SI_PID_PAT, pat_buf, len, &pat_sec);
 *
 * \param handle SI解析句柄
 * \param pid section pid
 * \param [in] buf section原始数据
 * \param len section原始数据长度
 * \param [out] sec 返回section解析后的数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
extern AM_ErrorCode_t AM_SI_DecodeSection(int handle, uint16_t pid, uint8_t *buf, uint16_t len, void **sec);

/**\brief 释放一个从 AM_SI_DecodeSection()返回的section
 * \param handle SI解析句柄
 * \param [in] sec 需要释放的section
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
extern AM_ErrorCode_t AM_SI_ReleaseSection(int handle, void *sec);

/**\brief 获得一个section头信息
 * \param handle SI解析句柄
 * \param [in] buf section原始数据
 * \param len section原始数据长度
 * \param [out] sec_header section header信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
extern AM_ErrorCode_t AM_SI_GetSectionHeader(int handle, uint8_t *buf, uint16_t len, AM_SI_SectionHeader_t *sec_header);

#ifdef __cplusplus
}
#endif

#endif

