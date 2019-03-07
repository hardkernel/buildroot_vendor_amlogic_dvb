#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * Description:
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
#include <math.h>
#include <signal.h>

#define USERDATA_POLL_TIMEOUT 100
#define MAX_CC_NUM			64
#define MAX_CC_DATA_LEN  (1024*5 + 4)

#define IS_H264(p)	((p[0] == 0xb5 && p[3] == 0x47 && p[4] == 0x41 && p[5] == 0x39 && p[6] == 0x34))
#define IS_DIRECTV(p) ((p[0] == 0xb5 && p[1] == 0x00 && p[2] == 0x2f))
#define IS_AVS(p)	 ((p[0] == 0x47) && (p[1] == 0x41) && (p[2] == 0x39) && (p[3] == 0x34))
#define IS_ATSC(p)	((p[0] == 0x47) && (p[1] == 0x41) && (p[2] == 0x39) && (p[3] == 0x34) && (p[4] == 0x3))
#define IS_SCTE(p)  ((p[0]==0x3) && ((p[1]&0x7f) == 1))

#define AMSTREAM_IOC_MAGIC  'S'
#define AMSTREAM_IOC_UD_LENGTH _IOR(AMSTREAM_IOC_MAGIC, 0x54, unsigned long)
#define AMSTREAM_IOC_UD_POC _IOR(AMSTREAM_IOC_MAGIC, 0x55, int)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA _IOR(AMSTREAM_IOC_MAGIC, 0x56, int)
#define AMSTREAM_IOC_UD_BUF_READ _IOR(AMSTREAM_IOC_MAGIC, 0x57, unsigned int)
#define AMSTREAM_IOC_UD_AVAIBLE_VDEC      _IOR(AMSTREAM_IOC_MAGIC, 0x5c, unsigned int)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef enum {
	INVALID_TYPE 	= 0,
	MPEG_CC_TYPE 	= 1,
	H264_CC_TYPE 	= 2,
	DIRECTV_CC_TYPE = 3,
	AVS_CC_TYPE 	= 4,
	SCTE_CC_TYPE = 5
} userdata_type;


typedef enum {
	/* 0 forbidden */
	I_TYPE = 1,
	P_TYPE = 2,
	B_TYPE = 3,
	D_TYPE = 4,
	/* 5 ... 7 reserved */
} picture_coding_type;

struct userdata_meta_info_t {
	uint32_t poc_number;
	/************ flags bit defination ***********
	bit 0:		//used for mpeg2
		1, group start
		0, not group start
	bit 1-2:	//used for mpeg2
		0, extension_and_user_data( 0 )
		1, extension_and_user_data( 1 )
		2, extension_and_user_data( 2 )
	bit 3-6:	//video format
		0,	VFORMAT_MPEG12
		1,	VFORMAT_MPEG4
		2,	VFORMAT_H264
		3,	VFORMAT_MJPEG
		4,	VFORMAT_REAL
		5,	VFORMAT_JPEG
		6,	VFORMAT_VC1
		7,	VFORMAT_AVS
		8,	VFORMAT_SW
		9,	VFORMAT_H264MVC
		10, VFORMAT_H264_4K2K
		11, VFORMAT_HEVC
		12, VFORMAT_H264_ENC
		13, VFORMAT_JPEG_ENC
		14, VFORMAT_VP9
	bit 7-9:	//frame type
		0, Unknown Frame Type
		1, I Frame
		2, B Frame
		3, P Frame
		4, D_Type_MPEG2
	bit 10:  //top_field_first_flag valid
		0: top_field_first_flag is not valid
		1: top_field_first_flag is valid
	bit 11: //top_field_first bit val
	**********************************************/
	uint32_t flags;
	uint32_t vpts;			/*video frame pts*/
	/******************************************
	0: pts is invalid, please use duration to calcuate
	1: pts is valid
	******************************************/
	uint32_t vpts_valid;
	/*duration for frame*/
	uint32_t duration;
	/* how many records left in queue waiting to be read*/
	uint32_t records_in_que;
	unsigned long long priv_data;
	uint32_t padding_data[4];
};


struct userdata_param_t {
	uint32_t version;
	uint32_t instance_id; /*input, 0~9*/
	uint32_t buf_len; /*input*/
	uint32_t data_size; /*output*/
	void* pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
};
#if 0
typedef struct {
	 unsigned int poc_info;
	 unsigned int poc_number;
} userdata_poc_info_t;
#endif

typedef struct AM_CCData_s AM_CCData;
struct AM_CCData_s {
	AM_CCData *next;
	uint8_t   *buf;
	int		size;
	int		cap;
	int		poc;
};

typedef struct {
	uint32_t picture_structure:16;
	uint32_t temporal_reference:10;
	uint32_t picture_coding_type:3;
	uint32_t reserved:3;
	uint32_t index:16;
	uint32_t offset:16;
	uint8_t  atsc_flag[4];
	uint8_t  cc_data_start[4];
} aml_ud_header_t;

typedef struct {
	pthread_t	   th;
	int			 fd;
	int vfmt;
	AM_Bool_t	   running;
	AM_CCData	  *cc_list;
	AM_CCData	  *free_list;
	int			 cc_num;
	userdata_type   format;
	int			 curr_poc;
	int scte_enable;
} AM_UDDrvData;


/****************************************************************************
 * Static data definitions
 ***************************************************************************/

static AM_ErrorCode_t aml_open(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para);
static AM_ErrorCode_t aml_close(AM_USERDATA_Device_t *dev);

const AM_USERDATA_Driver_t aml_ud_drv = {
.open  = aml_open,
.close = aml_close,
};

static void dump_cc_data(char *who, int poc, uint8_t *buff, int size)
{
	int i;
	char buf[4096];

	if (size > 1024)
		size = 1024;
	for (i=0; i<size; i++)
	{
		sprintf(buf+i*3, "%02x ", buff[i]);
	}

	AM_DEBUG(0, "CC DUMP:who:%s poc: %d :%s", who, poc, buf);
}

static void aml_free_cc_data (AM_CCData *cc)
{
	if (cc->buf)
		free(cc->buf);
	free(cc);
}

static void aml_swap_data(uint8_t *user_data, int ud_size)
{
	int swap_blocks, i, j, k, m;
	unsigned char c_temp;

	/* swap byte order */
	swap_blocks = ud_size >> 3;
	for (i = 0; i < swap_blocks; i ++) {
		j = i << 3;
		k = j + 7;
		for (m=0; m<4; m++) {
			c_temp = user_data[j];
			user_data[j++] = user_data[k];
			user_data[k--] = c_temp;
		}
	}
}

static uint8_t scte20_char_map[256];

static uint8_t
scte20_get_char (uint8_t c)
{
	if (scte20_char_map[1] == 0) {
		int i;

		for (i = 0; i < 256; i ++) {
			uint8_t v1, v2;
			int     b;

			v1 = i;
			v2 = 0;

			for (b = 0; b < 8; b ++) {
				if (v1 & (1 << b))
					v2 |= (1 << (7 - b));
			}

			scte20_char_map[i] = v2;
		}
	}

	return scte20_char_map[c];
}

static userdata_type aml_check_userdata_format (uint8_t *buf, int vfmt, int len)
{
	if (len < 8)
		return INVALID_TYPE;
	//vfmt:
	//0 for MPEG
	//2 for H264
	//7 for AVS
	if (vfmt == 2)
	{
		if (IS_H264(buf))
		{
			AM_DEBUG(4,"CC format is h264_cc_type");
			return H264_CC_TYPE;
		}
		else if (IS_DIRECTV(buf))
		{
			AM_DEBUG(4,"CC format is directv_cc_type");
			return DIRECTV_CC_TYPE;
		}
	}
	else if (vfmt == 7)
	{
		if (IS_AVS(buf)) {
			AM_DEBUG(4,"CC format is avs_cc_type");
			return AVS_CC_TYPE;
		}
	}
	else if (vfmt == 0)
	{
		if (len >= (int)sizeof(aml_ud_header_t))
		{
			aml_ud_header_t *hdr = (aml_ud_header_t*)buf;

			if (IS_ATSC(hdr->atsc_flag))
			{
				AM_DEBUG(4,"CC format is mpeg_cc_type");
				return MPEG_CC_TYPE;
			}
			else if (IS_SCTE(hdr->atsc_flag))
			{
				AM_DEBUG(4, "CC format is scte_cc_type");
				return SCTE_CC_TYPE;
			}
		}
	}

	return INVALID_TYPE;
}

static void aml_flush_cc_data(AM_USERDATA_Device_t *dev)
{
	AM_UDDrvData *ud = dev->drv_data;
	AM_CCData *cc, *ncc;
	char  buf[256];
	char *pp = buf;
	int   left = sizeof(buf), i, pr;

	for (cc = ud->cc_list; cc; cc = ncc) {
		ncc = cc->next;

		for (i = 0; i < cc->size; i ++) {
			pr = snprintf(pp, left, "%02x ", cc->buf[i]);
			if (pr < left) {
				pp   += pr;
				left -= pr;
			}
		}

		AM_DEBUG(1, "cc_write_package decode:%s", buf);

		dev->write_package(dev, cc->buf, cc->size);

		cc->next = ud->free_list;
		ud->free_list = cc;
	}

	ud->cc_list  = NULL;
	ud->cc_num   = 0;
	ud->curr_poc = -1;
}

static void aml_add_cc_data(AM_USERDATA_Device_t *dev, int poc, int type, uint8_t *p, int len)
{
	AM_UDDrvData *ud = dev->drv_data;
	AM_CCData **pcc, *cc;
	char  buf[256];
	char *pp = buf;
	int   left = sizeof(buf), i, pr;

	if (ud->cc_num >= MAX_CC_NUM) {
#if 0
		aml_flush_cc_data(dev);
#else
		cc = ud->cc_list;
		//dump_cc_data("add cc ",cc->poc,cc->buf, cc->size);
		dev->write_package(dev, cc->buf, cc->size);

		ud->cc_list = cc->next;
		cc->next = ud->free_list;
		ud->free_list = cc;
		ud->cc_num --;
#endif
	}

	for (i = 0; i < len; i ++) {
		pr = snprintf(pp, left, "%02x ", p[i]);
		if (pr < left) {
			pp   += pr;
			left -= pr;
		}
	}

	AM_DEBUG(0, "CC poc:%d ptype:%d data:%s", poc, type, buf);

	pcc = &ud->cc_list;
	if (*pcc && poc < ((*pcc)->poc - 30))
		aml_flush_cc_data(dev);

	while ((cc = *pcc)) {
		/*if (cc->poc == poc) {
			aml_flush_cc_data(dev);
			pcc = &ud->cc_list;
			break;
		}*/

		if (cc->poc > poc) {
			break;
		}

		pcc = &cc->next;
	}

	if (ud->free_list) {
		cc = ud->free_list;
		ud->free_list = cc->next;
	} else {
		cc = malloc(sizeof(AM_CCData));
		cc->buf  = NULL;
		cc->size = 0;
		cc->cap  = 0;
		cc->poc  = 0;
	}

	if (cc->cap < len) {
		cc->buf = realloc(cc->buf, len);
		cc->cap = len;
	}

	memcpy(cc->buf, p, len);

	cc->size = len;
	cc->poc  = poc;
	cc->next = *pcc;
	*pcc = cc;

	ud->cc_num ++;
}

static void aml_mpeg_userdata_package(AM_USERDATA_Device_t *dev, int poc, int type, uint8_t *p, int len)
{
	AM_UDDrvData *ud = dev->drv_data;
#if 0
	int i;
	char display_buffer[10240];
	for (i=0;i<len; i++)
		sprintf(&display_buffer[i*3], " %02x", p[i]);
	AM_DEBUG(0, "mpeg_process len:%d data:%s", len, display_buffer);
#endif

	if (len < 5)
		return;

	if (p[4+4] != 3)
		return;

	if (type == I_TYPE)
		aml_flush_cc_data(dev);

	if (poc == ud->curr_poc + 1) {
		AM_CCData *cc, **pcc;

		dev->write_package(dev, p, len);
		ud->curr_poc ++;

		pcc = &ud->cc_list;
		while ((cc = *pcc)) {
			if (ud->curr_poc + 1 != cc->poc)
				break;

			dev->write_package(dev, cc->buf, cc->size);
			*pcc = cc->next;
			ud->curr_poc ++;

			cc->next = ud->free_list;
			ud->free_list = cc;
		}

		return;
	}

	aml_add_cc_data(dev, poc, type, p, len);
}

static int aml_process_scte_userdata(AM_USERDATA_Device_t *dev, uint8_t *data, int len, int flags, uint32_t pts)
{
	int cc_count = 0, cnt, i;
	int field_num;
	uint8_t* cc_section;
	uint8_t cc_data[64] = {0};
	uint8_t* scte_head;
	uint8_t* scte_head_search_position;
	int head_posi = 0;
	int prio, field, line, cc1, cc2, mark, size, ptype, ref;
	int write_position, bits = 0, array_position = 0;
	uint8_t *p;
	int left = len;
	int left_bits;
	uint32_t v;
	uint32_t *pts_in_buffer;
	uint8_t userdata_with_pts[MAX_CC_DATA_LEN];
#if 0
	char display_buffer[10240];
#endif
	int top_bit_value, top_bit_valid;

	AM_UDDrvData *ud = dev->drv_data;

	if (ud->scte_enable != 1)
		return len;

	v = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
	top_bit_valid = flags & (1<<10);
	top_bit_value = flags & (1<<11);
	if (top_bit_valid == 0)
		top_bit_value = 1;

	ref = (v >> 16) & 0x3ff;
	ptype = (v >> 26) & 7;

#if 0
	for (i=0; i<len; i++)
		sprintf(display_buffer+3*i, " %02x", data[i]);
	AM_DEBUG(0, "scte buffer type %d ref %d top_bit %d %s", ptype, ref, top_bit_value, display_buffer);
#endif

	if (ptype == I_TYPE)
		aml_flush_cc_data(dev);

	scte_head = data;
	while (head_posi < len)
	{
		scte_head_search_position = &scte_head[head_posi];
		if (IS_SCTE(scte_head_search_position))
			break;
		head_posi += 8;
	}

	if ((len - head_posi) < 8)
		return len;

	p = &data[head_posi + 2];
	cc_data[0] = 0x47;
	cc_data[1] = 0x41;
	cc_data[2] = 0x39;
	cc_data[3] = 0x34;
	cc_data[4] = 0x3;

	left_bits = (len - head_posi) << 3;

#define NST_BITS(v, b, l) (((v) >> (b)) & (0xff >> (8 - (l))))
#define GET(n, b)\
	do \
	{\
		int off, bs;\
		if (bits + b > left_bits) goto error;\
		off = bits >> 3;\
		bs  = bits & 7;\
		if (8 - bs >= b) {\
		  n = NST_BITS(p[off], 8 - bs - b, b);\
		} else {\
		  int n1, n2, b1 = 8 - bs, b2 = b- b1;\
		  n1 = NST_BITS(p[off], 0, b1);\
		  n2 = NST_BITS(p[off + 1], 8 - b + b1, b - b1);\
		  n = (n1 << b2) | n2;\
		}\
		bits += b;\
	} while(0)

	GET(cnt, 5);
	array_position = 7;
	for (i = 0; i < cnt; i ++) {
		GET(prio, 2);
		GET(field, 2);
		GET(line, 5);
		GET(cc1, 8);
		GET(cc2, 8);
		GET(mark, 1);
		if (field == 3)
			field = 1;
		AM_DEBUG(0, "loop %d field %d line %d cc1 %x cc2 %x",
			i, field, line, cc1, cc2);
		if (field == 1)
			line = (top_bit_value)?line+10:line+273;
		else if (field == 2)
			line = (top_bit_value)?line+273:line+10;
		else
			continue;

		if (line == 21)
		{
			cc_data[array_position] = 4;
			cc_data[array_position + 1] = scte20_get_char(cc1);
			cc_data[array_position + 2] = scte20_get_char(cc2);
			array_position += 3;
			cc_count++;
		}
		else if (line == 284)
		{
			cc_data[array_position] = 4|1;
			cc_data[array_position + 1] = scte20_get_char(cc1);
			cc_data[array_position + 2] = scte20_get_char(cc2);
			array_position += 3;
			cc_count++;
		}
		else
			continue;
	}
	cc_data[5] = 0x40 |cc_count;
	size = 7 + cc_count*3;

	pts_in_buffer = userdata_with_pts;
	*pts_in_buffer = pts;
	memcpy(userdata_with_pts+4, cc_data, size);
#if 0
	for (i=0; i<size; i++)
			sprintf(display_buffer+3*i, " %02x", cc_data[i]);
		//AM_DEBUG(0, "scte_write_buffer len: %d data: %s", size, display_buffer);
#endif
	if (cc_count > 0)
		aml_add_cc_data(dev, ref, ptype, userdata_with_pts, size+4);
error:
	return len;
}

static int aml_process_mpeg_userdata(AM_USERDATA_Device_t *dev, uint8_t *data, int flag, uint32_t pts, int len)
{
	AM_UDDrvData *ud = dev->drv_data;
	uint8_t *pd = data;
	int left = len;
	int r = 0;
	int i;
	int package_count = 0;
	uint32_t *pts_in_buffer;
	int userdata_length;
	uint8_t userdata_with_pts[MAX_CC_DATA_LEN];

	while (left >= (int)sizeof(aml_ud_header_t)) {
		aml_ud_header_t *hdr = (aml_ud_header_t*)pd;
		int ref, ptype;

		if (IS_ATSC(hdr->atsc_flag) ) {
			aml_ud_header_t *nhdr;
			uint8_t *pp, t;
			uint32_t v;
			int pl;

			pp = (uint8_t*)&hdr->atsc_flag;
			pl = 8;

			v = (pd[4] << 24) | (pd[5] << 16) | (pd[6] << 8) | pd[7];
			ref   = (v >> 16) & 0x3ff;
			ptype = (v >> 26) & 7;

			/* We read one packet in one time, so treat entire buffer */
			if (!(flag & (1<<2)))
				return len;

			userdata_length = len-r-8;
			pts_in_buffer = userdata_with_pts;
			*pts_in_buffer = pts;
			memcpy(userdata_with_pts+4, pp, userdata_length);
			//AM_DEBUG(0, "CC header len: %d pts %x flag %d digi: %02x %02x %02x %02x %02x %02x %02x %02x", userdata_length, pts, flag,
			//	pd[0], pd[1], pd[2], pd[3], pd[4], pd[5], pd[6], pd[7]);
			aml_mpeg_userdata_package(dev, ref, ptype, userdata_with_pts, userdata_length+4);
			r = len;

			break;
		} else {
			pd   += 8;
			left -= 8;
			r	+= 8;
		}
	}

	return r;
}

static int aml_process_h264_userdata(AM_USERDATA_Device_t *dev, uint8_t *data, int len, int poc, uint32_t pts)
{
	AM_UDDrvData *ud = dev->drv_data;
	int fd = ud->fd;
	uint8_t *pd = data;
	int left = len;
	int r = 0;
	uint32_t *pts_in_buffer;
	uint8_t userdata_with_pts[MAX_CC_DATA_LEN];

	while (left >= 8) {
		if (IS_H264(pd) || IS_DIRECTV(pd) || IS_AVS(pd)) {
			int hdr = (ud->format == H264_CC_TYPE) ? 3 : 0;
			int pl;

			pd += hdr;

			pl = 8 + (pd[5] & 0x1f) * 3;

			if (pl + hdr > left) {
				break;
			}

			//AM_DEBUG(0, "CC poc_number:%x hdr:%d pl:%d", poc, hdr, pl);
			if (poc == 0) {
				aml_flush_cc_data(dev);
			}
			//dump_cc_data("process h264:", poc, pd, pl);
			pts_in_buffer = userdata_with_pts;
			*pts_in_buffer = pts;
			memcpy(userdata_with_pts+4, pd, pl);
			aml_add_cc_data(dev, poc, I_TYPE, userdata_with_pts, pl+4);

			pd   += pl;
			left -= pl + hdr;
			r	+= pl + hdr;
		} else {
			pd   += 8;
			left -= 8;
			r	+= 8;
		}
	}

	return r;
}

static void* aml_userdata_thread (void *arg)
{
	AM_USERDATA_Device_t *dev = (AM_USERDATA_Device_t*)arg;
	AM_UDDrvData *ud = dev->drv_data;
	int fd = ud->fd;
	int r, ret, i;
	struct pollfd pfd;
	uint8_t data[MAX_CC_DATA_LEN];
	uint8_t *pd = data;
#if 0
	char display_buffer[10*1024];
#endif
	int left = 0;
	int flush = 1;
	int vdec_ids;
	struct userdata_param_t user_para_info;

	pfd.events = POLLIN|POLLERR;
	pfd.fd	 = fd;

	while (ud->running) {
		//If scte and mpeg both exist, we need to ignore scte cc data,
		//so we need to check cc type every time.
		left = 0;

		ret = poll(&pfd, 1, USERDATA_POLL_TIMEOUT);
		AM_DEBUG(0, "userdata after poll ret %d", ret);
		if (!ud->running)
			break;
		if (ret != 1)
			continue;
		if (!(pfd.revents & POLLIN))
			continue;

		//For multi-instances support
		vdec_ids = 0;
		if (-1 == ioctl(fd, AMSTREAM_IOC_UD_AVAIBLE_VDEC, &vdec_ids)) {
			printf("get avaible vdec failed\n");
		} else {
			printf("get avaible vdec OK: 0x%x\n", vdec_ids);
		}

		if (!(vdec_ids & 1))
			continue;

		if (flush) {
			ioctl(fd, AMSTREAM_IOC_UD_FLUSH_USERDATA, NULL);
			flush = 0;
			continue;
		}

		do {
			memset(&user_para_info, 0, sizeof(struct userdata_param_t));
			user_para_info.pbuf_addr= (void*)(size_t)data;
			user_para_info.buf_len = sizeof(data);
			user_para_info.instance_id = 0;

			if (-1 == ioctl(fd, AMSTREAM_IOC_UD_BUF_READ, &user_para_info))
				AM_DEBUG(0, "call AMSTREAM_IOC_UD_BUF_READ failed\n");
			AM_DEBUG(0, "ioctl left data: %d",user_para_info.meta_info.records_in_que);

			r = user_para_info.data_size;
			r = (r > MAX_CC_DATA_LEN) ? MAX_CC_DATA_LEN : r;

			if (r <= 0)
				continue;
			aml_swap_data(data + left, r);
			left += r;
			pd = data;
#if 0
			for (i=0; i<left; i++)
				sprintf(&display_buffer[i*3], " %02x", data[i]);
			AM_DEBUG(0, "ud_aml_buffer: %s", display_buffer);
#endif
			ud->format = INVALID_TYPE;
			while (ud->format == INVALID_TYPE) {
				if (left < 8)
					break;

				ud->format = aml_check_userdata_format(pd, ud->vfmt, left);
				if (ud->format == INVALID_TYPE) {
					pd   += 8;
					left -= 8;
				}
			}

			if (ud->format == MPEG_CC_TYPE) {
				ud->scte_enable = 0;
				r = aml_process_mpeg_userdata(dev, pd, user_para_info.meta_info.flags,user_para_info.meta_info.vpts, left);
			} else if (ud->format == SCTE_CC_TYPE) {
				if (ud->scte_enable == 1)
					r = aml_process_scte_userdata(dev, pd, left, user_para_info.meta_info.flags, user_para_info.meta_info.vpts);
				else
					r = left;
			} else if (ud->format != INVALID_TYPE) {
				r = aml_process_h264_userdata(dev, pd, left, user_para_info.meta_info.poc_number , user_para_info.meta_info.vpts);
			} else {
				r = left;
			}

		if ((data != pd + r) && (r < left)) {
			memmove(data, pd + r, left - r);
		}

		left -= r;
		}while(user_para_info.meta_info.records_in_que > 1);
	}
	AM_DEBUG(0, "aml userdata thread exit");
	return NULL;
}

static AM_ErrorCode_t aml_open(AM_USERDATA_Device_t *dev, const AM_USERDATA_OpenPara_t *para)
{
	AM_UDDrvData *ud = malloc(sizeof(AM_UDDrvData));
	int r;

	if (!ud) {
		AM_DEBUG(0, "not enough memory");
		return AM_USERDATA_ERR_NO_MEM;
	}

	ud->fd = open("/dev/amstream_userdata", O_RDONLY);
	if (ud->fd == -1) {
		AM_DEBUG(0, "cannot open userdata device");
		free(ud);
		return AM_USERDATA_ERR_CANNOT_OPEN_DEV;
	}
	ud->vfmt = para->vfmt;
	ud->format	= INVALID_TYPE;
	ud->cc_list   = NULL;
	ud->free_list = NULL;
	ud->running   = AM_TRUE;
	ud->cc_num	= 0;
	ud->curr_poc  = -1;
	ud->scte_enable = 1;

	r = pthread_create(&ud->th, NULL, aml_userdata_thread, (void*)dev);
	if (r) {
		AM_DEBUG(0, "create userdata thread failed");
		close(ud->fd);
		free(ud);
		return AM_USERDATA_ERR_SYS;
	}
	AM_SigHandlerInit();

	dev->drv_data = ud;
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_close(AM_USERDATA_Device_t *dev)
{
	AM_UDDrvData *ud = dev->drv_data;
	AM_CCData *cc, *cc_next;

	ud->running = AM_FALSE;
	pthread_kill(ud->th, SIGALRM);
	pthread_join(ud->th, NULL);
	close(ud->fd);

	for (cc = ud->cc_list; cc; cc = cc_next) {
		cc_next = cc->next;
		aml_free_cc_data(cc);
	}

	for (cc = ud->free_list; cc; cc = cc_next) {
		cc_next = cc->next;
		aml_free_cc_data(cc);
	}


	free (ud);
	return AM_SUCCESS;
}

