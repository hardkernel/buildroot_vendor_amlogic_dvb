/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_db.c
 * \brief 数据库模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-20: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <am_debug.h>
#include <am_util.h>
#include "am_db_internal.h"
#include <am_db.h>
#include <am_mem.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
 
/*计算一个fileds所有字符串的总长度*/
#define DB_CAL_FIELD_STRING_LEN(_f, _s, _l)\
	AM_MACRO_BEGIN\
		int _i;\
		_l = 0;\
		for (_i=0; _i<(_s); _i++){\
			(_l) += strlen((_f)[_i]);\
			(_l) += 1;/* +1 存储 ",",最后一个field不加",",用于存储结束符*/\
		}\
	AM_MACRO_END

/*生成创建一个表的sql字符串*/
#define DB_GEN_TABLE_SQL_STRING(_t, _b)\
	AM_MACRO_BEGIN\
		int _i;\
		(_b)[0] = 0;\
		strcat(_b, "create table if not exists ");\
		strcat(_b, _t.table_name);\
		strcat(_b, "(");\
		for (_i=0; _i<_t.get_size(); _i++){\
			strcat(_b, _t.fields[_i]);\
			if (_i != (_t.get_size() - 1))\
				strcat(_b, ",");\
		}\
		strcat(_b, ")");\
	AM_MACRO_END

/**\brief 增加一个计算某个表的字段个数的函数*/
#define DEFINE_GET_FIELD_COUNT_FUNC(_f)\
static AM_INLINE int db_get_##_f##_cnt(void)\
{\
	return AM_ARRAY_SIZE(_f);\
}

/****************************************************************************
 * Static data
 ***************************************************************************/
 
/**\brief network table 字段定义*/
static const char *net_fields[] = 
{
#include "network.fld"
};

/**\brief ts table 字段定义*/
static const char *ts_fields[] = 
{
#include "ts.fld"
};

/**\brief service table 字段定义*/
static const char *srv_fields[] = 
{
#include "service.fld"
};

/**\brief event table 字段定义*/
static const char *evt_fields[] = 
{
#include "event.fld"
};

/**\brief record table 字段定义*/
static const char *rec_fields[] = 
{
#include "record.fld"
};

/**\brief group table 字段定义*/
static const char *grp_fields[] = 
{
#include "group.fld"
};

/**\brief group map table 字段定义*/
static const char *grp_map_fields[] = 
{
#include "group_map.fld"
};

/**\brief subtitle table 字段定义*/
static const char *subtitle_fields[] = 
{
#include "subtitle.fld"
};

/**\brief teletext table 字段定义*/
static const char *teletext_fields[] = 
{
#include "teletext.fld"
};

/*各表自动获取字段数目函数定义*/
DEFINE_GET_FIELD_COUNT_FUNC(net_fields)
DEFINE_GET_FIELD_COUNT_FUNC(ts_fields)
DEFINE_GET_FIELD_COUNT_FUNC(srv_fields)
DEFINE_GET_FIELD_COUNT_FUNC(evt_fields)
DEFINE_GET_FIELD_COUNT_FUNC(rec_fields)
DEFINE_GET_FIELD_COUNT_FUNC(grp_fields)
DEFINE_GET_FIELD_COUNT_FUNC(grp_map_fields)
DEFINE_GET_FIELD_COUNT_FUNC(subtitle_fields)
DEFINE_GET_FIELD_COUNT_FUNC(teletext_fields)

/**\brief 所有的表定义*/
static AM_DB_Table_t db_tables[] = 
{
	{"net_table", net_fields, db_get_net_fields_cnt},
	{"ts_table",  ts_fields,  db_get_ts_fields_cnt},
	{"srv_table", srv_fields, db_get_srv_fields_cnt},
	{"evt_table", evt_fields, db_get_evt_fields_cnt},
	{"rec_table", rec_fields, db_get_rec_fields_cnt},
	{"grp_table", grp_fields, db_get_grp_fields_cnt},
	{"grp_map_table", grp_map_fields, db_get_grp_map_fields_cnt},
	{"subtitle_table", subtitle_fields, db_get_subtitle_fields_cnt},
	{"teletext_table", teletext_fields, db_get_teletext_fields_cnt},
};

/**\brief 分析数据类型列表，并生成相应结构*/
static AM_ErrorCode_t db_select_parse_types(const char *fmt, AM_DB_TableSelect_t *ps)
{
	char *p = (char*)fmt;
	char *td, *sd;
	
	assert(fmt && ps);
	
	ps->col = 0;
	while (*p)
	{
		/*去空格*/
		if (*p == ' ')
		{
			p++;
			continue;
		}
		/*分析'%'*/
		if (*p != '%')
		{
			AM_DEBUG(1, "DBase Select: fmt error, '%%' expected,but found '%c'", *p);
			return AM_DB_ERR_INVALID_PARAM;
		}
		/*column 是否超出*/
		if (ps->col >= MAX_SELECT_COLMUNS)
		{
			AM_DEBUG(1, "DBase Select: too many columns!");
			return AM_DB_ERR_INVALID_PARAM;
		}
		/*分析类型字符*/
		p++;
		switch (*p)
		{
			case 'd':
				ps->types[ps->col] = AM_DB_INT;
				ps->sizes[ps->col++] = sizeof(int);
				break;
			case 'f':
				ps->types[ps->col] = AM_DB_FLOAT;
				ps->sizes[ps->col++] = sizeof(double);
				break;
			case 's':
				ps->types[ps->col] = AM_DB_STRING;
				/*解析字符串数组长度*/
				if (*(++p) != ':')
				{
					AM_DEBUG(1, "DBase Select: fmt error, ':' expected,but found '%c'", *p);
					return AM_DB_ERR_INVALID_PARAM;
				}
				if (! isdigit(*(++p)))
				{
					AM_DEBUG(1, "DBase Select: fmt error, digit expected,but found '%c'", *p);
					return AM_DB_ERR_INVALID_PARAM;
				}

				/*取出数字*/
				td = p;
				while (*p && isdigit(*p))
					p++;
					
				sd = (char*)malloc(p - td + 1);
				if (sd == NULL)
				{
					AM_DEBUG(1, "DBase Select: no memory!");
					return AM_DB_ERR_NO_MEM;
				}
				strncpy(sd, td, p - td);
				sd[p - td] = '\0';
				/*AM_DEBUG(1, "Get string array size %d", atoi(sd));*/
				ps->sizes[ps->col] = atoi(sd);
				free(sd);
				
				/*没有指定字符串数组大小?*/
				if (ps->sizes[ps->col] <= 0)
				{
					AM_DEBUG(1, "DBase Select: invalid %%s array size %d", ps->sizes[ps->col - 1]);
					return AM_DB_ERR_INVALID_PARAM;
				}
				ps->col++;
				p--;
				break;
			default:
				AM_DEBUG(1, "DBase Select: fmt error, '[s][d][f]' expected,but found '%c'", *p);
				return AM_DB_ERR_INVALID_PARAM;
				break;
		}
		p++;
		/*查找分隔符','*/
		while (*p && *p == ' ')
			p++;
		
		if (*p && *p++ != ',')
		{
			AM_DEBUG(1, "DBase Select: fmt error, ',' expected,but found '%c'", *p);
			return AM_DB_ERR_INVALID_PARAM;
		}
	}

	/*没有指定任何数据类型*/
	if (ps->col == 0)
	{
		AM_DEBUG(1, "DBase Select: fmt error, no types specified");
		return AM_DB_ERR_INVALID_PARAM;
	}

	return AM_SUCCESS;
}

/**\brief 根据表的fileds定义计算并分配一个合适大小的buf用于存储sql字符串*/
static char *db_get_sql_string_buf(void)
{
	int temp, max, i;

	max = 0;
	for (i=0; i<AM_ARRAY_SIZE(db_tables); i++)
	{
		DB_CAL_FIELD_STRING_LEN(db_tables[i].fields, db_tables[i].get_size(), temp);
		temp += strlen(db_tables[i].table_name);
		max = AM_MAX(max, temp);
	}

	max += strlen("create table if not exsits ()");

	/*AM_DEBUG(1, "buf len %d", max);*/
	return (char*)malloc(max);
}


/****************************************************************************
 * API functions
 ***************************************************************************/
 
/**\brief 初始化数据库模块
 * \param [out] handle 返回数据库操作句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
AM_ErrorCode_t AM_DB_Init(sqlite3 **handle)
{
	assert(handle);
	
	/*打开数据库*/
	AM_DEBUG(1, "Opening DBase...");
	if (sqlite3_open(":memory:", handle) != SQLITE_OK)
	{
		AM_DEBUG(1, "DBase:Cannot open DB in memory!");
		return AM_DB_ERR_OPEN_DB_FAILED;
	}

	AM_TRY(AM_DB_CreateTables(*handle));
	
	AM_DEBUG(1, "DBase handle %p", *handle);
	return AM_SUCCESS;
}

/**\brief 终止数据库模块
 * param [in] handle AM_DB_Init()中返回的handle
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
AM_ErrorCode_t AM_DB_Quit(sqlite3 *handle)
{
	if (handle != NULL)
	{
		AM_DEBUG(1, "Closing DBase(handle=%p)...", handle);
		sqlite3_close(handle);
	}

	return AM_SUCCESS;
}

/**\brief 从数据库取数据
 * param [in] handle AM_DB_Init()中返回的handle
 * param [in] sql 用于查询的sql语句,只支持select语句
 * param [in out]max_row  输入指定最大组数，输出返回查询到的组数
 * param [in] fmt 需要查询的数据类型字符串，格式形如"%d, %s:64, %f",
 *				  其中,顺序必须与select语句中的要查询的字段对应,%d对应int, %s对应字符串,:后指定字符串buf大小,%f对应double
 * param 可变参数 参数个数与要查询的字段个数相同，参数类型均为void *
 *
 * e.g.1 从数据库中查询service的db_id为1的service的aud_pid和name(单组记录)
 *	int aud_pid;
 	char name[256];
 	int row = 1;
  	AM_DB_Select(pdb, "select aud1_pid,name from srv_table where db_id='1'", &row,
 					  "%d, %s:256", (void*)&aud_pid, (void*)name);
 *
 * e.g.2 从数据库中查找service_id大于1的所有service的vid_pid和name(多组记录)
 *	int aud_pid[300];
 	char name[300][256];
 	int row = 300;
 	AM_DB_Select(pdb, "select aud1_pid,name from srv_table where service_id > '1'", &row,
 					  "%d, %s:256",(void*)&aud_pid, (void*)name);
 *
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
AM_ErrorCode_t AM_DB_Select(sqlite3 *handle, const char *sql, int *max_row, const char *fmt, ...)
{
	AM_DB_TableSelect_t ts;
	va_list ap;
	void *vp;
	int va_cnt = 0;
	int col, row, i, j, cur;
	char **db_result = NULL;
	char *errmsg;
	
	assert(handle && sql && fmt && max_row);

	if (strncmp(sql, "select", 6) || *max_row <= 0)
		return AM_DB_ERR_INVALID_PARAM;

	/*取数据类型列表*/
	AM_TRY(db_select_parse_types(fmt, &ts));

	/*分析存储列表*/
	va_start(ap, fmt);
	while ((vp = va_arg(ap, void*)) != NULL && va_cnt < ts.col)
	{
		ts.dat[va_cnt++] = vp;
	}
	va_end(ap);

	/*检查类型列表和参数列表个数是否匹配*/
	if (va_cnt != ts.col)
	{
		AM_DEBUG(1, "DBase Select:fmt error, dismatch the arg list");
		return AM_DB_ERR_INVALID_PARAM;
	}

	/*从数据库查询数据*/
	if (sqlite3_get_table(handle, sql, &db_result, &row, &col, &errmsg) != SQLITE_OK)
	{
		AM_DEBUG(1, "DBase Select:select error, reason [%s]", errmsg);
		return AM_DB_ERR_SELECT_FAILED;
	}

	/*sql语句中指定的column数与fmt字符串中指定的个数不匹配*/
	if (col != ts.col && col != 0)
	{
		AM_DEBUG(1, "DBase Select:sql columns != columns described in fmt");
		sqlite3_free_table(db_result);
		return AM_DB_ERR_INVALID_PARAM;
	}

	if (row < *max_row)
		*max_row = row;

	row = *max_row;

	/*按用户指定的数据类型存储数据*/
	cur = col;
	for (i=0; i<row; i++)
	{
		for (j=0; j<col; j++)
		{
			switch (ts.types[j])
			{
				case AM_DB_INT:
					*(int*)((char*)ts.dat[j] + i * ts.sizes[j]) = db_result[cur] ? atoi(db_result[cur]) : 0;
					break;
				case AM_DB_FLOAT:
					*(double*)((char*)ts.dat[j] + i * ts.sizes[j]) = db_result[cur] ? atof(db_result[cur]) : 0.0;
					break;
				case AM_DB_STRING:
					if (db_result[cur])
					{
						strncpy((char*)ts.dat[j] + i * ts.sizes[j], db_result[cur], ts.sizes[j] - 1);
						*((char*)ts.dat[j] + (i + 1) * ts.sizes[j] - 1) = '\0';
					}
					else
					{
						*((char*)ts.dat[j] + i * ts.sizes[j]) = '\0';
					}
					break;
				default:
					/*This will never occur*/
					break;
			}

			cur++;
		}
	}

	sqlite3_free_table(db_result);
	return AM_SUCCESS;
}
	
/**\brief 在一个指定的数据库中创建表
 * param [in] handle sqlite3数据库句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
AM_ErrorCode_t AM_DB_CreateTables(sqlite3 *handle)
{
	char *buf, *errmsg;
	int i;

	assert(handle);
	
	/*获取存储sql字符串的buf*/
	buf = db_get_sql_string_buf();
	if (buf == NULL)
	{
		AM_DEBUG(1, "DBase:Cannot create DB tables, no enough memory");
		return AM_DB_ERR_NO_MEM;
	}
	
	/*依次创建表*/
	for (i=0; i<AM_ARRAY_SIZE(db_tables); i++)
	{
		AM_DEBUG(1, "Creating table [%s]", db_tables[i].table_name);
	
		/*生成sql 字符串*/
		DB_GEN_TABLE_SQL_STRING(db_tables[i], buf);

		AM_DEBUG(2, "sql string:[%s]", buf);
		/*创建表*/
		if (sqlite3_exec(handle, buf, NULL, NULL, &errmsg) != SQLITE_OK)
		{
			AM_DEBUG(1, "DBase:Cannot create table, reason [%s]", errmsg);
		}
	}

	free(buf);

	return AM_SUCCESS;
}

