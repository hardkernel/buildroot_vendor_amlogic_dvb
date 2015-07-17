
#ifndef DVB_SUB_H
#define DVB_SUB_H

#include "includes.h"
#include "am_osd.h"
#include "am_sub2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef AM_OSD_Color_t    sub_clut_t;

typedef AM_SUB2_Region_t  sub_pic_region_t;

typedef AM_SUB2_Picture_t dvbsub_picture_t;

typedef void (*dvbsub_callback_t)(INT32U handle, dvbsub_picture_t *display);

INT32S dvbsub_decoder_create(INT16U composition_id, INT16U ancillary_id, dvbsub_callback_t cb, INT32U* handle);
INT32S dvbsub_decoder_destroy(INT32U handle);

INT32S dvbsub_parse_pes_packet(INT32U handle, const INT8U* packet, INT32U length);

dvbsub_picture_t* dvbsub_get_display_set(INT32U handle);
INT32S dvbsub_remove_display_picture(INT32U handle, dvbsub_picture_t* pic);

#ifdef __cplusplus
}
#endif

#endif

