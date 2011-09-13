
#ifndef _ATSC_RRT_H
#define _ATSC_RRT_H

#include "atsc_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define RRT_SECTION_HEADER_LEN          (9)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

#pragma pack(1)

typedef struct rrt_section_header
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
    INT8U 							:8;
    INT8U rating_region			:8;
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
}rrt_section_header_t;

#pragma pack()

typedef struct rrt_rating_value
{
    INT8U abbrev_rating_value_text[8];
    INT8U *rating_value_text;
}rrt_rating_value_t;

typedef struct rrt_dimensions_info
{
    INT8U dimensions_name[20];
    INT8U values_defined;
    struct rrt_rating_value rating_value[16];	
    struct rrt_dimensions_info *p_next;
}rrt_dimensions_info_t;

typedef struct rrt_section_info
{
	struct rrt_section_info *p_next;
	INT8U i_table_id;
    INT16U rating_region;
    INT8U  version_number;
    INT8U  dimensions_defined;
    INT8U  rating_region_name[32];
    struct rrt_dimensions_info  *dimensions_info;
    struct descriptor *desc;          
}rrt_section_info_t;

/*****************************************************************************
 * Function prototypes	
 *****************************************************************************/

INT32S atsc_psip_parse_rrt(INT8U* data, INT32U length, rrt_section_info_t *info);
void   atsc_psip_clear_rrt_info(rrt_section_info_t *info);

rrt_section_info_t *atsc_psip_new_rrt_info(void);
void   atsc_psip_free_rrt_info(rrt_section_info_t *info);

void   atsc_psip_dump_rrt_info(rrt_section_info_t *info);

#ifdef __cplusplus
}

#endif

#endif
