/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_si.c
 * \brief SI Decoder 模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-15: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 0

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <am_debug.h>
#include "am_si.h"
#include "am_si_internal.h"
#include <am_mem.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/*增加一个描述符及其对应的解析函数*/
#define SI_ADD_DESCR_DECODE_FUNC(_tag, _func)\
		case _tag:\
			_func(des);\
			break;
			
/**\brief 解析ATSC表*/
#define si_decode_psip_table(p_table, tab, _type, _buf, _len)\
	AM_MACRO_BEGIN\
		_type *p_sec = atsc_psip_new_##tab##_info();\
		if (p_sec == NULL){\
			ret = AM_SI_ERR_NO_MEM;\
		} else {\
			ret = atsc_psip_parse_##tab(_buf,(uint32_t)_len, p_sec);\
		}\
		if (ret == AM_SUCCESS){\
			p_sec->i_table_id = _buf[0];\
			p_table = (void*)p_sec;\
		} else if (p_sec){\
			atsc_psip_free_##tab##_info(p_sec);\
			p_table = NULL;\
		}\
	AM_MACRO_END

/****************************************************************************
 * Static data
 ***************************************************************************/

extern void dvbpsi_DecodePATSections(dvbpsi_pat_t *p_pat,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodePMTSections(dvbpsi_pmt_t *p_pmt,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeCATSections(dvbpsi_cat_t *p_cat,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeNITSections(dvbpsi_nit_t *p_nit,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeSDTSections(dvbpsi_sdt_t *p_sdt,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeEITSections(dvbpsi_eit_t *p_eit,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeTOTSections(dvbpsi_tot_t *p_tot,dvbpsi_psi_section_t* p_section);
extern void dvbpsi_DecodeBATSections(dvbpsi_bat_t *p_tot,dvbpsi_psi_section_t* p_section);


static const char * const si_prv_data = "AM SI Decoder";



/**\brief 从dvbpsi_psi_section_t结构创建PAT表*/
static AM_ErrorCode_t si_decode_pat(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_pat_t *p_pat;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_pat = (dvbpsi_pat_t*)malloc(sizeof(dvbpsi_pat_t));
	if (p_pat == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_pat*/
	dvbpsi_InitPAT(p_pat,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next);
	/*Decode*/
	dvbpsi_DecodePATSections(p_pat, p_section);
	
	p_pat->i_table_id = p_section->i_table_id;
    *p_table = (void*)p_pat;

    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建PMT表*/
static AM_ErrorCode_t si_decode_pmt(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_pmt_t *p_pmt;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_pmt = (dvbpsi_pmt_t*)malloc(sizeof(dvbpsi_pmt_t));
	if (p_pmt == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_pmt*/
	dvbpsi_InitPMT(p_pmt,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next,
                   ((uint16_t)(p_section->p_payload_start[0] & 0x1f) << 8)\
                   | (uint16_t)p_section->p_payload_start[1]);
	/*Decode*/
	dvbpsi_DecodePMTSections(p_pmt, p_section);

	p_pmt->i_table_id = p_section->i_table_id;
    *p_table = (void*)p_pmt;
    
    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建CAT表*/
static AM_ErrorCode_t si_decode_cat(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_cat_t *p_cat;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_cat = (dvbpsi_cat_t*)malloc(sizeof(dvbpsi_cat_t));
	if (p_cat == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_cat*/
	dvbpsi_InitCAT(p_cat,
                   p_section->i_version,
                   p_section->b_current_next);
	/*Decode*/
	dvbpsi_DecodeCATSections(p_cat, p_section);

	p_cat->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_cat;
	
    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建NIT表*/
static AM_ErrorCode_t si_decode_nit(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_nit_t *p_nit;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_nit = (dvbpsi_nit_t*)malloc(sizeof(dvbpsi_nit_t));
	if (p_nit == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_nit*/
	dvbpsi_InitNIT(p_nit,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next);
	/*Decode*/
	dvbpsi_DecodeNITSections(p_nit, p_section);

	p_nit->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_nit;
	
    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建BAT表*/
static AM_ErrorCode_t si_decode_bat(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_bat_t *p_bat;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_bat = (dvbpsi_bat_t*)malloc(sizeof(dvbpsi_bat_t));
	if (p_bat == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_bat*/
	dvbpsi_InitBAT(p_bat,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next);
	/*Decode*/
	dvbpsi_DecodeBATSections(p_bat, p_section);

	p_bat->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_bat;
	
    return AM_SUCCESS;
}


/**\brief 从dvbpsi_psi_section_t结构创建SDT表*/
static AM_ErrorCode_t si_decode_sdt(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_sdt_t *p_sdt;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_sdt = (dvbpsi_sdt_t*)malloc(sizeof(dvbpsi_sdt_t));
	if (p_sdt == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_sdt*/
	dvbpsi_InitSDT(p_sdt,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next,
                   ((uint16_t)(p_section->p_payload_start[0]) << 8)\
                   | (uint16_t)p_section->p_payload_start[1]);
	/*Decode*/
	dvbpsi_DecodeSDTSections(p_sdt, p_section);

	p_sdt->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_sdt;
	
    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建EIT表*/
static AM_ErrorCode_t si_decode_eit(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_eit_t *p_eit;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_eit = (dvbpsi_eit_t*)malloc(sizeof(dvbpsi_eit_t));
	if (p_eit == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_eit*/
	dvbpsi_InitEIT(p_eit,
                   p_section->i_extension,
                   p_section->i_version,
                   p_section->b_current_next,
                   ((uint16_t)(p_section->p_payload_start[0]) << 8)\
                   | (uint16_t)p_section->p_payload_start[1],
                   ((uint16_t)(p_section->p_payload_start[2]) << 8)\
                   | (uint16_t)p_section->p_payload_start[3],
                   p_section->p_payload_start[4],
                   p_section->p_payload_start[5]);
	/*Decode*/
	dvbpsi_DecodeEITSections(p_eit, p_section);

	p_eit->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_eit;
	
    return AM_SUCCESS;
}

/**\brief 从dvbpsi_psi_section_t结构创建TOT表*/
static AM_ErrorCode_t si_decode_tot(void **p_table, dvbpsi_psi_section_t *p_section)
{
	dvbpsi_tot_t *p_tot;
	
	assert(p_table && p_section);

	/*Allocate a new table*/
	p_tot = (dvbpsi_tot_t*)malloc(sizeof(dvbpsi_tot_t));
	if (p_tot == NULL)
	{
		*p_table = NULL;
		return AM_SI_ERR_NO_MEM;
	}
	
	/*Init the p_tot*/
	dvbpsi_InitTOT(p_tot,   
				   ((uint64_t)p_section->p_payload_start[0] << 32)\
                   | ((uint64_t)p_section->p_payload_start[1] << 24)\
                   | ((uint64_t)p_section->p_payload_start[2] << 16)\
                   | ((uint64_t)p_section->p_payload_start[3] <<  8)\
                   |  (uint64_t)p_section->p_payload_start[4]);
	/*Decode*/
	dvbpsi_DecodeTOTSections(p_tot, p_section);

	p_tot->i_table_id = p_section->i_table_id;
	*p_table = (void*)p_tot;
	
    return AM_SUCCESS;
}

/**\brief 检查句柄是否有效*/
static AM_INLINE AM_ErrorCode_t si_check_handle(int handle)
{
	if (handle && ((SI_Decoder_t*)handle)->allocated &&
		(((SI_Decoder_t*)handle)->prv_data == si_prv_data))
		return AM_SUCCESS;

	AM_DEBUG(1, "SI Decoder got invalid handle");
	return AM_SI_ERR_INVALID_HANDLE;
}

/**\brief 解析一个section头*/
static AM_ErrorCode_t si_get_section_header(uint8_t *buf, AM_SI_SectionHeader_t *sec_header)
{
	assert(buf && sec_header);

	sec_header->table_id = buf[0];
	sec_header->syntax_indicator = (buf[1] & 0x80) >> 7;
	sec_header->private_indicator = (buf[1] & 0x40) >> 6;
	sec_header->length = (((uint16_t)(buf[1] & 0x0f)) << 8) | (uint16_t)buf[2];
	sec_header->extension = ((uint16_t)buf[3] << 8) | (uint16_t)buf[4];
	sec_header->version = (buf[5] & 0x3e) >> 1;
	sec_header->cur_next_indicator = buf[5] & 0x1;
	sec_header->sec_num = buf[6];
	sec_header->last_sec_num = buf[7];

	return AM_SUCCESS;
}

/**\brief 从section原始数据生成dvbpsi_psi_section_t类型的数据*/
static AM_ErrorCode_t si_gen_dvbpsi_section(uint8_t *buf, uint16_t len, dvbpsi_psi_section_t **psi_sec)
{	
	dvbpsi_psi_section_t * p_section;
	AM_SI_SectionHeader_t header;

	assert(buf && psi_sec);

	/*Check the section header*/
	AM_TRY(si_get_section_header(buf, &header));
	if ((header.length + 3) != len)
	{
		AM_DEBUG(1, "Invalid section header");
		return AM_SI_ERR_INVALID_SECTION_DATA;
	}
	
	/* Allocate the dvbpsi_psi_section_t structure */
	p_section  = (dvbpsi_psi_section_t*)malloc(sizeof(dvbpsi_psi_section_t));
 	if(p_section == NULL)
 	{
 		AM_DEBUG(1, "Cannot alloc new psi section, no enough memory");
 		return AM_SI_ERR_NO_MEM;
 	}

	/*Fill the p_section*/
	p_section->i_table_id = header.table_id;
	p_section->b_syntax_indicator = header.syntax_indicator;
	p_section->b_private_indicator = header.private_indicator;
	p_section->i_length = header.length;
	p_section->i_extension = header.extension;
	p_section->i_version = header.version;
	p_section->b_current_next = header.cur_next_indicator;
	p_section->i_number = header.sec_num;
	p_section->i_last_number = header.last_sec_num;
	p_section->p_data = buf;
	if (header.table_id == AM_SI_TID_TDT || header.table_id == AM_SI_TID_TOT)
		p_section->p_payload_start = buf + 3;
	else
		p_section->p_payload_start = buf + 8;
	p_section->p_payload_end = buf + len;
 	p_section->p_next = NULL;

 	*psi_sec = p_section;

 	return AM_SUCCESS;
}

/**\brief 解析一个描述符,自行查找解析函数,为libdvbsi调用*/
void si_decode_descriptor(dvbpsi_descriptor_t *des)
{
	assert(des);

	/*Decode*/
	switch (des->i_tag)
	{
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_VIDEO_STREAM,	dvbpsi_DecodeVStreamDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_AUDIO_STREAM,	dvbpsi_DecodeAStreamDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_HIERARCHY,	  	dvbpsi_DecodeHierarchyDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_REGISTRATION,	dvbpsi_DecodeRegistrationDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_DS_ALIGNMENT,	dvbpsi_DecodeDSAlignmentDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_TARGET_BG_GRID,dvbpsi_DecodeTargetBgGridDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_VIDEO_WINDOW,	dvbpsi_DecodeVWindowDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_CA,			dvbpsi_DecodeCADr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_ISO639,		dvbpsi_DecodeISO639Dr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SYSTEM_CLOCK,	dvbpsi_DecodeSystemClockDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULX_BUF_UTILIZATION,  dvbpsi_DecodeMxBuffUtilizationDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_COPYRIGHT,		dvbpsi_DecodeCopyrightDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MAX_BITRATE,	dvbpsi_DecodeMaxBitrateDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_PRIVATE_DATA_INDICATOR,dvbpsi_DecodePrivateDataDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_NETWORK_NAME, 	dvbpsi_DecodeNetworkNameDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SERVICE_LIST, 	dvbpsi_DecodeServiceListDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_STUFFING, 		dvbpsi_DecodeStuffingDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SATELLITE_DELIVERY, dvbpsi_DecodeSatDelivSysDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_CABLE_DELIVERY, dvbpsi_DecodeCableDeliveryDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_VBI_DATA, 		dvbpsi_DecodeVBIDataDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_VBI_TELETEXT, 			NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_BOUQUET_NAME, 	dvbpsi_DecodeBouquetNameDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SERVICE, 		dvbpsi_DecodeServiceDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_LINKAGE, 		dvbpsi_DecodeLinkageDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_NVOD_REFERENCE, 		NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_TIME_SHIFTED_SERVICE,	NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SHORT_EVENT, 	dvbpsi_DecodeShortEventDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_EXTENDED_EVENT,dvbpsi_DecodeExtendedEventDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_TIME_SHIFTED_EVENT, 	NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_COMPONENT, 			NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MOSAIC, 				NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_STREAM_IDENTIFIER, dvbpsi_DecodeStreamIdentifierDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_CA_IDENTIFIER, 		NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_CONTENT, 				dvbpsi_DecodeContentDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_PARENTAL_RATING, dvbpsi_DecodeParentalRatingDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_TELETEXT, 		dvbpsi_DecodeTeletextDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_TELPHONE, 				NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_LOCAL_TIME_OFFSET,	dvbpsi_DecodeLocalTimeOffsetDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_SUBTITLING, 		dvbpsi_DecodeSubtitlingDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_NETWORK_NAME, 	NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_BOUQUET_NAME, 	NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_SERVICE_NAME, 	NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_COMPONENT, 		NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_DATA_BROADCAST, 		NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_DATA_BROADCAST_ID, 	NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_PDC, 			dvbpsi_DecodePDCDr)
		default:
			break;
	}
	
}

/****************************************************************************
 * API functions
 ***************************************************************************/
/**\brief 创建一个SI解析器
 * \param [out] handle 返回SI解析句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_Create(int *handle)
{
	SI_Decoder_t *dec;

	assert(handle);
	
	dec = (SI_Decoder_t *)malloc(sizeof(SI_Decoder_t));
	if (dec == NULL)
	{
		AM_DEBUG(1, "Cannot create SI Decoder, no enough memory");
		return AM_SI_ERR_NO_MEM;
	}

	dec->prv_data = (void*)si_prv_data;
	dec->allocated = AM_TRUE;

	*handle = (int)dec;

	return AM_SUCCESS;
}

/**\brief 销毀一个SI解析器
 * \param handle SI解析句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_Destroy(int handle)
{
	SI_Decoder_t *dec = (SI_Decoder_t*)handle;

	AM_TRY(si_check_handle(handle));

	dec->allocated = AM_FALSE;
	dec->prv_data = NULL;
	free(dec);

	return AM_SUCCESS;
}

/**\brief 解析一个section,并返回解析数据
 * 支持的表(相应返回结构):CAT(dvbpsi_cat_t) PAT(dvbpsi_pat_t) PMT(dvbpsi_pmt_t) 
 * SDT(dvbpsi_sdt_t) EIT(dvbpsi_eit_t) TOT(dvbpsi_tot_t) NIT(dvbpsi_nit_t).
 * CVCT(cvct_section_info_t) TVCT(tvct_section_info_t) MGT(mgt_section_info_t)
 * RRT(rrt_section_info_t) STT(stt_section_info_t)
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
AM_ErrorCode_t AM_SI_DecodeSection(int handle, uint16_t pid, uint8_t *buf, uint16_t len, void **sec)
{
	dvbpsi_psi_section_t *psi_sec = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	uint8_t table_id;

	assert(buf && sec);
	AM_TRY(si_check_handle(handle));
	
	table_id = buf[0];
	
	if (table_id <= AM_SI_TID_TOT)
	{
		/*生成dvbpsi section*/
		AM_TRY(si_gen_dvbpsi_section(buf, len, &psi_sec));
	}

	*sec = NULL;
	/*Decode*/
	switch (table_id)
	{
		case AM_SI_TID_PAT:
			if (pid != AM_SI_PID_PAT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_pat(sec, psi_sec);
			break;
		case AM_SI_TID_PMT:
			ret = si_decode_pmt(sec, psi_sec);
			break;
		case AM_SI_TID_CAT:
			if (pid != AM_SI_PID_CAT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_cat(sec, psi_sec);
			break;
		case AM_SI_TID_NIT_ACT:
		case AM_SI_TID_NIT_OTH:
			if (pid != AM_SI_PID_NIT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_nit(sec, psi_sec);
			break;
		case AM_SI_TID_BAT:
			if (pid != AM_SI_PID_BAT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_bat(sec, psi_sec);
			break;
		case AM_SI_TID_SDT_ACT:
		case AM_SI_TID_SDT_OTH:
			if (pid != AM_SI_PID_SDT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_sdt(sec, psi_sec);
			break;
		case AM_SI_TID_EIT_PF_ACT:
		case AM_SI_TID_EIT_PF_OTH:
		case AM_SI_TID_EIT_SCHE_ACT:
		case AM_SI_TID_EIT_SCHE_OTH:
		case (AM_SI_TID_EIT_SCHE_ACT + 1):
		case (AM_SI_TID_EIT_SCHE_OTH + 1):
			if (pid != AM_SI_PID_EIT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_eit(sec, psi_sec);
			break;
		case AM_SI_TID_TOT:
		case AM_SI_TID_TDT:
			if (pid != AM_SI_PID_TOT)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				ret = si_decode_tot(sec, psi_sec);
			break;
		case AM_SI_TID_PSIP_MGT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, mgt, mgt_section_info_t, buf, len);
			break;
		case AM_SI_TID_PSIP_TVCT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, tvct, tvct_section_info_t, buf, len);
			break;
		case AM_SI_TID_PSIP_CVCT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, cvct, cvct_section_info_t, buf, len);
			break;
		case AM_SI_TID_PSIP_RRT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, rrt, rrt_section_info_t, buf, len);
			break;
		case AM_SI_TID_PSIP_STT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, stt, stt_section_info_t, buf, len);
			break;
		default:
			ret = AM_SI_ERR_NOT_SUPPORTED;
			break;
	}

	/*release the psi_sec*/
	free(psi_sec);

	return ret;
}

/**\brief 释放一个从 AM_SI_DecodeSection()返回的section
 * \param handle SI解析句柄
 * \param [in] sec 需要释放的section
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_ReleaseSection(int handle, void *sec)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(sec);
	AM_TRY(si_check_handle(handle));

	switch (((dvbpsi_pat_t*)sec)->i_table_id)
	{
		case AM_SI_TID_PAT:
			dvbpsi_DeletePAT((dvbpsi_pat_t*)sec);
			break;
		case AM_SI_TID_PMT:
			dvbpsi_DeletePMT((dvbpsi_pmt_t*)sec);
			break;
		case AM_SI_TID_CAT:
			dvbpsi_DeleteCAT((dvbpsi_cat_t*)sec);
			break;
		case AM_SI_TID_NIT_ACT:
		case AM_SI_TID_NIT_OTH:
			dvbpsi_DeleteNIT((dvbpsi_nit_t*)sec);
			break;
		case AM_SI_TID_BAT:
			dvbpsi_DeleteBAT((dvbpsi_bat_t*)sec);
			break;
		case AM_SI_TID_SDT_ACT:
		case AM_SI_TID_SDT_OTH:
			dvbpsi_DeleteSDT((dvbpsi_sdt_t*)sec);
			break;
		case AM_SI_TID_EIT_PF_ACT:
		case AM_SI_TID_EIT_PF_OTH:
		case AM_SI_TID_EIT_SCHE_ACT:
		case AM_SI_TID_EIT_SCHE_OTH:
		case (AM_SI_TID_EIT_SCHE_ACT + 1):
		case (AM_SI_TID_EIT_SCHE_OTH + 1):
			dvbpsi_DeleteEIT((dvbpsi_eit_t*)sec);
			break;
		case AM_SI_TID_TOT:
		case AM_SI_TID_TDT:
			dvbpsi_DeleteTOT((dvbpsi_tot_t*)sec);
			break;
		case AM_SI_TID_PSIP_MGT:
			atsc_psip_free_mgt_info((mgt_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_TVCT:
			atsc_psip_free_tvct_info((tvct_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_CVCT:
			atsc_psip_free_cvct_info((cvct_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_RRT:
			atsc_psip_free_rrt_info((rrt_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_STT:
			atsc_psip_free_stt_info((stt_section_info_t*)sec);
			break;
		default:
			ret = AM_SI_ERR_INVALID_SECTION_DATA;
	}

	return ret;
}

/**\brief 获得一个section头信息
 * \param handle SI解析句柄
 * \param [in] buf section原始数据
 * \param len section原始数据长度
 * \param [out] sec_header section header信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_GetSectionHeader(int handle, uint8_t *buf, uint16_t len, AM_SI_SectionHeader_t *sec_header)
{
	assert(buf && sec_header);
	AM_TRY(si_check_handle(handle));

	if (len < 8)
		return AM_SI_ERR_INVALID_SECTION_DATA;
		
	AM_TRY(si_get_section_header(buf, sec_header));

	return AM_SUCCESS;
}
