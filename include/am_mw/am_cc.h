/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_cc.h
 * \brief CC模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-12-27: create the document
 ***************************************************************************/

#ifndef _AM_CC_H
#define _AM_CC_H

#include <am_types.h>

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
/**\brief CC 模块错误代码*/
enum AM_CC_ErrorCode
{
	AM_CC_ERROR_BASE=AM_ERROR_BASE(AM_MOD_CC),
	AM_CC_ERR_INVALID_PARAM,   	/**< 参数无效*/
	AM_CC_ERR_SYS,				/**< 系统错误*/
	AM_CC_ERR_NO_MEM,
	AM_CC_ERR_LIBZVBI,
	AM_CC_ERR_BUSY,
	AM_CC_ERR_END
};


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief CC 更新屏幕回调*/
typedef void* AM_CC_Handle_t;
typedef struct AM_CC_DrawPara AM_CC_DrawPara_t;
typedef void (*AM_CC_DrawBegin_t)(AM_CC_Handle_t handle, AM_CC_DrawPara_t *draw_para);
typedef void (*AM_CC_DrawEnd_t)(AM_CC_Handle_t handle, AM_CC_DrawPara_t *draw_para);

typedef enum {
    CC_STATE_RUNNING      = 0x1001,
    CC_STATE_STOP                 ,

    CMD_CC_START          = 0x2001,
    CMD_CC_STOP                   ,

    CMD_CC_BEGIN          = 0x3000,
    CMD_CC_1                      ,
    CMD_CC_2                      ,
    CMD_CC_3                      ,
    CMD_CC_4                      ,

    //this doesn't support currently
    CMD_TT_1              = 0x3005,
    CMD_TT_2                      ,
    CMD_TT_3                      ,
    CMD_TT_4                      ,

    //cc service
    CMD_SERVICE_1         = 0x4001,
    CMD_SERVICE_2                 ,
    CMD_SERVICE_3                 ,
    CMD_SERVICE_4                 ,
    CMD_SERVICE_5                 ,
    CMD_SERVICE_6                 ,
    CMD_CC_END                    ,

    CMD_SET_COUNTRY_BEGIN = 0x5000,
    CMD_SET_COUNTRY_USA           ,
    CMD_SET_COUNTRY_KOREA         ,
    CMD_SET_COUNTRY_END           ,
    /*set CC source type according ATV or DTV*/
    CMD_CC_SET_VBIDATA   = 0x7001,
    CMD_CC_SET_USERDATA ,
	
	CMD_CC_SET_CHAN_NUM = 0x8001,
	CMD_VCHIP_RST_CHGSTAT = 0x9001,

    CMD_CC_MAX
} AM_CLOSECAPTION_cmd_t;

/**\brief caption mode*/
typedef enum
{
	AM_CC_CAPTION_DEFAULT,
	/*NTSC CC channels*/
	AM_CC_CAPTION_CC1,
	AM_CC_CAPTION_CC2,
	AM_CC_CAPTION_CC3,
	AM_CC_CAPTION_CC4,
	AM_CC_CAPTION_TEXT1,
	AM_CC_CAPTION_TEXT2,
	AM_CC_CAPTION_TEXT3,
	AM_CC_CAPTION_TEXT4,
	/*DTVCC services*/
	AM_CC_CAPTION_SERVICE1,
	AM_CC_CAPTION_SERVICE2,
	AM_CC_CAPTION_SERVICE3,
	AM_CC_CAPTION_SERVICE4,
	AM_CC_CAPTION_SERVICE5,
	AM_CC_CAPTION_SERVICE6,
	AM_CC_CAPTION_MAX
}AM_CC_CaptionMode_t;

/**\brief 字体大小定义，详见 CEA-708**/
typedef enum
{
	AM_CC_FONTSIZE_DEFAULT,
	AM_CC_FONTSIZE_SMALL,
	AM_CC_FONTSIZE_STANDARD,
	AM_CC_FONTSIZE_BIG,
	AM_CC_FONTSIZE_MAX
}AM_CC_FontSize_t;

/**\brief 字体风格, 详见 CEA-708*/
typedef enum
{
	AM_CC_FONTSTYLE_DEFAULT,
	AM_CC_FONTSTYLE_MONO_SERIF,
	AM_CC_FONTSTYLE_PROP_SERIF,
	AM_CC_FONTSTYLE_MONO_NO_SERIF,
	AM_CC_FONTSTYLE_PROP_NO_SERIF,
	AM_CC_FONTSTYLE_CASUAL,
	AM_CC_FONTSTYLE_CURSIVE,
	AM_CC_FONTSTYLE_SMALL_CAPITALS,
	AM_CC_FONTSTYLE_MAX
}AM_CC_FontStyle_t;

/**\brief 颜色透明性，详见 CEA-708**/
typedef enum
{
	AM_CC_OPACITY_DEFAULT,
	AM_CC_OPACITY_TRANSPARENT,
	AM_CC_OPACITY_TRANSLUCENT,
	AM_CC_OPACITY_SOLID,
	AM_CC_OPACITY_FLASH,
	AM_CC_OPACITY_MAX
}AM_CC_Opacity_t;

/**\brief 颜色定义，目前仅支持8种，详见 CEA-708-D**/
typedef enum
{
	AM_CC_COLOR_DEFAULT,
	AM_CC_COLOR_WHITE,
	AM_CC_COLOR_BLACK,
	AM_CC_COLOR_RED,
	AM_CC_COLOR_GREEN,
	AM_CC_COLOR_BLUE,
	AM_CC_COLOR_YELLOW,
	AM_CC_COLOR_MAGENTA,
	AM_CC_COLOR_CYAN,
	AM_CC_COLOR_MAX
}AM_CC_Color_t;

/*CC绘制参数*/
struct AM_CC_DrawPara
{
	int caption_width;
	int caption_height;
};

/**\brief  CC 用户可覆盖选项*/
typedef struct
{
	AM_CC_FontSize_t        font_size;	/**< 字体大小*/
	AM_CC_FontStyle_t       font_style;	/**< 字体风格*/
	AM_CC_Color_t           fg_color;	/**< 前景色*/
	AM_CC_Opacity_t         fg_opacity;	/**< 前景色透明性*/
	AM_CC_Color_t           bg_color;	/**< 背景色*/
	AM_CC_Opacity_t         bg_opacity;	/**< 背景色透明性*/
}AM_CC_UserOptions_t;

/**\brief CC创建参数*/
typedef struct
{
	AM_CC_DrawBegin_t   draw_begin;
	AM_CC_DrawEnd_t     draw_end;
	uint8_t             *bmp_buffer;
	int                 pitch;
	void                *user_data;
}AM_CC_CreatePara_t;

/**\brief CC启动参数*/
typedef struct
{
	AM_CC_CaptionMode_t caption; /**< caption mode*/
	AM_CC_UserOptions_t	user_options;
}AM_CC_StartPara_t;


/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 创建CC
 * \param [in] para 创建参数
 * \param [out] handle 返回句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern AM_ErrorCode_t AM_CC_Create(AM_CC_CreatePara_t *para, AM_CC_Handle_t *handle);

/**\brief 销毁CC
 * \param [out] handle CC句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern AM_ErrorCode_t AM_CC_Destroy(AM_CC_Handle_t handle);

/**\brief 开始CC数据接收处理
 * \param handle CC handle
  * \param [in] para 启动参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern AM_ErrorCode_t AM_CC_Start(AM_CC_Handle_t handle, AM_CC_StartPara_t *para);

/**\brief 停止CC处理
 * \param handle CC handle
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern AM_ErrorCode_t AM_CC_Stop(AM_CC_Handle_t handle);

/**\brief 设置CC用户选项，用户选项可以覆盖运营商的设置,这些options由应用保存管理
 * \param handle CC handle
 * \param [in] options 选项集
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern AM_ErrorCode_t AM_CC_SetUserOptions(AM_CC_Handle_t handle, AM_CC_UserOptions_t *options);

/**\brief 获取用户数据
 * \param handle CC 句柄
 * \return [out] 用户数据
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_cc.h)
 */
extern void *AM_CC_GetUserData(AM_CC_Handle_t handle);

/*
** the following Interface added for new architecture of ATSC Close Caption
*/
typedef void(*AM_CC_CallBack)(char *str, int cnt, int data_buf[], int cmd_buf[], void *user_data);
typedef void(*AM_VCHIP_CallBack)(int vchip_stat, void *user_data);

extern void AM_CC_Cmd(int cmd);
extern void AM_CC_Set_CallBack(AM_CC_CallBack notify, void *user_data);
extern void AM_Set_CurrentChanNumber(int ntscchannumber);
extern void AM_VCHIP_Set_CallBack(AM_VCHIP_CallBack notify, void *user_data);

#ifdef __cplusplus
}
#endif

#endif

