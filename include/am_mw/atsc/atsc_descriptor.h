
#ifndef _ATSC_DESCRIPTOR_H
#define _ATSC_DESCRIPTOR_H

#include "atsc_types.h"

typedef struct 
{
	uint8_t		i_string_count;
	struct
	{
		uint8_t		iso_639_code[3];
		uint8_t		string[256];
	}string[90];
}atsc_multiple_string_t;

typedef struct atsc_descriptor_s
{
	uint8_t                       i_tag;          /*!< descriptor_tag */
	uint8_t                       i_length;       /*!< descriptor_length */

	uint8_t *                     p_data;         /*!< content */

	struct atsc_descriptor_s *  p_next;         /*!< next element of
		                                             the list */

	void *                        p_decoded;      /*!< decoded descriptor */

} atsc_descriptor_t;


/*ATSC Descriptors definition*/

/*Service Location*/
typedef struct
{
	uint8_t		i_elem_count;
	uint16_t	i_pcr_pid;
	struct 
	{
		uint8_t		i_stream_type;
		uint16_t	i_pid;
		uint8_t		iso_639_code[3];    /*!< ISO_639_language_code */
	} elem[44];

} atsc_service_location_dr_t;



atsc_descriptor_t* atsc_NewDescriptor(uint8_t i_tag, uint8_t i_length,
                                          uint8_t* p_data);
void atsc_DeleteDescriptors(atsc_descriptor_t* p_descriptor);

INT32S short_channel_name_parse(INT8U* pstr, INT8U *out_str);
void parse_multiple_string(unsigned char *buffer_data ,unsigned char *p_out_buffer);
void atsc_decode_multiple_string_structure(INT8U *buffer_data, atsc_multiple_string_t *out);

#endif /* end */
