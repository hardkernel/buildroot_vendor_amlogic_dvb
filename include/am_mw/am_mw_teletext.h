/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief Teletext模块
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-18: create the document
 ***************************************************************************/


#ifndef _AM_MW_TELETEXT_H
#define _AM_MW_TELETEXT_H

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/



/**\brief TeleText模块错误代码*/
enum AM_MW_TELETEXT_ErrorCode
{
	AM_MW_TELETEXT_ERROR_BASE=AM_ERROR_BASE(AM_MOD_MW_TELETEXT),
	AM_MW_TELETEXT_ERR_INVALID_PARAM,			 /**< 参数无效*/
	AM_MW_TELETEXT_ERR_INVALID_HANDLE,          /**< 句柄无效*/
	AM_MW_TELETEXT_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_MW_TELETEXT_ERR_CREATE_DECODE,                  /**< 打开TELETEXT解码器失败*/
	AM_MW_TELETEXT_ERR_OPEN_PES,                  /**< 打开PES通道失败*/
	AM_MW_TELETEXT_ERR_SET_BUFFER,                  /**< 失置PES 缓冲区失败*/
	AM_MW_TELETEXT_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_MW_TELETEXT_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_MW_TELETEXT_ERR_NOT_RUN,    /**< 没有运行teletext*/
	AM_MW_TELETEXT_ERR_NOT_START,    /**< 没有开始*/
	AM_MW_TELETEXT_ERR_PAGE_NOT_EXSIT,    /**< 请求页不存在*/
	AM_MW_TELETEXT_INIT_DISPLAY_ERROR,    /**< 初始化显示屏幕失败*/
	AM_MW_TELETEXT_INIT_FONT_ERROR,    /**< 初始化字体失败*/






	AM_MW_TELETEXT_ERR_END
};

enum AM_teletext_event_code
{
	TT_EVENT_TIMEOUT=0,
	TT_EVENT_PAGEUPDATE,
	TT_EVENT_ERRORPAGE,

};

/**\brief 填充矩形
 * left,top,width,height为矩形属性
 * color 为填充颜色
 */
typedef int (*fill_rectangle)(int left, int top, unsigned int width, unsigned int height, unsigned int color);

/**\brief 事件回调
 * evt为事件编码
 */

typedef void (*evt_callback)(int evt);



/**\brief 绘制字符
 * x,y,width,height字体所在区域
 * text,len 字符串及长度
 * color 为填充颜色
 * w_scale,h_scale 绘制比例
 */
typedef int (*draw_text)(int x, int y, unsigned int width, unsigned int height, unsigned short *text, int len, unsigned int color, unsigned int w_scale, unsigned int h_scale);

/**\brief 颜色转换
 * index颜色索引值(0:黑色;1:蓝色;2:绿色;3:青色;4:红色;5:紫色;6:黄色;15:白色;)
 */
typedef unsigned int (*convert_color)(unsigned int index);
/**\brief 获取字体高度
 * (无参数)
 */
typedef unsigned int (*get_font_height)(void);
/**\brief 获取字体宽度
 * (无参数)
 */
typedef unsigned int (*get_font_max_width)(void);
/**\brief 更新屏幕
 * (无参数)
 */
typedef unsigned int (*display_update)(void);

/**\brief 更新屏幕
 * (无参数)
 */
typedef  void (*clean_osd)(void);


/**\brief 马赛克ARGB颜色转换
 * alpha,red,green,blue颜色属性
 */
typedef unsigned int (*mosaic_convert_color)(unsigned char alpha, unsigned char red,  unsigned char green,  unsigned char blue);

/**\brief 初始化teletext 模块
 * \param  buffer_size  分配缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_init(int buffer_size);
/**\brief 卸载teletext 模块 请先调用am_mw_teletext_stop
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */

int am_mw_teletext_deinit();
/**\brief 开始处理teletext(包括数据接收与解析)
 * \param dmx_id  demux id号
 * \param pid  packet id
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_start(int pid,int dmx_id);
/**\brief 停止处理teletext
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_stop();
/**\brief 获取当前页编码
 * \return
 *   - 当前页码与字页码(低十六位为页编码,高十六位为子页编码)
 */
unsigned int am_mw_get_cur_page_code();

/**\brief 改变当前页编码
 * \param wPageCode  页编码
 * \param wSubPageCode  字页编码
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_set_cur_page_code(unsigned short wPageCode, unsigned short wSubPageCode);
/**\brief 子页下翻
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_get_next_sub_page();
/**\brief 子页上翻
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_get_pre_sub_page();
/**\brief 页下翻
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_get_next_page();

/**\brief 页上翻
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_get_pre_page();
/**\brief 进入链接页
 * \param color  颜色索引(取值0~5)
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_perform_color_link(unsigned char color);
/**\brief 注册图形相关接口函数
 * \param  fr 填充矩形函数
 * \param  dt 绘制字符函数
 * \param  cc 颜色索引转换函数 
 * \param  gfh 获取字体高度函数
 * \param  gfw 获取字体宽度函数
 * \param  mcc 颜色转换函数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_teletext_register_display_fn(fill_rectangle fr,draw_text dt,convert_color cc,get_font_height gfh,get_font_max_width gfw,display_update du,mosaic_convert_color mcc,clean_osd co);

/**\brief 注册事件回调函数
 * \param  cb 函数指针
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_mw_teletext.h)
 */
int am_mw_register_cb(evt_callback cb);











#ifdef __cplusplus
}
#endif


#endif
