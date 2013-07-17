#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include <am_pes.h>
#include <am_util.h>

typedef struct
{
	uint8_t *buf;
	int      size;
	int      len;
	AM_PES_Para_t para;
}AM_PES_Parser_t;

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

	if(!handle || !para)
	{
		return AM_PES_ERR_INVALID_PARAM;
	}

	parser = (AM_PES_Parser_t*)malloc(sizeof(AM_PES_Parser_t));
	if(!parser)
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
	AM_PES_Parser_t *parser = (AM_PES_Parser_t*)handle;

	if(!parser)
	{
		return AM_PES_ERR_INVALID_HANDLE;
	}

	if(parser->buf)
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
	AM_PES_Parser_t *parser = (AM_PES_Parser_t*)handle;
	int pos, total, left;

	if(!parser)
	{
		return AM_PES_ERR_INVALID_HANDLE;
	}

	if(!buf || !size)
	{
		return AM_PES_ERR_INVALID_PARAM;
	}

	total = AM_MAX(size + parser->len, parser->size);
	if(total > parser->size)
	{
		uint8_t *buf;

		buf = realloc(parser->buf, total);
		if(!buf)
		{
			return AM_PES_ERR_NO_MEM;
		}

		parser->buf  = buf;
		parser->size = total;
	}

	memcpy(parser->buf + parser->len, buf, size);
	parser->len += size;
	pos = 0;

	do{
		AM_Bool_t found = AM_FALSE;
		int i, plen;

		for(i=pos; i< parser->len - 4; i++)
		{
			if((parser->buf[i] == 0) && (parser->buf[i+1] == 0) && (parser->buf[i+2] == 1) && (parser->buf[i+3] == 0xBD))
			{
				found = AM_TRUE;
				pos = i;
				break;
			}
		}

		if(!found || (parser->len - pos < 6))
		{
			goto end;
		}

		plen = (parser->buf[pos+4]<<8) | (parser->buf[pos+5]);
		
		if((parser->len - pos) < (plen + 6))
		{
			goto end;
		}

		if(parser->para.packet)
		{
			parser->para.packet(handle, parser->buf + pos, plen + 6);
		}

		pos += plen + 6;
	} while(pos < parser->len);

end:
	left = parser->len - pos;

	if(left)
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

	if(!parser)
	{
		return NULL;
	}

	return parser->para.user_data;
}

