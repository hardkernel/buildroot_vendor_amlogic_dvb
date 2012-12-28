/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_caman.h
 * \brief CA管理头文件
 *
 * \author 
 * \date 
 ***************************************************************************/
 
#ifndef _AM_CAMAN_H_
#define _AM_CAMAN_H_

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif
/****************************************************************************
 * Macro definitions
 ***************************************************************************/
 
 
/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief CA管理模块错误代码*/
enum AM_CAMAN_ErrorCode
{
	AM_CAMAN_ERROR_BASE=AM_ERROR_BASE(AM_MOD_CAMAN),
	AM_CAMAN_ERROR_CA_UNKOWN,                         /**< 未知CA*/
	AM_CAMAN_ERROR_CA_EXISTS,                         /**< CA已存在*/
	AM_CAMAN_ERROR_CA_ERROR,                          /**< CA报错*/
	AM_CAMAN_ERROR_NO_MEM,                            /**< 内存告急*/
	AM_CAMAN_ERROR_CANNOT_CREATE_THREAD,              /**< 线程创建失败*/
	AM_CAMAN_ERROR_BADPARAM,                          /**< 参数错误*/
	AM_CAMAN_ERROR_NOTOPEN,                           /**< CA管理模块未打开*/
	AM_CAMAN_ERROR_END                                
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/
 
/**\brief CA类型标识*/
typedef enum {
	AM_CA_TYPE_CA,
	AM_CA_TYPE_CI
} AM_CA_Type_t;

/**\brief CA消息目的地标识*/
typedef enum {
	AM_CA_MSG_DST_CA,
	AM_CA_MSG_DST_CAMAN,
	AM_CA_MSG_DST_APP
} AM_CA_Msg_Dst_t;

/**\brief CAMAN打开参数之一*/
typedef struct {
	int fend_fd;           /**< frontend设备号*/
	int dmx_fd;            /**< dmx设备号*/
	int pmt_timeout;       /**< pmt超时时间，单位毫秒*/
	int pmt_mon_interval;  /**< pmt监视检测间隔，单位毫秒*/
	int pat_timeout;       /**< pat超时时间，单位毫秒*/
	int pat_mon_interval;  /**< pat监视检测间隔，单位毫秒*/
	int cat_timeout;       /**< cat超时时间，单位毫秒*/
	int cat_mon_interval;  /**< cat监视检测间隔，单位毫秒*/
} AM_CAMAN_Ts_t;

/**\brief CAMAN打开参数*/
typedef struct {
	AM_CAMAN_Ts_t ts;
} AM_CAMAN_OpenParam_t;

/**\brief CA消息结构*/
typedef struct {
	int type;             /**< 消息类型，私有*/
	int dst;              /**< 消息目的地，见AM_CA_Msg_Dst_t*/
	unsigned char *data; /**< 消息内容，私有*/
	unsigned int len;    /**< 消息内容长度，单位字节*/
} AM_CA_Msg_t;

/*
		CA				CAMAN					APP

	AM_Ca_t	ca			man register	<--	registerCA
												|
	open,enable(1) <--	call			<--	CA_open()
												|
						get CAT		<--	FEND_EVT_STATUS_CHANGE
						get PAT				startService(sid, caname) force-mode
							|
						match PMT
							|
						get PMT
					
						
	camatch()	<--		(camatch) 				|
	/ts_changed()  					 			|
												|
	startpmt()	<--		PMT(s)		<--	
	stoppmt()	<--		call			<-- CA_Stop(name)
				.......................................
				
	msg_send	-->		in the pool      <-- CA_getMsg(display/notify)
	msg_receive  <--		call			<-- CA_putMsg(trigger/reply)									

	close,enable(0) <--	call			<-- CA_Close
						man unregister<-- unregisterCA
						  
*/

/**\brief CA结构体
	每个要注册到CA管理模块的CA需要生成如下适配结构体。
	CA管理模块同过该结构体来操作管理CA
*/
typedef struct {
	/*init/term the ca*/
	int (*open)(AM_CAMAN_Ts_t *ts);/**< 打开CA*/
	int (*close)(void);/**< 关闭CA*/

	/*check if the ca can work with the ca system marked as the caid*/
	int (*camatch)(unsigned int caid);/**< 判定CA是否匹配*/

	/*caman triggers the ca when there is a ts-change event*/
	int (*ts_changed)(void);/**< 通知CA TS发生变化*/

	/*notify the new cat*/
	int (*new_cat)(unsigned char *cat, unsigned int size);/**< 通知CA新的CAT表*/
	
	/*caman ask the ca to start/stop working on a pmt*/
	int (*start_pmt)(int service_id, unsigned char *pmt, unsigned int size);/**< 通知CA开始Service*/
	int (*stop_pmt)(int service_id);/**< 通知CA停止service*/

	/*ca can exchange messages with caman only if enable() is called with a non-zero argument.
	    ca must stop exchanging msgs with msg funcs after enable(0) is called*/
	int (*enable)(int enable);/**< CA使能，非使能调用后，CA应停止与上层的msg交换*/

	/*the ca will use the func_msg registered to send msgs with the upper layer through caman
		send_msg()'s return value:
			  0 - success
			-1 - wrong name
			-2 - mem fail
	*/
	/**\brief 注册消息发送函数到CA
		CA使用注册的send_msg函数指针发送消息到APP或CA管理模块本身
	*/
	int (*register_msg_send)(int (*send_msg)(char *name, AM_CA_Msg_t *msg));
	/*with which the caman can free the space occupied by the msg*/
	void (*free_msg)(AM_CA_Msg_t *msg);/**< 回收消息空间*/
	/*msg replys will come within the arg of this callback*/
	int (*msg_receive)(AM_CA_Msg_t *msg);/**< 推动消息到CA*/
}AM_CA_Ops_t;

typedef struct {
	/*ca type in AM_CA_Type_t*/
	AM_CA_Type_t type;
	
	/*ca ops*/
	AM_CA_Ops_t ops;
}AM_CA_t;

typedef struct {
	/*if the ca will be checked for the auto-match*/
	int auto_disable;
}AM_CA_Opt_t;

/*CAMAN open/close*/

/**\brief 打开CA管理模块
 * \param [in] para 打开参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_Open(AM_CAMAN_OpenParam_t *para);

/**\brief 关闭CA管理模块
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_Close(void);

/**\brief 暂停CA管理模块
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_Pause(void);

/**\brief 恢复CA管理模块
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_Resume(void);

/*CA open/close*/

/**\brief 打开已注册CA
 * \param [in] name CA别名
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_openCA(char *name);

/**\brief 关闭已注册CA
 * \param [in] name CA别名
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_closeCA(char *name);

/**\brief 开始解扰Service
 * \param [in] service_id Serviceid
 * \param [in] caname     指定CA解扰或NULL=自动选择已注册CA解扰
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_startService(int service_id, char *caname);

/**\brief 停止解扰Service
 * \param [in] service_id Serviceid
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_stopService(int service_id);

/**\brief 停止CA解扰工作
 * \param [in] caname     指定CA解扰或NULL=自动选择已注册CA解扰
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_stopCA(char *name);

/*get/free Msgs from CAMAN/CA
    get -> use -> free
*/

/**\brief 获取CA消息，主动方式取得CA消息，回调方式见AM_CAMAN_setCallback
 * \param [out]    caname 消息由别名为caname的CA发出
 * \param [in out] msg    传入存放消息指针的指针，*msg指向传出消息。消息空间由调用者调用AM_CAMAN_freeMsg回收
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_getMsg(char *caname, AM_CA_Msg_t **msg);

/**\brief 释放消息
 * \param [in] msg 要释放的消息指针，只能传入由AM_CAMAN_getMsg()获取的消息指针
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_freeMsg(AM_CA_Msg_t *msg);

/*send Msg to CAMAN/CA, it's the caller's responsibility to free the Msg*/

/**\brief 发消息给指定CA或CA管理模块或广播消息
 * \param [in] caname 指定CA或NULL=广播
 * \param [in] msg    msg指向要发送的消息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_putMsg(char *name, AM_CA_Msg_t *msg);

/*register/unregister CAs*/

/**\brief 注册CA到CA管理模块
 * \param [in] caname 指定CA别名
 * \param [in] ca     指向要注册的CA结构体
 * \param [in] opt    注册参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_registerCA(char *name, AM_CA_t *ca, AM_CA_Opt_t *opt);

/**\brief 从CA管理模块解除注册CA
 * \param [in] caname 指定CA别名
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_unregisterCA(char *name);

/**\brief 设定CA消息回调函数，回调方式取得CA消息，主动获取见AM_CAMAN_getMsg
 * \param [in] caname 注册到指定CA或NULL=注册到已注册所有CA
 * \param [in] cb     要注册的回调函数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_caman.h)
 */
AM_ErrorCode_t AM_CAMAN_setCallback(char *name, int (*cb)(char *name, AM_CA_Msg_t *msg));

#ifdef __cplusplus
}
#endif

#endif

