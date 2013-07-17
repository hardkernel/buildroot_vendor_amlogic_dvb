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

/****************************************************************************
 * Type definitions
 ***************************************************************************/

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
		if (uh->atsc_id == 0x47413934)
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
	int i;
	char buf[4096];

	if (size > 1024)
		size = 1024;
	for (i=0; i<size; i++)
	{
		sprintf(buf+i*3, "%02x ", buff[i]);
	}

	AM_DEBUG(1, "%s", buf);
}

/**\brief CC data thread*/
static void *aml_userdata_thread(void *arg)
{
	AM_USERDATA_Device_t *dev = (AM_USERDATA_Device_t*)arg;
	aml_ud_drv_data_t *drv_data = (aml_ud_drv_data_t*)dev->drv_data;
	uint8_t buf[5*1024];
	uint8_t cc_data[256];/*In fact, 99 is enough*/
	uint8_t cc_data_cnt;
	int cnt, left, fd;
	struct pollfd fds;
	aml_ud_header_t pheader;
	struct aml_ud_reorder reorder;
	int last_pkg_idx = -1;
	
    AM_DEBUG(1, "user data thread start.");

	fd = drv_data->fd;

	left = 0;

	memset(&reorder, 0, sizeof(reorder));
	memset(&pheader, 0, sizeof(pheader));
	
	while (drv_data->running)
	{
		fds.events = POLLIN|POLLERR;
		fds.fd     = fd;
		if (poll(&fds, 1, 100) > 0)
		{
			cnt = read(fd, buf+left, sizeof(buf)-left);
			if (cnt > 0)
			{
				AM_DEBUG(3, "read %d bytes user_data", cnt);
#if 1
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
#else
				dump_cc_data(buf, cnt);
				left = 0;
#endif
			}
		}
	}
	
		
	AM_DEBUG(1, "user data thread exit now");
	return NULL;
}

static AM_ErrorCode_t aml_open(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para)
{
	char dev_name[64];
	int fd, rc;
	aml_ud_drv_data_t *drv_data;

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
	
	return AM_SUCCESS;
}


