#ifndef __TVIN_H
#define __TVIN_H
#include "tvin_api.h"

#define DEV_6M30_ADDR   (0x40>>1)
#define DEV_6M30_REG    0x1301
#define LEN_6M30_DATA   5

typedef enum tvin_path_id_e {
    TV_PATH_VDIN_AMVIDEO,
    TV_PATH_VDIN_DEINTERLACE_AMVIDEO,
    TV_PATH_VDIN_3D_AMVIDEO,
    TV_PATH_VDIN_NEW3D_AMVIDEO,
    TV_PATH_DECODER_3D_AMVIDEO,
    TV_PATH_DECODER_AMVIDEO,
    TV_PATH_VDIN_FREESCALE_AMVIDEO,
    TV_PATH_DECODER_NEW3D_AMVIDEO,
    TV_PATH_MAX,
} tvin_path_id_t;

typedef enum tvin_path_type_e {
    TV_PATH_TYPE_DEFAULT,
    TV_PATH_TYPE_TVIN,
    TV_PATH_TYPE_TVIN_PREVIEW,
    TV_PATH_TYPE_MAX,
} tvin_path_type_t;

typedef enum tvin_audio_channel_e {
    TV_AUDIO_LINE_IN_0,
    TV_AUDIO_LINE_IN_1,
    TV_AUDIO_LINE_IN_2,
    TV_AUDIO_LINE_IN_3,
    TV_AUDIO_LINE_IN_4,
    TV_AUDIO_LINE_IN_5,
    TV_AUDIO_LINE_IN_6,
    TV_AUDIO_LINE_IN_7,
    TV_AUDIO_LINE_IN_MAX,
} tvin_audio_channel_t;

typedef enum tvin_source_input_e {
    SOURCE_TV,
    SOURCE_AV1,
    SOURCE_AV2,
    SOURCE_YPBPR1,
    SOURCE_YPBPR2,
    SOURCE_HDMI1,
    SOURCE_HDMI2,
    SOURCE_HDMI3,
    SOURCE_VGA,
    SOURCE_MPEG,
    SOURCE_DTV,
    SOURCE_SVIDEO,
    SOURCE_MAX,
} tvin_source_input_t;

typedef enum tvin_audio_input_e {
    SOURCE_AUDIO_TV,
    SOURCE_AUDIO_AV1,
    SOURCE_AUDIO_AV2,
    SOURCE_AUDIO_YPBPR1,
    SOURCE_AUDIO_YPBPR2,
    SOURCE_AUDIO_HDMI1,
    SOURCE_AUDIO_HDMI2,
    SOURCE_AUDIO_HDMI3,
    SOURCE_AUDIO_VGA,
    SOURCE_AUDIO_MPEG,
    SOURCE_AUDIO_DTV,
    SOURCE_AUDIO_SVIDEO,
    SOURCE_AUDIO_MAX,
} tvin_audio_input_t;

typedef enum tvin_source_input_type_e {
    SOURCE_TYPE_TV,
    SOURCE_TYPE_AV,
    SOURCE_TYPE_COMPONENT,
    SOURCE_TYPE_HDMI,
    SOURCE_TYPE_VGA,
    SOURCE_TYPE_MPEG,
    SOURCE_TYPE_DTV,
    SOURCE_TYPE_SVIDEO,
    SOURCE_TYPE_MAX,
} tvin_source_input_type_t;

typedef enum tvin_status_e {
    TVIN_STATUS_IDLE,
    TVIN_STATUS_PREVIEW_START,
    TVIN_STATUS_PREVIEW_STOP,
    TVIN_STATUS_NORMAL_START,
    TVIN_STATUS_NORMAL_STOP,
    TVIN_STATUS_MAX,
} tvin_status_t;

typedef enum tvin_adc_calibration_input_e {
    ADC_CALIBRATION_INPUT_VGA,
    ADC_CALIBRATION_INPUT_YPBPR1,
    ADC_CALIBRATION_INPUT_YPBPR2,
    ADC_CALIBRATION_INPUT_MAX,
} tvin_adc_calibration_input_t;

typedef enum tvin_fmt_ratio_e {
    RATIO_43,
    RATIO_169,
    RATIO_MAX,
} tvin_fmt_ratio_t;

typedef enum tvin_color_fmt_e {
    COLOR_RGB444,
    COLOR_YUV422,
    COLOR_YUV444,
    COLOR_MAX,
} tvin_color_fmt_t;

typedef enum tvin_3d_status_e {
    STATUS3D_DISABLE,
    STATUS3D_AUTO,
    STATUS3D_2D_TO_3D,
    STATUS3D_LR,
    STATUS3D_BT,
    STATUS3D_AUTO_LR,
    STATUS3D_AUTO_BT,
    STATUS3D_MAX,
} tvin_3d_status_t;

typedef enum tvin_3d_mode_e {
    MODE3D_DISABLE,
    MODE3D_AUTO,
    MODE3D_2D_TO_3D,
    MODE3D_LR,
    MODE3D_BT,
    MODE3D_OFF_LR_SWITCH,
    MODE3D_ON_LR_SWITCH,
    MODE3D_FIELD_DEPTH,
    MODE3D_OFF_3D_TO_2D,
    MODE3D_L_3D_TO_2D,
    MODE3D_R_3D_TO_2D,
    MODE3D_OFF_LR_SWITCH_BT,
    MODE3D_ON_LR_SWITCH_BT,
    MODE3D_OFF_3D_TO_2D_BT,
    MODE3D_L_3D_TO_2D_BT,
    MODE3D_R_3D_TO_2D_BT,
    MODE3D_MAX,
} tvin_3d_mode_t;

typedef enum MODE_3D_2D_FOR_H03{
    MODE_3D_2D_OFF,
	MODE_3D_2D_SIDE_BY_SIDE,
	MODE_3D_2D_TOP_BOTTOM,
	MODE_3D_2D_FRAME_PACKING,
	MODE_3D_2D_ALTERNATIVE,
	MODE_3D_2D_AUTO,
	MODE_3D_2D_MAX,
}tvin_3d_2d_for_h03;


typedef enum tvin_vga_auto_adjust_status_e {
    TV_VGA_AUTO_ADJUST_STATUS_START,
    TV_VGA_AUTO_ADJUST_STATUS_PROCESSING,
    TV_VGA_AUTO_ADJUST_STATUS_STOP,
} tvin_vga_auto_adjust_status_t;

typedef enum tvin_adc_auto_calibration_status_e {
    TV_ADC_AUTO_CALIBRATION_INIT,
    TV_ADC_AUTO_CALIBRATION_YPBPR1,
    TV_ADC_AUTO_CALIBRATION_YPBPR1_START,
    TV_ADC_AUTO_CALIBRATION_YPBPR1_WAIT,
    TV_ADC_AUTO_CALIBRATION_YPBPR2,
    TV_ADC_AUTO_CALIBRATION_YPBPR2_START,
    TV_ADC_AUTO_CALIBRATION_YPBPR2_WAIT,
    TV_ADC_AUTO_CALIBRATION_VGA,
    TV_ADC_AUTO_CALIBRATION_VGA_START,
    TV_ADC_AUTO_CALIBRATION_VGA_WAIT,
    TV_ADC_AUTO_CALIBRATION_END,
    TV_ADC_AUTO_CALIBRATION_FAILED,
} tvin_adc_auto_calibration_status_t;

typedef enum tvin_product_id_s {
    TV_PRODUCT_E00REF,
    TV_PRODUCT_E01REF,
    TV_PRODUCT_E02REF,
    TV_PRODUCT_E03REF,
    TV_PRODUCT_E04REF,
    TV_PRODUCT_E05REF,
    TV_PRODUCT_E06REF,
    TV_PRODUCT_E07REF,
    TV_PRODUCT_E08REF,
    TV_PRODUCT_E09REF,
    TV_PRODUCT_E10REF,
    TV_PRODUCT_E11REF,
    TV_PRODUCT_E12REF,
    TV_PRODUCT_E13REF,
    TV_PRODUCT_E14REF,
    TV_PRODUCT_E15REF,
    TV_PRODUCT_E16REF,
    TV_PRODUCT_E17REF,
    TV_PRODUCT_E18REF,
    TV_PRODUCT_E19REF,
    TV_PRODUCT_E20REF,
    TV_PRODUCT_H00REF,
    TV_PRODUCT_H01REF,
    TV_PRODUCT_H02REF,
    TV_PRODUCT_H03REF,
    TV_PRODUCT_H04REF,
    TV_PRODUCT_H05REF,
    TV_PRODUCT_MAX,
} tvin_product_id_t;


typedef enum tvin_window_mode_e {
    NORMAL_WONDOW,
    PREVIEW_WONDOW,
} tvin_window_mode_t;

typedef struct tvin_api_para_s {
    struct tvin_parm_s tvin_para;
    enum tvin_status_e tvin_status;
    enum tvin_status_e tvin_pre_status;
    enum tvin_source_input_e source_input;
    enum tvin_source_input_e last_source_input;
    enum tvin_audio_input_e source_audio_input;
    enum tvin_path_id_e pathid;
    enum tvin_audio_channel_e audio_channel;
    enum tvin_fmt_ratio_e fmt_ratio;
    enum tvin_color_fmt_e color_fmt;
    enum tvin_color_system_e av_color_system;
    int input_window;
    int atv_port;
    int av1_port;
    int av2_port;
    int ypbpr1_port;
    int ypbpr2_port;
    int hdmi1_port;
    int hdmi2_port;
    int hdmi3_port;
    int vga_port;
    /** for L/R or B/T 3d mode overscan **/
    int cut_top;
    int cut_left;
    /** for 3D **/
    enum tvin_3d_mode_e mode_3d;
    enum tvin_3d_status_e status_3d;
    /** for signal detect and source switch status **/
    bool is_decoder_start;
    bool is_turnon_signal_detect_thread;
    bool is_signal_detect_thread_start;
    bool is_signal_detect_exec_done;
    bool is_turnon_source_switch_thread;
    bool is_source_switch_thread_start;
    bool is_source_switch_exec_done;
    bool is_turnon_overscan;
    bool is_suspend_source_signal_detect;
    /** for HDMI-in sampling detection. **/
    bool is_hdmi_sr_detect_start;
    int  hdmi_sampling_rate;
    /* for source switch signal stable check counter */
    unsigned int source_switch_donecount;
    unsigned int stable_count;
    unsigned int check_stable_count;
    unsigned int pre_check_stable_count;
    /* for adc calibration status*/
    bool is_turnon_adc_autocalibration_thread;
    bool is_adc_autocalibration_start;
    tvin_adc_auto_calibration_status_t adc_autocalibration_status;
    /* for tv product*/
    enum tvin_product_id_s product_id;
    /* for mpeg flag*/
    int is_mpeg;
    int mpeg_port_xscale_offset;
    int mpeg_port_yscale_offset;
} tvin_api_para_t;

typedef struct tvin_sig_detect_para_s {
    enum tvin_sig_fmt_e pre_fmt;
    enum tvin_trans_fmt pre_trans_fmt;
    enum tvin_sig_status_e pre_sig_status;
} tvin_sig_detect_para_t;

typedef struct tvin_vga_auto_adjust_para_s {
    tvin_vga_auto_adjust_status_t vga_auto_adjust_status;
    tvafe_cmd_status_t vga_auto_adjust_cmd_status;
    unsigned char vga_auto_adjust_status_check_count;
    unsigned char vga_auto_adjust_force_start;
    tvafe_vga_parm_t vga_auto_adjust_para;
} tvin_vga_auto_adjust_para_t;

typedef struct tvin_src_switch_para_s {
    tvin_port_t port;
    int input_window;
    tvin_audio_channel_t audio_channel;
} tvin_src_switch_para_t;

typedef struct tvin_config_s {
    unsigned int dvi_audio_vga;
    unsigned int pin_ctrl_3D;
    unsigned int peripheral_3D_6M30;
    unsigned int autoset_displayfreq;
    unsigned int hdmi_eq;
    unsigned int atv_keeplastframe;
    unsigned int multi_db;
    unsigned int overscan_3d;
    unsigned int tv_out_counter;
    unsigned int vag_force_adjust;
    unsigned int hdmi_auto3d;
    unsigned int preview_freescale;
    unsigned int source_pg_lock;
    unsigned int adccal_autoswitchsource;
    unsigned int checkfs;
    unsigned int userpet;
    unsigned int userpet_timeout;
    unsigned int userpet_reset;
    unsigned int autoreboot;
    unsigned int seria_debug;
    unsigned int socket_interface;
    unsigned int new_d2d3;
    unsigned int new_overscan;
    unsigned int new_2d3ddepth;
    unsigned int new_3dautodetc;
	unsigned int new_set3dfunction;
    unsigned int source_detectfunction;
	unsigned int glass_3d_enable;

	unsigned int depth_reverse;
} tvin_config_t;

extern tvin_source_input_t Tvin_GetLastSourceInput(void);
extern int Tv_Open(void);
extern int Tv_Close(void);
extern int Tv_Start(int mode);
extern int Tv_Stop(void);
extern int Tv_AudioPause(void);
extern int Tv_AudioResume(void);
extern int Tv_SetInputSourceAudioChannelIndex(tvin_port_t source_port, int audio_channel_ind);
extern tvin_audio_channel_t Tv_GetInputSourceAudioChannelIndex(tvin_port_t source_port);
extern int Tv_SetSourceSwitchInput(int window_sel, tvin_source_input_t source_input, tvin_audio_channel_t audio_channel);
extern tvin_port_t Tv_GetSourceSwitchInput(void);
extern int Tv_IsSourceSwtichDone(void);
extern tvin_source_input_t Tv_GetCurrentSourceInput(void);
extern tvin_source_input_t Tv_GetLastSourceInput(void);
extern tvin_info_t Tv_GetCurrentSignalInfo(void);
extern tvin_status_t Tv_GetTvStatus(void);
extern int Tv_Set3DMode(tvin_3d_mode_t mode);
extern int Tv_Set3DModeWithoutSave(tvin_3d_mode_t mode);
extern int Tv_Save3DMode(tvin_3d_mode_t mode);
extern tvin_3d_mode_t Tv_Get3DMode(void);
extern int Tv_Set3DLRSwith(int on_off, tvin_3d_status_t status);
extern int Tv_Get3DLRSwith(void);
extern int Tv_Set3DTo2DMode(int mode, tvin_3d_status_t status);
extern int Tv_Set3DTo2DModeWithoutSave(int mode, tvin_3d_status_t status);
extern int Tv_Save3DTo2DMode(int mode, tvin_3d_status_t status);
extern int Tv_Get3DTo2DMode(void);
extern int Tv_Set3DDepth(int value);
extern int Tv_Set3DDepthWithoutSave(int value);
extern int Tv_Save3DDepth(int value);
extern int Tv_Get3DDepth(void);
extern int Tv_RunVGAAutoAdjust(void);
extern int Tv_SetVGAAjustPara(tvafe_vga_parm_t adjparam, tvin_sig_fmt_t vga_fmt);
extern int Tv_GetVGAAjustPara(tvafe_vga_parm_t *adjparam, tvin_sig_fmt_t vga_fmt);
extern tvafe_vga_parm_t Tv_AdjustParmSet(tvafe_vga_parm_t adjparam, unsigned char buff[]);
extern int Tv_SetVGAHPos(int value, tvin_sig_fmt_t vga_fmt);
extern int Tv_GetVGAHPos(tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAHPosWithoutSave(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SaveVGAHPos(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAVPos(int value, tvin_sig_fmt_t vga_fmt);
extern int Tv_GetVGAVPos(tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAVPosWithoutSave(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SaveVGAVPos(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAClock(int value, tvin_sig_fmt_t vga_fmt);
extern int Tv_GetVGAClock(tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAClockWithoutSave(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SaveVGAClock(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAPhase(int value, tvin_sig_fmt_t vga_fmt);
extern int Tv_GetVGAPhase(tvin_sig_fmt_t vga_fmt);
extern int Tv_SetVGAPhaseWithoutSave(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_SaveVGAPhase(int value,tvin_sig_fmt_t vga_fmt);
extern int Tv_IsVGAAutoAdjustDone(tvin_sig_fmt_t vga_fmt);
extern int Tv_RunADCAutoCalibration(void);
extern int Tv_IsADCAutoCalibrationDone(void);
extern int TvinSetADCAutoCalibration_Result();
extern tvin_vga_auto_adjust_status_t Tv_GetVagAutoAdjustStatus(void);
extern int Tv_SetTvinParamDefault(void);
extern int Tv_KeepLastFrame(int enable);
extern int Tv_SetCVBSStd(tvin_color_system_e std_val);
extern tvin_color_system_e Tv_GetCVBSStd();
extern int Tv_SetMpegFlag(int is_mpeg);

extern tvin_source_input_type_t Tvin_GetSrcInputType();
extern int TvinSSMRestoreDefault();
extern int TvinSSMFacRestoreDefault();
extern tvin_3d_status_t Tvin_Get3DStatus(void);
extern tvin_sig_fmt_t Tvin_GetSigFormat();
extern tvin_trans_fmt_t Tvin_GetSigTransFormat();
extern void Tvin_TurnOnBlueScreen(int type);
extern int Tvin_StopDecoder(int windowSel);
extern void Tvin_SetCheckStableCount(int count);
extern tvin_source_input_t Tvin_GetSourceInput(void);
extern tvin_sig_status_t Tvin_GetSigStatus();
extern int Tvin_IsDVISignal();
extern int Tvin_isVgaFmtInHdmi();
extern tvin_port_t Tvin_GetSrcPort(void);
extern tvin_port_t Tvin_GetSourcePortBySourceType(tvin_source_input_type_t source_type);
extern tvin_port_t Tvin_GetSourcePortBySourceInput(tvin_source_input_t source_input);
extern int Tv_FactorySetOverscan(tvin_source_input_type_t source_type, tvin_sig_fmt_t fmt, tvin_3d_status_t status, tvin_trans_fmt_t trans_fmt, tvin_cutwin_t cutwin_t);
extern tvin_cutwin_t Tv_FactoryGetOverscan(tvin_source_input_type_t source_type, tvin_sig_fmt_t fmt, tvin_3d_status_t status, tvin_trans_fmt_t trans_fmt);
extern int TvinADCGainOffsetInit(tvin_adc_calibration_input_t input, tvafe_adc_cal_s *adc_cal_parm);
extern int TvinADCYPbPrGainOffsetInit(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t *adc_cal_parm);
extern int TvinADCAutoCalibration_SavePara(tvin_adc_calibration_input_t input, tvafe_adc_cal_s adc_cal_parm);
extern int TvinADCAutoCalibration_GetPara(tvin_adc_calibration_input_t input, tvafe_adc_cal_s *adc_cal_parm);
extern int TvinADCYPbPrAutoCalibration_SavePara(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t adc_cal_parm);
extern int TvinADCPbPrAutoCalibration_GetPara(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t *adc_cal_parm);
extern tvin_product_id_t Tvin_GetTvProductId(void);
extern void Tvin_SetDisplayModeFor3D(int mode3D_on_off);
extern int TvinAudioSwitch(tvin_source_input_t source_input);
extern int TvinResetVgaAjustParam(void);
extern bool Tvin_StartSigDetect(void);
extern int Tvin_StopSigDetect(void);
extern tvin_status_t Tvin_GetTvinStatus(void);
extern int Tvin_IsMpeg(void);
extern int Tv_GetMpegPortxScaleOffset(void);
extern void Tv_SetMpegPortxScaleOffset(int offset);
extern int Tv_GetMpegPortyScaleOffset(void);
extern void Tv_SetMpegPortyScaleOffset(int offset);
extern int Tvin_GetSourceLocked(void);
extern int Tv_StartPlay();
extern int Tv_StopPlay();
extern void GetSoftwareVersion(unsigned char *data);
extern void GetConsoleStatus(void) ;
extern int GetSkyModuleName(void);
extern int SSMReadPowerOnOffChannel();
extern int Tv_Set3DAutoDectect(int on_off);
extern int Tv_Get3DAutoDectect();
extern int Tv_Update3DAutoDectect(int on_off);
extern int Tvin_Set3DFunction(tvin_3d_mode_t mode);
extern void Tvin_SuspendSourceDetect(bool flag );
extern int Tv_Set3DTo2DModeforh03(int mode);
extern int Tv_Get3DTo2DModeforh03();

void Tvin_AddPath(tvin_path_id_t pathid);
void Tvin_RemovePath(tvin_path_type_t pathtype);
#endif
