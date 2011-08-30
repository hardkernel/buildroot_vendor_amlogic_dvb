/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_db.h
 * \brief 数据库模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-20: create the document
 ***************************************************************************/

#ifndef _AM_DB_H
#define _AM_DB_H

#include <am_types.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/**\brief 单个源中最多包含的TS数目*/
#define AM_DB_MAX_TS_CNT_PER_SRC 100

/**\brief 单个源中最多包含的service数目*/
#define AM_DB_MAX_SRV_CNT_PER_SRC 1000

/**\brief 节目名称最大长度定义*/
#define AM_DB_MAX_SRV_NAME_LEN 64

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief 数据库模块错误代码*/
enum AM_DB_ErrorCode
{
	AM_DB_ERROR_BASE=AM_ERROR_BASE(AM_MOD_DB),
	AM_DB_ERR_OPEN_DB_FAILED,    		/**< 数据库打开失败*/
	AM_DB_ERR_CREATE_TABLE_FAILED,    	/**< 表创建失败*/
	AM_DB_ERR_NO_MEM,					/**< 空闲内存不足*/
	AM_DB_ERR_INVALID_PARAM,			/**< 参数不正确*/
	AM_DB_ERR_SELECT_FAILED,			/**< 查询失败*/
	AM_DB_ERR_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 初始化数据库模块
 * \param [out] handle 返回数据库操作句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
extern AM_ErrorCode_t AM_DB_Init(sqlite3 **handle);

/**\brief 终止数据库模块
 * param [in] handle AM_DB_Init()中返回的handle
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
extern AM_ErrorCode_t AM_DB_Quit(sqlite3 *handle);

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
extern AM_ErrorCode_t AM_DB_Select(sqlite3 *handle, const char *sql, int *max_row, const char *fmt, ...);

/**\brief 在一个指定的数据库中创建表
 * param [in] handle sqlite3数据库句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_db.h)
 */
extern AM_ErrorCode_t AM_DB_CreateTables(sqlite3 *handle);

#ifdef __cplusplus
}
#endif

#endif

