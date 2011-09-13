
#ifndef _ATSC_DESCRIPTOR_H
#define _ATSC_DESCRIPTOR_H

INT32S short_channel_name_parse(INT8U* pstr, INT8U *out_str);
void parse_multiple_string(unsigned char *buffer_data ,unsigned char *p_out_buffer);
INT8U *audio_stream_desc_parse(INT8U *ptrData);
INT8U *caption_service_desc_parse(INT8U *ptrData);
INT8U *content_advisory_desc_parse(INT8U *ptrData);

#endif /* end */
