#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define AM_DEBUG_LEVEL 5

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <am_debug.h>
#include <am_pes.h>
#include <am_util.h>

typedef struct
{
	uint8_t *buf;
	int      size;
	int      len;
	AM_PES_Para_t para;
}AM_PES_Parser_t;


static int calc_invalid_pes_offset(uint8_t  *data, int data_len, int *plen, int *dpos, int *dlen)
{
	int posn = 6;
	int i = 0;
	int pes_head_len = 0;
	int found = 0;
	pes_head_len = data[8];
	/*find next 00 00 01 head*/
	for (i=posn; i< data_len - 4; i++)
	{
		if ((data[i] == 0) && (data[i+1] == 0) && (data[i+2] == 1))
		{
			posn = i;
			found = 1;
			break;
		}
	}
	if (found ==1) {
		if (posn < pes_head_len + 9) {
	      /*del data*/
		  *dpos = *dpos + posn;
		  *dlen = 0;
		  *plen = posn - 6;
		} else {
		  *dpos = *dpos + pes_head_len + 9;
		  *dlen = posn -(pes_head_len + 9);
		  *plen = posn - 6;
		}
	} else {
		return -1;
	}
	return 0;
}

static int calc_mpeg1_pes_offset(uint8_t  *data, int data_len)
{
	int posn = 6;

	while (posn < data_len && data[posn] == 0xFF)
		posn++;

	if (posn < data_len)
	{
		if ((data[posn] & 0xC0) == 0x40)
		posn += 2;

		if ((data[posn] & 0xF0) == 0x20)
			posn += 5;
		else if ((data[posn] & 0xF0) == 0x30)
			posn += 10;
		else if (data[posn] == 0x0F)
			posn ++;
		else
			posn ++;
	}
	return posn;
}


/**\brief 创建一个PES分析器
 * \param[out] 返回创建的句柄
 * \param[in] para PES分析器参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Create(AM_PES_Handle_t *handle, AM_PES_Para_t *para)
{
	AM_PES_Parser_t *parser;

	if (!handle || !para)
	{
		return AM_PES_ERR_INVALID_PARAM;
	}

	parser = (AM_PES_Parser_t *)malloc(sizeof(AM_PES_Parser_t));
	if (!parser)
	{
		return AM_PES_ERR_NO_MEM;
	}

	memset(parser, 0, sizeof(AM_PES_Parser_t));

	parser->para = *para;

	*handle = parser;

	return AM_SUCCESS;
}

/**\brief 释放一个PES分析器
 * \param 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Destroy(AM_PES_Handle_t handle)
{
	AM_PES_Parser_t *parser = (AM_PES_Parser_t *)handle;

	if (!parser)
	{
		return AM_PES_ERR_INVALID_HANDLE;
	}

	if (parser->buf)
	{
		free(parser->buf);
	}

	free(parser);

	return AM_SUCCESS;
}

/**\brief 分析PES数据
 * \param 句柄
 * \param[in] buf PES数据缓冲区
 * \param size 缓冲区中数据大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Decode(AM_PES_Handle_t handle, uint8_t *buf, int size)
{
	AM_PES_Parser_t *parser = (AM_PES_Parser_t *)handle;
	int pos, total, left;

	if (!parser)
	{
		return AM_PES_ERR_INVALID_HANDLE;
	}

	if (!buf || !size)
	{
		return AM_PES_ERR_INVALID_PARAM;
	}

	total = AM_MAX(size + parser->len, parser->size);
	if (total > parser->size)
	{
		uint8_t *buf;

		buf = realloc(parser->buf, total);
		if (!buf)
		{
			return AM_PES_ERR_NO_MEM;
		}

		parser->buf  = buf;
		parser->size = total;
	}


	memcpy(parser->buf + parser->len, buf, size);
	parser->len += size;
	pos = 0;

	do {
		AM_Bool_t found = AM_FALSE;
		int i, plen = 0;
		int dpos = 0, dlen = 0;
		int hlen = 0;
		int invalid = 0;

		for (i=pos; i< parser->len - 4; i++)
		{
			if ((parser->buf[i] == 0) && (parser->buf[i+1] == 0) && (parser->buf[i+2] == 1))
			{
				found = AM_TRUE;
				pos = i;
				break;
			}
		}

		if (!found || (parser->len - pos < 6))
		{
			goto end;
		}

		plen = (parser->buf[pos+4]<<8) | (parser->buf[pos+5]);
		if (plen == 0xffff || plen == 0x0) {
			invalid = 1;
		}

		if ((parser->len - pos) < (plen + 6) && invalid == 0)
		{
			goto end;
		}

		if (parser->para.payload_only) {
			if (invalid == 1) {

				dpos = pos;
				if (calc_invalid_pes_offset(parser->buf +pos, parser->len - pos, &plen, &dpos, &dlen) != 0) {
					AM_DEBUG(1, "pes packet calc error");
					goto end;
				} else {
					AM_DEBUG(1, "pes packet calc plen=[%d]dpos[%d]dlen[%d]", plen, dpos, dlen);
				}
			} else {
				if ((parser->buf[pos+6] & 0xC0) == 0x80) {
					hlen = 6 + 3 + parser->buf[pos+8];
				} else {
					AM_DEBUG(1, "pes packet assume MPEG-1 [%#x]\n", parser->buf[pos+6]);
					hlen = calc_mpeg1_pes_offset(parser->buf +pos, plen);
				}
				dpos = pos + hlen;
				dlen = plen + 6 - hlen;
			}

		} else {
			dpos = pos;
			dlen = plen + 6;
		}

		if (parser->para.packet)
		{
			parser->para.packet(handle, parser->buf + dpos, dlen);
		}

		pos += plen + 6;
	} while(pos < parser->len);

end:
	left = parser->len - pos;

	if (left)
	{
		memmove(parser->buf, parser->buf + pos, left);
	}
	parser->len = left;
	return AM_SUCCESS;
}

/**\brief 取得分析器中用户定义数据
 * \param 句柄
 * \return 用户定义数据
 */
void*          AM_PES_GetUserData(AM_PES_Handle_t handle)
{
	AM_PES_Parser_t *parser = (AM_PES_Parser_t*)handle;

	if (!parser)
	{
		return NULL;
	}

	return parser->para.user_data;
}

