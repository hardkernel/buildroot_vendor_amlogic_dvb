#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_si.c
 * \brief SI Decoder 模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-15: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <am_debug.h>
#include "am_si.h"
#include "am_si_internal.h"
#include <am_mem.h>
#include <am_av.h>
#include <am_iconv.h>
#include <errno.h>

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

/*DVB字符默认编码,在进行DVB字符转码时会强制使用该编码为输入编码*/
static char forced_dvb_text_coding[32] = {0};

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
	{
		p_section->p_payload_start = buf + 3;
		p_section->p_payload_end = buf + len;
	}
	else
	{
		p_section->p_payload_start = buf + 8;
		p_section->p_payload_end = buf + len - 4;
	}
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
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_SERVICE_NAME, 	dvbpsi_DecodeMultiServiceNameDr)
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_MULTI_COMPONENT, 		NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_DATA_BROADCAST, 		NULL)*/
		/*SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_DATA_BROADCAST_ID, 	NULL)*/
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_PDC, 			dvbpsi_DecodePDCDr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_LCN_83, 			dvbpsi_DecodeLogicalChannelNumber83Dr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_LCN_87, 			dvbpsi_DecodeLogicalChannelNumber87Dr)
		SI_ADD_DESCR_DECODE_FUNC(AM_SI_DESCR_LCN_88, 			dvbpsi_DecodeLogicalChannelNumber88Dr)
		default:
			break;
	}
	
}


/**\brief 将ISO6937转换为UTF-8编码*/
static AM_ErrorCode_t si_convert_iso6937_to_utf8(const char *src, int src_len, char *dest, int *dest_len)
{     
	uint16_t ch;
	int dlen, i;
	uint8_t b;
	char *ucs2 = NULL;

#define READ_BYTE()\
	({\
		uint8_t ret;\
		if (i >= src_len) ret=0;\
		else ret = (uint8_t)src[i];\
		i++;\
		ret;\
	})
	
	if (!src || !dest || !dest_len || src_len <= 0)
		return -1;
		
	/* first covert to UCS-2, then iconv to utf8 */
	ucs2 = (char *)malloc(src_len*2);
	if (!ucs2)
		return -1;
	dlen = 0;
	i=0;
	b = READ_BYTE();
	if (b < 0x20)
	{
		/* ISO 6937 encoding must start with character between 0x20 and 0xFF
		 otherwise it is dfferent encoding table
		 for example 0x05 means encoding table 8859-9 */
		return -1;
	}
	
	while (b != 0)
	{
		ch = 0x00;
		switch (b)
		{
			/* at first single byte characters */
			case 0xA8: ch = 0x00A4; break;
			case 0xA9: ch = 0x2018; break;
			case 0xAA: ch = 0x201C; break;
			case 0xAC: ch = 0x2190; break;
			case 0xAD: ch = 0x2191; break;
			case 0xAE: ch = 0x2192; break;
			case 0xAF: ch = 0x2193; break;
			case 0xB4: ch = 0x00D7; break;
			case 0xB8: ch = 0x00F7; break;
			case 0xB9: ch = 0x2019; break;
			case 0xBA: ch = 0x201D; break;
			case 0xD0: ch = 0x2014; break;
			case 0xD1: ch = 0xB9; break;
			case 0xD2: ch = 0xAE; break;
			case 0xD3: ch = 0xA9; break;
			case 0xD4: ch = 0x2122; break;
			case 0xD5: ch = 0x266A; break;
			case 0xD6: ch = 0xAC; break;
			case 0xD7: ch = 0xA6; break;
			case 0xDC: ch = 0x215B; break;
			case 0xDD: ch = 0x215C; break;
			case 0xDE: ch = 0x215D; break;
			case 0xDF: ch = 0x215E; break;
			case 0xE0: ch = 0x2126; break;
			case 0xE1: ch = 0xC6; break;
			case 0xE2: ch = 0xD0; break;
			case 0xE3: ch = 0xAA; break;
			case 0xE4: ch = 0x126; break;
			case 0xE6: ch = 0x132; break;
			case 0xE7: ch = 0x013F; break;
			case 0xE8: ch = 0x141; break;
			case 0xE9: ch = 0xD8; break;
			case 0xEA: ch = 0x152; break;
			case 0xEB: ch = 0xBA; break;
			case 0xEC: ch = 0xDE; break;
			case 0xED: ch = 0x166; break;
			case 0xEE: ch = 0x014A; break;
			case 0xEF: ch = 0x149; break;
			case 0xF0: ch = 0x138; break;
			case 0xF1: ch = 0xE6; break;
			case 0xF2: ch = 0x111; break;
			case 0xF3: ch = 0xF0; break;
			case 0xF4: ch = 0x127; break;
			case 0xF5: ch = 0x131; break;
			case 0xF6: ch = 0x133; break;
			case 0xF7: ch = 0x140; break;
			case 0xF8: ch = 0x142; break;
			case 0xF9: ch = 0xF8; break;
			case 0xFA: ch = 0x153; break;
			case 0xFB: ch = 0xDF; break;
			case 0xFC: ch = 0xFE; break;
			case 0xFD: ch = 0x167; break;
			case 0xFE: ch = 0x014B; break;
			case 0xFF: ch = 0xAD; break;
			/* multibyte */
			case 0xC1:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC0; break;
					case 0x45: ch = 0xC8; break;
					case 0x49: ch = 0xCC; break;
					case 0x4F: ch = 0xD2; break;
					case 0x55: ch = 0xD9; break;
					case 0x61: ch = 0xE0; break;
					case 0x65: ch = 0xE8; break;
					case 0x69: ch = 0xEC; break;
					case 0x6F: ch = 0xF2; break;
					case 0x75: ch = 0xF9; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC2:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xB4; break;
					case 0x41: ch = 0xC1; break;
					case 0x43: ch = 0x106; break;
					case 0x45: ch = 0xC9; break;
					case 0x49: ch = 0xCD; break;
					case 0x4C: ch = 0x139; break;
					case 0x4E: ch = 0x143; break;
					case 0x4F: ch = 0xD3; break;
					case 0x52: ch = 0x154; break;
					case 0x53: ch = 0x015A; break;
					case 0x55: ch = 0xDA; break;
					case 0x59: ch = 0xDD; break;
					case 0x5A: ch = 0x179; break;
					case 0x61: ch = 0xE1; break;
					case 0x63: ch = 0x107; break;
					case 0x65: ch = 0xE9; break;
					case 0x69: ch = 0xED; break;
					case 0x6C: ch = 0x013A; break;
					case 0x6E: ch = 0x144; break;
					case 0x6F: ch = 0xF3; break;
					case 0x72: ch = 0x155; break;
					case 0x73: ch = 0x015B; break;
					case 0x75: ch = 0xFA; break;
					case 0x79: ch = 0xFD; break;
					case 0x7A: ch = 0x017A; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;

			case 0xC3:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC2; break;
					case 0x43: ch = 0x108; break;
					case 0x45: ch = 0xCA; break;
					case 0x47: ch = 0x011C; break;
					case 0x48: ch = 0x124; break;
					case 0x49: ch = 0xCE; break;
					case 0x4A: ch = 0x134; break;
					case 0x4F: ch = 0xD4; break;
					case 0x53: ch = 0x015C; break;
					case 0x55: ch = 0xDB; break;
					case 0x57: ch = 0x174; break;
					case 0x59: ch = 0x176; break;
					case 0x61: ch = 0xE2; break;
					case 0x63: ch = 0x109; break;
					case 0x65: ch = 0xEA; break;
					case 0x67: ch = 0x011D; break;
					case 0x68: ch = 0x125; break;
					case 0x69: ch = 0xEE; break;
					case 0x6A: ch = 0x135; break;
					case 0x6F: ch = 0xF4; break;
					case 0x73: ch = 0x015D; break;
					case 0x75: ch = 0xFB; break;
					case 0x77: ch = 0x175; break;
					case 0x79: ch = 0x177; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC4:
				b = READ_BYTE();
				switch (b)
				{
					case 0x41: ch = 0xC3; break;
					case 0x49: ch = 0x128; break;
					case 0x4E: ch = 0xD1; break;
					case 0x4F: ch = 0xD5; break;
					case 0x55: ch = 0x168; break;
					case 0x61: ch = 0xE3; break;
					case 0x69: ch = 0x129; break;
					case 0x6E: ch = 0xF1; break;
					case 0x6F: ch = 0xF5; break;
					case 0x75: ch = 0x169; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC5:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xAF; break;
					case 0x41: ch = 0x100; break;
					case 0x45: ch = 0x112; break;
					case 0x49: ch = 0x012A; break;
					case 0x4F: ch = 0x014C; break;
					case 0x55: ch = 0x016A; break;
					case 0x61: ch = 0x101; break;
					case 0x65: ch = 0x113; break;
					case 0x69: ch = 0x012B; break;
					case 0x6F: ch = 0x014D; break;
					case 0x75: ch = 0x016B; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC6:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02D8; break;
					case 0x41: ch = 0x102; break;
					case 0x47: ch = 0x011E; break;
					case 0x55: ch = 0x016C; break;
					case 0x61: ch = 0x103; break;
					case 0x67: ch = 0x011F; break;
					case 0x75: ch = 0x016D; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC7:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02D9; break;
					case 0x43: ch = 0x010A; break;
					case 0x45: ch = 0x116; break;
					case 0x47: ch = 0x120; break;
					case 0x49: ch = 0x130; break;
					case 0x5A: ch = 0x017B; break;
					case 0x63: ch = 0x010B; break;
					case 0x65: ch = 0x117; break;
					case 0x67: ch = 0x121; break;
					case 0x7A: ch = 0x017C; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xC8:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xA8; break;
					case 0x41: ch = 0xC4; break;
					case 0x45: ch = 0xCB; break;
					case 0x49: ch = 0xCF; break;
					case 0x4F: ch = 0xD6; break;
					case 0x55: ch = 0xDC; break;
					case 0x59: ch = 0x178; break;
					case 0x61: ch = 0xE4; break;
					case 0x65: ch = 0xEB; break;
					case 0x69: ch = 0xEF; break;
					case 0x6F: ch = 0xF6; break;
					case 0x75: ch = 0xFC; break;
					case 0x79: ch = 0xFF; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCA:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DA; break;
					case 0x41: ch = 0xC5; break;
					case 0x55: ch = 0x016E; break;
					case 0x61: ch = 0xE5; break;
					case 0x75: ch = 0x016F; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCB:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0xB8; break;
					case 0x43: ch = 0xC7; break;
					case 0x47: ch = 0x122; break;
					case 0x4B: ch = 0x136; break;
					case 0x4C: ch = 0x013B; break;
					case 0x4E: ch = 0x145; break;
					case 0x52: ch = 0x156; break;
					case 0x53: ch = 0x015E; break;
					case 0x54: ch = 0x162; break;
					case 0x63: ch = 0xE7; break;
					case 0x67: ch = 0x123; break;
					case 0x6B: ch = 0x137; break;
					case 0x6C: ch = 0x013C; break;
					case 0x6E: ch = 0x146; break;
					case 0x72: ch = 0x157; break;
					case 0x73: ch = 0x015F; break;
					case 0x74: ch = 0x163; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCD:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DD; break;
					case 0x4F: ch = 0x150; break;
					case 0x55: ch = 0x170; break;
					case 0x6F: ch = 0x151; break;
					case 0x75: ch = 0x171; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCE:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02DB; break;
					case 0x41: ch = 0x104; break;
					case 0x45: ch = 0x118; break;
					case 0x49: ch = 0x012E; break;
					case 0x55: ch = 0x172; break;
					case 0x61: ch = 0x105; break;
					case 0x65: ch = 0x119; break;
					case 0x69: ch = 0x012F; break;
					case 0x75: ch = 0x173; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			case 0xCF:
				b = READ_BYTE();
				switch (b)
				{
					case 0x20: ch = 0x02C7; break;
					case 0x43: ch = 0x010C; break;
					case 0x44: ch = 0x010E; break;
					case 0x45: ch = 0x011A; break;
					case 0x4C: ch = 0x013D; break;
					case 0x4E: ch = 0x147; break;
					case 0x52: ch = 0x158; break;
					case 0x53: ch = 0x160; break;
					case 0x54: ch = 0x164; break;
					case 0x5A: ch = 0x017D; break;
					case 0x63: ch = 0x010D; break;
					case 0x64: ch = 0x010F; break;
					case 0x65: ch = 0x011B; break;
					case 0x6C: ch = 0x013E; break;
					case 0x6E: ch = 0x148; break;
					case 0x72: ch = 0x159; break;
					case 0x73: ch = 0x161; break;
					case 0x74: ch = 0x165; break;
					case 0x7A: ch = 0x017E; break;
					// unknown character --> fallback
					default: ch = b; break;
				}
				break;
			/* rest is the same */
			default: ch = b; break;
		}
		if (b != 0)
		{
			b = READ_BYTE();
		}
		if (ch != 0)
		{
			/* dest buffer not enough, and this will not happen */
			if ((dlen+1) >= (src_len*2))
				goto iso6937_end;
			ucs2[dlen++] = (ch&0xff00) >> 8;
			ucs2[dlen++] = ch;
		}
	}
	
iso6937_end:
	if (dlen > 0)
	{
		iconv_t handle;
		char *org_ucs2 = ucs2;
		char **pin=&ucs2;
		char **pout=&dest;
		
		handle=iconv_open("utf-8","ucs-2");

		if (handle == (iconv_t)-1)
		{
			AM_DEBUG(1, "iconv_open err: %s",strerror(errno));
			return AM_FAILURE;
		}

		if((int)iconv(handle,pin,(size_t *)&dlen,pout,(size_t *)dest_len) == -1)
		{
		    AM_DEBUG(1, "iconv err: %s", strerror(errno));
		    iconv_close(handle);
		    return AM_FAILURE;
		}
		
		free(org_ucs2);

		return iconv_close(handle);
    }
    
    free(ucs2);
    
    return -1;
} 

static void si_add_audio(AM_SI_AudioInfo_t *ai, int aud_pid, int aud_fmt, char lang[3])
{
	int i;
	
	for (i=0; i<ai->audio_count; i++)
	{
		if (ai->audios[i].pid == aud_pid &&
			ai->audios[i].fmt == aud_fmt &&
			! memcmp(ai->audios[i].lang, lang, 3))
		{
			AM_DEBUG(0, "Skipping a exist audio: pid %d, fmt %d, lang %c%c%c",
				aud_pid, aud_fmt, lang[0], lang[1], lang[2]);
			return;
		}
	}
	if (ai->audio_count >= AM_SI_MAX_AUD_CNT)
	{
		AM_DEBUG(1, "Too many audios, Max count %d", AM_SI_MAX_AUD_CNT);
		return;
	}
	if (ai->audio_count < 0)
		ai->audio_count = 0;
	ai->audios[ai->audio_count].pid = aud_pid;
	ai->audios[ai->audio_count].fmt = aud_fmt;
	memset(ai->audios[ai->audio_count].lang, 0, sizeof(ai->audios[ai->audio_count].lang));
	if (lang[0] != 0)
	{
		memcpy(ai->audios[ai->audio_count].lang, lang, 3);
	}
	else
	{
		sprintf(ai->audios[ai->audio_count].lang, "Audio%d", ai->audio_count+1);
	}
	
	AM_DEBUG(0, "Add a audio: pid %d, fmt %d, language: %s", aud_pid, aud_fmt, ai->audios[ai->audio_count].lang);

	ai->audio_count++;
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
 * VCT(vct_section_info_t) MGT(mgt_section_info_t)
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
		case AM_SI_TID_PSIP_CVCT:
			if (pid != AM_SI_ATSC_BASE_PID)
				ret = AM_SI_ERR_NOT_SUPPORTED;
			else
				si_decode_psip_table(*sec, vct, vct_section_info_t, buf, len);
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
		case AM_SI_TID_PSIP_EIT:
			si_decode_psip_table(*sec, eit, eit_section_info_t, buf, len);
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
 * \param table_id 用于标示section类型
 * \param [in] sec 需要释放的section
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_ReleaseSection(int handle, uint8_t table_id, void *sec)
{
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(sec);
	AM_TRY(si_check_handle(handle));

	switch (table_id)
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
		case AM_SI_TID_PSIP_CVCT:
			atsc_psip_free_vct_info((vct_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_RRT:
			atsc_psip_free_rrt_info((rrt_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_STT:
			atsc_psip_free_stt_info((stt_section_info_t*)sec);
			break;
		case AM_SI_TID_PSIP_EIT:
			atsc_psip_free_eit_info((eit_section_info_t*)sec);
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

/**\brief 设置默认的DVB编码方式，当前端流未按照DVB标准，即第一个
 * 字符没有指定编码方式时，可以调用该函数来指定一个强制转换的编码。
 * \param [in] code 默认进行强制转换的字符编码方式,如GB2312，BIG5等.
 * \return
 */
void AM_SI_SetDefaultDVBTextCoding(const char *coding)
{
	snprintf(forced_dvb_text_coding, sizeof(forced_dvb_text_coding), "%s", coding);
}

/**\brief 按DVB标准将输入字符转成UTF-8编码
 * \param [in] in_code 需要转换的字符数据
 * \param in_len 需要转换的字符数据长度
 * \param [out] out_code 转换后的字符数据
 * \param out_len 输出字符缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_ConvertDVBTextCode(char *in_code,int in_len,char *out_code,int out_len)
{
    iconv_t handle;
    char **pin=&in_code;
    char **pout=&out_code;
    char fbyte;
    char cod[32];
    
	if (!in_code || !out_code || in_len <= 0 || out_len <= 0)
		return AM_FAILURE;

	memset(out_code,0,out_len);
	 
	/*查找输入编码方式*/
	if (in_len <= 1)
	{
		pin = &in_code;
		strcpy(cod, "ISO-8859-1");
	}
	else
	{
		fbyte = in_code[0];
		if (fbyte >= 0x01 && fbyte <= 0x0B)
			sprintf(cod, "ISO-8859-%d", fbyte + 4);
		else if (fbyte >= 0x0C && fbyte <= 0x0F)
		{
			/*Reserved for future use, we set to ISO8859-1*/
			strcpy(cod, "ISO-8859-1");
		}
		else if (fbyte == 0x10 && in_len >= 3)
		{
			uint16_t val = (uint16_t)(((uint16_t)in_code[1]<<8) | (uint16_t)in_code[2]);
			if (val >= 0x0001 && val <= 0x000F)
			{
				sprintf(cod, "ISO-8859-%d", val);
			}
			else
			{
				/*Reserved for future use, we set to ISO8859-1*/
				strcpy(cod, "ISO-8859-1");
			}
			in_code += 2;
			in_len -= 2;
		}
		else if (fbyte == 0x11)
			strcpy(cod, "UTF-16");
		else if (fbyte == 0x13)
			strcpy(cod, "GB2312");
		else if (fbyte == 0x14)
			strcpy(cod, "UTF-16BE");
		else if (fbyte == 0x15)
			strcpy(cod, "utf-8");
		else if (fbyte >= 0x20)
		{
			if (strcmp(forced_dvb_text_coding, ""))
			{
				/*强制将输入按默认编码处理*/
				strcpy(cod, forced_dvb_text_coding);
			}
			else
			{
				strcpy(cod, "ISO6937");
			}
		}
		else
			return AM_FAILURE;

		/*调整输入*/
		if (fbyte < 0x20)
		{
			in_code++;
			in_len--;
		}
		pin = &in_code;
		
	}

	if (! strcmp(cod, "ISO6937"))
	{
		return si_convert_iso6937_to_utf8(in_code,in_len,out_code,&out_len);
	}	
	else if(! strcmp(cod, "utf-8"))
	{
		return AM_Check_UTF8(in_code,in_len,out_code,&out_len);
		
	}
	else
	{
		handle=iconv_open("utf-8",cod);

		if (handle == (iconv_t)-1)
		{
			AM_DEBUG(1, "Covert DVB text code failed, iconv_open err: %s",strerror(errno));
			return AM_FAILURE;
		}

		if((int)iconv(handle,pin,(size_t *)&in_len,pout,(size_t *)&out_len) == -1)
		{
		    AM_DEBUG(1, "Covert DVB text code failed, iconv err: %s, in_len %d, out_len %d", 
		    	strerror(errno), in_len, out_len);
		    iconv_close(handle);
		    return AM_FAILURE;
		}

		return iconv_close(handle);
	}
}

/**\brief 从一个ES流中提取音视频
 * \param [in] es ES流
 * \param [out] vid 提取出的视频PID
 * \param [out] vfmt 提取出的视频压缩格式
 * \param [out] aud_info 提取出的音频数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_ExtractAVFromES(dvbpsi_pmt_es_t *es, int *vid, int *vfmt, AM_SI_AudioInfo_t *aud_info)
{
	char lang_tmp[3];
	int afmt_tmp, vfmt_tmp;
	dvbpsi_descriptor_t *descr;
	
	afmt_tmp = -1;
	vfmt_tmp = -1;
	memset(lang_tmp, 0, sizeof(lang_tmp));
	switch (es->i_type)
	{
		/*override by parse descriptor*/
		case 0x6:
			AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
				switch (descr->i_tag)
				{
					case AM_SI_DESCR_AC3:
						AM_DEBUG(1, "!!Found AC3 Descriptor!!!");
						afmt_tmp = AFORMAT_AC3;
						break;
					case AM_SI_DESCR_ENHANCED_AC3:
						AM_DEBUG(1, "!!Found Enhanced AC3 Descriptor!!!");
						afmt_tmp = AFORMAT_EAC3;
						break;
					case AM_SI_DESCR_AAC:
						AM_DEBUG(1, "!!Found AAC Descriptor!!!");
						afmt_tmp = AFORMAT_AAC;
						break;
					case AM_SI_DESCR_DTS:
						AM_DEBUG(1, "!!Found DTS Descriptor!!!");
						afmt_tmp = AFORMAT_DTS;
						break;
                                        case AM_SI_DESCR_DRA:
                                                 AM_DEBUG(1, "!!Found DRA Descriptor!!!");
                                                 afmt_tmp = AFORMAT_DRA;
                                                 break;

					default:
						break;
				}
			AM_SI_LIST_END()
			break;
		/*video pid and video format*/
		case 0x1:
		case 0x2:
			vfmt_tmp = VFORMAT_MPEG12;
			break;
		case 0x10:
			vfmt_tmp = VFORMAT_MPEG4;
			break;
		case 0x1b:
			vfmt_tmp = VFORMAT_H264;
			break;
		case 0xea:
			vfmt_tmp = VFORMAT_VC1;
			break;
		case 0x42:
			vfmt_tmp = VFORMAT_AVS;
			break;
		/*audio pid and audio format*/ 
		case 0x3:
		case 0x4:
			afmt_tmp = AFORMAT_MPEG;
			break;
		case 0x0f:
			afmt_tmp = AFORMAT_AAC;
			break;
		case 0x11:
			afmt_tmp = AFORMAT_AAC_LATM;
			break;
		case 0x81:
			afmt_tmp = AFORMAT_AC3;
			break;
		case 0x8A:
        case 0x82:
        case 0x85:
        case 0x86:
        	afmt_tmp = AFORMAT_DTS;
			break;
		default:
			break;
	}
	
	/*添加音视频流*/
	if (vfmt_tmp != -1)
	{
		*vid = (es->i_pid >= 0x1fff) ? 0x1fff : es->i_pid;	
		AM_DEBUG(3, "Set video format to %d", vfmt_tmp);
		*vfmt = vfmt_tmp;
	}
	if (afmt_tmp != -1)
	{
		AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
			if (descr->i_tag == AM_SI_DESCR_ISO639 && descr->p_decoded != NULL)
			{
				dvbpsi_iso639_dr_t *pisod = (dvbpsi_iso639_dr_t*)descr->p_decoded;
				if (pisod->i_code_count > 0) 
				{
					memcpy(lang_tmp, pisod->code[0].iso_639_code, sizeof(lang_tmp));
					break;
				}
			}
		AM_SI_LIST_END()
		/* Add a audio */
		si_add_audio(aud_info, es->i_pid, afmt_tmp, lang_tmp);
	}
	
	return AM_SUCCESS;
}

/**\brief 按ATSC标准从一个ATSC visual channel中提取音视频
 * \param [in] es ES流
 * \param [out] vid 提取出的视频PID
 * \param [out] vfmt 提取出的视频压缩格式
 * \param [out] aud_info 提取出的音频数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_si.h)
 */
AM_ErrorCode_t AM_SI_ExtractAVFromATSCVC(vct_channel_info_t *vcinfo, int *vid, int *vfmt, AM_SI_AudioInfo_t *aud_info)
{
	char lang_tmp[3];
	int afmt_tmp, vfmt_tmp, i;
	atsc_descriptor_t *descr;
	
	afmt_tmp = -1;
	vfmt_tmp = -1;
	memset(lang_tmp, 0, sizeof(lang_tmp));

	AM_SI_LIST_BEGIN(vcinfo->desc, descr)			
		if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SERVICE_LOCATION)
		{
			atsc_service_location_dr_t *asld = (atsc_service_location_dr_t*)descr->p_decoded;
			for (i=0; i<asld->i_elem_count; i++)
			{
				afmt_tmp = -1;
				vfmt_tmp = -1;
				memset(lang_tmp, 0, sizeof(lang_tmp));
				switch (asld->elem[i].i_stream_type)
				{
					/*video pid and video format*/
					case 0x02:
						vfmt_tmp = VFORMAT_MPEG12;
						break;
					/*audio pid and audio format*/
					case 0x81:
						afmt_tmp = AFORMAT_AC3;
						break;
					default:
						break;
				}
				if (vfmt_tmp != -1)
				{
					*vid = (asld->elem[i].i_pid >= 0x1fff) ? 0x1fff : asld->elem[i].i_pid;
					*vfmt = vfmt_tmp;
				}
				if (afmt_tmp != -1)
				{
					memcpy(lang_tmp, asld->elem[i].iso_639_code, sizeof(lang_tmp));
					si_add_audio(aud_info, asld->elem[i].i_pid, afmt_tmp, lang_tmp);
				}
			}
		}
	AM_SI_LIST_END()

	return AM_SUCCESS;
}

AM_ErrorCode_t AM_SI_ExtractDVBSubtitleFromES(dvbpsi_pmt_es_t *es, AM_SI_SubtitleInfo_t *sub_info)
{
	dvbpsi_descriptor_t *descr;
	
	AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
		if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_SUBTITLING)
		{
			int isub, i;
			dvbpsi_subtitle_t *tmp_sub;
			dvbpsi_subtitling_dr_t *psd = (dvbpsi_subtitling_dr_t*)descr->p_decoded;

			for (isub=0; isub<psd->i_subtitles_number; isub++)
			{	
				tmp_sub = &psd->p_subtitle[isub];
				
				/* already added ? */
				for (i=0; i<sub_info->subtitle_count; i++)
				{
					if (es->i_pid                      == sub_info->subtitles[i].pid &&
						tmp_sub->i_subtitling_type     == sub_info->subtitles[i].type &&
						tmp_sub->i_ancillary_page_id   == sub_info->subtitles[i].anci_page_id &&
						tmp_sub->i_composition_page_id == sub_info->subtitles[i].comp_page_id &&
						! memcmp(tmp_sub->i_iso6392_language_code, sub_info->subtitles[i].lang, 3))
					{
						AM_DEBUG(1, "Skipping a exist subtitle: pid %d, lang %c%c%c",
							es->i_pid, tmp_sub->i_iso6392_language_code[0], 
							tmp_sub->i_iso6392_language_code[1], 
							tmp_sub->i_iso6392_language_code[2]);
						break;
					}
				}

				if (i < sub_info->subtitle_count)
					continue;
				
				if (sub_info->subtitle_count >= AM_SI_MAX_SUB_CNT)
				{
					AM_DEBUG(1, "Too many subtitles, Max count %d", AM_SI_MAX_SUB_CNT);
					return AM_SUCCESS;
				}
				
				if (sub_info->subtitle_count < 0)
					sub_info->subtitle_count = 0;
				
				sub_info->subtitles[sub_info->subtitle_count].pid          = es->i_pid;
				sub_info->subtitles[sub_info->subtitle_count].type         = tmp_sub->i_subtitling_type;
				sub_info->subtitles[sub_info->subtitle_count].comp_page_id = tmp_sub->i_composition_page_id;
				sub_info->subtitles[sub_info->subtitle_count].anci_page_id = tmp_sub->i_ancillary_page_id;
				if (tmp_sub->i_iso6392_language_code[0] == 0)
				{
					sprintf(sub_info->subtitles[sub_info->subtitle_count].lang, "Subtitle%d", sub_info->subtitle_count+1);
				}
				else
				{
					memcpy(sub_info->subtitles[sub_info->subtitle_count].lang, tmp_sub->i_iso6392_language_code, 3);
					sub_info->subtitles[sub_info->subtitle_count].lang[3] = 0;
				}
				
				AM_DEBUG(0, "Add a subtitle: pid %d, language: %s", es->i_pid, sub_info->subtitles[sub_info->subtitle_count].lang);

				sub_info->subtitle_count++;
			}
		}
	AM_SI_LIST_END()

	return AM_SUCCESS;
}


AM_ErrorCode_t AM_SI_ExtractDVBTeletextFromES(dvbpsi_pmt_es_t *es, AM_SI_TeletextInfo_t *ttx_info)
{
	dvbpsi_descriptor_t *descr;
	
	AM_SI_LIST_BEGIN(es->p_first_descriptor, descr)
		if (descr->p_decoded && descr->i_tag == AM_SI_DESCR_TELETEXT)
		{
			int itel, i;
			dvbpsi_teletextpage_t *tmp_ttx;
			dvbpsi_teletextpage_t def_ttx;
			dvbpsi_teletext_dr_t *ptd = (dvbpsi_teletext_dr_t*)descr->p_decoded;

			memset(&def_ttx, 0, sizeof(def_ttx));
			def_ttx.i_teletext_magazine_number = 1;
			
			for (itel=0; itel<ptd->i_pages_number; itel++)
			{	
				if (ptd != NULL)
					tmp_ttx = &ptd->p_pages[itel];
				else
					tmp_ttx = &def_ttx;

				/* already added ? */
				for (i=0; i<ttx_info->teletext_count; i++)
				{
					if (es->i_pid                           == ttx_info->teletexts[i].pid &&
						tmp_ttx->i_teletext_type            == ttx_info->teletexts[i].type &&
						tmp_ttx->i_teletext_magazine_number == ttx_info->teletexts[i].magazine_no &&
						tmp_ttx->i_teletext_page_number     == ttx_info->teletexts[i].page_no &&
						! memcmp(tmp_ttx->i_iso6392_language_code, ttx_info->teletexts[i].lang, 3))
					{
						AM_DEBUG(1, "Skipping a exist teletext: pid %d, lang %c%c%c",
							es->i_pid, tmp_ttx->i_iso6392_language_code[0], 
							tmp_ttx->i_iso6392_language_code[1], 
							tmp_ttx->i_iso6392_language_code[2]);
						break;
					}
				}

				if (i < ttx_info->teletext_count)
					continue;
				
				if (ttx_info->teletext_count >= AM_SI_MAX_TTX_CNT)
				{
					AM_DEBUG(1, "Too many teletexts, Max count %d", AM_SI_MAX_TTX_CNT);
					return AM_SUCCESS;
				}
				
				if (ttx_info->teletext_count < 0)
					ttx_info->teletext_count = 0;
				
				ttx_info->teletexts[ttx_info->teletext_count].pid          = es->i_pid;
				ttx_info->teletexts[ttx_info->teletext_count].type         = tmp_ttx->i_teletext_type;
				ttx_info->teletexts[ttx_info->teletext_count].magazine_no  = tmp_ttx->i_teletext_magazine_number;
				ttx_info->teletexts[ttx_info->teletext_count].page_no      = tmp_ttx->i_teletext_page_number;
				if (tmp_ttx->i_iso6392_language_code[0] == 0)
				{
					sprintf(ttx_info->teletexts[ttx_info->teletext_count].lang, "Teletext%d", ttx_info->teletext_count+1);
				}
				else
				{
					memcpy(ttx_info->teletexts[ttx_info->teletext_count].lang, tmp_ttx->i_iso6392_language_code, 3);
					ttx_info->teletexts[ttx_info->teletext_count].lang[3] = 0;
				}
				
				AM_DEBUG(0, "Add a teletext: pid %d, language: %s", es->i_pid, ttx_info->teletexts[ttx_info->teletext_count].lang);

				ttx_info->teletext_count++;
			}
		}
	AM_SI_LIST_END()

	return AM_SUCCESS;
}

