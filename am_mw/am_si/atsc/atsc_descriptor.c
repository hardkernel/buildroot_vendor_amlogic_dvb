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

