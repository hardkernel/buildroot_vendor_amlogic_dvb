
#ifndef _ATSC_TVCT_H
#define _ATSC_TVCT_H

#include "atsc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define TVCT_SECTION_HEADER_LEN          (10)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

#pragma pack(1)

typedef struct tvct_section_header
{
	INT8U table_id                      	:8;
#if BYTE_ORDER == BIG_ENDIAN
    INT8U section_syntax_indicator	:1;
    INT8U private_indicator 		:1;
    INT8U                               		:2;
    INT8U section_length_hi             :4;
#else
    INT8U section_length_hi             :4;
    INT8U                               		:2;
    INT8U private_indicator 		:1;
    INT8U section_syntax_indicator	:1;
#endif
    INT8U section_length_lo             :8;
    INT8U transport_stream_id_hi	:8;
    INT8U transport_stream_id_lo	:8;
#if BYTE_ORDER == BIG_ENDIAN    
    INT8U                               		:2;
    INT8U version_number         	:5;
    INT8U current_next_indicator 	:1;
#else
    INT8U current_next_indicator	:1;
    INT8U version_number         	:5;
    INT8U                               		:2;
#endif
    INT8U section_number                :8;
    INT8U last_section_number		:8;
    INT8U protocol_version                :8;
    INT8U num_channels_in_section	:8;

}tvct_section_header_t;

typedef struct tvct_sect_chan_info
{
    INT8U short_name[14];
#if BYTE_ORDER == BIG_ENDIAN
    INT8U 				:4;
    INT8U major_channel_number_hi  :4;
#else
    INT8U major_channel_number_hi  :4;
    INT8U 				:4;
#endif
#if BYTE_ORDER == BIG_ENDIAN
    INT8U major_channel_number_lo	:6;
    INT8U minor_channel_number_hi 	:2;
#else
    INT8U minor_channel_number_hi	:2;
    INT8U major_channel_number_lo 	:6;
#endif	
    INT8U minor_channel_number_lo     :8;
    INT8U modulation_mode 			:8;
    INT8U carrier_frequency_hi			:8;
    INT8U carrier_frequency_mh			:8;
    INT8U carrier_frequency_ml			:8;
    INT8U carrier_frequency_lo			:8;
    INT8U channel_TSID_hi			:8;
    INT8U channel_TSID_lo			:8;
    INT8U program_number_hi		:8;
    INT8U program_number_lo		:8;
#if BYTE_ORDER == BIG_ENDIAN
   INT8U  ETM_location		:2;
   INT8U  access_controlled	:1;
   INT8U  hidden			:1;
   INT8U  					:2;
   INT8U  hide_guide		:1;
   INT8U  					:1;
#else
   INT8U  					:1;
   INT8U  hide_guide		:1;
   INT8U  					:2;
   INT8U  hidden			:1;
   INT8U  access_controlled	:1;
   INT8U  ETM_location		:2;
#endif
#if BYTE_ORDER == BIG_ENDIAN
    INT8U  			:2;
    INT8U service_type   :6;
#else
    INT8U service_type   :6;    
    INT8U  			:2;
#endif
    INT8U source_id_hi :8;
    INT8U source_id_lo 	:8;
#if BYTE_ORDER == BIG_ENDIAN
    INT8U 			:6;
    INT8U  descriptors_length_hi  :2;
#else
    INT8U  descriptors_length_hi  :2;
    INT8U 			:6;
#endif
    INT8U descriptors_length_lo 	:8;
}tvct_sect_chan_info_t;

#pragma pack()

typedef struct tvct_channel_info
{
    INT8U short_name[14];
    INT16U major_channel_number;
    INT16U minor_channel_number;
    INT8U modulation_mode;
    INT32U carrier_frequency;
    INT16U channel_TSID;
    INT16U program_number;
    INT8U access_controlled;
    INT8U service_type;
    INT16U source_id;
    INT8U hidden;
    INT8U hide_guide;
    struct descriptor *desc;                // --
    struct tvct_channel_info *p_next;
}tvct_channel_info_t;

typedef struct tvct_section_info
{
    struct tvct_section_info *p_next;
	INT8U i_table_id;
    INT16U transport_stream_id;
    INT8U  version_number;
    INT8U  num_channels_in_section;
    struct tvct_channel_info  *vct_chan_info;
}tvct_section_info_t;

/*****************************************************************************
 * Function prototypes	
 *****************************************************************************/

INT32S atsc_psip_parse_tvct(INT8U* data, INT32U length, tvct_section_info_t *info);
void   atsc_psip_clear_tvct_info(tvct_section_info_t *info);

tvct_section_info_t *atsc_psip_new_tvct_info(void);
void   atsc_psip_free_tvct_info(tvct_section_info_t *info);

void   atsc_psip_dump_tvct_info(tvct_section_info_t *info);

#ifdef __cplusplus
}

#endif

#endif
