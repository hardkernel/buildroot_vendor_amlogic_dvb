#include "atsc_types.h"
#include "atsc_descriptor.h"
#include "huffman_decode.h"

#define SHORT_NAME_LEN (14)

static unsigned char BMPtoChar(unsigned char codeBMP1,unsigned char codeBMP0)
{
	unsigned char codeChar;
	unsigned short int temp;
	if((codeBMP0)<=0x7f)
		codeChar = codeBMP0;
	else{
		temp = (codeBMP1&0x3)<<6;
		temp += codeBMP0&0x3f;
		codeChar = temp;
	}
	return codeChar;
}

INT32S short_channel_name_parse(INT8U* pstr, INT8U *out_str)
{
	INT8U i;
	for (i=0; i<(SHORT_NAME_LEN/2); i++)
	{
		out_str[i] = BMPtoChar(pstr[i*2], pstr[i*2+1]);
	}
	return 0;
}


void parse_multiple_string(unsigned char *buffer_data ,unsigned char *p_out_buffer)
{
	unsigned char *p;
	unsigned char number_strings;
	unsigned char i,j,k;
	unsigned long ISO_639_language_code;
	unsigned char number_segments;
	unsigned char compression_type;
	unsigned char mode;
	unsigned char number_bytes;
	unsigned char *tmp_buff;
	int str_bytes;

	tmp_buff = p_out_buffer;
	p = buffer_data;
	
	number_strings = p[0];
	if(0 == number_strings)
	{
		return;
	}
	p++;
	
	for (i=0; i< number_strings; i++) 
	{
		ISO_639_language_code = (p[0]<<16)|(p[1]<<8)|p[2];
		number_segments = p[3];
		
		if(0 == number_segments)
		{
			return;
		}
		
		p +=4;
		for (j=0; j< number_segments; j++) 
		{
			compression_type = p[0];	
			mode = p[1];

			number_bytes = p[2];

			if(0 == number_bytes)
			{
				return;
			}
			p+=3;
#if 1
			if ((mode == 0) && ((compression_type == 1) | (compression_type == 2)))
			{
				str_bytes = atsc_huffman_to_string(tmp_buff, p, number_bytes, compression_type);
				tmp_buff += str_bytes;
				p+=number_bytes;
			}
			else
#endif
			{
		    		if(NULL == tmp_buff)
				{
					AM_TRACE("No mem space!\n");
				}
				else
				{
					for (k=0; k< number_bytes; k++)
					{
						*tmp_buff = p[k];  // 
						tmp_buff++;	
					}
				}		
				p+=number_bytes;
			}
		}
	}	
}

void atsc_decode_multiple_string_structure(INT8U *buffer_data, atsc_multiple_string_t *out)
{
	unsigned char *p;
	unsigned char number_strings;
	unsigned char i,j,k;
	unsigned long ISO_639_language_code;
	unsigned char number_segments;
	unsigned char compression_type;
	unsigned char mode;
	unsigned char number_bytes;
	unsigned char *tmp_buff;
	int str_bytes;

	if (!buffer_data || !out)
		return;
		
	p = buffer_data;
	
	number_strings = p[0];
	if(0 == number_strings)
	{
		return;
	}
	p++;
	memset(out, 0, sizeof(atsc_multiple_string_t));
	out->i_string_count = number_strings;
	for (i=0; i< number_strings; i++) 
	{
		out->string[i].iso_639_code[0] = p[0];
		out->string[i].iso_639_code[1] = p[1];
		out->string[i].iso_639_code[2] = p[2];
		
		AM_TRACE("multiple_string_structure-->lang '%c%c%c'", p[0], p[1], p[2]);
		
		number_segments = p[3];
		
		if(0 == number_segments)
		{
			p += 4;
			continue;
		}
		
		p +=4;
		tmp_buff = out->string[i].string;
		for (j=0; j< number_segments; j++) 
		{
			compression_type = p[0];	
			mode = p[1];

			number_bytes = p[2];

			if(0 == number_bytes)
			{
				p += 3;
				continue;
			}
			p+=3;
#if 1
			if ((mode == 0) && ((compression_type == 1) | (compression_type == 2)))
			{
				str_bytes = atsc_huffman_to_string(tmp_buff, p, number_bytes, compression_type);
				tmp_buff += str_bytes;
				p+=number_bytes;
			}
			else
#endif
			{
				memcpy(tmp_buff, p, number_bytes);
				tmp_buff += number_bytes;
				p+=number_bytes;
			}
		}
	}	
}

INT8U *audio_stream_desc_parse(INT8U *ptrData)
{
	return NULL;
}

INT8U *caption_service_desc_parse(INT8U *ptrData)
{
	return NULL;
}

INT8U *content_advisory_desc_parse(INT8U *ptrData)
{
#if 0
	INT32U i;
	INT8U rating_region_count;
	INT8U rating_desc_length;
	INT8U rating_dimentions;
//	INT8U rating_region; 
	INT8U *ptr = ptrData;
	INT8U *str;
	
	rating_region_count  = RatingRegionCount(ptr);
	ptr += 3;
	for(i=0; i<rating_region_count; i++)
	{
		ptr++;
		rating_dimentions = *ptr;
		ptr += rating_dimentions * 2;

		rating_desc_length = *ptr;
		// test 
		ptr++;
		str = MemMalloc(rating_desc_length);
		parse_multiple_string(ptr, str);
		ptr += rating_desc_length;
		AM_TRACE("%s\n", str);
	}
#endif
	return NULL;
}


static atsc_service_location_dr_t* atsc_DecodeServiceLocationDr(atsc_descriptor_t * p_descriptor)
{
	atsc_service_location_dr_t * p_decoded;
	int i;

	/* Check the tag */
	if(p_descriptor->i_tag != 0xa1)
	{
		AM_TRACE("dr_service_location decoder,bad tag (0x%x)", p_descriptor->i_tag);
		return NULL;
	}

	/* Don't decode twice */
	if(p_descriptor->p_decoded)
		return p_descriptor->p_decoded;

	/* Allocate memory */
	p_decoded = (atsc_service_location_dr_t*)malloc(sizeof(atsc_service_location_dr_t));
	if(!p_decoded)
	{
		AM_TRACE("dr_service_location decoder,out of memory");
		return NULL;
	}

	/* Decode data and check the length */
	if((p_descriptor->i_length < 3))
	{
		AM_TRACE("dr_service_location decoder,bad length (%d)",
				         p_descriptor->i_length);
		free(p_decoded);
		return NULL;
	}
	
	p_decoded->i_elem_count = p_descriptor->p_data[2];
	p_decoded->i_pcr_pid = ((uint16_t)(p_descriptor->p_data[0] & 0x1f) << 8) | p_descriptor->p_data[1];
	i = 0;
	while( i < p_decoded->i_elem_count) 
	{
		
		p_decoded->elem[i].i_stream_type = p_descriptor->p_data[i*6+3];
		p_decoded->elem[i].i_pid = ((uint16_t)(p_descriptor->p_data[i*6+4] & 0x1f) << 8) | p_descriptor->p_data[i*6+5];
		p_decoded->elem[i].iso_639_code[0] = p_descriptor->p_data[i*6+6];
		p_decoded->elem[i].iso_639_code[1] = p_descriptor->p_data[i*6+7];
		p_decoded->elem[i].iso_639_code[2] = p_descriptor->p_data[i*6+8];
		AM_TRACE("ServiceLocationDesc: stream_type %d, pid %d", p_decoded->elem[i].i_stream_type, p_decoded->elem[i].i_pid);
		i++;
	}
	p_descriptor->p_decoded = (void*)p_decoded;

	return p_decoded;
}

static void atsc_decode_descriptor(atsc_descriptor_t *p_descriptor)
{
	switch (p_descriptor->i_tag)
	{
		case 0xA1:
			/*service location descriptor*/
			atsc_DecodeServiceLocationDr(p_descriptor);
			break;
		default:
			break;
	}
}


atsc_descriptor_t* atsc_NewDescriptor(uint8_t i_tag, uint8_t i_length, uint8_t* p_data)
{
	atsc_descriptor_t* p_descriptor
                = (atsc_descriptor_t*)malloc(sizeof(atsc_descriptor_t));

	if(p_descriptor)
	{
		p_descriptor->p_data = (uint8_t*)malloc(i_length * sizeof(uint8_t));

		if(p_descriptor->p_data)
		{
			p_descriptor->i_tag = i_tag;
			p_descriptor->i_length = i_length;
			if(p_data)
				memcpy(p_descriptor->p_data, p_data, i_length);
			p_descriptor->p_decoded = NULL;
			p_descriptor->p_next = NULL;

			/*Decode it*/
			atsc_decode_descriptor(p_descriptor);
		}
		else
		{
			free(p_descriptor);
			p_descriptor = NULL;
		}
	}

	return p_descriptor;
}

void atsc_DeleteDescriptors(atsc_descriptor_t* p_descriptor)
{
	while(p_descriptor != NULL)
	{ 
		atsc_descriptor_t* p_next = p_descriptor->p_next;

		if(p_descriptor->p_data != NULL)
			free(p_descriptor->p_data);

		if(p_descriptor->p_decoded != NULL)
			free(p_descriptor->p_decoded);

		free(p_descriptor);
		p_descriptor = p_next;
	}
}

