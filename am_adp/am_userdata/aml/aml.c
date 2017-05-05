#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2013 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief aml user data driver
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2013-3-13: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <am_debug.h>
#include <am_mem.h>
#include "../am_userdata_internal.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
/****************************************************************************
 * Type definitions
 ***************************************************************************/
#define AMSTREAM_IOC_MAGIC  'S'
//#define AMSTREAM_IOC_UD_PICTYPE _IOR(AMSTREAM_IOC_MAGIC, 0x55, unsigned long)
#define AMSTREAM_IOC_UD_LENGTH _IOR(AMSTREAM_IOC_MAGIC, 0x54, unsigned long)
#define AMSTREAM_IOC_UD_POC _IOR(AMSTREAM_IOC_MAGIC, 0x55, unsigned long)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA _IOR(AMSTREAM_IOC_MAGIC, 0x56, int)

#define DMA_BLOCK_LEN 8
#define INVALID_POC 0xFFFFFFF

#define USE_MAX_CACHE 1
#define MAX_POC_CACHE 5

enum user_data_type {
	INVALID_TYPE 	= 0,
	MPEG_CC_TYPE 	= 1,
	H264_CC_TYPE 	= 2,
	DIRECTV_CC_TYPE = 3,
	AVS_CC_TYPE 	=4
};

enum picture_coding_type {
	/* 0 forbidden */
	I_TYPE = 1,
	P_TYPE = 2,
	B_TYPE = 3,
	D_TYPE = 4,
	/* 5 ... 7 reserved */
};

enum picture_structure {
	/* 0 reserved */
	TOP_FIELD = 1,
	BOTTOM_FIELD = 2,
	FRAME_PICTURE = 3
};

typedef struct
{
     unsigned int poc_info;
     unsigned int poc_number;
}userdata_poc_info_t;

typedef struct
{
	int fd;
	AM_Bool_t running;
	pthread_t thread;

	AM_USERDATA_Device_t *dev;
}aml_ud_drv_data_t;

struct aml_ud_reorder {
	/* Parameters of the current picture. */
	enum picture_coding_type	picture_coding_type;
	enum picture_structure		picture_structure;
	unsigned int			picture_temporal_reference;

	/* Describes the contents of the reorder_buffer[]:
	   Bit 0 - a top field in reorder_buffer[0],
	   Bit 1 - a bottom field in reorder_buffer[1],
	   Bit 2 - a frame in reorder_buffer[0]. Only the
	   combinations 0, 1, 2, 3, 4 are valid. */
	unsigned int			reorder_pictures;

	unsigned int			reorder_n_bytes[2];

	/* Buffer to convert picture user data from coded
	   order to display order, for the top and bottom
	   field. Maximum size required: 11 + cc_count * 3,
	   where cc_count = 0 ... 31. */
	uint8_t				reorder_buffer[2][128];

	aml_ud_drv_data_t *drv_data;
};
struct aml_cc_block_s
{
	int 	i_poc;
	int 	i_pic_type;
	int 	i_len;
	uint8_t * p_buf;
	struct aml_cc_block_s * p_prev;
	struct aml_cc_block_s * p_next;
};
typedef struct aml_cc_block_s aml_cc_block_t;

/****************************************************************************
 * Static data definitions
 ***************************************************************************/

static AM_ErrorCode_t aml_open(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para);
static AM_ErrorCode_t aml_close(AM_USERDATA_Device_t *dev);

const AM_USERDATA_Driver_t aml_ud_drv = {
.open  = aml_open,
.close = aml_close,
};


/****************************************************************************
 * Static functions
 ***************************************************************************/

static void aml_reorder_decode_cc_data	(struct aml_ud_reorder *vd,
				 const uint8_t *	buf,
				 unsigned int		n_bytes)
{
	aml_ud_drv_data_t *drv_data = vd->drv_data;
	AM_USERDATA_Device_t *dev = drv_data->dev;

	n_bytes = AM_MIN (n_bytes, (unsigned int)
		       sizeof (vd->reorder_buffer[0]));

	switch (vd->picture_structure) {
	case FRAME_PICTURE:
		AM_DEBUG(2, "FRAME_PICTURE, 0x%02x", vd->reorder_pictures);
		if (0 != vd->reorder_pictures) {
			if (vd->reorder_pictures & 5) {
				/* Top field or top and bottom field. */
				dev->write_package(dev,
						vd->reorder_buffer[0],
						vd->reorder_n_bytes[0]);
			}
			if (vd->reorder_pictures & 2) {
				/* Bottom field. */
				dev->write_package(dev,
						vd->reorder_buffer[1],
						vd->reorder_n_bytes[1]);
			}
		}

		memcpy (vd->reorder_buffer[0], buf, n_bytes);
		vd->reorder_n_bytes[0] = n_bytes;

		/* We have a frame. */
		vd->reorder_pictures = 4;

		break;

	case TOP_FIELD:
		AM_DEBUG(2, "TOP_FIELD, 0x%02x", vd->reorder_pictures);
		if (vd->reorder_pictures >= 3) {
			/* Top field or top and bottom field. */
			dev->write_package(dev, vd->reorder_buffer[0], vd->reorder_n_bytes[0]);

			vd->reorder_pictures &= 2;
		} else if (1 == vd->reorder_pictures) {
			/* Apparently we missed a bottom field. */
		}

		memcpy (vd->reorder_buffer[0], buf, n_bytes);
		vd->reorder_n_bytes[0] = n_bytes;

		/* We have a top field. */
		vd->reorder_pictures |= 1;

		break;

	case BOTTOM_FIELD:
		AM_DEBUG(2, "BOTTOM_FIELD, 0x%02x", vd->reorder_pictures);
		if (vd->reorder_pictures >= 3) {
			if (vd->reorder_pictures >= 4) {
				/* Top and bottom field. */
				dev->write_package(dev,
						vd->reorder_buffer[0],
						vd->reorder_n_bytes[0]);
			} else {
				/* Bottom field. */
				dev->write_package(dev,
						vd->reorder_buffer[1],
						vd->reorder_n_bytes[1]);
			}

			vd->reorder_pictures &= 1;
		} else if (2 == vd->reorder_pictures) {
			/* Apparently we missed a top field. */
		}

		memcpy (vd->reorder_buffer[1], buf, n_bytes);
		vd->reorder_n_bytes[1] = n_bytes;

		/* We have a bottom field. */
		vd->reorder_pictures |= 2;

		break;

	default: /* invalid */
		break;
	}
}
static int aml_h264_get_min_poc(aml_cc_block_t * p_header)
{
	int min_poc = INVALID_POC;
	aml_cc_block_t * p_block = p_header->p_next;
	while(p_block != NULL)
	{
		min_poc = min_poc<p_block->i_poc?min_poc:p_block->i_poc;
		p_block = p_block->p_next;
	}
	return min_poc;
}
static int aml_h264_get_poc_num(aml_cc_block_t * p_header)
{
	int num = 0;
	aml_cc_block_t * p_block = p_header->p_next;
	while(p_block != NULL)
	{
		num++;
		p_block = p_block->p_next;
	}
	return num;
}
static int aml_h264_get_max_poc(aml_cc_block_t * p_header)
{
	int max_poc = 0;
	aml_cc_block_t * p_block = p_header->p_next;
	while(p_block != NULL)
	{
		max_poc = max_poc>p_block->i_poc?max_poc:p_block->i_poc;
		p_block = p_block->p_next;
	}
	return max_poc;
}
static void aml_h264_userdata_del(aml_cc_block_t * p_header)
{
	aml_cc_block_t * p_block , * p_next_block;
	for(p_block = p_header; p_block != NULL; p_block = p_next_block)
	{
		p_next_block = p_block->p_next;
		if(p_block->p_buf)
		{
			free(p_block->p_buf);
			p_block->p_buf = NULL;
			p_block->i_len = 0;
		}
		p_block->p_next = NULL;
		p_block->p_prev = NULL;
		free(p_block);
		p_block = NULL;
	}
	p_header = NULL;
}
static void aml_h264_userdata_add(aml_cc_block_t * p_header,
	 int poc, int pic_type, unsigned int min_bytes_valid, const uint8_t * buf)
{
	aml_cc_block_t * p_prev_block = NULL, * p_block = NULL;
	for(p_block = p_header; p_block != NULL; p_block = p_block->p_next)
	{
		p_prev_block = p_block;
	}

	p_block = malloc(sizeof(aml_cc_block_t));
	p_block->i_poc = poc;
	p_block->i_pic_type = pic_type;
	p_block->i_len = min_bytes_valid;
	p_block->p_buf = malloc(p_block->i_len);
	memcpy(p_block->p_buf, buf, min_bytes_valid);
	p_block->p_prev = p_prev_block;
	p_block->p_next = NULL;
	p_prev_block->p_next = p_block;

}
static void aml_h264_userdata_push(AM_USERDATA_Device_t *dev,
	aml_cc_block_t * p_header, int min_poc)
{
	int debug_flag = 0;
	aml_cc_block_t * p_block = p_header->p_next;

    if (dev == NULL || dev->write_package == NULL)
		return;

	while(p_block != NULL)
	{
		if(min_poc == p_block->i_poc)
		{
			debug_flag = 1;
			break;
		}
		p_block = p_block->p_next;
	}
	if(p_block->p_buf == NULL)
	{
		//AM_DEBUG(1, "####aml_h264_userdata_push error write pBUff is NULL ");
	}
	//AM_DEBUG(1, "####push poc:%d, len:%d, %02x, %02x, %02x, %02x",
		//p_block->i_poc, p_block->i_len, p_block->p_buf[0], p_block->p_buf[1], p_block->p_buf[2], p_block->p_buf[3]);

	dev->write_package(dev, p_block->p_buf, p_block->i_len);
	if(p_block->p_prev != NULL)
	{
		p_block->p_prev->p_next = p_block->p_next;
	}
	else
	{
		//AM_DEBUG(1, "####aml_h264_userdata_push error p_block->p_prev is NULL ");
	}
	if(p_block->p_next != NULL)
	{
		p_block->p_next->p_prev = p_block->p_prev;
	}
	else
	{
		//AM_DEBUG(1, "####aml_h264_userdata_push error p_block->p_next is NULL ");
	}

	if(p_block->p_buf) free(p_block->p_buf);
	if(p_block) free(p_block);
}

static void aml_h264_reorder_user_data			(struct aml_ud_reorder *vd,
				 const uint8_t *	buf,
				 unsigned int		min_bytes_valid,
				 int poc,
				 aml_cc_block_t * 	p_header)
{
	aml_ud_drv_data_t *drv_data = vd->drv_data;
	AM_USERDATA_Device_t *dev = drv_data->dev;
	int max_poc, min_poc, poc_num;


	if(poc == 0)
	{
		if(p_header->p_next == NULL)
		{
			aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
		}
		else
		{
			while(p_header->p_next != NULL)
			{
				min_poc = aml_h264_get_min_poc(p_header);
				aml_h264_userdata_push(dev, p_header, min_poc);
			}
			aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
		}
	}
	else
	{
		#if USE_MAX_CACHE
			poc_num = aml_h264_get_poc_num(p_header);
			if(poc_num > MAX_POC_CACHE)
			{
				min_poc = aml_h264_get_min_poc(p_header);
				if(min_poc != INVALID_POC) aml_h264_userdata_push(dev, p_header, min_poc);
				aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
			}
			else
			{
				aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
			}
		#else
			max_poc = aml_h264_get_max_poc(p_header);
			min_poc = aml_h264_get_min_poc(p_header);
			if(poc > max_poc && max_poc != INVALID_POC && min_poc != INVALID_POC)
			{
				aml_h264_userdata_push(dev, p_header, min_poc);
				aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
			}
			else
			{
				aml_h264_userdata_add(p_header, poc, vd->picture_coding_type, min_bytes_valid, buf);
			}
		#endif
	}
}

static void aml_reorder_user_data			(struct aml_ud_reorder *vd,
				 const uint8_t *	buf,
				 unsigned int		min_bytes_valid)
{
	aml_ud_drv_data_t *drv_data = vd->drv_data;
	AM_USERDATA_Device_t *dev = drv_data->dev;

	/* CEA 708-C Section 4.4.1.1 */

	switch (vd->picture_coding_type) {
	case I_TYPE:
	case P_TYPE:
		AM_DEBUG(2, "%s, 0x%02x", vd->picture_coding_type==I_TYPE ? "I_TYPE" : "P_TYPE", vd->reorder_pictures);
		aml_reorder_decode_cc_data (vd, buf, min_bytes_valid);
		break;

	case B_TYPE:
		AM_DEBUG(2, "B_TYPE, 0x%02x", vd->reorder_pictures);
		/* To prevent a gap in the caption stream we must not
		   decode B pictures until we have buffered both
		   fields of the temporally following I or P picture. */
		if (vd->reorder_pictures < 3) {
			vd->reorder_pictures = 0;
			break;
		}

		/* To do: If a B picture appears to have a higher
		   temporal_reference than the picture it forward
		   references we lost that I or P picture. */
		{
			dev->write_package(dev,  buf,
					min_bytes_valid);
		}

		break;

	default: /* invalid */
		break;
	}
}

typedef struct
{
	uint32_t picture_structure:16;
	uint32_t temporal_reference:10;
	uint32_t picture_coding_type:3;
	uint32_t reserved:3;

	uint32_t index:16;
	uint32_t offset:16;

	uint8_t cc_data_start[4];

	uint32_t atsc_id;
}aml_ud_header_t;

static int aml_extract_package(uint8_t *user_data, int ud_size, uint8_t *cc_data, uint8_t *cc_size, aml_ud_header_t *pkg_header)
{
	uint8_t *p = user_data;
	uint8_t cs = 0, copied;
	int atsc_id_count = 0;
	int left_size = ud_size;
	aml_ud_header_t *uh;

#define COPY_4_BYTES(_p)\
	AM_MACRO_BEGIN\
		cc_data[cs++] = (_p)[3];\
		cc_data[cs++] = (_p)[2];\
		cc_data[cs++] = (_p)[1];\
		cc_data[cs++] = (_p)[0];\
	AM_MACRO_END

	*cc_size = 0;
	if (ud_size < 12)
		return left_size;

	while (left_size >= (int)sizeof(aml_ud_header_t))
	{
		uh = (aml_ud_header_t*)p;
		if (uh->atsc_id == 0x47413934 && uh->cc_data_start[3] == 0x3)//only process 0x3(ATSC CC)
		{
			if (atsc_id_count == 0)
			{
				atsc_id_count++;

				COPY_4_BYTES((uint8_t*)&uh->atsc_id);
				COPY_4_BYTES(uh->cc_data_start);

				left_size -= sizeof(aml_ud_header_t);
				p += sizeof(aml_ud_header_t);

				*pkg_header = *uh;

				AM_DEBUG(2, "package index %d", uh->index);
				continue;
			}
			else if (atsc_id_count >= 1)
			{
				atsc_id_count++;
				AM_DEBUG(2, "got next package index %d", uh->index);
				break;
			}
		}

		if (atsc_id_count == 1)
		{
			/* Copy cc data */
			copied = cs;
			if (left_size >= (int)(sizeof(aml_ud_header_t) + 4))
			{
				uh = (aml_ud_header_t*)(p+4);
				if (uh->atsc_id != 0x47413934)
					COPY_4_BYTES(p+4);
			}

			COPY_4_BYTES(p);

			copied = cs - copied;
			left_size -= copied;
			p += copied;
			continue;
		}

		AM_DEBUG(1, "Warning: skip 4 bytes data %02x %02x %02x %02x", p[0], p[1], p[2], p[3]);
		left_size -= 4;
		p += 4;
	}

	if (atsc_id_count < 2)
	{
		cs = 0;
		left_size = ud_size;
	}

	*cc_size = cs;

	return left_size;
}

static void dump_cc_data(uint8_t *buff, int size)
{
	AM_DEBUG(1,  "-------------------------------------------\n");
	AM_DEBUG(1,  "read %d bytes data:\n", size);

	int i = 0;
	for (i=0; i<size; )
	{
		AM_DEBUG(1, "%02x %02x %02x %02x", buff[i], buff[i+1], buff[i+2], buff[i+3]);
		i += 4;
	}
	AM_DEBUG(1,  "-------------------------------------------\n");
}
static void aml_swap_data(uint8_t *user_data, int ud_size)
{
	int swap_blocks, i, j, k, m;
    unsigned char c_temp;

	/* swap byte order */
    swap_blocks = ud_size / 8;
    for (i=0; i<swap_blocks; i++) {
        j = i * 8;
        k = j + 7;
        for (m=0; m<4; m++) {
            c_temp = user_data[j];
            user_data[j++] = user_data[k];
            user_data[k--] = c_temp;
        }
    }
}
static int aml_check_userdata_format(uint8_t *buf, int len)
{
	AM_DEBUG(1,"check format len:%d", len);
	if(len <= 0)
		return INVALID_TYPE;
	uint8_t *tmp_buf = (uint8_t *)malloc(len);
	if(tmp_buf == NULL)
		return INVALID_TYPE;
	memcpy(tmp_buf, buf, len);
	aml_swap_data(tmp_buf, len);
	if(tmp_buf[0] == 0xb5 && tmp_buf[3] == 0x47 && tmp_buf[4] == 0x41
			&& tmp_buf[5] == 0x39 && tmp_buf[6] == 0x34)
	{
		AM_DEBUG(1,"@check format is h264_cc_type");
		free(tmp_buf);
		return H264_CC_TYPE;
	}
	else if(tmp_buf[0] == 0xb5 && tmp_buf[1] == 0x00 && tmp_buf[2] == 0x2f)
	{
		AM_DEBUG(1,"@check format is directv_cc_type");
		free(tmp_buf);
		return DIRECTV_CC_TYPE;
	}
	else if(tmp_buf[0] == 0x47 && tmp_buf[1] == 0x41 && tmp_buf[2] == 0x39 && tmp_buf[3] == 0x34)
	{
		AM_DEBUG(1,"@check format is avs_cc_type");
		free(tmp_buf);
		return AVS_CC_TYPE;
	}
	else
	{
		AM_DEBUG(1,"@check format is mpeg_cc_type");
		free(tmp_buf);
		return MPEG_CC_TYPE;
	}

}
static unsigned int aml_get_pic_type(unsigned int slice_type)
{
	unsigned int pic_type = 0;
	switch( slice_type&0xFF )
    {
	    case 0: case 5:
	        pic_type = P_TYPE;
	        break;
	    case 1: case 6:
	        pic_type = B_TYPE;
	        break;
	    case 2: case 7:
	        pic_type = I_TYPE;
	        break;
	    case 3: case 8: // SP
	        pic_type = P_TYPE;
	        break;
	    case 4: case 9:
	        pic_type = I_TYPE;
	        break;
	    default:
	        pic_type = I_TYPE;
	        break;
    }
    AM_DEBUG(3, "get slice_type:%d, slice_id:%d, pic_type:%d", slice_type&0xFF, slice_type>>8, pic_type);
    return pic_type;
}
int vbi_to_ascii(int c)
{
     if (c < 0)
        return '?';

     c &= 0x7F;

     if (c < 0x20 || c >= 0x7F)
         return '.';

     return c;
}

static void dump_cc_ascii(const uint8_t *buf, int poc)
{
    int cc_flag;
    int cc_count;
    int i;

    cc_flag = buf[1] & 0x40;
	if (!cc_flag)
    {
        AM_DEBUG(0, "### cc_flag is invalid");
        return;
    }
    cc_count = buf[1] & 0x1f;

	for (i = 0; i < cc_count; ++i)
    {
        unsigned int b0;
        unsigned int cc_valid;
        unsigned int cc_type;
        unsigned char cc_data1;
        unsigned char cc_data2;

        b0 = buf[3 + i * 3];
        cc_valid = b0 & 4;
        cc_type = b0 & 3;
        cc_data1 = buf[4 + i * 3];
        cc_data2 = buf[5 + i * 3];


		if (cc_type == 0 ) //NTSC pair, Line 21
        {
            AM_DEBUG(0, "### get poc:%d, cc data: %c %c", poc, vbi_to_ascii(cc_data1), vbi_to_ascii(cc_data2));
			if (!cc_valid || i >= 3)
                break;
        }
    }
}
static void *aml_userdata_thread(void *arg)
{
	AM_USERDATA_Device_t *dev = (AM_USERDATA_Device_t*)arg;
	aml_ud_drv_data_t *drv_data = (aml_ud_drv_data_t*)dev->drv_data;
	uint8_t buf[5*1024];
	uint8_t *p;
	uint8_t cc_data[256];/*In fact, 99 is enough*/
	uint8_t cc_data_cnt;
	int fd, ud_format = INVALID_TYPE, poc;
	unsigned int cnt, left, min_bytes_valid = 0;
	struct pollfd fds;
	aml_ud_header_t pheader;
	struct aml_ud_reorder reorder;
	int last_pkg_idx = -1;
	unsigned int slice_type, pic_type;
	aml_cc_block_t * 	p_header;
	int first_check = 0;
	userdata_poc_info_t poc_block;
	int flush = 0;

    AM_DEBUG(1, "user data thread start.");

	fd = drv_data->fd;

	left = 0;

	memset(&reorder, 0, sizeof(reorder));
	memset(&pheader, 0, sizeof(pheader));
	memset(buf, 0, sizeof(buf));

	p_header = malloc(sizeof(aml_cc_block_t));
	p_header->i_poc = INVALID_POC;
	p_header->p_prev = p_header;
	p_header->p_next = NULL;
	p_header->p_buf = NULL;
	p_header->i_len = 0;

	while (drv_data->running)
	{
		fds.events = POLLIN|POLLERR;
		fds.fd     = fd;
		if (poll(&fds, 1, 100) > 0)
		{
			cnt = read(fd, buf+left, sizeof(buf)-left);

			if (flush == 0)
			{
				int ret = ioctl(fd, AMSTREAM_IOC_UD_FLUSH_USERDATA, NULL);
				AM_DEBUG(1, "==try to flush userdata cache:%d, cache_cnt", ret);
				flush = 1;
				continue;
			}


			while(cnt > 0 && ud_format == INVALID_TYPE && drv_data->running)
			{
				ud_format = aml_check_userdata_format(buf, cnt);
			}
			if (cnt > 0)
			{
				AM_DEBUG(3, "read %d bytes user_data", cnt);
				if(ud_format == MPEG_CC_TYPE)
				{
					cnt += left;
					do
					{
						/* try to read a package */
						left = aml_extract_package(buf, cnt, cc_data, &cc_data_cnt, &pheader);
						if (cc_data_cnt > 0)
						{
							if ((last_pkg_idx + 1) != pheader.index)
							{
								AM_DEBUG(1, "Warning: package index discontinue, %d->%d",
									last_pkg_idx, pheader.index);
							}

							reorder.drv_data = drv_data;
							reorder.picture_coding_type = pheader.picture_coding_type;
							reorder.picture_structure = pheader.picture_structure;
							reorder.picture_temporal_reference = pheader.temporal_reference;

							aml_reorder_user_data(&reorder, cc_data, cc_data_cnt);

							last_pkg_idx = pheader.index;
						}

						if (left < cnt && left > 0)
						{
							/* something has been read */
							memmove(buf, buf+cnt-left, left);
						}
						cnt = left;
					}while(cc_data_cnt > 0 && cnt > 0);
				}
				else if(ud_format == H264_CC_TYPE || ud_format == DIRECTV_CC_TYPE || ud_format == AVS_CC_TYPE)
				{
					cnt += left;
					left = 0;
					aml_swap_data(buf, cnt);
					p = buf;
					while (cnt > 0 && p !=NULL && cnt < sizeof(buf))
					{
                        if (cnt > DMA_BLOCK_LEN)
						{
							if ( (p[0] == 0xb5 && p[3] == 0x47 && p[4] == 0x41 && p[5] == 0x39 && p[6] == 0x34)
								|| (p[0] == 0xb5 && p[1] == 0x0 && p[2] == 0x2f)
								|| (p[0] == 0x47 && p[1] == 0x41 && p[2] == 0x39 && p[3] == 0x34)
							   )
							{

								if (ud_format == H264_CC_TYPE)
									min_bytes_valid = 11 + (p[8] & 0x1F)*3;
								else if(ud_format == DIRECTV_CC_TYPE)
									min_bytes_valid = 8 + (p[5] & 0x1F)*3;
								else
								{
									min_bytes_valid = 8 + (p[5] & 0x1F)*3; //wait avs cc spec
								}
								if (cnt >= min_bytes_valid)
								{
									memset(&poc_block, 0, sizeof(userdata_poc_info_t));
									slice_type = 0;
									ioctl(fd, AMSTREAM_IOC_UD_POC, &poc_block);
									/*
									 *	通常本地播放从pos=0开始播放, 都能产生正确的poc-userdata配对, 但是如果Seek, 或者DTV播放, 这样的场景
									 *	我们的驱动会产生错误的poc, 需要丢弃当前的数据以及poc, 否则cc会乱码. poc_info 可以计算出当前的poc是
									 *  kernel送出来的第几帧poc, 如果发现开始播放第一次取出来的poc并不是第一帧poc, 那么就要丢弃这个poc,
									 * 	并且cc数据也要一起丢弃.
									 */
									#if 0
									//driver 增加flush机制, 弃用下面check code.
									if (first_check == 0)
									{
										unsigned int driver_userdata_pkt_len = min_bytes_valid;
										while (driver_userdata_pkt_len%DMA_BLOCK_LEN)
											driver_userdata_pkt_len++;
										AM_DEBUG(1, "read cc cnt:%d, min_bytes_valid:%d, pkt_len:%d", cnt, min_bytes_valid, driver_userdata_pkt_len);
										AM_DEBUG(1,"@@@ check_first pocinfo:%#x, poc:%d ,jump %d package\n",
											poc_block.poc_info&0xFFFF, poc_block.poc_number,
											(poc_block.poc_info&0xFFFF)/min_bytes_valid);
										if (cnt > driver_userdata_pkt_len && (poc_block.poc_info&0xFFFF) > min_bytes_valid)
										{

											AM_DEBUG(1,"break, pocinfo:%#x, poc:%d ,jump %d package\n",
												poc_block.poc_info&0xFFFF, poc_block.poc_number,
												(poc_block.poc_info&0xFFFF)/min_bytes_valid);
											cnt = 0;
											break;
										}
										first_check = 1;
									}
									#endif

								    reorder.drv_data = drv_data;
									reorder.picture_coding_type = 0;
									reorder.picture_structure = FRAME_PICTURE;
									reorder.picture_temporal_reference = 2;
									if (cnt >= min_bytes_valid) {
										#ifdef DEBUG_CC_ENABLE
										dump_cc_ascii(p+3+4, poc_block.poc_number);
										#endif
										if (ud_format == H264_CC_TYPE)
											aml_h264_reorder_user_data(&reorder, p+3, min_bytes_valid-3, poc_block.poc_number, p_header);//skip 0xb5 0x00 0x31
										else if(ud_format == DIRECTV_CC_TYPE)
											aml_h264_reorder_user_data(&reorder, p, min_bytes_valid, poc_block.poc_number, p_header);
										else if(ud_format == AVS_CC_TYPE){
										//dump_cc_ascii(p+4, poc_block.poc_number);
											aml_h264_reorder_user_data(&reorder, p, min_bytes_valid, poc_block.poc_number, p_header);
											}
									}

									while (min_bytes_valid%DMA_BLOCK_LEN)
										min_bytes_valid++;
									cnt -= min_bytes_valid;
									p += min_bytes_valid;
									continue;
								}
								else
								{
									left = cnt;
									memmove(buf, p, left);
									aml_swap_data(buf, left);
									AM_DEBUG(1, "####cnt:%d, min_bytes:%d, memmove %d buf", cnt, min_bytes_valid, left);
									break;
								}
							}
							cnt -= DMA_BLOCK_LEN;
							p += DMA_BLOCK_LEN;
						}
						else if(cnt > 0)
						{
							left = cnt;
							memmove(buf, p, left);
							aml_swap_data(buf, left);
								AM_DEBUG(1, "@@@@memmove %d buf", left);
							break;
						}
					}
				}
			}
		}
	}

	aml_h264_userdata_del(p_header);
	AM_DEBUG(1, "user data thread exit now");
	return NULL;
}

static AM_ErrorCode_t aml_open(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para)
{
	char dev_name[64];
	int fd, rc;
	aml_ud_drv_data_t *drv_data;

	UNUSED(para);

	drv_data = (aml_ud_drv_data_t*)malloc(sizeof(aml_ud_drv_data_t));
	if (drv_data == NULL)
	{
		return AM_USERDATA_ERR_NO_MEM;
	}

	snprintf(dev_name, sizeof(dev_name), "/dev/amstream_userdata");
	fd = open(dev_name, O_RDONLY);
	if (fd == -1)
	{
		AM_DEBUG(1, "cannot open \"%s\" (%s)", dev_name, strerror(errno));
		return AM_USERDATA_ERR_CANNOT_OPEN_DEV;
	}
	//fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK, 0);

	drv_data->fd = fd;
	drv_data->running = AM_TRUE;
	drv_data->dev = dev;

	dev->drv_data = (void*)drv_data;

	/* start the user data thread */
	rc = pthread_create(&drv_data->thread, NULL, aml_userdata_thread, (void*)dev);
	if (rc)
	{
		close(fd);
		free(drv_data);
		dev->drv_data = NULL;
		AM_DEBUG(0, "%s:%s", __func__, strerror(rc));
		return AM_USERDATA_ERR_SYS;
	}

	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_close(AM_USERDATA_Device_t *dev)
{
	aml_ud_drv_data_t *drv_data = (aml_ud_drv_data_t*)dev->drv_data;

	close(drv_data->fd);

	drv_data->running = AM_FALSE;
	pthread_join(drv_data->thread, NULL);
	free(drv_data);
	dev->drv_data = NULL;

	return AM_SUCCESS;
}


