#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/types.h>
#include <errno.h>
#include <dlfcn.h>
#include <cutils/properties.h>
#include <cutils/log.h>
#include <cutils/android_reboot.h>
#include <utils/threads.h>
#include <time.h>
#include <sys/prctl.h>

#include "../ssm/ssm_api.h"
//#include "../audioctl/audioctl_api.h"
//#include "../audioctl/audioctl_cfg.h"
#include "../audioctl/audio_effect_ctl.h"
#include "../tvinterface/tvinterface_api.h"
#include "../vpp/vpp.h"
#include "../vpp/vpp_table.h"
#include "../atv/atv_api.h"
#include "../database/pqdata.h"
#include "../tvmisc/tvmisc.h"
#include "../tvmisc/CPQdb.h"
#include "../serialcmd/cmdmain.h"
#include "../tvconfig/tvconfig_api.h"
#include "../tvconfig/tvconfig_logcfg.h"
#include "tvin.h"
#include <hardware_legacy/power.h>
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "Tvin"
#endif

#define ALOGD(...) \
    if( config_log_cfg_get(CC_LOG_MODULE_TVIN) & CC_LOG_CFG_DEBUG) { \
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__); }\
 
#define ALOGW(...) \
    if( config_log_cfg_get(CC_LOG_MODULE_TVIN) & CC_LOG_CFG_ERROR) { \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); }\
 
Mutex start_Lock;
Mutex source_switch_lock;

tvin_api_para_t gTvinApiPara;
tvafe_adc_cal_s ADCGainOffset_VGA;
tvafe_adc_comp_cal_t ADCGainOffset_Ypbpr1;
tvafe_adc_comp_cal_t ADCGainOffset_Ypbpr2;

unsigned char data_buf_3DON[5] = {0xf0, 0x0, 0x0, 0x0, 0x0};
unsigned char data_buf_3DOFF[5] = {0x00, 0x0, 0x0, 0x0, 0x0};

int auto_detect_3d = -1;

static int mTvStartedFlag = 0;
static int mTvStopFlag = 0;
static tvin_config_t gTvinConfig;
static char config_3d_pin_enable[64];
static char config_3d_pin_disable[64];
static char config_preview_video_axis[64];
static char config_tv_path[64];
static char config_default_path[64];
static char config_3d_glass_enable[64];
static char config_3d_glass_reset[64];
static tvin_3d_2d_for_h03 Mode3D2Dforh03;


static int SetAudioLineInCaptureVolume(tvin_port_t curPort);
static int get_hdmi_sampling_rate();

typedef void (*GetHandle)();
GetHandle  DrvDynaBacklight= NULL;

tvin_audio_channel_t Tv_GetSourceSwitchAudioChannel(void);
int TvinSourceSwitch_RealSwitchSource(int window_sel, tvin_port_t source_port, tvin_audio_channel_e audio_channel);

//only for tcl source detect
#define MSG_SOURCE_CHANGE  0x0111
#define MSG_SIGNAL_STATUS_UPDATED 0x0112
#define MSG_SIGNAL_AUTO_SWITCH 0x113
#define MSG_CURRENT_SOURCE_PLUG_OUT 0x114
#define MAX_SOURCE  9

static pthread_t TvSourceDetecthreadId = 0;
int  source_mapping_table[MAX_SOURCE];
int  sourceDetectIndex = 0;
static bool  bCurDetResult[MAX_SOURCE];
static bool  bPreDetResult[MAX_SOURCE];
static bool  bMappingInitFinish = false;

//-----------------------------
void int_source_mapping_table()
{

    memset(bCurDetResult, '\0', sizeof(bCurDetResult));
    memset(bPreDetResult, '\0', sizeof(bPreDetResult));

    source_mapping_table[0] = gTvinApiPara.atv_port;
    source_mapping_table[1]=  gTvinApiPara.av1_port;
    source_mapping_table[2] = gTvinApiPara.av2_port;
    source_mapping_table[3] = gTvinApiPara.ypbpr1_port;
    source_mapping_table[4] = gTvinApiPara.ypbpr2_port;
    source_mapping_table[5] = gTvinApiPara.hdmi1_port;
    source_mapping_table[6] = gTvinApiPara.hdmi2_port;
    source_mapping_table[7] = gTvinApiPara.hdmi3_port;
    source_mapping_table[8] = gTvinApiPara.vga_port;
    bMappingInitFinish  =  true ;
    gTvinApiPara.is_suspend_source_signal_detect = false;
    TvinApi_OpenPinMuxOn(true);//first open PinMux or hdmi can not read connet status
}
static int get_hdmi_sampling_rate()
{
    int fd;
    int val = 0;
    char  bcmd[16];
    fd = open("/sys/module/tvin_hdmirx/parameters/audio_sample_rate", O_RDONLY);
    if (fd >= 0) {
        read(fd, bcmd, sizeof(bcmd));
        val = strtol(bcmd, NULL, 10);
        close(fd);
    }
    return val;
}

tvin_product_id_t Tvin_GetTvProductId(void)
{
    return gTvinApiPara.product_id;
}

tvin_port_t Tvin_GetSrcPort(void)
{
    return gTvinApiPara.tvin_para.port;
}

void Tvin_SetSrcPort(tvin_port_t port)
{
    gTvinApiPara.tvin_para.port = port;
}

tvin_sig_fmt_t Tvin_GetSigFormat()
{
    return gTvinApiPara.tvin_para.info.fmt;
}

void Tvin_SetSigFormat(tvin_sig_fmt_t fmt)
{
    gTvinApiPara.tvin_para.info.fmt = fmt;
}

tvin_trans_fmt_t Tvin_GetSigTransFormat()
{
    return gTvinApiPara.tvin_para.info.trans_fmt;
}

tvin_sig_status_t Tvin_GetSigStatus()
{
    return gTvinApiPara.tvin_para.info.status;
}

int Tvin_IsDVISignal()
{
    ALOGD("%s, reserved[0x%x].", __FUNCTION__, gTvinApiPara.tvin_para.info.reserved);
    return (gTvinApiPara.tvin_para.info.reserved&0x1);
}

tvin_info_t Tvin_GetSigInfo()
{
    return gTvinApiPara.tvin_para.info;
}

tvin_source_input_type_t Tvin_GetSrcInputType()
{
	if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ){
		if ( Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_STOP ) {
    		ALOGE("%s, H02REF, preview-STOP, return MEPG type !!", __FUNCTION__);
			return SOURCE_TYPE_MPEG;
		}
	}
	
    if (Tvin_GetSrcPort() == TVIN_PORT_CVBS0) {
        return SOURCE_TYPE_TV;
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_CVBS1 && Tvin_GetSrcPort() <= TVIN_PORT_SVIDEO7) {
        return SOURCE_TYPE_AV;
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_COMP0 && Tvin_GetSrcPort() <= TVIN_PORT_COMP7) {
        return SOURCE_TYPE_COMPONENT;
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_VGA0 && Tvin_GetSrcPort() <= TVIN_PORT_VGA7) {
        return SOURCE_TYPE_VGA;
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_HDMI0 && Tvin_GetSrcPort() <= TVIN_PORT_HDMI7) {
        return SOURCE_TYPE_HDMI;
    } else if (Tvin_GetSrcPort() == TVIN_PORT_DTV) {
        if (Tvin_IsMpeg() != 1) {
            return SOURCE_TYPE_DTV;
        } else {
            ALOGD("%s, you will get mpeg source type, though source port is dtv in deactive.", __FUNCTION__);
        }
    }
    return SOURCE_TYPE_MPEG;
}

int Tvin_GetFmtRatio()
{
    if (Tvin_GetSrcPort() >= TVIN_PORT_VGA0 && Tvin_GetSrcPort() <= TVIN_PORT_VGA7) {
        gTvinApiPara.fmt_ratio = RATIO_169;
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_COMP0 && Tvin_GetSrcPort() <= TVIN_PORT_COMP7) {
        if ((gTvinApiPara.tvin_para.info.fmt >= TVIN_SIG_FMT_COMP_480P_60HZ_D000) && (gTvinApiPara.tvin_para.info.fmt <= TVIN_SIG_FMT_COMP_576I_50HZ_D000)) {
            gTvinApiPara.fmt_ratio = RATIO_43;
            ALOGD("%s, Component format ratio(4:3).", __FUNCTION__);
        } else {
            gTvinApiPara.fmt_ratio = RATIO_169;
            ALOGD("%s, Component format ratio(16:9).", __FUNCTION__);
        }
    } else if (Tvin_GetSrcPort() >= TVIN_PORT_CVBS0 && Tvin_GetSrcPort() <= TVIN_PORT_SVIDEO7) {
        gTvinApiPara.fmt_ratio = RATIO_43;
        ALOGD("%s, Cvbs format ratio(4:3).", __FUNCTION__);
    } else {
        gTvinApiPara.fmt_ratio = RATIO_169;
    }

    if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
        gTvinApiPara.fmt_ratio = RATIO_169;
        ALOGD("%s, MPEG set 16:9.", __FUNCTION__);
    }
    return gTvinApiPara.fmt_ratio;
}

tvin_source_input_t Tvin_GetSourceInput(void)
{
    return gTvinApiPara.source_input;
}

tvin_source_input_t Tvin_GetLastSourceInput(void)
{
    return gTvinApiPara.last_source_input;
}

void Tvin_SetSourceInput(int port)
{
    gTvinApiPara.last_source_input = gTvinApiPara.source_input;
    gTvinApiPara.is_mpeg = 0;
    if (port == gTvinApiPara.atv_port)
        gTvinApiPara.source_input = SOURCE_TV;
    else if (port == gTvinApiPara.av1_port)
        gTvinApiPara.source_input = SOURCE_AV1;
    else if (port == gTvinApiPara.av2_port)
        gTvinApiPara.source_input = SOURCE_AV2;
    else if (port == gTvinApiPara.ypbpr1_port)
        gTvinApiPara.source_input = SOURCE_YPBPR1;
    else if (port == gTvinApiPara.ypbpr2_port)
        gTvinApiPara.source_input = SOURCE_YPBPR2;
    else if (port == gTvinApiPara.hdmi1_port)
        gTvinApiPara.source_input = SOURCE_HDMI1;
    else if (port == gTvinApiPara.hdmi2_port)
        gTvinApiPara.source_input = SOURCE_HDMI2;
    else if (port == gTvinApiPara.hdmi3_port)
        gTvinApiPara.source_input = SOURCE_HDMI3;
    else if (port == gTvinApiPara.vga_port)
        gTvinApiPara.source_input = SOURCE_VGA;
    else if (port == TVIN_PORT_DTV) {
        gTvinApiPara.source_input = SOURCE_DTV;
    } else {
        gTvinApiPara.source_input = SOURCE_MPEG;
        gTvinApiPara.is_mpeg = 1;
    }

    SSMMacAddressCheck();
    ALOGD("%s, source_input[%d], last_source_input[%d].", __FUNCTION__, gTvinApiPara.source_input, gTvinApiPara.last_source_input);
}

void Tvin_SetSourceAudioInput(tvin_audio_input_t source_audio_input)
{
    gTvinApiPara.source_audio_input = source_audio_input;
}

tvin_audio_input_t Tvin_GetSourceAudioInput(void)
{
    return gTvinApiPara.source_audio_input;
}

tvin_port_t Tvin_GetSourcePortBySourceType(tvin_source_input_type_t source_type)
{
    tvin_port_t source_port;
    switch (source_type) {
    case SOURCE_TYPE_TV:
        source_port = TVIN_PORT_CVBS0;
        break;
    case SOURCE_TYPE_AV:
        source_port = TVIN_PORT_CVBS1;
        break;
    case SOURCE_TYPE_COMPONENT:
        source_port = TVIN_PORT_COMP0;
        break;
    case SOURCE_TYPE_HDMI:
        source_port = TVIN_PORT_HDMI0;
        break;
    case SOURCE_TYPE_VGA:
        source_port = TVIN_PORT_VGA0;
        break;
    case SOURCE_TYPE_MPEG:
    default:
        source_port = TVIN_PORT_MPEG0;
        break;
    }
    return source_port;
}

tvin_port_t Tvin_GetSourcePortBySourceInput(tvin_source_input_t source_input)
{
    tvin_port_t source_port;
    if ((source_input < SOURCE_TV || source_input >= SOURCE_MAX) && (source_input != SOURCE_MPEG)) {
        ALOGW("%s, it is not a tvin input!", __FUNCTION__);
    } else if (source_input == SOURCE_MPEG) {
        ALOGW("%s, it is not a tvin input [%d]!", __FUNCTION__, source_input);
        source_input = (tvin_source_input_t) SSMReadLastSelectSourceType();
        ALOGW("%s, but I guess you want to switch to source type [%d] :-)", __FUNCTION__, source_input);
    }
    switch (source_input) {
    case SOURCE_TV:
        source_port = (tvin_port_t)gTvinApiPara.atv_port;
        break;
    case SOURCE_AV1:
        source_port = (tvin_port_t)gTvinApiPara.av1_port;
        break;
    case SOURCE_AV2:
        source_port = (tvin_port_t)gTvinApiPara.av2_port;
        break;
    case SOURCE_YPBPR1:
        source_port = (tvin_port_t)gTvinApiPara.ypbpr1_port;
        break;
    case SOURCE_YPBPR2:
        source_port = (tvin_port_t)gTvinApiPara.ypbpr2_port;
        break;
    case SOURCE_HDMI1:
        source_port = (tvin_port_t)gTvinApiPara.hdmi1_port;
        break;
    case SOURCE_HDMI2:
        source_port = (tvin_port_t)gTvinApiPara.hdmi2_port;
        break;
    case SOURCE_HDMI3:
        source_port = (tvin_port_t)gTvinApiPara.hdmi3_port;
        break;
    case SOURCE_VGA:
        source_port = (tvin_port_t)gTvinApiPara.vga_port;
        break;
    case SOURCE_MPEG:
    default:
        source_port = TVIN_PORT_MPEG0;
        break;
    }
    return source_port;
}

int Tvin_IsMpeg(void)
{
    if (gTvinApiPara.is_mpeg == 1) {
        return 1;
    }
    return 0;
}

void Tvin_SetAudioChannel(tvin_audio_channel_t audio_channel)
{
    gTvinApiPara.audio_channel = audio_channel;
}

tvin_audio_channel_t Tvin_GetAudioChannel(void)
{
    return gTvinApiPara.audio_channel;
}

tvin_color_system_t Tvin_GetAvColorSys(void)
{
    return gTvinApiPara.av_color_system;
}

void Tvin_SetAvColorSys(tvin_color_system_t val)
{
    gTvinApiPara.av_color_system = val;
}

int Tvin_GetInputWindow(void)
{
    return gTvinApiPara.input_window;
}

void Tvin_SetInputWindow(int input)
{
    gTvinApiPara.input_window = input;
}

bool Tvin_IsDeinterlaceFmt(void)
{
    if (gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_480I_59HZ_D940
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_576I_50HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_47HZ_D952
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_48HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_A
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_B
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_C
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_60HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X480I_120HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X480I_240HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X480I_60HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X576I_100HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X576I_200HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X576I_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_100HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_120HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_60HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_2880X480I_60HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_2880X576I_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_NTSC_M
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_NTSC_443
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_60
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_CN
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_M
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_SECAM) {
        ALOGD("%s, Interlace format.", __FUNCTION__);
        return true;
    } else {
        ALOGD("%s, Progressive format.", __FUNCTION__);
        return false;
    }
}

bool Tvin_is50HzFrameRateFmt(void)
{
    /** component **/
    if (gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_576P_50HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_576I_50HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_720P_50HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080P_50HZ_D000
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_A
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_B
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_COMP_1080I_50HZ_D000_C
        /** hdmi **/
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_720X576P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1280X720P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X576I_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X288P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_2880X576I_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_2880X288P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1440X576P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080P_50HZ
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1280X720P_50HZ_FRAME_PACKING
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING
        /** cvbs **/
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_M
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_PAL_CN
        || gTvinApiPara.tvin_para.info.fmt == TVIN_SIG_FMT_CVBS_SECAM) {
        ALOGD("%s, Frame rate == 50Hz.", __FUNCTION__);
        return true;
    } else {
        ALOGD("%s, Frame rate != 50Hz.", __FUNCTION__);
        return false;
    }
}

int Tvin_isVgaFmtInHdmi(void)
{
    tvin_sig_fmt_t fmt = Tvin_GetSigFormat();

    if (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
        if (fmt == TVIN_SIG_FMT_HDMI_640X480P_60HZ
            || fmt == TVIN_SIG_FMT_HDMI_800X600_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1024X768_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_720X400_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1280X768_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1280X800_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1280X960_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1280X1024_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1360X768_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1366X768_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1600X1200_00HZ
            || fmt == TVIN_SIG_FMT_HDMI_1920X1200_00HZ) {
            ALOGD("%s, HDMI source : VGA format.", __FUNCTION__);
            return 1;
        }
    } else {
        ALOGD("%s, HDMI source : not VGA format.", __FUNCTION__);
        return -1;
    }

    return -1;
}

#ifndef NELEM
# define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

static ItfInfo gMethods[] = {
//Tvin interface methods
    { (char*) "Set3DMode", (char*) "(I)I", (void *) Tv_Set3DMode }, // Set3DMode
    { (char*) "Get3DMode", (char*) "()I", (void *) Tv_Get3DMode }, // Get3DMode
    { (char*) "SetVGAPhase", (char*) "(II)I", (void *) Tv_SetVGAPhase }, // SetVGAPhase
    { (char*) "GetVGAPhase", (char*) "(I)I", (void *) Tv_GetVGAPhase }, // GetVGAPhase
    { (char*) "SetVGAClock", (char*) "(II)I", (void *) Tv_SetVGAClock }, // SetVGAClock
    { (char*) "GetVGAClock", (char*) "(I)I", (void *) Tv_GetVGAClock }, // GetVGAClock
    { (char*) "SetVGAHPos", (char*) "(II)I", (void *) Tv_SetVGAHPos }, // SetVGAHPos
    { (char*) "GetVGAHPos", (char*) "(I)I", (void *) Tv_GetVGAHPos }, // GetVGAHPos
    { (char*) "SetVGAVPos", (char*) "(II)I", (void *) Tv_SetVGAVPos }, // SetVGAVPos
    { (char*) "GetVGAVPos", (char*) "(I)I", (void *) Tv_GetVGAVPos }, // GetVGAVPos
    { (char*) "RunVGAAutoAdjust", (char*) "()I", (void *) Tv_RunVGAAutoAdjust }, //RunVGAAutoAdjust
};

int TvinRegisterInterface()
{
    return tv_itf_reg(gMethods, NELEM(gMethods));
}

void Tvin_SetCheckStableCount(int count)
{
    gTvinApiPara.check_stable_count = count;
}

bool Tvin_IsSrcSwitchExecDone(void)
{
    return gTvinApiPara.is_source_switch_exec_done;
}

bool Tvin_IsSigDetectExecDone(void)
{
    return gTvinApiPara.is_signal_detect_exec_done;
}

tvin_status_t Tvin_GetTvinStatus(void)
{
    return gTvinApiPara.tvin_status;
}

tvin_status_t Tvin_GetTvinPrePlayStatus(void)
{
    return gTvinApiPara.tvin_pre_status;
}

bool Tvin_StartSigDetect(void)
{
    gTvinApiPara.tvin_para.info.status = TVIN_SIG_STATUS_UNSTABLE;
    gTvinApiPara.tvin_para.info.fmt = TVIN_SIG_FMT_NULL;
    gTvinApiPara.tvin_para.info.trans_fmt = TVIN_TFMT_2D;
    gTvinApiPara.is_signal_detect_thread_start = true;
    ALOGD("%s.", __FUNCTION__);
    return true;
}

int Tvin_StopSigDetect(void)
{
    int i = 0, dly = 10;
    gTvinApiPara.is_signal_detect_thread_start = false;
    gTvinApiPara.is_hdmi_sr_detect_start = false;
    gTvinApiPara.hdmi_sampling_rate = 0;
    for (i = 0; i < 50; i++) {
        if (Tvin_IsSigDetectExecDone() == false) {
            ALOGW("%s, Source detect thread is busy, please wait %d ms...", __FUNCTION__, (i*dly));
            usleep(dly * 1000);
        } else {
            ALOGD("%s, after %d ms, source detect thread is idle now, let's go ...", __FUNCTION__, (i*dly));
            break;
        }
    }
    if (i == 50) {
        ALOGW("%s, %d ms delay Timeout, we have to go", __FUNCTION__, (i*dly));
        return -1;
    }
    ALOGD("%s, Done.", __FUNCTION__);
    return 0;
}

bool Tvin_StartSourceSwitch(void)
{
    gTvinApiPara.is_source_switch_exec_done = true;
    gTvinApiPara.is_source_switch_thread_start = true;
    ALOGD("%s.", __FUNCTION__);
    return true;
}

void Tvin_StopSourceSwitch(void)
{
    int i = 0, dly = 100;
    gTvinApiPara.is_source_switch_thread_start = false;
    for (i = 0; i < 30; i++) {
        if (Tvin_IsSrcSwitchExecDone() == false) {
            ALOGW("%s, source switch thread is busy, please wait %d ms...", __FUNCTION__, (i*dly));
            usleep(dly * 1000);
        } else {
            ALOGD("%s, after %d ms, source switch thread is idle now, let's go ...", __FUNCTION__, (i*dly));
            break;
        }
    }
    if (i == 30) {
        ALOGW("%s, after %d ms delay timeout, have to go.", __FUNCTION__, (i*dly));
    }
    gTvinApiPara.is_source_switch_exec_done = true;
    ALOGD("%s.", __FUNCTION__);
}

tvin_3d_mode_t Tvin_Get3DMode(void)
{
    return gTvinApiPara.mode_3d;
}

void Tvin_Set3DMode(tvin_3d_mode_t mode)
{
    gTvinApiPara.mode_3d = mode;
}

void Tvin_Set3DStatus(tvin_3d_status_t status)
{
    int panelty = SSMReadPanelType();
    tvin_3d_status_t status_temp = status;

    if (status == STATUS3D_AUTO_LR || status == STATUS3D_AUTO_BT) {
        status_temp = STATUS3D_AUTO;
    }
    gTvinApiPara.status_3d = status_temp;
    gTvinApiPara.mode_3d = (tvin_3d_mode_t) status_temp;

    if (status == STATUS3D_DISABLE) {
        switch(panelty) {
        case PANEL_39_IW:
        case PANEL_42_IW:
        case PANEL_50_IW:
            if (gTvinConfig.autoset_displayfreq != 0x55) {
                TvinApi_SetDisplayVFreq(50);
                ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
            }
            IW7023_Set3Dto2D();
            ALOGD("%s, iw7023 3D->2D setting.", __FUNCTION__);
            break;
        case PANEL_39_CM:
        case PANEL_42_CM:
        case PANEL_50_CM:
        case PANEL_42_SL:
            if (gTvinConfig.autoset_displayfreq != 0x55) {
                TvinApi_SetDisplayVFreq(50);
                ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
            }
            break;
        }
        if (gTvinConfig.pin_ctrl_3D == 0x55) {
            TvMiscGPIOCtrl(config_3d_pin_disable);
            if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DOFF);
            }
            ALOGD("%s, disable 3D panel!", __FUNCTION__);
        }
        //sendMessage
        android::TvService::getIntance()->Send3DState((int)status);
    } else if (status == STATUS3D_AUTO || status == STATUS3D_AUTO_LR || status == STATUS3D_AUTO_BT) {
        switch(panelty) {
        case PANEL_39_IW:
        case PANEL_42_IW:
        case PANEL_50_IW:
            TvinApi_SetDisplayVFreq(60);
            IW7023_Set2Dto3D();
            ALOGD("%s, iw7023 2D->3D setting, set LVDS 60Hz for 3D display", __FUNCTION__);
            break;
        case PANEL_39_CM:
        case PANEL_42_CM:
        case PANEL_50_CM:
        case PANEL_42_SL:
            TvinApi_SetDisplayVFreq(60);
            ALOGD("%s, cm panel, set LVDS 60Hz for 3D display", __FUNCTION__);
            break;
        }
        if (gTvinConfig.pin_ctrl_3D == 0x55) {
            TvMiscGPIOCtrl(config_3d_pin_enable);
            if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DON);
            }
            ALOGD("%s, enable 3D panel!", __FUNCTION__);
        }
        //sendMessage
        android::TvService::getIntance()->Send3DState((int)status);
    }
    SSMSave3DMode((unsigned char) gTvinApiPara.status_3d);
}

tvin_3d_status_t Tvin_Get3DStatus(void)
{
    return gTvinApiPara.status_3d;
}

void IsDisableVideo(bool isDisable)
{
    int fd_video = -1;

    if((fd_video = open("/sys/class/video/disable_video", O_RDWR)) < 0) {
        ALOGW("open /sys/class/video/disable_video fail.");
    }

    if(fd_video >= 0) {
        if(isDisable == true)
            write(fd_video, "1", 1);
        else
            write(fd_video, "0", 1);
    }

    close(fd_video);
}

void Tvin_TurnOnBlueScreen(int type)
{
    if(SSMReadAgingMode() ==1 ) {
        Tv_FactorySetTestPattern(4); //white screen
    } else if(Tvin_GetSourceLocked() == 1) {
        Tv_FactorySetTestPattern(VPP_TEST_PATTERN_BLUE); //white screen
    } else {
        if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
            ALOGD("%s, mpeg type:black screen.", __FUNCTION__);
            Vpp_SetVppScreenColor(0, 16, 128, 128); // Show black with vdin0, postblending disabled
            return;
        }
        if(gTvinConfig.preview_freescale == 0x55 && gTvinApiPara.tvin_status == TVIN_STATUS_PREVIEW_START) {
            Vpp_SetVppScreenColor(3, 16, 128, 128);
            if(type == 1 || type ==2)
                IsDisableVideo(true);
            else
                IsDisableVideo(false);
            ALOGD("%s, set black screen when freescale enable.", __FUNCTION__);
            return;
        }
        if (type == 0) {
            Vpp_SetVppScreenColor(3, 16, 128, 128); // Show black with vdin0, postblending enabled
        } else if (type == 1) {
            Vpp_SetVppScreenColor(0, 41, 240, 110); // Show blue with vdin0, postblending disabled
        } else {
            Vpp_SetVppScreenColor(0, 16, 128, 128); // Show black with vdin0, postblending disabled
        }
    }
}

int Tvin_GetSourceLocked(void)
{
    char prop_value[PROPERTY_VALUE_MAX];

    if (gTvinConfig.source_pg_lock != 0x55)
        return 0;

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);

    tvin_source_input_t source_input = Tvin_GetSourceInput();
    if (Tvin_GetTvProductId() == TV_PRODUCT_E04REF || Tvin_GetTvProductId() == TV_PRODUCT_E08REF) {
        if(source_input == SOURCE_AV1) {
            property_get("persist.tv.source_lock.av", prop_value, "0");
        } else if(source_input == SOURCE_YPBPR1) {
            property_get("persist.tv.source_lock.ypbpr", prop_value, "0");
        } else if(source_input == SOURCE_VGA) {
            property_get("persist.tv.source_lock.pc", prop_value, "0");
        } else if(source_input == SOURCE_HDMI1) {
            property_get("persist.tv.source_lock.hdmi1", prop_value, "0");
        } else if(source_input == SOURCE_HDMI2) {
            property_get("persist.tv.source_lock.hdmi2", prop_value, "0");
        }
        if (strcmp(prop_value, "1") == 0)
            return 1;
        else
            return 0;
    } else {
        return 0;
    }
}
void GetSoftwareVersion(unsigned char *data)
{
    char prop_value[PROPERTY_VALUE_MAX];
    char wday[][4] = {"sun","mon","tue","wen","thu","fri","sat"};
    char tmp[4];
    char *token = NULL;
    const char *strDelimit = ".";
    //char str[] = "01.011.132";
    int i = 0;
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.build.skyversion",prop_value,"VERSION_ERROR");
    //memcpy(prop_value, str,sizeof(str));

    ALOGD("skyver = %s",prop_value);
    if (strcmp(prop_value, "\0") != 0) {
        token = strtok(prop_value, strDelimit);
        while (token != NULL) {
            tmp[i] = strtol(token, NULL, 10);
            token = strtok(NULL, strDelimit);
        }
        i++;
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.build.date.utc",prop_value,"VERSION_ERROR");
    {
        time_t timep = atoi(prop_value);
        struct tm *p;
        //if(strcmp(prop_value, "VERSION_ERROR"))
        p = localtime(&timep);
        ALOGD("%d %02d %02d",1900+p->tm_year,1+p->tm_mon,p->tm_mday);
        ALOGD("%s %02d: %02d: %02d",wday[p->tm_wday],p->tm_hour,p->tm_min,p->tm_sec);
        tmp[i++] = (((1+p->tm_mon)/10)<<4)+(1+p->tm_mon)%10;
        tmp[i++] = ((p->tm_mday/10)<<4)+p->tm_mday%10;
        tmp[i] = '\0';
    }
    memcpy(data, tmp,sizeof(tmp));
}

void TvinApi_Set3DOverScan(bool isTurnOn)
{
    if (isTurnOn == false) {
        ALOGD("%s, turn off Set-3D-OvserScan.", __FUNCTION__);
        TvinApi_Set3DOvserScan(0, 0);
        return;
    }

    if (Tvin_Get3DStatus() == STATUS3D_LR || Tvin_Get3DStatus() == STATUS3D_BT) {
        ALOGD("%s, Set-3D-OverScan in L/R or B/T mode:", __FUNCTION__);
        ALOGD("%s, cut_top:%d, cut_left:%d.", __FUNCTION__, gTvinApiPara.cut_top, gTvinApiPara.cut_left);
        TvinApi_Set3DOvserScan(gTvinApiPara.cut_top, gTvinApiPara.cut_left);
    } else {
        ALOGD("%s, not 3D L/R or B/T mode, turn off Set-3D-OvserScan.", __FUNCTION__);
        TvinApi_Set3DOvserScan(0, 0);
    }
}

tvin_cutwin_t Tv_GetOverscan(tvin_source_input_type_t source_type, tvin_sig_fmt_t fmt, tvin_3d_status_t status, tvin_trans_fmt_t trans_fmt)
{
    int ret = -1;
    tvin_cutwin_t cutwin_t;
    memset(&cutwin_t, 0, sizeof(cutwin_t));

    if (source_type < SOURCE_TYPE_TV || source_type >= SOURCE_TYPE_MAX)
        return cutwin_t;
    if (fmt < TVIN_SIG_FMT_NULL || fmt >= TVIN_SIG_FMT_MAX)
        return cutwin_t;
    if (trans_fmt < TVIN_TFMT_2D || trans_fmt > TVIN_TFMT_3D_LDGD)
        return cutwin_t;
#ifdef PQ_ENABLE_DB
    vpp_display_mode_t scrmode = Tv_GetDisplayMode(source_type);
    ret = PQ_GetOverscanParams(source_type, fmt, status, trans_fmt, scrmode, &cutwin_t);
    if (ret == 0) {
        ALOGD("%s, source_type[%d], fmt[%d], status[%d], trans_fmt[%d], cutwin_t.hs[%d], cutwin_t.he[%d], cutwin_t.vs[%d], cutwin_t.ve[%d].\n", __FUNCTION__,
              source_type, fmt, status, trans_fmt, cutwin_t.hs, cutwin_t.he, cutwin_t.vs, cutwin_t.ve);
    } else {
        ALOGW("%s, PQ_GetOverscanParams faild.\n", __FUNCTION__);
        if (source_type == SOURCE_TYPE_TV) {
            usleep(20000);
            if (fmt == TVIN_SIG_FMT_CVBS_NTSC_M
                || fmt == TVIN_SIG_FMT_CVBS_NTSC_443) {
                ALOGW("%s, CVBS 480i set default cut-window!\n", __FUNCTION__);
                cutwin_t.hs = 30;
                cutwin_t.he = 720 -30 -1;
                cutwin_t.vs = 2;
                cutwin_t.ve = 240 - 2 -1;
            } else if (fmt >= TVIN_SIG_FMT_CVBS_PAL_I && fmt <= TVIN_SIG_FMT_CVBS_SECAM) {
                ALOGW("%s, CVBS 576i set default cut-window!\n", __FUNCTION__);
                cutwin_t.hs = 32;
                cutwin_t.he = 720 -32 -1;
                cutwin_t.vs = 6;
                cutwin_t.ve = 288 - 8 -1;
            }
        }
    }
#endif
    return cutwin_t;
}


void Tvin_SetOverScan(bool isTurnOn)
{
    tvin_cutwin_t cutwin;
    tvin_source_input_type_t source_type;
    tvin_sig_fmt_t fmt;
    tvin_3d_status_t status;
    tvin_trans_fmt_t trans_fmt;
    gTvinApiPara.tvin_para.cutwin.hs = 0;
    gTvinApiPara.tvin_para.cutwin.he = 0;
    gTvinApiPara.tvin_para.cutwin.vs = 0;
    gTvinApiPara.tvin_para.cutwin.ve = 0;

    if (isTurnOn == false) {

        gTvinApiPara.is_turnon_overscan = false;
        ALOGD("%s, turn off overscan.", __FUNCTION__);
        return;
    }

    memset(&cutwin, 0, sizeof(tvin_cutwin_t));
    source_type = Tvin_GetSrcInputType();
    fmt = Tvin_GetSigFormat();
    status = Tvin_Get3DStatus();
    trans_fmt = Tvin_GetSigTransFormat();

    if (status == STATUS3D_DISABLE
        || status == STATUS3D_2D_TO_3D
        || status == STATUS3D_LR
        || status == STATUS3D_BT) {
        ALOGD("%s, 3D mode is Disable/2D-3D/LR/BT, turn on overscan().", __FUNCTION__);
    } else if (status == STATUS3D_AUTO) {
        if (gTvinApiPara.tvin_para.info.trans_fmt == TVIN_TFMT_2D) {
            ALOGD("%s, 3D mode is auto, 2D -> turn on overscan().", __FUNCTION__);
        } else {
            gTvinApiPara.is_turnon_overscan = false;
            ALOGD("%s, 3D mode is auto, 3D -> turn off overscan().", __FUNCTION__);
            return; // return when 3D turn on
        }
    } else {
        gTvinApiPara.is_turnon_overscan = false;
        ALOGD("%s, turn off overscan in 3D mode.", __FUNCTION__);
        return; // return when 3D turn on
    }

    {
        cutwin = Tv_GetOverscan(source_type, fmt, status, trans_fmt);
        gTvinApiPara.tvin_para.cutwin.hs = cutwin.hs;
        gTvinApiPara.tvin_para.cutwin.he = cutwin.he;
        gTvinApiPara.tvin_para.cutwin.vs = cutwin.vs;
        gTvinApiPara.tvin_para.cutwin.ve = cutwin.ve;
        ALOGD("%s, Tv_FactoryGetOverscan, source_type[%d], fmt[%d], 3Dstatus[%d], trans_fmt[%d], cutwin.hs[%d], cutwin.he[%d], cutwin.vs[%d], cutwin.ve[%d].",
              __FUNCTION__, source_type, fmt, status, trans_fmt, cutwin.hs, cutwin.he, cutwin.vs, cutwin.ve);

        if (gTvinConfig.overscan_3d == 1) {
            if (status == STATUS3D_LR || status == STATUS3D_BT) {
                gTvinApiPara.cut_top = gTvinApiPara.tvin_para.cutwin.vs;
                gTvinApiPara.cut_left = gTvinApiPara.tvin_para.cutwin.hs;
                gTvinApiPara.tvin_para.cutwin.hs = 0;
                gTvinApiPara.tvin_para.cutwin.he = 0;
                gTvinApiPara.tvin_para.cutwin.vs = 0;
                gTvinApiPara.tvin_para.cutwin.ve = 0;
                gTvinApiPara.is_turnon_overscan = true;
                TvinApi_Set3DOverScan(true);
            } else {
                gTvinApiPara.is_turnon_overscan = true;
                TvinApi_Set3DOverScan(false);
            }
            ALOGD("%s, turn on overscan in STATUS3D_LR or STATUS3D_BT!", __FUNCTION__);
        }
    }

    if(gTvinConfig.new_overscan == 0x55) {
        if(isTurnOn == true) {
            if(Tvin_Get3DStatus() == STATUS3D_DISABLE || Tvin_Get3DStatus() == STATUS3D_2D_TO_3D) {
                Vpp_SetVppVideoCrop((int)gTvinApiPara.tvin_para.cutwin.vs, (int)gTvinApiPara.tvin_para.cutwin.hs, (int)gTvinApiPara.tvin_para.cutwin.ve, (int)gTvinApiPara.tvin_para.cutwin.he);
            } else {
                Vpp_SetVppVideoCrop(0,0,0,0);
            }
            if(Tvin_GetSourceInput() == SOURCE_HDMI3 && TvinApi_GetMhlInputMode() == 1) {
                Vpp_SetVppVideoCrop(0,0,0,0);
            }
        } else {
            Vpp_SetVppVideoCrop(0,0,0,0);
        }
    }

}

int Tvin_SetPreviewVideoAxis(char *pset)
{
    if( Tvin_GetTvProductId() != TV_PRODUCT_H02REF )
        return -1;

    if (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
        return SetFileAttrValue("/sys/class/video/axis",config_preview_video_axis);
    }

    return -1;
}


int Tvin_StopDecoder(int windowSel)
{
    ALOGD("%s, doing...", __FUNCTION__);
    if (gTvinApiPara.is_decoder_start == true) {
        if (TvinApi_StopDec(windowSel) >= 0) {
            ALOGD("%s, StopDecoder ok!", __FUNCTION__);
            gTvinApiPara.is_decoder_start = false;
            return 0;
        } else {
            ALOGD("%s, StopDecoder Failed!", __FUNCTION__);
        }
    } else {
        ALOGD("%s, decoder never start, no need StopDecoder!", __FUNCTION__);
        return 1;
    }
    return -1;
}

bool IsInvalidADCValue(tvin_adc_calibration_input_t input)
{
    switch(input) {
    case ADC_CALIBRATION_INPUT_YPBPR1:
        if(ADCGainOffset_Ypbpr1.comp_cal_val[0].a_analog_gain == 0 || ADCGainOffset_Ypbpr1.comp_cal_val[0].a_digital_gain == 0) {
            ALOGW("%s, YPBPR-1 ADC value error !!!", __FUNCTION__);
            TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
            return true;
        }
        break;
    case ADC_CALIBRATION_INPUT_YPBPR2:
        if(ADCGainOffset_Ypbpr2.comp_cal_val[0].a_analog_gain == 0 || ADCGainOffset_Ypbpr2.comp_cal_val[0].a_digital_gain == 0) {
            ALOGW("%s, YPBPR-2 ADC value error !!!", __FUNCTION__);
            TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
            return true;
        }
        break;
    case ADC_CALIBRATION_INPUT_VGA:
        if(ADCGainOffset_VGA.a_analog_gain == 0 || ADCGainOffset_VGA.a_digital_gain == 0) {
            ALOGW("%s, VGA ADC value error !!!", __FUNCTION__);
            TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
            return true;
        }
        break;
    }

    return false;
}

int Tvin_StartDecoder(int windowSel, bool is_turnon_overscan)
{
    tvin_color_system_e color_system = COLOR_SYSTEM_AUTO;
    if(Tvin_GetSourceLocked()) {
        ALOGW("%s, source is locked..............\n", __FUNCTION__);
        return -1;
    }
    if ((gTvinApiPara.is_decoder_start == false) && (gTvinApiPara.tvin_para.info.fmt != TVIN_SIG_FMT_NULL) && (gTvinApiPara.tvin_para.info.status == TVIN_SIG_STATUS_STABLE)) {
        ALOGD("%s, SetOverScan, is_turnon_overscan : %d.", __FUNCTION__, is_turnon_overscan);
        Tvin_SetOverScan(is_turnon_overscan);
        Tvin_SetPreviewVideoAxis(config_preview_video_axis);
        if (TvinApi_StartDec(windowSel, gTvinApiPara.tvin_para) >= 0) {
            ALOGD("%s, StartDecoder succeed.", __FUNCTION__);
            gTvinApiPara.is_decoder_start = true;

            if (Tvin_GetSrcInputType() == SOURCE_TYPE_VGA) {
                if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_VGA) == true) {
                    TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
                }
                TvinApi_SetADCGainOffset(ADCGainOffset_VGA);
            } else if (Tvin_GetSrcInputType() == SOURCE_TYPE_COMPONENT) {
                if(Tvin_GetSrcPort() == TVIN_PORT_COMP0) {
                    if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_YPBPR1) == true) {
                        TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
                    }
                    TvinApi_SetYPbPrADCGainOffset(ADCGainOffset_Ypbpr1);
                    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
                        if(Tvin_GetSigFormat() == TVIN_SIG_FMT_COMP_1080P_60HZ_D000) {
                            SetFileAttrValue("/sys/class/register/reg", "wa 0x3259 0x8");
                        } else if(Tvin_GetSigFormat() == TVIN_SIG_FMT_COMP_1080P_30HZ_D000) {
                            SetFileAttrValue("/sys/class/register/reg", "wa 0x3259 0x6 ");
                        }
                    }
                } else if(Tvin_GetSrcPort() == TVIN_PORT_COMP1) {
                    if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_YPBPR2) == true) {
                        TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
                    }
                    TvinApi_SetYPbPrADCGainOffset(ADCGainOffset_Ypbpr2);
                } else {
                    if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_YPBPR1) == true) {
                        TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
                    }
                    TvinApi_SetYPbPrADCGainOffset(ADCGainOffset_Ypbpr1);
                }

                if(Tvin_GetTvProductId() == TV_PRODUCT_E12REF || Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
                    if(Tvin_GetSigFormat()== TVIN_SIG_FMT_COMP_576I_50HZ_D000) {
                        SetFileAttrValue("/sys/class/amdbg/reg","wa 0x1a56 0x04");
                    }
                }
            } else if (Tvin_GetSrcInputType() == SOURCE_TYPE_AV) {
                color_system = Tv_GetCVBSStd();
                TvinApi_SetCVBSStd((int)color_system);
                ALOGD("%s, SetCVBSStd, color_system : %d.", __FUNCTION__, color_system);
            }
        } else {
            ALOGW("%s, StartDecoder failed.", __FUNCTION__);
            return -1;
        }
    } else {
        ALOGW("%s, Can not StartDecoder, is_decoder_start: %d, fmt: %d.",
              __FUNCTION__, gTvinApiPara.is_decoder_start, gTvinApiPara.tvin_para.info.fmt);
        return -1;
    }
    return 0;
}

void Tvin_RemovePath(tvin_path_type_t pathtype)
{
    int ret = -1;
    int i = 0, dly = 10;
    if (pathtype == TV_PATH_TYPE_DEFAULT) {
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmDefPath();
            if (ret >= 0) {
                ALOGD("%s, remove default path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGW("%s, remove default path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
    } else if(pathtype == TV_PATH_TYPE_TVIN) {
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmTvPath();
            if (ret >= 0) {
                ALOGD("%s, remove tvin path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGW("%s, remove tvin path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
        gTvinApiPara.pathid = TV_PATH_DECODER_3D_AMVIDEO;
    } else if(pathtype == TV_PATH_TYPE_TVIN_PREVIEW) {
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmPreviewPath();
            if (ret >= 0) {
                ALOGD("%s, remove preview path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGW("%s, remove preview path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
//        TvinApi_DisableFreeScale(1);
        gTvinApiPara.pathid = TV_PATH_DECODER_3D_AMVIDEO;
    }
}

void Tvin_AddPath(tvin_path_id_t pathid)
{
    int ret = -1;
    int i = 0, dly = 10;

    if (pathid >= TV_PATH_VDIN_AMVIDEO && pathid < TV_PATH_DECODER_3D_AMVIDEO) {
        if (gTvinApiPara.pathid == pathid) {
            ALOGW("%s, no need to add the same tvin path.", __FUNCTION__);
            return;
        }
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmTvPath();
            if (ret >= 0) {
                ALOGD("%s, remove tvin path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGW("%s, remove tvin path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
    } else {
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmDefPath();
            if (ret >= 0) {
                ALOGD("%s, remove default path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGW("%s, remove default path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
    }

    for (i = 0; i < 50; i++) {
        if( pathid >= TV_PATH_VDIN_AMVIDEO && pathid <= TV_PATH_VDIN_NEW3D_AMVIDEO ||
            pathid == TV_PATH_VDIN_FREESCALE_AMVIDEO) {
            if (strcmp(config_tv_path, "null") == 0) {
                ret = TvinApi_AddTvPath(pathid);
            } else {
                ret = TvinApi_ManualSetPath(config_tv_path);
            }
        } else {
            if (strcmp(config_default_path, "null") == 0) {
                ret = TvinApi_AddTvPath(pathid);
            } else {
                ret = TvinApi_ManualSetPath(config_default_path);
            }
        }

        if (ret >= 0) {
            ALOGD("%s, add pathid[%d] ok, %d ms gone.", __FUNCTION__, pathid, i);
            break;
        } else {
            ALOGW("%s, add pathid[%d] faild, %d ms gone.", __FUNCTION__, pathid, i);
            usleep(dly * 1000);
        }
    }
    if (pathid >= TV_PATH_VDIN_AMVIDEO && pathid < TV_PATH_DECODER_3D_AMVIDEO) {
        gTvinApiPara.pathid = pathid;
    }
}

void Tvin_SetViewMode(int mode)
{
    Vpp_SetVppScaleParam(0, 0, 0, 0);
    Vpp_SetVppDisplayMode(VIDEO_WIDEOPTION_FULL_STRETCH);
    TvinApi_SetPpmgrMode(mode);
}

void Tvin_SetScreenMode(int mode)
{
    Vpp_SetVppDisplayMode(mode);
}

void Tvin_SetDisplayModeFor3D(int mode3D_on_off)
{
    vpp_display_mode_t displaymode = VPP_DISPLAY_MODE_MODE43;

    displaymode = Tv_GetDisplayMode(Tvin_GetSrcInputType());
    if (1 == mode3D_on_off) {
        if (displaymode == VPP_DISPLAY_MODE_MODE43) {
            TvinApi_SetPpmgrMode(1);
        } else {
            TvinApi_SetPpmgrMode(2);
        }
        Vpp_SetVppScaleParam(0, 0, (1920 - 1), (1080 - 1));
        Vpp_SetVppDisplayMode(VIDEO_WIDEOPTION_FULL_STRETCH);
        ALOGD("%s, Set vpp display mode 1 for 3D.", __FUNCTION__);
    } else {
        TvinApi_SetPpmgrMode(0);
        if (Tvin_GetTvProductId() == TV_PRODUCT_E06REF || Tvin_GetTvProductId() == TV_PRODUCT_E12REF
            || Tvin_GetTvProductId() == TV_PRODUCT_E13REF || Tvin_GetTvProductId() == TV_PRODUCT_E15REF) {
            if (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
                Vpp_SetDisplayMode(VPP_DISPLAY_MODE_169, SOURCE_TYPE_MPEG, Tvin_GetSigFormat());
            } else {
                if (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
                    if (Tvin_isVgaFmtInHdmi() == 1) {
                        if (displaymode != VPP_DISPLAY_MODE_169
                            && displaymode != VPP_DISPLAY_MODE_MODE43) {
                            Vpp_SetDisplayMode(VPP_DISPLAY_MODE_169, SOURCE_TYPE_HDMI, Tvin_GetSigFormat());
                            ALOGD("%s, VGA format in HDMI source, set vpp display mode 1.", __FUNCTION__);
                        }
                    } else {
                        Vpp_SetDisplayMode(displaymode, Tvin_GetSrcInputType(), Tvin_GetSigFormat());
                    }
                } else {
                    Vpp_SetDisplayMode(displaymode, Tvin_GetSrcInputType(), Tvin_GetSigFormat());
                }
            }
        } else if (Tvin_GetTvProductId() == TV_PRODUCT_E04REF || Tvin_GetTvProductId() == TV_PRODUCT_E08REF) {
            if (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
                if (Tvin_IsDVISignal()== 1) {
                    if (displaymode != VPP_DISPLAY_MODE_169
                        && displaymode != VPP_DISPLAY_MODE_MODE43) {
                        Vpp_SetDisplayMode(VPP_DISPLAY_MODE_169, SOURCE_TYPE_HDMI, Tvin_GetSigFormat());
                        ALOGD("%s, VGA format in HDMI source, set vpp display mode 1.", __FUNCTION__);
                    } else {
                        Vpp_SetDisplayMode(displaymode, SOURCE_TYPE_HDMI, Tvin_GetSigFormat());
                    }
                } else {
                    Vpp_SetDisplayMode(displaymode, Tvin_GetSrcInputType(), Tvin_GetSigFormat());
                }
            } else if(Tvin_GetSrcInputType() == SOURCE_TYPE_VGA) {
                if (displaymode != VPP_DISPLAY_MODE_169
                    && displaymode != VPP_DISPLAY_MODE_MODE43) {
                    Vpp_SetDisplayMode(VPP_DISPLAY_MODE_169, SOURCE_TYPE_VGA, Tvin_GetSigFormat());
                    ALOGD("%s, VGA  source, set vpp display mode 1.", __FUNCTION__);
                } else {
                    Vpp_SetDisplayMode(displaymode, SOURCE_TYPE_VGA, Tvin_GetSigFormat());
                }
            } else {
                Vpp_SetDisplayMode(displaymode, Tvin_GetSrcInputType(), Tvin_GetSigFormat());
            }
        } else {
            Vpp_SetDisplayMode(displaymode, Tvin_GetSrcInputType(), Tvin_GetSigFormat());
        }
    }
}

int Tvin_SetPanelParamsFor3D(tvin_3d_mode_t mode)
{
    int panelty = SSMReadPanelType();

    if( mode == MODE3D_DISABLE
        //|| mode == MODE3D_LR
        //|| mode == MODE3D_BT
        //|| mode == MODE3D_L_3D_TO_2D
        //|| mode == MODE3D_R_3D_TO_2D
        || mode == MODE3D_MAX ) {
        ALOGD("%s, Set Panel params for 2D display.",__FUNCTION__);

        if (gTvinConfig.pin_ctrl_3D == 0x55) {
            TvMiscGPIOCtrl(config_3d_pin_disable);
            if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DOFF);
            }
            ALOGD("%s, disable 3D panel!", __FUNCTION__);
        }
        switch(panelty) {
        case PANEL_39_IW:
        case PANEL_42_IW:
        case PANEL_50_IW:
            if (gTvinConfig.autoset_displayfreq != 0x55) {
                TvinApi_SetDisplayVFreq(50);
                ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
            }
            IW7023_Set3Dto2D();
            ALOGD("%s, iw7023 3D->2D setting.", __FUNCTION__);
            break;
        case PANEL_39_CM:
        case PANEL_42_CM:
        case PANEL_50_CM:
        case PANEL_42_SL:
            if (gTvinConfig.autoset_displayfreq != 0x55) {
                TvinApi_SetDisplayVFreq(50);
                ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
            }
            break;
        default:
            break;
        }

    } else {
        ALOGD("%s, Set Panel params for 3D display.",__FUNCTION__);
        if (gTvinConfig.pin_ctrl_3D == 0x55) {
            TvMiscGPIOCtrl(config_3d_pin_enable);
            if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DON);
            }
            ALOGD("%s, enable 3D panel!", __FUNCTION__);
        }
        switch(panelty) {
        case PANEL_39_IW:
        case PANEL_42_IW:
        case PANEL_50_IW:
            TvinApi_SetDisplayVFreq(60);
            IW7023_Set2Dto3D();
            ALOGD("%s, iw7023 2D->3D setting, set LVDS 60Hz for 3D display", __FUNCTION__);
            break;
        case PANEL_39_CM:
        case PANEL_42_CM:
        case PANEL_50_CM:
        case PANEL_42_SL:
            TvinApi_SetDisplayVFreq(60);
            ALOGD("%s, cm panel, set LVDS 60Hz for 3D display", __FUNCTION__);
            break;
        default:
            break;
        }

    }

    return 0;
}

int Tvin_BypassModulesFor3D(tvin_3d_mode_t mode)
{
    ALOGD("%s, ----> Set mode(%d) <----",__FUNCTION__, mode);

    if( mode == MODE3D_DISABLE ) {
        TvinApi_SetD2D3Bypass(1);
        TvinApi_SetDIBypassPost(0);

        if (Tvin_GetSrcInputType() == SOURCE_TYPE_DTV) {
            TvinApi_SetDIBypasshd(0);
        } else {
            TvinApi_SetDIBypasshd(0);
        }

        if (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
            TvinApi_SetDIBypassAll(1);
        } else {
            TvinApi_SetDIBypassAll(0);
        }
        if(Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
            TvinApi_SetDIBypasshd(1);
        }
        TvinApi_SetRDMA(1);

        TvinApi_Send3DCommand(MODE3D_DISABLE);

    } else if( mode == MODE3D_2D_TO_3D ) {
        TvinApi_SetD2D3Bypass(0);
        TvinApi_SetRDMA(0);
        if(Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG
           || Tvin_GetSrcInputType() == SOURCE_TYPE_DTV) {
            TvinApi_SetDIBypassPost(0);
            TvinApi_SetDIBypasshd(1);
        } else {
            TvinApi_SetDIBypassPost(0);
            TvinApi_SetDIBypasshd(0);
        }
        TvinApi_SetDIBypassAll(0);
        TvinApi_Send3DCommand(MODE3D_DISABLE);

    } else {
        TvinApi_SetD2D3Bypass(1);
        TvinApi_SetRDMA(0);
        TvinApi_SetDIBypassPost(1);
        usleep(50 * 1000);
        TvinApi_SetDIBypasshd(1);
        usleep(50 * 1000);
        TvinApi_SetDIBypassAll(1);
        TvinApi_Send3DCommand(mode);
    }

    return 0;
}

int Tvin_Set3DParamsBySrcType(tvin_3d_mode_t mode)
{
    tvin_3d_mode_t pre_mode = gTvinApiPara.mode_3d;

    if( Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG
        || Tvin_GetSrcInputType() == SOURCE_TYPE_DTV ) {
        if( mode == MODE3D_DISABLE ) {
            Tvin_SetDisplayModeFor3D(0);
            //TvinApi_SetPpmgrView_mode(0);
            TvinApi_SetVdinFlag(MEMP_DCDR_WITHOUT_3D);
        } else {
            Tvin_SetDisplayModeFor3D(1);
            //TvinApi_SetPpmgrView_mode(3);
            TvinApi_SetVdinFlag(MEMP_DCDR_WITH_3D);
        }
    } else {
        if( Tvin_GetTvProductId() != TV_PRODUCT_H02REF ) {
            AudioSetSysMuteStatus(CC_MUTE_ON);
        }
        Tvin_TurnOnBlueScreen(2);

        if( Tvin_GetTvProductId() != TV_PRODUCT_H02REF ) {
            Tvin_StopSigDetect();
            usleep(10 * 1000);
            Tvin_StopDecoder(0);
            usleep(20 * 1000);
        } else {
            if( mode == MODE3D_DISABLE || mode == MODE3D_2D_TO_3D) {
                if( pre_mode != MODE3D_DISABLE || pre_mode != MODE3D_2D_TO_3D ) {
                    Tvin_StopSigDetect();
                    usleep(10 * 1000);
                    Tvin_StopDecoder(0);
                    usleep(20 * 1000);
                }
            }
        }

        if (mode == MODE3D_DISABLE) {
            if (Tvin_GetSourceInput() == SOURCE_TV) {
                TvinApi_SetVdinFlag(MEMP_ATV_WITHOUT_3D);
                if (gTvinConfig.atv_keeplastframe != 0) {
                    TvinApi_SetBlackOutPolicy(0);
                }
            } else {
                TvinApi_SetVdinFlag(MEMP_VDIN_WITHOUT_3D);
            }
            //TvinApi_SetPpmgrView_mode(0);
            TvinApi_Set2Dto3D(0);
        } else {
            TvinApi_SetDIBuffMgrMode(0);
            //TvinApi_SetPpmgrView_mode(3);
            if (Tvin_GetSourceInput() == SOURCE_TV) {
                TvinApi_SetVdinFlag(MEMP_ATV_WITH_3D);
            } else {
                TvinApi_SetVdinFlag(MEMP_VDIN_WITH_3D);
            }
            if ( mode == MODE3D_2D_TO_3D ) {
                TvinApi_Set2Dto3D(0);
            } else {
                TvinApi_Set2Dto3D(1);
            }
        }
    }

    return 0;
}

int Tvin_Set3DFunctionNew(tvin_3d_mode_t mode)
{
    tvin_3d_mode_t pre_mode = gTvinApiPara.mode_3d;
    int pre_mvc_mode = 0;

    if (pre_mode == mode || Tvin_GetSrcInputType() == SOURCE_TYPE_VGA) {
        ALOGW("%s, Set3DMode faild!!! pre_mode(%d), mode(%d), source_type(%d).", __FUNCTION__, pre_mode, mode, Tvin_GetSrcInputType());
        if(mode == MODE3D_DISABLE && gTvinConfig.new_d2d3 == 0x55)
            TvinApi_SetD2D3Bypass(1);
        return -1;
    }

    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
        SetFileAttrValue("/sys/class/video/disable_video","1");
    }

    if (mode != MODE3D_FIELD_DEPTH)
        gTvinApiPara.mode_3d = mode;

    if ((mode == MODE3D_DISABLE) || (mode == MODE3D_AUTO) || (mode == MODE3D_2D_TO_3D) || (mode == MODE3D_LR) || (mode == MODE3D_BT)) {
        gTvinApiPara.status_3d = (tvin_3d_status_t) gTvinApiPara.mode_3d;
        android::TvService::getIntance()->Send3DState((int)gTvinApiPara.status_3d);
        ALOGD("%s, Set 3D status: %d.", __FUNCTION__, gTvinApiPara.status_3d);
    }

    Tvin_SetPanelParamsFor3D(mode);
    Tvin_BypassModulesFor3D(mode);
    Tvin_Set3DParamsBySrcType(mode);

    if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG || Tvin_GetSrcInputType() == SOURCE_TYPE_DTV ) {
        Vpp_LoadBasicRegs(SOURCE_TYPE_MPEG, TVIN_SIG_FMT_NULL, Tvin_Get3DStatus(), TVIN_TFMT_2D);
    }

    Vpp_SetSharpness(Tv_GetSharpness(Tvin_GetSrcInputType()), Tvin_GetSrcInputType(), 0, Tvin_Get3DStatus());

    SSMSave3DMode((unsigned char)gTvinApiPara.status_3d);
    //sendMessage
    android::TvService::getIntance()->Send3DState((int)gTvinApiPara.status_3d);
    switch (mode) {
    case MODE3D_OFF_LR_SWITCH:
    case MODE3D_OFF_LR_SWITCH_BT:
        SSMSave3DLRSwitch(0);
        break;
    case MODE3D_ON_LR_SWITCH:
    case MODE3D_ON_LR_SWITCH_BT:
        SSMSave3DLRSwitch(1);
        break;
    case MODE3D_OFF_3D_TO_2D:
    case MODE3D_OFF_3D_TO_2D_BT:
        SSMSave3DTO2D(0);
        break;
    case MODE3D_L_3D_TO_2D:
    case MODE3D_L_3D_TO_2D_BT:
        SSMSave3DTO2D(1);
        break;
    case MODE3D_R_3D_TO_2D:
    case MODE3D_R_3D_TO_2D_BT:
        SSMSave3DTO2D(2);
        break;
    case MODE3D_DISABLE:
    case MODE3D_AUTO:
    case MODE3D_2D_TO_3D:
    case MODE3D_LR:
    case MODE3D_BT:
        SSMSave3DLRSwitch(0);
        SSMSave3DTO2D(0);
        break;
    case MODE3D_FIELD_DEPTH:
    default:
        break;
    }

    if (Tvin_GetSrcInputType() != SOURCE_TYPE_MPEG && Tvin_GetSrcInputType() != SOURCE_TYPE_DTV ) {
        if( Tvin_GetTvProductId() != TV_PRODUCT_H02REF ) {
            Tvin_StartSigDetect();
        } else {
            if(mode == MODE3D_DISABLE || mode == MODE3D_2D_TO_3D) {
                if( pre_mode != MODE3D_DISABLE || pre_mode != MODE3D_2D_TO_3D ) {
                    Tvin_StartSigDetect();
                }
            }
        }
    }

    //delay the frequency of key press and let the thread of signal detect, started to run.
    usleep(500 * 1000);

    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
        SetFileAttrValue("/sys/class/video/disable_video","0");
    }

    return 0;

}

int Tvin_Set3DFunction(tvin_3d_mode_t mode)
{
    if(gTvinConfig.new_set3dfunction == 0x55) {
        return Tvin_Set3DFunctionNew(mode);
    } else {
        tvin_3d_mode_t pre_mode = gTvinApiPara.mode_3d;
        int pre_mvc_mode = 0;
        int need_to_restart_signal_detect_thread = 0;
        int panelty = SSMReadPanelType();

        if (pre_mode == mode || Tvin_GetSrcInputType() == SOURCE_TYPE_VGA || Tvin_GetSrcInputType() == SOURCE_TYPE_DTV) {
            ALOGW("%s, Set3DMode faild, pre_mode:%d, mode:%d, source_type:%d.", __FUNCTION__, pre_mode, mode, Tvin_GetSrcInputType());
            if(mode == MODE3D_DISABLE && gTvinConfig.new_d2d3 == 0x55)
                TvinApi_SetD2D3Bypass(1);
            return -1;
        }

        if (mode != MODE3D_FIELD_DEPTH)
            gTvinApiPara.mode_3d = mode;

        if ((mode == MODE3D_DISABLE) || (mode == MODE3D_AUTO) || (mode == MODE3D_2D_TO_3D) || (mode == MODE3D_LR) || (mode == MODE3D_BT)) {
            gTvinApiPara.status_3d = (tvin_3d_status_t) gTvinApiPara.mode_3d;
            //sendMessage
            android::TvService::getIntance()->Send3DState((int)gTvinApiPara.status_3d);
            ALOGD("%s, Set 3D status: %d.", __FUNCTION__, gTvinApiPara.status_3d);
        }

        if (mode == MODE3D_DISABLE) { // 3D -> normal
            ALOGD("%s, 3D->Normal, 3D->Tvin path.", __FUNCTION__);
            if (gTvinConfig.new_d2d3 == 0x55 ) {
                TvinApi_SetD2D3Bypass(1);
                ALOGE("%s, bypass new 2D3D!", __FUNCTION__);
            }
            switch(panelty) {
            case PANEL_39_IW:
            case PANEL_42_IW:
            case PANEL_50_IW:
                if (gTvinConfig.autoset_displayfreq != 0x55) {
                    TvinApi_SetDisplayVFreq(50);
                    ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
                }
                IW7023_Set3Dto2D();
                ALOGD("%s, iw7023 3D->2D setting.", __FUNCTION__);
                break;
            case PANEL_39_CM:
            case PANEL_42_CM:
            case PANEL_50_CM:
            case PANEL_42_SL:
                if (gTvinConfig.autoset_displayfreq != 0x55) {
                    TvinApi_SetDisplayVFreq(50);
                    ALOGD("%s, Set LVDS 50Hz. ", __FUNCTION__);
                }
                break;
            }
            if (gTvinConfig.pin_ctrl_3D == 0x55) {
                TvMiscGPIOCtrl(config_3d_pin_disable);
                if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                    I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DOFF);
                }
                ALOGD("%s, disable 3D panel!", __FUNCTION__);
            }
            TvinApi_SetDIBypassAll(0);
            TvinApi_SetDIBypassPost(0);
            if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
                Tvin_SetDisplayModeFor3D(0);
                TvinApi_SetVdinFlag(MEMP_DCDR_WITHOUT_3D);
            } else {
                AudioSetSysMuteStatus(CC_MUTE_ON);
                Tvin_TurnOnBlueScreen(2);
                Tvin_StopSigDetect();
                usleep(10 * 1000);
                Tvin_StopDecoder(0);
                usleep(20 * 1000);
                TvinApi_Set2Dto3D(0);
                if (Tvin_GetSourceInput() == SOURCE_TV) {
                    TvinApi_SetVdinFlag(MEMP_ATV_WITHOUT_3D);
                    if (gTvinConfig.atv_keeplastframe != 0) {
                        TvinApi_SetBlackOutPolicy(0);
                    }
                } else {
                    TvinApi_SetVdinFlag(MEMP_VDIN_WITHOUT_3D);
                }
                TvinApi_Set2Dto3D(0);
                need_to_restart_signal_detect_thread = 1;
            }
        } else if (pre_mode == MODE3D_DISABLE) { // normal -> 3D
            ALOGD("%s, Normal->3D, Tvin path->3D path.", __FUNCTION__);
            switch(panelty) {
            case PANEL_39_IW:
            case PANEL_42_IW:
            case PANEL_50_IW:
                TvinApi_SetDisplayVFreq(60);
                IW7023_Set2Dto3D();
                ALOGD("%s, iw7023 2D->3D setting, set LVDS 60Hz for 3D display", __FUNCTION__);
                break;
            case PANEL_39_CM:
            case PANEL_42_CM:
            case PANEL_50_CM:
            case PANEL_42_SL:
                TvinApi_SetDisplayVFreq(60);
                ALOGD("%s, cm panel, set LVDS 60Hz for 3D display", __FUNCTION__);
                break;
            }
            if (gTvinConfig.pin_ctrl_3D == 0x55) {
                TvMiscGPIOCtrl(config_3d_pin_enable);
                if(gTvinConfig.glass_3d_enable==0x55) {
                    TvMiscGPIOCtrl(config_3d_glass_reset);
                    ALOGD("%s,reset 3d glass %s",__FUNCTION__,config_3d_glass_reset);
                }
                if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                    I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DON);
                }
                ALOGD("%s, enable 3D panel!", __FUNCTION__);
            }
            if (gTvinConfig.new_3dautodetc == 0x55 && mode == MODE3D_AUTO) {
                TvinApi_SetDI3DDetc(MODE3D_AUTO);
            } else {
                TvinApi_SetDIBypassAll(1);
            }
            if (gTvinConfig.new_d2d3 == 0x55) {
                if(mode == MODE3D_LR || mode == MODE3D_BT) {
                    TvinApi_Send3DCommand(MODE3D_2D_TO_3D);
                    //TvinApi_SetPpmgrView_mode(3);
                }
            }
            if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
                TvinApi_SetDIBypassAll(1);
                Tvin_SetDisplayModeFor3D(1);
                TvinApi_SetVdinFlag(MEMP_DCDR_WITH_3D);
            } else {
                AudioSetSysMuteStatus(CC_MUTE_ON);
                Tvin_TurnOnBlueScreen(2);
                Tvin_StopSigDetect();
                TvinApi_SetDIBuffMgrMode(0);
                usleep(10 * 1000);
                Tvin_StopDecoder(0);
                usleep(20 * 1000);
                if ((mode == MODE3D_2D_TO_3D) && (gTvinConfig.new_d2d3 == 0x55)) {
                    TvinApi_SetD2D3Bypass(0);
                } else {
                    TvinApi_Set2Dto3D(1);
                }
                if (Tvin_GetSourceInput() == SOURCE_TV) {
                    TvinApi_SetVdinFlag(MEMP_ATV_WITH_3D);
                } else {
                    TvinApi_SetVdinFlag(MEMP_VDIN_WITH_3D);
                }
                need_to_restart_signal_detect_thread = 1;
            }
        } else if (pre_mode == MODE3D_AUTO) {
            if (mode == MODE3D_2D_TO_3D || mode == MODE3D_LR || mode == MODE3D_BT) {
                switch(panelty) {
                case PANEL_39_IW:
                case PANEL_42_IW:
                case PANEL_50_IW:
                    TvinApi_SetDisplayVFreq(60);
                    IW7023_Set2Dto3D();
                    ALOGD("%s, iw7023 2D->3D setting, set LVDS 60Hz for 3D display", __FUNCTION__);
                    break;
                case PANEL_39_CM:
                case PANEL_42_CM:
                case PANEL_50_CM:
                case PANEL_42_SL:
                    TvinApi_SetDisplayVFreq(60);
                    ALOGD("%s, cm panel, set LVDS 60Hz for 3D display", __FUNCTION__);
                    break;
                }
                if (gTvinConfig.pin_ctrl_3D == 0x55) {
                    TvMiscGPIOCtrl(config_3d_pin_enable);
                    if(gTvinConfig.glass_3d_enable==0x55) {
                        TvMiscGPIOCtrl(config_3d_glass_reset);
                        ALOGD("%s,reset 3d glass %s",__FUNCTION__,config_3d_glass_reset);
                    }
                    if (gTvinConfig.peripheral_3D_6M30 == 0x55) {
                        I2C_WriteNbyte(1, DEV_6M30_ADDR, DEV_6M30_REG, LEN_6M30_DATA, data_buf_3DON);
                    }
                    ALOGD("%s, enable 3D panel!", __FUNCTION__);
                }
                if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
                    TvinApi_SetDIBypassAll(1);
                    Tvin_SetDisplayModeFor3D(1);
                } else {
                    if (gTvinApiPara.pathid != TV_PATH_VDIN_3D_AMVIDEO) {
                        AudioSetSysMuteStatus(CC_MUTE_ON);
                        Tvin_TurnOnBlueScreen(2);
                        Tvin_StopSigDetect();
                        usleep(10 * 1000);
                        Tvin_StopDecoder(0);
                        usleep(20 * 1000);
                        if (!((mode == MODE3D_2D_TO_3D) && (gTvinConfig.new_d2d3 == 0x55))) {
                            TvinApi_Set2Dto3D(1);
                        }
                        need_to_restart_signal_detect_thread = 1;
                        ALOGD("%s, VDIN Normal AUTO->2D_3D/LR/BT, Tvin path->3D path.", __FUNCTION__);
                    } else {
                        ALOGW("%s, VDIN Normal AUTO->2D_3D/LR/BT, It's already 3D path.", __FUNCTION__);
                    }
                }
            }
            if (gTvinConfig.new_3dautodetc == 0x55) {
                TvinApi_SetDI3DDetc(MODE3D_DISABLE);
            }
        } else if (pre_mode == MODE3D_2D_TO_3D) {
            if (mode == MODE3D_LR || mode == MODE3D_BT) {
                //TvinApi_SetDIBypassPost(1);
                TvinApi_SetDIBypassAll(1);
                //TvinApi_SetPpmgrView_mode(3);
                if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
                    if (gTvinConfig.new_d2d3 == 0x55 ) {
                        TvinApi_SetD2D3Bypass(1);
                        TvinApi_Send3DCommand(MODE3D_2D_TO_3D);
                    }
                } else {
                    AudioSetSysMuteStatus(CC_MUTE_ON);
                    Tvin_TurnOnBlueScreen(2);
                    if (gTvinConfig.new_d2d3 == 0x55 ) {
                        TvinApi_Send3DCommand(MODE3D_2D_TO_3D);
                        Tvin_StopDecoder(0);
                        Tvin_StartDecoder(0,true);
                    }
                    Tvin_TurnOnBlueScreen(2);
                    Tvin_StopSigDetect();
                    usleep(10 * 1000);
                    Tvin_StopDecoder(0);
                    usleep(20 * 1000);
                    TvinApi_Set2Dto3D(1);
                    need_to_restart_signal_detect_thread = 1;
                }
            }
        } else if (pre_mode == MODE3D_LR || pre_mode == MODE3D_BT) {
            if (mode == MODE3D_2D_TO_3D) {
                TvinApi_SetDIBypassPost(0);
                if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
                    if (gTvinConfig.new_d2d3 == 0x55 ) {
                        TvinApi_Send3DCommand(MODE3D_DISABLE);
                    }
                } else {
                    AudioSetSysMuteStatus(CC_MUTE_ON);
                    Tvin_TurnOnBlueScreen(2);
                    if (gTvinConfig.new_d2d3 == 0x55 ) {
                        TvinApi_Send3DCommand(MODE3D_DISABLE);
                    }
                    Tvin_StopSigDetect();
                    usleep(10 * 1000);
                    Tvin_StopDecoder(0);
                    usleep(20 * 1000);
                    TvinApi_Set2Dto3D(0);
                    need_to_restart_signal_detect_thread = 1;
                }
            }
        }

        if (Tvin_GetSrcInputType() == SOURCE_TYPE_MPEG) {
            Vpp_LoadBasicRegs(SOURCE_TYPE_MPEG, TVIN_SIG_FMT_NULL, Tvin_Get3DStatus(), TVIN_TFMT_2D);
        }

        int val = Tv_GetSharpness(Tvin_GetSrcInputType());
        Vpp_SetSharpness(val, Tvin_GetSrcInputType(), 0, Tvin_Get3DStatus());
        if (gTvinConfig.new_d2d3 == 0x55 ) {
            if(mode == MODE3D_2D_TO_3D) {
                TvinApi_SetDIBypassAll(0);
                TvinApi_SetD2D3Bypass(0);
            } else {
                TvinApi_SetD2D3Bypass(1);
                TvinApi_Send3DCommand(mode);
            }
        } else {
            TvinApi_Send3DCommand(mode);
        }

        if (pre_mode == MODE3D_AUTO && ((mode == MODE3D_L_3D_TO_2D) || (mode == MODE3D_R_3D_TO_2D))) {
            if(Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
                Tvin_StopDecoder(0);
                usleep(50*1000);
                Tvin_StartDecoder(0,true);
            }
        }
        if (need_to_restart_signal_detect_thread == 1) {
            Tvin_StartSigDetect();
            need_to_restart_signal_detect_thread = 0;
        }
        //delay the frequency of key press and let the thread of signal detect, started to run.
        usleep(500 * 1000);
        return 0;
    }

}

void Tvin_3DSTATUS_SSM(tvin_3d_status_t status_3d)
{
    SSMSave3DMode((unsigned char)status_3d);
    android::TvService::getIntance()->Send3DState((int)status_3d);
}

void Tvin_3DMODE_SSM(tvin_3d_mode_t mode)
{
    switch (mode) {
    case MODE3D_OFF_LR_SWITCH:
    case MODE3D_OFF_LR_SWITCH_BT:
        SSMSave3DLRSwitch(0);
        break;
    case MODE3D_ON_LR_SWITCH:
    case MODE3D_ON_LR_SWITCH_BT:
        SSMSave3DLRSwitch(1);
        break;
    case MODE3D_OFF_3D_TO_2D:
    case MODE3D_OFF_3D_TO_2D_BT:
        SSMSave3DTO2D(0);
        break;
    case MODE3D_L_3D_TO_2D:
    case MODE3D_L_3D_TO_2D_BT:
        SSMSave3DTO2D(1);
        break;
    case MODE3D_R_3D_TO_2D:
    case MODE3D_R_3D_TO_2D_BT:
        SSMSave3DTO2D(2);
        break;
    case MODE3D_DISABLE:
    case MODE3D_AUTO:
    case MODE3D_2D_TO_3D:
    case MODE3D_LR:
    case MODE3D_BT:
        SSMSave3DLRSwitch(0);
        SSMSave3DTO2D(0);
        break;
    case MODE3D_FIELD_DEPTH:
    default:
        break;
    }
}

void Tvin_SetDepthOf2Dto3D(int value)
{
    //value = -16~16
    int tmp_value = DepthTable_2DTO3D[value + 16];
    if(gTvinConfig.new_2d3ddepth == 0x55) {
        if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
            if (gTvinConfig.depth_reverse == 0x55) {
                if(value > 0)
                    tmp_value = 11*8 + value*2;
                else
                    tmp_value = value*2 - 11*8;
            } else {
                if(value > 0)
                    tmp_value = (0- value)*2 - 11*8;
                else
                    tmp_value = 11*8 - value*2;
            }
            ALOGD("%s, value: %d", __FUNCTION__,tmp_value);
            TvinApi_Set2D3DDepth(tmp_value);
        } else {
            ALOGD("%s, value: %d", __FUNCTION__,value);
            TvinApi_Set2D3DDepth(value);
        }
    } else {
        switch(SSMReadPanelType()) {
        case PANEL_39_IW:
        case PANEL_42_IW:
        case PANEL_50_IW:
        case PANEL_39_CM:
        case PANEL_42_CM:
        case PANEL_50_CM:
        case PANEL_42_SL:
            if(value ==0) {
                TvinApi_SetDepthOfField(-19);
                return;
            }
            break;
        }
        TvinApi_SetDepthOfField(tmp_value * 2);
    }
}

int TvinResetVgaAjustParam(void)
{
    unsigned char buff[TVIN_DATA_POS_VGA_ADJ_VALUE_SIZE];
    memset(buff, 0, TVIN_DATA_POS_VGA_ADJ_VALUE_SIZE);
    return SSMSaveVGAAdjustValue(0, TVIN_DATA_POS_VGA_ADJ_VALUE_SIZE, buff);
}

pthread_t TvinSigDetect_ThreadId = 0;
tvin_sig_detect_para_t gTvinSigDetectPara = {
    //.pre_fmt =
    TVIN_SIG_FMT_NULL,
    //.pre_trans_fmt =
    TVIN_TFMT_2D,
    //.pre_sig_status =
    TVIN_SIG_STATUS_NULL,
};
tvin_vga_auto_adjust_para_t gTvinVgaAutoAdjustPara = {
    TV_VGA_AUTO_ADJUST_STATUS_STOP,
    TVAFE_CMD_STATUS_IDLE,
    0,
    0,
    {
        0,
        0,
        0,
        0,
        0,
    },
};

bool TvinSigDetect_IsFmtChange(void)
{
    if (gTvinSigDetectPara.pre_fmt != gTvinApiPara.tvin_para.info.fmt) {
        ALOGD("%s, FORMAT Change: %d--->%d.", __FUNCTION__, gTvinSigDetectPara.pre_fmt, gTvinApiPara.tvin_para.info.fmt);
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigChange(void)
{
    if (gTvinSigDetectPara.pre_sig_status != Tvin_GetSigStatus()) {
        if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_STABLE) {
            //vdinJNI.TurnOnBlueScreen(2);
            ALOGD("%s, If pre status is stable, enable blackscreen in IsSignalChange!", __FUNCTION__);
            AudioSetSysMuteStatus(CC_MUTE_ON);
            ALOGD("%s, STATUS Change: MUTE AUDIO in IsSignalChange.", __FUNCTION__);
        }
        if (gTvinApiPara.check_stable_count != 0) {
            gTvinApiPara.check_stable_count = 0;
            ALOGD("%s, Clear CheckStableCount in IsSigChange.", __FUNCTION__);
        }
        if (gTvinApiPara.source_switch_donecount != 0) {
            gTvinApiPara.source_switch_donecount = 0;
            ALOGD("%s, Clear SourceSwitchDoneCount in IsSigChange.", __FUNCTION__);
        }
        ALOGD("%s, STATUS Change: %d--->%d.", __FUNCTION__, gTvinSigDetectPara.pre_sig_status, Tvin_GetSigStatus());
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigTrans2D_3DFmtChange(void)
{
    if (gTvinSigDetectPara.pre_trans_fmt != gTvinApiPara.tvin_para.info.trans_fmt) {
        if ((gTvinSigDetectPara.pre_trans_fmt == TVIN_TFMT_2D) || (gTvinApiPara.tvin_para.info.trans_fmt == TVIN_TFMT_2D)) {
            ALOGD("%s, Trans_Fmt 2D_3D Change: %d--->%d.", __FUNCTION__, gTvinSigDetectPara.pre_trans_fmt, gTvinApiPara.tvin_para.info.trans_fmt);
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigStableToUnStable(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_STABLE && Tvin_GetSigStatus() == TVIN_SIG_STATUS_UNSTABLE) {
        ALOGD("%s, STATUS change: STABLE->UNSTABLE", __FUNCTION__);
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigStableToUnSupport(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_STABLE && Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOTSUP) {
        ALOGD("%s, STATUS change: STABLE->NOT_SUPPORT.", __FUNCTION__);
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigStableToNull(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_STABLE) {
        if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NULL) {
            ALOGD("%s, STATUS change: STABLE->NULL.", __FUNCTION__);
            return true;
        } else if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOSIG) {
            ALOGD("%s, STATUS change: STABLE->NO_SIGNAL.", __FUNCTION__);
            return true;
        } else
            return false;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigUnStableToUnSupport()
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_UNSTABLE && Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOTSUP) {
        ALOGD("%s, STATUS change: UNSTABLE->NOT_SUPPORT.", __FUNCTION__);
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigUnStableToNull(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_UNSTABLE) {
        if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NULL) {
            ALOGD("%s, STATUS change: UNSTABLE->NULL.", __FUNCTION__);
            return true;
        } else if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOSIG) {
            ALOGD("%s, STATUS change: UNSTABLE->NO_SIGNAL.", __FUNCTION__);
            return true;
        } else
            return false;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsNullToNoSig(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_NULL) {
        if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOSIG) {
            ALOGD("%s, STATUS change: NULL -> NO_SIGNAL.", __FUNCTION__);
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool TvinSigDetect_IsNoSigToUnstable(void)
{
    if (gTvinSigDetectPara.pre_sig_status == TVIN_SIG_STATUS_NOSIG
        && Tvin_GetSigStatus() == TVIN_SIG_STATUS_UNSTABLE) {
        ALOGD("%s, STATUS change: NO_SIGNAL -> UNSTABLE.", __FUNCTION__);
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSigStable(void)
{
    if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_STABLE) {
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsNoSignal(void)
{
    if ((Tvin_GetSigStatus() == TVIN_SIG_STATUS_NULL) || (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOSIG)) {
        return true;
    } else {
        return false;
    }
}

bool TvinSigDetect_IsSignalUnSupport(void)
{
    if (Tvin_GetSigStatus() == TVIN_SIG_STATUS_NOTSUP) {
        return true;
    } else {
        return false;
    }
}

tvin_vga_auto_adjust_status_t TvinGetVagAutoAdjustStatus(void)
{
    return gTvinVgaAutoAdjustPara.vga_auto_adjust_status;
}

int TvinSetVagAutoAdjustStatus(tvin_vga_auto_adjust_status_t status)
{
    gTvinVgaAutoAdjustPara.vga_auto_adjust_status = status;
    return 0;
}

int TvinGetVgaAutoAdjustForceStart(void)
{
    return gTvinVgaAutoAdjustPara.vga_auto_adjust_force_start;
}

int TvinSetVgaAutoAdjustForceStart(int flag)
{
    gTvinVgaAutoAdjustPara.vga_auto_adjust_force_start = flag;
    return 0;
}

void TvinSourceSignalDetect()
{
    if(bMappingInitFinish == false || gTvinApiPara.is_suspend_source_signal_detect == true) {
        //ALOGW("%s, wait for bMappingInitFinish", __FUNCTION__);
        return ;
    }
    if( sourceDetectIndex >= MAX_SOURCE) {
        sourceDetectIndex = 0;
    }

    if( sourceDetectIndex == SOURCE_TV ||
        sourceDetectIndex == SOURCE_YPBPR2||
        sourceDetectIndex == SOURCE_HDMI3 ||
        sourceDetectIndex == SOURCE_MPEG ||
        sourceDetectIndex == SOURCE_DTV ||
        sourceDetectIndex == SOURCE_SVIDEO
      ) { //patch for some source can not auto detect
        sourceDetectIndex ++;
        return;
    }

    if (sourceDetectIndex ==Tvin_GetSourceInput()) {

        if (TvinApi_GetVdinPortSignal(source_mapping_table[sourceDetectIndex]) == true)
            bCurDetResult[sourceDetectIndex] =true;
        else
            bCurDetResult[sourceDetectIndex] =false;
    } else {
        bCurDetResult[sourceDetectIndex] = TvinApi_GetVdinPortSignal(source_mapping_table[sourceDetectIndex] );
    }

    if (bCurDetResult[sourceDetectIndex] !=  bPreDetResult[sourceDetectIndex]) {
        bPreDetResult[sourceDetectIndex] = bCurDetResult[sourceDetectIndex];

        if ((bCurDetResult[sourceDetectIndex] == true) && (sourceDetectIndex == Tvin_GetSourceInput())) {
            //sendMessage
            ALOGE("%s, 1.MSG_CURRENT_SOURCE_PLUG_OUT,Source:%d", __FUNCTION__,sourceDetectIndex);
            android::TvService::getIntance()->SendSourceDetectState(sourceDetectIndex,MSG_CURRENT_SOURCE_PLUG_OUT);
        }
        /*check plug in*/
        else if(bCurDetResult[sourceDetectIndex] == false) {
            //sendMessage
            ALOGE("%s, 2.MSG_SIGNAL_AUTO_SWITCH,Source:%d", __FUNCTION__,sourceDetectIndex);
            android::TvService::getIntance()->SendSourceDetectState(sourceDetectIndex,MSG_SIGNAL_AUTO_SWITCH);
        } else {
            //sendMessage
            ALOGE("%s, 3.MSG_SIGNAL_STATUS_UPDATED,Source:%d", __FUNCTION__,sourceDetectIndex);
            android::TvService::getIntance()->SendSourceDetectState(sourceDetectIndex,MSG_SIGNAL_STATUS_UPDATED);
        }

    }

    sourceDetectIndex ++;
}

void Tvin_SuspendSourceDetect(bool flag )
{
    gTvinApiPara.is_suspend_source_signal_detect = flag;
}
void *TvinSourceDetect_TreadRun(void* data)
{
    int delayCnt = 0;
    prctl(PR_SET_NAME,(unsigned long)"TvinSourceDetect_TreadRun");
    while(1) {
        if(delayCnt < 20) { //delay 10S wait for client stable
            usleep(1000 * 1000);//delay 1S
            delayCnt ++ ;
            continue;
        }
        usleep(500 * 1000);
        TvinSourceSignalDetect();
    }
    return ((void*) 0);

}
void *TvinSigDetect_TreadRun(void* data)
{
    prctl(PR_SET_NAME,(unsigned long)"TvinSigDetect_TreadRun");
    while (gTvinApiPara.is_turnon_signal_detect_thread == true) {
        if(DrvDynaBacklight != NULL) { //
            DrvDynaBacklight();
        }
        if ((gTvinApiPara.is_hdmi_sr_detect_start == true)
            && (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI)) {
            int sr = get_hdmi_sampling_rate();
            if ((sr > 0) && (sr != gTvinApiPara.hdmi_sampling_rate)) {
                if (0 == gTvinApiPara.hdmi_sampling_rate) {
                    ALOGD("[%s:%d] Init HDMI audio, sampling rate:%d", __FUNCTION__, __LINE__, sr);
                    AudioAlsaInit(48000, sr);
                } else {
                    ALOGD("[%s:%d] Reset HDMI sampling rate to %d", __FUNCTION__, __LINE__, sr);
                    AudioSetTrackerSr(sr);
                }
                gTvinApiPara.hdmi_sampling_rate = sr;
            }
        }
        if (gTvinApiPara.is_signal_detect_thread_start == true) {
            {
                usleep(10 * 1000);
                if (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_STOP || Tvin_GetTvinStatus() == TVIN_STATUS_NORMAL_STOP) {
                    gTvinApiPara.is_signal_detect_thread_start = false;
                    gTvinApiPara.is_signal_detect_exec_done = true;
                    gTvinApiPara.is_source_switch_exec_done = true;
                    continue;
                }
                gTvinSigDetectPara.pre_trans_fmt = Tvin_GetSigTransFormat();
                gTvinSigDetectPara.pre_fmt = Tvin_GetSigFormat();
                gTvinSigDetectPara.pre_sig_status = Tvin_GetSigStatus();
                TvinApi_GetSignalInfo(0, &(gTvinApiPara.tvin_para.info));
            }
            {
                if (TvinSigDetect_IsSigChange() == true) {
                    if (TvinSigDetect_IsSigStable() == true) {
                        gTvinApiPara.is_signal_detect_exec_done = false;
                        //if (Tvin_GetSrcPort() != TVIN_PORT_CVBS0) {
                        if (gTvinConfig.autoset_displayfreq == 0x55) {
                            if (Tvin_is50HzFrameRateFmt()) {
                                TvinApi_SetDisplayVFreq(50);
                                ALOGD("%s, SetDisplayVFreq 50HZ.", __FUNCTION__);
                            } else {
                                TvinApi_SetDisplayVFreq(60);
                                ALOGD("%s, SetDisplayVFreq 60HZ.", __FUNCTION__);
                            }
                        }
                        switch(SSMReadPanelType()) {
                        case PANEL_39_IW:
                        case PANEL_42_IW:
                        case PANEL_50_IW:
                        case PANEL_39_CM:
                        case PANEL_42_CM:
                        case PANEL_50_CM:
                        case PANEL_42_SL:
                            if(Tvin_Get3DMode()!= MODE3D_DISABLE && Tvin_Get3DMode()!= MODE3D_MAX ) {
                                ALOGD("%s, current 3d mode =%d......", __FUNCTION__, Tvin_Get3DMode());
                                TvinApi_SetDisplayVFreq(60);
                            }
                            break;
                        }
                        DreamPanelSetResumeLastBLFlag(1);
                        //}
                        if ((Tvin_GetTvinStatus() != TVIN_STATUS_PREVIEW_START) && (gTvinConfig.hdmi_auto3d != 0x55) && (SSMReadDisable3D() != 0x55)) {
                            //if (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
                            {
                                if (Tvin_GetSigTransFormat() == TVIN_TFMT_2D) {
                                    if (Tvin_Get3DStatus() == STATUS3D_AUTO || Tvin_Get3DStatus() == STATUS3D_AUTO_LR || Tvin_Get3DStatus() == STATUS3D_AUTO_BT) {
                                        Tvin_Set3DStatus(STATUS3D_DISABLE);
                                        TvinApi_Send3DCommand(MODE3D_DISABLE);
                                        if (gTvinConfig.new_3dautodetc == 0x55) {
                                            TvinApi_SetDI3DDetc(MODE3D_DISABLE);
                                        }
                                        TvinApi_Set2Dto3D(0);
                                        SSMSave3DLRSwitch(0);
                                        SSMSave3DTO2D(0);
                                        ALOGD("%s, Add 2D Path in 2D-AUTO", __FUNCTION__);
                                    }
                                } else if (Tvin_GetSigTransFormat() == TVIN_TFMT_3D_DET_TB) {
                                    if (Tvin_Get3DStatus() != STATUS3D_AUTO) {
                                        Tvin_Set3DStatus(STATUS3D_AUTO_BT);
                                        TvinApi_Send3DCommand(MODE3D_BT);
                                        if (gTvinConfig.new_3dautodetc == 0x55) {
                                            TvinApi_SetDI3DDetc(MODE3D_AUTO);
                                        }
                                        SSMSave3DLRSwitch(0);
                                        SSMSave3DTO2D(0);
                                    }
                                } else if (Tvin_GetSigTransFormat() == TVIN_TFMT_3D_DET_LR) {
                                    if (Tvin_Get3DStatus() != STATUS3D_AUTO) {
                                        Tvin_Set3DStatus(STATUS3D_AUTO_LR);
                                        TvinApi_Send3DCommand(MODE3D_LR);
                                        if (gTvinConfig.new_3dautodetc == 0x55) {
                                            TvinApi_SetDI3DDetc(MODE3D_AUTO);
                                        }
                                        SSMSave3DLRSwitch(0);
                                        SSMSave3DTO2D(0);
                                    }
                                } else {
                                    if (Tvin_Get3DStatus() != STATUS3D_AUTO) {
                                        Tvin_Set3DStatus(STATUS3D_AUTO);
                                        TvinApi_Send3DCommand(MODE3D_AUTO);
                                        if (gTvinConfig.new_3dautodetc == 0x55) {
                                            TvinApi_SetDI3DDetc(MODE3D_AUTO);
                                        }
                                        SSMSave3DLRSwitch(0);
                                        SSMSave3DTO2D(0);
                                    }
                                    if (gTvinConfig.new_d2d3 != 0x55) {
                                        TvinApi_Set2Dto3D(1);
                                    }
                                    ALOGD("%s, Add 3D Path in 3D-AUTO", __FUNCTION__);
                                }
                            }
                        }
                        if (gTvinConfig.preview_freescale == 0x55 &&
                            Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
                            ALOGD("%s, PREVIEW SET MPEG PQ DATA ALWAYS.", __FUNCTION__);
                            Tv_LoadVppSettings(SOURCE_TYPE_MPEG, TVIN_SIG_FMT_NULL, STATUS3D_DISABLE, TVIN_TFMT_2D);
                        } else {
                            if(Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
                                ALOGE("%s, PREVIEW SET MPEG PQ DATA ALWAYS.", __FUNCTION__);
                                ALOGE("%s.Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START.\n", __FUNCTION__);
                                Tv_LoadVppSettings(SOURCE_TYPE_MPEG, TVIN_SIG_FMT_NULL, STATUS3D_DISABLE, TVIN_TFMT_2D);
                            } else {
                                Tv_LoadVppSettings(Tvin_GetSrcInputType(), Tvin_GetSigFormat(), Tvin_Get3DStatus(), Tvin_GetSigTransFormat());
                            }
                        }
                        if ((Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI)) {
                            if (Tvin_IsDVISignal()) {
                                if (gTvinConfig.dvi_audio_vga == 0x55) {
                                    if (Tvin_GetSourceAudioInput() != SOURCE_AUDIO_VGA) {
                                        TvinAudioSwitch(SOURCE_VGA);
                                        ALOGD("%s, HDMI audio switch to DVI.", __FUNCTION__);
                                    }
                                }
                            } else {
                                if (gTvinConfig.dvi_audio_vga == 0x55) {
                                    if (Tvin_GetSourceAudioInput() == SOURCE_AUDIO_VGA) {
                                        TvinAudioSwitch(Tvin_GetSourceInput());
                                        ALOGD("%s, DVI audio switch to HDMI.", __FUNCTION__);
                                    }
                                }
                            }
                        }
                        if (0 == Tvin_StartDecoder(0, true)) {
                            if (gTvinConfig.tv_out_counter > 60) {
                                gTvinApiPara.stable_count = gTvinApiPara.stable_count + 10;
                            } else {
                                gTvinApiPara.stable_count = 60;
                            }
                            ALOGD("%s, Signal change to stable, start decoder and counter[%d] down!", __FUNCTION__, gTvinApiPara.stable_count);
                        } else {
                            gTvinApiPara.is_signal_detect_exec_done = true;
                            ALOGD("%s, StartDecoder faild in signal change stable!", __FUNCTION__);
                            continue;
                        }
                    } else {
                        if (TvinSigDetect_IsSigStableToUnStable() == true) {
                            Tvin_TurnOnBlueScreen(2);
                            Tvin_StopDecoder(0);
                            if (Tvin_GetSourceInput() == SOURCE_VGA) {
                                if (TvinGetVagAutoAdjustStatus() != TV_VGA_AUTO_ADJUST_STATUS_STOP) {
                                    //sendMessage
                                    android::TvService::getIntance()->SendVGAAutoAdjustState(-4);
                                    ALOGW("%s, TvinSetVagAutoAdjust is terminated by signal change.", __FUNCTION__);
                                }
                                TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_START);
                                ALOGD("%s, It's VGA, in StableToUnStable!", __FUNCTION__);
                            } else {
                                ALOGD("%s, It's not VGA, in StableToUnStable!", __FUNCTION__);
                            }
                            ALOGD("%s, Enable blackscreen for signal change in StableToUnStable!", __FUNCTION__);
                        } else if (TvinSigDetect_IsSigStableToUnSupport() == true) {
                            Tvin_TurnOnBlueScreen(1);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                Tvin_StopDecoder(0);
                                //sendMessage
                                android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Enable bluescreen for signal change in StableToUnSupport!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable bluescreen for signal change in source switch StableToUnSupport, source switch count down.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsSigUnStableToUnSupport() == true) {
                            Tvin_TurnOnBlueScreen(1);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                Tvin_StopDecoder(0);
                                //sendMessage
                                android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Enable bluescreen for signal change in UnStableToUnSupport!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable bluescreen for signal change in source switch UnStableToUnSupport, source switch count down.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsSigStableToNull() == true) {
                            Tvin_TurnOnBlueScreen(1);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                Tvin_StopDecoder(0);
                                if (Tvin_GetSourceInput() == SOURCE_VGA) {
                                    if (TvinGetVagAutoAdjustStatus() != TV_VGA_AUTO_ADJUST_STATUS_STOP) {
                                        //sendMessage
                                        android::TvService::getIntance()->SendVGAAutoAdjustState(-4);
                                        ALOGW("%s, TvinSetVagAutoAdjust is terminated by signal change.", __FUNCTION__);
                                    }
                                    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_START);
                                    ALOGD("%s, It's VGA, in StableToNull!", __FUNCTION__);
                                } else {
                                    ALOGD("%s, It's not VGA, in StableToNull!", __FUNCTION__);
                                }
                                //sendMessage
                                android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Enable bluescreen for signal change in StableToNull!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable blackscreen for signal change in source switch StableToNull, source switch count down.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsSigUnStableToNull() == true) {
                            Tvin_TurnOnBlueScreen(1);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                //sendMessage
                                android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Enable bluescreen for signal change in UnStableToNull!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable blackscreen for signal change in source switch UnStableToNull, source switch count down.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsNullToNoSig() == true) {
                            Tvin_TurnOnBlueScreen(1);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                //sendMessage
                                android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Enable bluescreen for signal change in NullToNoSignal!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable bluescreen for signal change in source switch NullToNoSignal, source switch count down.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsNoSigToUnstable() == true) {
                            Tvin_TurnOnBlueScreen(2);
                            if (Tvin_IsSrcSwitchExecDone()) {
                                ALOGD("%s, Enable blackscreen for signal change in NoSigToUnstable!", __FUNCTION__);
                            } else {
                                gTvinApiPara.source_switch_donecount = 10;
                                ALOGD("%s, Enable blackscreen for signal change in source switch NoSigToUnstable, source switch count down.", __FUNCTION__);
                            }
                        }
                    }
                } else {
                    if (TvinSigDetect_IsSigStable() == true) {
                        if (TvinSigDetect_IsFmtChange() == true) {
                            gTvinApiPara.is_signal_detect_exec_done = false;
                            Tvin_TurnOnBlueScreen(2);
                            Tvin_StopDecoder(0);
                            gTvinApiPara.check_stable_count = 20;
                            if (Tvin_GetSourceInput() == SOURCE_VGA) {
                                TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_START);
                                ALOGD("%s, It's VGA, in fmt change.", __FUNCTION__);
                            } else {
                                ALOGD("%s, It's not VGA, in fmt change.", __FUNCTION__);
                            }
                        } else if (TvinSigDetect_IsSigTrans2D_3DFmtChange()) {
                            gTvinApiPara.is_signal_detect_exec_done = false;
                            Tvin_TurnOnBlueScreen(2);
                            Tvin_StopDecoder(0);
                        } else {
                            if ((Tvin_GetSourceInput() != SOURCE_VGA)
                                || (TvinGetVagAutoAdjustStatus() == TV_VGA_AUTO_ADJUST_STATUS_STOP)) {
                                if (gTvinApiPara.stable_count > 0) {
                                    if (--gTvinApiPara.stable_count == gTvinConfig.tv_out_counter) {
                                        Tvin_TurnOnBlueScreen(0);
                                        // adjust dsp read and write pointer
                                        AudioResetDSPRWPtr();

                                        //sendMessage
                                        android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                        ALOGD("%s, Enable video layer for signal stable, tvin_para.info.reserved[0x%x].", __FUNCTION__, gTvinApiPara.tvin_para.info.reserved);
                                    } else if (gTvinApiPara.stable_count == 0) {
                                        if (Tvin_GetSourceInput() == SOURCE_TV) {
                                            if (ATVGetChannelSearchFlag() == 1) {
                                                AudioSetSysMuteStatus(CC_MUTE_ON);
                                            } else {
                                                AudioSetSysMuteStatus(CC_MUTE_OFF);
                                                ALOGD("%s, Mute off when atv get stable!", __FUNCTION__);
                                            }
                                        } else {
                                            AudioSetSysMuteStatus(CC_MUTE_OFF);
                                            ALOGD("%s, Mute off when singal get stable!", __FUNCTION__);
                                        }
                                        if (gTvinApiPara.is_source_switch_exec_done == false) {
                                            gTvinApiPara.source_switch_donecount = 2;
                                        }
                                    } else {
                                        //ALOGD("TvinSigDetect_TreadRun, In signal stable, StableCount : = %d.", gTvinApiPara.stable_count);
                                    }
                                } else if (gTvinApiPara.check_stable_count > 0) {
                                    if (--gTvinApiPara.check_stable_count == 0) {
                                        gTvinApiPara.tvin_para.info.status = TVIN_SIG_STATUS_UNSTABLE;
                                        gTvinApiPara.is_signal_detect_exec_done = true;
                                        ALOGD("%s, Set status to UN_STABLE for fmt change or tv channel swich!", __FUNCTION__);
                                        continue;
                                    }
                                } else if (gTvinApiPara.source_switch_donecount > 0) {
                                    //ALOGD("TvinSigDetect_TreadRun, In signal stable, gTvinApiPara.source_switch_donecount : = %d", gTvinApiPara.source_switch_donecount);
                                    if (--gTvinApiPara.source_switch_donecount == 0) {
                                        gTvinApiPara.is_source_switch_exec_done = true;
                                        //sendMessage
                                        android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
                                        ALOGD("%s, Signal stable, source switch done.", __FUNCTION__);
                                    }
                                }
                            } else {
                                if (TvinGetVagAutoAdjustStatus() == TV_VGA_AUTO_ADJUST_STATUS_PROCESSING) {
                                    if (gTvinVgaAutoAdjustPara.vga_auto_adjust_status_check_count) {
                                        if (--gTvinVgaAutoAdjustPara.vga_auto_adjust_status_check_count == 0) {
                                            if (TvinApi_GetVGAAutoAdjustCMDStatus(&gTvinVgaAutoAdjustPara.vga_auto_adjust_cmd_status) == 0) {
                                                switch (gTvinVgaAutoAdjustPara.vga_auto_adjust_cmd_status) {
                                                case TVAFE_CMD_STATUS_PROCESSING:
                                                    if (gTvinConfig.vag_force_adjust == 1) {
                                                        Tvin_TurnOnBlueScreen(0);
                                                        ALOGD("%s, enable video when vag auto adjusting.", __FUNCTION__);
                                                    }
                                                    gTvinVgaAutoAdjustPara.vga_auto_adjust_status_check_count = 255;
                                                    break;
                                                case TVAFE_CMD_STATUS_SUCCESSFUL:
                                                    if (TvinApi_GetVGACurTimingAdj(&gTvinVgaAutoAdjustPara.vga_auto_adjust_para) != 0) {
                                                        if (TvinGetVgaAutoAdjustForceStart()) {
                                                            //sendMessage
                                                            android::TvService::getIntance()->SendVGAAutoAdjustState(-2);
                                                        }
                                                        ALOGW("%s, TvinApi_GetVGACurTimingAdj failed.", __FUNCTION__);
                                                    } else {
                                                        if (Tv_SetVGAAjustPara(gTvinVgaAutoAdjustPara.vga_auto_adjust_para, Tvin_GetSigFormat()) != 0) {
                                                            if (TvinGetVgaAutoAdjustForceStart()) {
                                                                //sendMessage
                                                                android::TvService::getIntance()->SendVGAAutoAdjustState(-3);
                                                            }
                                                            ALOGW("%s, Tv_SetVGAAjustPara failed.", __FUNCTION__);
                                                        } else {
                                                            if (TvinGetVgaAutoAdjustForceStart()) {
                                                                //sendMessage
                                                                android::TvService::getIntance()->SendVGAAutoAdjustState(1);
                                                            }
                                                            ALOGD("%s, TvinSetVagAutoAdjust successful----1.", __FUNCTION__);
                                                        }
                                                    }
                                                    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                                    TvinSetVgaAutoAdjustForceStart(0);
                                                    break;
                                                case TVAFE_CMD_STATUS_FAILED:
                                                    ALOGW("%s, TvinSetVagAutoAdjust failed.", __FUNCTION__);
                                                    if (TvinGetVgaAutoAdjustForceStart()) {
                                                        //sendMessage
                                                        android::TvService::getIntance()->SendVGAAutoAdjustState(-1);
                                                    }
                                                    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                                    TvinSetVgaAutoAdjustForceStart(0);
                                                    break;
                                                case TVAFE_CMD_STATUS_TERMINATED:
                                                    ALOGW("%s, TvinSetVagAutoAdjust terminated.", __FUNCTION__);
                                                    if (TvinGetVgaAutoAdjustForceStart()) {
                                                        //sendMessage
                                                        android::TvService::getIntance()->SendVGAAutoAdjustState(-4);
                                                    }
                                                    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                                    TvinSetVgaAutoAdjustForceStart(0);
                                                    break;
                                                case TVAFE_CMD_STATUS_IDLE:
                                                default:
                                                    ALOGW("%s, TvinSetVagAutoAdjust idle.", __FUNCTION__);
                                                    //sendMessage
                                                    android::TvService::getIntance()->SendVGAAutoAdjustState(-5);
                                                    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                                    TvinSetVgaAutoAdjustForceStart(0);
                                                    break;
                                                }
                                            } else {
                                                TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                            }
                                        }
                                    }
                                } else if (TvinGetVagAutoAdjustStatus() == TV_VGA_AUTO_ADJUST_STATUS_START) {
                                    if (TvinGetVgaAutoAdjustForceStart() || (Tv_IsVGAAutoAdjustDone(Tvin_GetSigFormat()) != 1)) {
                                        if (TvinApi_SetVGAAutoAdjust() != 0) {
                                            TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                            TvinSetVgaAutoAdjustForceStart(0);
                                            ALOGW("%s, TvinSetVagAutoAdjust failed.", __FUNCTION__);
                                        } else {
                                            TvinSetVgaAutoAdjustForceStart(1);
                                            TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_PROCESSING);
                                            gTvinVgaAutoAdjustPara.vga_auto_adjust_status_check_count = 255;
                                            //sendMessage
                                            //android::TvService::getIntance()->SendVGAAutoAdjustState(0);
                                            ALOGD("%s, TvinSetVagAutoAdjust start.", __FUNCTION__);
                                        }
                                    } else {
                                        if (0 == Tv_GetVGAAjustPara(&gTvinVgaAutoAdjustPara.vga_auto_adjust_para, Tvin_GetSigFormat())) {
                                            if (Tv_SetVGAAjustPara(gTvinVgaAutoAdjustPara.vga_auto_adjust_para, Tvin_GetSigFormat()) != 0) {
                                                ALOGW("%s, Tv_SetVGAAjustPara failed.", __FUNCTION__);
                                            } else {
                                                ALOGD("%s, Tv_SetVGAAjustPara successful---2.", __FUNCTION__);
                                            }
                                        } else {
                                            ALOGW("%s, Tv_GetVGAAjustPara failed.", __FUNCTION__);
                                        }
                                        TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_STOP);
                                    }
                                }
                            }
                        }
                    } else if (TvinSigDetect_IsNoSignal() == true) {
                        if (gTvinApiPara.source_switch_donecount > 0) {
                            if (--gTvinApiPara.source_switch_donecount == 0) {
                                gTvinApiPara.is_source_switch_exec_done = true;
                                //sendMessage
                                //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
                                //sendMessage
                                //android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, No signal, source switch done.", __FUNCTION__);
                            }
                            //ALOGD("TvinSigDetect_TreadRun, In no signal, gTvinApiPara.source_switch_donecount : = %d", gTvinApiPara.source_switch_donecount);
                        }
                    } else if (TvinSigDetect_IsSignalUnSupport() == true) {
                        if (gTvinApiPara.source_switch_donecount > 0) {
                            if (--gTvinApiPara.source_switch_donecount == 0) {
                                gTvinApiPara.is_source_switch_exec_done = true;
                                //sendMessage
                                //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
                                //sendMessage
                                //android::TvService::getIntance()->SendSigInfo(Tvin_GetSigInfo());
                                ALOGD("%s, Unsupport signal, source switch done.", __FUNCTION__);
                            } else {
                                //ALOGD("TvinSigDetect_TreadRun, In unsupport signal, gTvinApiPara.source_switch_donecount :=%d ", gTvinApiPara.source_switch_donecount);
                            }
                        }
                    } else {
                        if (gTvinApiPara.source_switch_donecount > 0) {
                            if (--gTvinApiPara.source_switch_donecount == 0) {
                                gTvinApiPara.is_source_switch_exec_done = true;
                                //sendMessage
                                //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
                                ALOGD("%s, Unstable signal, source switch done.", __FUNCTION__);
                            } else {
                                //ALOGD("TvinSigDetect_TreadRun, In unstable signal, gTvinApiPara.source_switch_donecount :=%d ", gTvinApiPara.source_switch_donecount);
                            }
                        }
                    }
                }
            }
            gTvinApiPara.is_signal_detect_exec_done = true;
        } else if (gTvinApiPara.is_source_switch_thread_start == true) {
            if (TvinSourceSwitch_RealSwitchSource(Tvin_GetInputWindow(), Tv_GetSourceSwitchInput(), Tv_GetSourceSwitchAudioChannel()) == 0) {
                Tvin_StartSigDetect();
            }
            gTvinApiPara.is_source_switch_thread_start = false;
            ALOGD("%s, isSourceSwitchExecuteDone == false, source swith thread done.", __FUNCTION__);
        } else {
            usleep(10 * 1000);
        }
    }
    return ((void*) 0);
}

int TvinSigDetect_CreateThread(void)
{
    int ret = 0;
    pthread_attr_t attr;
    struct sched_param param;

    gTvinApiPara.is_turnon_signal_detect_thread = true;
    gTvinApiPara.is_signal_detect_exec_done = true;
    gTvinApiPara.is_signal_detect_thread_start = false;
    gTvinApiPara.is_signal_detect_thread_start = false;
    gTvinApiPara.is_suspend_source_signal_detect = false;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(&TvinSigDetect_ThreadId, &attr, &TvinSigDetect_TreadRun, NULL);
    pthread_attr_destroy(&attr);
    return ret;
}

void TvinSigDetect_KillThread(void)
{
    int i = 0, dly = 10;
    gTvinApiPara.is_turnon_signal_detect_thread = false;
    Tvin_StopSigDetect();
    Tvin_StopSourceSwitch();
    pthread_join(TvinSigDetect_ThreadId, NULL);
    TvinSigDetect_ThreadId = 0;
    ALOGD("%s, done.", __FUNCTION__);
}

pthread_t TvinADCAutoCalibration_ThreadId = 0;
tvin_parm_s ADCAutoCalibration_Para;
int TvinADCYPbPrGainOffsetInit(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t *adc_cal_parm)
{
    int i = 0;
    if (input == ADC_CALIBRATION_INPUT_YPBPR1 || input == ADC_CALIBRATION_INPUT_YPBPR2) {
        if(Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
            for (i = 0; i < 3; i++) {
                adc_cal_parm->comp_cal_val[i].a_analog_gain = 0x8a;
                adc_cal_parm->comp_cal_val[i].a_analog_clamp = 0x46;
                adc_cal_parm->comp_cal_val[i].a_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].a_digital_gain = 0x3cf;
                adc_cal_parm->comp_cal_val[i].a_digital_offset2 = 0x1c;

                adc_cal_parm->comp_cal_val[i].b_analog_gain = 0xae;
                adc_cal_parm->comp_cal_val[i].b_analog_clamp = 0x43;
                adc_cal_parm->comp_cal_val[i].b_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].b_digital_gain = 0x383;
                adc_cal_parm->comp_cal_val[i].b_digital_offset2 = 0x40;

                adc_cal_parm->comp_cal_val[i].c_analog_gain = 0xa4;
                adc_cal_parm->comp_cal_val[i].c_analog_clamp = 0x47;
                adc_cal_parm->comp_cal_val[i].c_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].c_digital_gain = 0x383;
                adc_cal_parm->comp_cal_val[i].c_digital_offset2 = 0x40;

                adc_cal_parm->comp_cal_val[i].d_analog_gain = 0x00;
                adc_cal_parm->comp_cal_val[i].d_analog_clamp = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_gain = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_offset2 = 0x00;
            }
        } else {
            for (i = 0; i < 3; i++) {
                adc_cal_parm->comp_cal_val[i].a_analog_gain = 0x77;
                adc_cal_parm->comp_cal_val[i].a_analog_clamp = 0x47;
                adc_cal_parm->comp_cal_val[i].a_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].a_digital_gain = 0x3cf;
                adc_cal_parm->comp_cal_val[i].a_digital_offset2 = 0x1c;

                adc_cal_parm->comp_cal_val[i].b_analog_gain = 0xb0;
                adc_cal_parm->comp_cal_val[i].b_analog_clamp = 0x42;
                adc_cal_parm->comp_cal_val[i].b_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].b_digital_gain = 0x383;
                adc_cal_parm->comp_cal_val[i].b_digital_offset2 = 0x40;

                adc_cal_parm->comp_cal_val[i].c_analog_gain = 0x94;
                adc_cal_parm->comp_cal_val[i].c_analog_clamp = 0x3c;
                adc_cal_parm->comp_cal_val[i].c_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].c_digital_gain = 0x383;
                adc_cal_parm->comp_cal_val[i].c_digital_offset2 = 0x40;

                adc_cal_parm->comp_cal_val[i].d_analog_gain = 0x00;
                adc_cal_parm->comp_cal_val[i].d_analog_clamp = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_offset1 = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_gain = 0x00;
                adc_cal_parm->comp_cal_val[i].d_digital_offset2 = 0x00;
            }
        }
        return 0;
    }
    return -1;
}
int TvinADCGainOffsetInit(tvin_adc_calibration_input_t input, tvafe_adc_cal_s *adc_cal_parm)
{
    if (input == ADC_CALIBRATION_INPUT_VGA) {
        if(Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
            adc_cal_parm->a_analog_gain = 0xa8;
            adc_cal_parm->a_analog_clamp = 0x44;
            adc_cal_parm->a_digital_offset1 = 0x00;
            adc_cal_parm->a_digital_gain = 0x400;
            adc_cal_parm->a_digital_offset2 = 0x00;

            adc_cal_parm->b_analog_gain = 0xa7;
            adc_cal_parm->b_analog_clamp = 0x40;
            adc_cal_parm->b_digital_offset1 = 0x00;
            adc_cal_parm->b_digital_gain = 0x400;
            adc_cal_parm->b_digital_offset2 = 0x00;

            adc_cal_parm->c_analog_gain = 0x9d;
            adc_cal_parm->c_analog_clamp = 0x43;
            adc_cal_parm->c_digital_offset1 = 0x00;
            adc_cal_parm->c_digital_gain = 0x400;
            adc_cal_parm->c_digital_offset2 = 0x00;

            adc_cal_parm->d_analog_gain = 0x00;
            adc_cal_parm->d_analog_clamp = 0x00;
            adc_cal_parm->d_digital_offset1 = 0x00;
            adc_cal_parm->d_digital_gain = 0x00;
            adc_cal_parm->d_digital_offset2 = 0x00;
        } else {
            adc_cal_parm->a_analog_gain = 0x9e;
            adc_cal_parm->a_analog_clamp = 0x3e;
            adc_cal_parm->a_digital_offset1 = 0x00;
            adc_cal_parm->a_digital_gain = 0x400;
            adc_cal_parm->a_digital_offset2 = 0x00;

            adc_cal_parm->b_analog_gain = 0xb0;
            adc_cal_parm->b_analog_clamp = 0x42;
            adc_cal_parm->b_digital_offset1 = 0x00;
            adc_cal_parm->b_digital_gain = 0x400;
            adc_cal_parm->b_digital_offset2 = 0x00;

            adc_cal_parm->c_analog_gain = 0x94;
            adc_cal_parm->c_analog_clamp = 0x3b;
            adc_cal_parm->c_digital_offset1 = 0x00;
            adc_cal_parm->c_digital_gain = 0x400;
            adc_cal_parm->c_digital_offset2 = 0x00;

            adc_cal_parm->d_analog_gain = 0x00;
            adc_cal_parm->d_analog_clamp = 0x00;
            adc_cal_parm->d_digital_offset1 = 0x00;
            adc_cal_parm->d_digital_gain = 0x00;
            adc_cal_parm->d_digital_offset2 = 0x00;

        }
    }
    return 0;
}

int TvinADCAutoCalibration_SavePara(tvin_adc_calibration_input_t input, tvafe_adc_cal_s adc_cal_parm)
{
    unsigned char val = 0x55;
    if (input < ADC_CALIBRATION_INPUT_VGA || input >= ADC_CALIBRATION_INPUT_MAX)
        return -1;
    if (SSMSaveADCCalibrationValue(((sizeof(unsigned short))*20*input), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm) == 0) {
        ALOGD("********************************\n");
        ALOGD("%s for input[%d].", __FUNCTION__, input);
        ALOGD("a_analog_clamp = 0x%x.", adc_cal_parm.a_analog_clamp);
        ALOGD("a_analog_gain = 0x%x.", adc_cal_parm.a_analog_gain);
        ALOGD("a_digital_offset1 = 0x%x.", adc_cal_parm.a_digital_offset1);
        ALOGD("a_digital_gain = 0x%x.", adc_cal_parm.a_digital_gain);
        ALOGD("a_digital_offset2 = 0x%x.", adc_cal_parm.a_digital_offset2);
        ALOGD("b_analog_clamp = 0x%x.", adc_cal_parm.b_analog_clamp);
        ALOGD("b_analog_gain = 0x%x.", adc_cal_parm.b_analog_gain);
        ALOGD("b_digital_offset1 = 0x%x.", adc_cal_parm.b_digital_offset1);
        ALOGD("b_digital_gain = 0x%x.", adc_cal_parm.b_digital_gain);
        ALOGD("b_digital_offset2 = 0x%x.", adc_cal_parm.b_digital_offset2);
        ALOGD("c_analog_clamp = 0x%x.", adc_cal_parm.c_analog_clamp);
        ALOGD("c_analog_gain = 0x%x.", adc_cal_parm.c_analog_gain);
        ALOGD("c_digital_offset1 = 0x%x.", adc_cal_parm.c_digital_offset1);
        ALOGD("c_digital_gain = 0x%x.", adc_cal_parm.c_digital_gain);
        ALOGD("c_digital_offset2 = 0x%x.", adc_cal_parm.c_digital_offset2);
        ALOGD("d_analog_clamp = 0x%x.", adc_cal_parm.d_analog_clamp);
        ALOGD("d_analog_gain = 0x%x.", adc_cal_parm.d_analog_gain);
        ALOGD("d_digital_offset1 = 0x%x.", adc_cal_parm.d_digital_offset1);
        ALOGD("d_digital_gain = 0x%x.", adc_cal_parm.d_digital_gain);
        ALOGD("d_digital_offset2 = 0x%x.", adc_cal_parm.d_digital_offset2);
        ALOGD("********************************\n");
        if (SSMSaveADCCalibrationFlagValue((1*input), 1, &val) == 0)
            return 0;
    }
    return -1;
}
int TvinADCAutoCalibration_GetPara(tvin_adc_calibration_input_t input, tvafe_adc_cal_s *adc_cal_parm)
{
    if (adc_cal_parm == NULL || input < ADC_CALIBRATION_INPUT_VGA || input > ADC_CALIBRATION_INPUT_YPBPR2)
        return -1;
    memset(adc_cal_parm, sizeof(tvafe_adc_cal_s), 0);
    if (SSMReadADCCalibrationValue(((sizeof(unsigned short))*20*input), (sizeof(unsigned short))*20, (unsigned char *)adc_cal_parm) == 0) {
        ALOGD("********************************\n");
        ALOGD("%s for input[%d].", __FUNCTION__, input);
        ALOGD("a_analog_clamp = 0x%x.", adc_cal_parm->a_analog_clamp);
        ALOGD("a_analog_gain = 0x%x.", adc_cal_parm->a_analog_gain);
        ALOGD("a_digital_offset1 = 0x%x.", adc_cal_parm->a_digital_offset1);
        ALOGD("a_digital_gain = 0x%x.", adc_cal_parm->a_digital_gain);
        ALOGD("a_digital_offset2 = 0x%x.", adc_cal_parm->a_digital_offset2);
        ALOGD("b_analog_clamp = 0x%x.", adc_cal_parm->b_analog_clamp);
        ALOGD("b_analog_gain = 0x%x.", adc_cal_parm->b_analog_gain);
        ALOGD("b_digital_offset1 = 0x%x.", adc_cal_parm->b_digital_offset1);
        ALOGD("b_digital_gain = 0x%x.", adc_cal_parm->b_digital_gain);
        ALOGD("b_digital_offset2 = 0x%x.", adc_cal_parm->b_digital_offset2);
        ALOGD("c_analog_clamp = 0x%x.", adc_cal_parm->c_analog_clamp);
        ALOGD("c_analog_gain = 0x%x.", adc_cal_parm->c_analog_gain);
        ALOGD("c_digital_offset1 = 0x%x.", adc_cal_parm->c_digital_offset1);
        ALOGD("c_digital_gain = 0x%x.", adc_cal_parm->c_digital_gain);
        ALOGD("c_digital_offset2 = 0x%x.", adc_cal_parm->c_digital_offset2);
        ALOGD("d_analog_clamp = 0x%x.", adc_cal_parm->d_analog_clamp);
        ALOGD("d_analog_gain = 0x%x.", adc_cal_parm->d_analog_gain);
        ALOGD("d_digital_offset1 = 0x%x.", adc_cal_parm->d_digital_offset1);
        ALOGD("d_digital_gain = 0x%x.", adc_cal_parm->d_digital_gain);
        ALOGD("d_digital_offset2 = 0x%x.", adc_cal_parm->d_digital_offset2);
        ALOGD("********************************\n");
    } else {
        if (input == ADC_CALIBRATION_INPUT_VGA ) {
            if(Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
                adc_cal_parm->a_analog_gain = 0xa8;
                adc_cal_parm->a_analog_clamp = 0x44;
                adc_cal_parm->a_digital_offset1 = 0x00;
                adc_cal_parm->a_digital_gain = 0x400;
                adc_cal_parm->a_digital_offset2 = 0x00;

                adc_cal_parm->b_analog_gain = 0xa7;
                adc_cal_parm->b_analog_clamp = 0x40;
                adc_cal_parm->b_digital_offset1 = 0x00;
                adc_cal_parm->b_digital_gain = 0x400;
                adc_cal_parm->b_digital_offset2 = 0x00;

                adc_cal_parm->c_analog_gain = 0x9d;
                adc_cal_parm->c_analog_clamp = 0x43;
                adc_cal_parm->c_digital_offset1 = 0x00;
                adc_cal_parm->c_digital_gain = 0x400;
                adc_cal_parm->c_digital_offset2 = 0x00;

                adc_cal_parm->d_analog_gain = 0x00;
                adc_cal_parm->d_analog_clamp = 0x00;
                adc_cal_parm->d_digital_offset1 = 0x00;
                adc_cal_parm->d_digital_gain = 0x00;
                adc_cal_parm->d_digital_offset2 = 0x00;
            } else {
                adc_cal_parm->a_analog_gain = 0x9e;
                adc_cal_parm->a_analog_clamp = 0x3e;
                adc_cal_parm->a_digital_offset1 = 0x00;
                adc_cal_parm->a_digital_gain = 0x400;
                adc_cal_parm->a_digital_offset2 = 0x00;

                adc_cal_parm->b_analog_gain = 0xb0;
                adc_cal_parm->b_analog_clamp = 0x42;
                adc_cal_parm->b_digital_offset1 = 0x00;
                adc_cal_parm->b_digital_gain = 0x400;
                adc_cal_parm->b_digital_offset2 = 0x00;

                adc_cal_parm->c_analog_gain = 0x94;
                adc_cal_parm->c_analog_clamp = 0x3b;
                adc_cal_parm->c_digital_offset1 = 0x00;
                adc_cal_parm->c_digital_gain = 0x400;
                adc_cal_parm->c_digital_offset2 = 0x00;

                adc_cal_parm->d_analog_gain = 0x00;
                adc_cal_parm->d_analog_clamp = 0x00;
                adc_cal_parm->d_digital_offset1 = 0x00;
                adc_cal_parm->d_digital_gain = 0x00;
                adc_cal_parm->d_digital_offset2 = 0x00;
            }
        }
    }
    return 0;
}

int TvinADCYPbPrAutoCalibration_SavePara(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t adc_cal_parm)
{
    unsigned char val = 0x55;
    int offset = -1;
    int ret = -1;
    int i = 0;

    if (input <= ADC_CALIBRATION_INPUT_VGA || input >= ADC_CALIBRATION_INPUT_MAX)
        return -1;

    if (input == ADC_CALIBRATION_INPUT_YPBPR1)
        offset = 0;
    else
        offset = 1;

    if (offset == 0) {
        ret = SSMSaveYPbPrADCCalibrationValue(0, (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[0]);
        ret |= SSMSaveYPbPrADCCalibrationValue(((sizeof(unsigned short))*20), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[1]);
        ret |= SSMSaveYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*2), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[2]);
    } else {
        ret = SSMSaveYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*3), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[0]);
        ret |= SSMSaveYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*4), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[1]);
        ret |= SSMSaveYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*5), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm.comp_cal_val[2]);
    }

    if (ret == 0) {
        ALOGW("*************save comp_cal_val[%d]*******************\n",input);
        ALOGW("%s for input[%d].", __FUNCTION__, input);
#if 1
        for(i = 0; i < 3; i++) {
            ALOGW("a_analog_clamp = 0x%x.", adc_cal_parm.comp_cal_val[i].a_analog_clamp);
            ALOGW("a_analog_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].a_analog_gain);
            ALOGW("a_digital_offset1 = 0x%x.", adc_cal_parm.comp_cal_val[i].a_digital_offset1);
            ALOGW("a_digital_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].a_digital_gain);
            ALOGW("a_digital_offset2 = 0x%x.", adc_cal_parm.comp_cal_val[i].a_digital_offset2);
            ALOGW("b_analog_clamp = 0x%x.", adc_cal_parm.comp_cal_val[i].b_analog_clamp);
            ALOGW("b_analog_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].b_analog_gain);
            ALOGW("b_digital_offset1 = 0x%x.", adc_cal_parm.comp_cal_val[i].b_digital_offset1);
            ALOGW("b_digital_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].b_digital_gain);
            ALOGW("b_digital_offset2 = 0x%x.", adc_cal_parm.comp_cal_val[i].b_digital_offset2);
            ALOGW("c_analog_clamp = 0x%x.", adc_cal_parm.comp_cal_val[i].c_analog_clamp);
            ALOGW("c_analog_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].c_analog_gain);
            ALOGW("c_digital_offset1 = 0x%x.", adc_cal_parm.comp_cal_val[i].c_digital_offset1);
            ALOGW("c_digital_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].c_digital_gain);
            ALOGW("c_digital_offset2 = 0x%x.", adc_cal_parm.comp_cal_val[i].c_digital_offset2);
            ALOGW("d_analog_clamp = 0x%x.", adc_cal_parm.comp_cal_val[i].d_analog_clamp);
            ALOGW("d_analog_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].d_analog_gain);
            ALOGW("d_digital_offset1 = 0x%x.", adc_cal_parm.comp_cal_val[i].d_digital_offset1);
            ALOGW("d_digital_gain = 0x%x.", adc_cal_parm.comp_cal_val[i].d_digital_gain);
            ALOGW("d_digital_offset2 = 0x%x.", adc_cal_parm.comp_cal_val[i].d_digital_offset2);
            ALOGW("****************************************************\n");
        }
#endif

        ret |= SSMSaveADCCalibrationFlagValue((1*input), 1, &val);
    }

    return ret;
}

int TvinADCYPbPrAutoCalibration_GetPara(tvin_adc_calibration_input_t input, tvafe_adc_comp_cal_t *adc_cal_parm)
{
    int offset = -1;
    int ret = -1;
    int i = 0;

    if (adc_cal_parm == NULL || input <= ADC_CALIBRATION_INPUT_VGA || input > ADC_CALIBRATION_INPUT_YPBPR2)
        return -1;

    if (input == ADC_CALIBRATION_INPUT_YPBPR1)
        offset = 0;
    else
        offset = 1;
    memset(adc_cal_parm, sizeof(tvafe_adc_comp_cal_t), 0);

    if (offset == 0) {
        ret = SSMReadYPbPrADCCalibrationValue(0, (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[0]);
        ret |= SSMReadYPbPrADCCalibrationValue(((sizeof(unsigned short))*20), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[1]);
        ret |= SSMReadYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*2), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[2]);
    } else {
        ret = SSMReadYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*3), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[0]);
        ret |= SSMReadYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*4), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[1]);
        ret |= SSMReadYPbPrADCCalibrationValue(((sizeof(unsigned short))*20*5), (sizeof(unsigned short))*20, (unsigned char *)&adc_cal_parm->comp_cal_val[2]);
    }

    if(ret == 0) {
        ALOGW("*****************read comp_cal_val[%d]***************\n",input);
        ALOGW("%s for input[%d].", __FUNCTION__, input);
        for(i = 0; i < 3; i++) {
            ALOGW("a_analog_clamp = 0x%x.", adc_cal_parm->comp_cal_val[i].a_analog_clamp);
            ALOGW("a_analog_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].a_analog_gain);
            ALOGW("a_digital_offset1 = 0x%x.", adc_cal_parm->comp_cal_val[i].a_digital_offset1);
            ALOGW("a_digital_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].a_digital_gain);
            ALOGW("a_digital_offset2 = 0x%x.", adc_cal_parm->comp_cal_val[i].a_digital_offset2);
            ALOGW("b_analog_clamp = 0x%x.", adc_cal_parm->comp_cal_val[i].b_analog_clamp);
            ALOGW("b_analog_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].b_analog_gain);
            ALOGW("b_digital_offset1 = 0x%x.", adc_cal_parm->comp_cal_val[i].b_digital_offset1);
            ALOGW("b_digital_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].b_digital_gain);
            ALOGW("b_digital_offset2 = 0x%x.", adc_cal_parm->comp_cal_val[i].b_digital_offset2);
            ALOGW("c_analog_clamp = 0x%x.", adc_cal_parm->comp_cal_val[i].c_analog_clamp);
            ALOGW("c_analog_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].c_analog_gain);
            ALOGW("c_digital_offset1 = 0x%x.", adc_cal_parm->comp_cal_val[i].c_digital_offset1);
            ALOGW("c_digital_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].c_digital_gain);
            ALOGW("c_digital_offset2 = 0x%x.", adc_cal_parm->comp_cal_val[i].c_digital_offset2);
            ALOGW("d_analog_clamp = 0x%x.", adc_cal_parm->comp_cal_val[i].d_analog_clamp);
            ALOGW("d_analog_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].d_analog_gain);
            ALOGW("d_digital_offset1 = 0x%x.", adc_cal_parm->comp_cal_val[i].d_digital_offset1);
            ALOGW("d_digital_gain = 0x%x.", adc_cal_parm->comp_cal_val[i].d_digital_gain);
            ALOGW("d_digital_offset2 = 0x%x.", adc_cal_parm->comp_cal_val[i].d_digital_offset2);
            ALOGW("**************************************************************\n");
        }
    } else {
        if (input == ADC_CALIBRATION_INPUT_YPBPR1 || input == ADC_CALIBRATION_INPUT_YPBPR2) {
            if(Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
                for(i = 0; i < 3; i++) {
                    adc_cal_parm->comp_cal_val[i].a_analog_gain = 0x8a;
                    adc_cal_parm->comp_cal_val[i].a_analog_clamp = 0x46;
                    adc_cal_parm->comp_cal_val[i].a_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].a_digital_gain = 0x3cf;
                    adc_cal_parm->comp_cal_val[i].a_digital_offset2 = 0x1c;

                    adc_cal_parm->comp_cal_val[i].b_analog_gain = 0xae;
                    adc_cal_parm->comp_cal_val[i].b_analog_clamp = 0x43;
                    adc_cal_parm->comp_cal_val[i].b_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].b_digital_gain = 0x383;
                    adc_cal_parm->comp_cal_val[i].b_digital_offset2 = 0x40;

                    adc_cal_parm->comp_cal_val[i].c_analog_gain = 0xa4;
                    adc_cal_parm->comp_cal_val[i].c_analog_clamp = 0x47;
                    adc_cal_parm->comp_cal_val[i].c_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].c_digital_gain = 0x383;
                    adc_cal_parm->comp_cal_val[i].c_digital_offset2 = 0x40;

                    adc_cal_parm->comp_cal_val[i].d_analog_gain = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_analog_clamp = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_gain = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_offset2 = 0x00;
                }
            } else {
                for(i = 0; i < 3; i++) {
                    adc_cal_parm->comp_cal_val[i].a_analog_gain = 0x77;
                    adc_cal_parm->comp_cal_val[i].a_analog_clamp = 0x47;
                    adc_cal_parm->comp_cal_val[i].a_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].a_digital_gain = 0x3cf;
                    adc_cal_parm->comp_cal_val[i].a_digital_offset2 = 0x1c;

                    adc_cal_parm->comp_cal_val[i].b_analog_gain = 0xb0;
                    adc_cal_parm->comp_cal_val[i].b_analog_clamp = 0x42;
                    adc_cal_parm->comp_cal_val[i].b_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].b_digital_gain = 0x383;
                    adc_cal_parm->comp_cal_val[i].b_digital_offset2 = 0x40;

                    adc_cal_parm->comp_cal_val[i].c_analog_gain = 0x94;
                    adc_cal_parm->comp_cal_val[i].c_analog_clamp = 0x3c;
                    adc_cal_parm->comp_cal_val[i].c_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].c_digital_gain = 0x383;
                    adc_cal_parm->comp_cal_val[i].c_digital_offset2 = 0x40;

                    adc_cal_parm->comp_cal_val[i].d_analog_gain = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_analog_clamp = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_offset1 = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_gain = 0x00;
                    adc_cal_parm->comp_cal_val[i].d_digital_offset2 = 0x00;
                }
            }
        }
    }
    return 0;
}
int TvinADCAutoCalibration_SelectSource(int window_sel, tvin_port_t source_port)
{
    tvin_port_t prePort = gTvinApiPara.tvin_para.port;
    tvin_port_t curPort = source_port;
    int ret = -1;

    ALOGD("%s, StopSigDetectThr.", __FUNCTION__);
    Tvin_StopSigDetect();

    if (prePort == TVIN_PORT_CVBS0) {
        ATVKillAutoAFCThread();
        ATVMuteAudioStream();
    } else if (prePort == TVIN_PORT_MPEG0 && curPort != TVIN_PORT_CVBS0) {
        ATVMuteAudioStream();
    }
    ALOGD("%s, prePort = 0x%x", __FUNCTION__ ,prePort);
    if (prePort != TVIN_PORT_MPEG0) {

        ALOGD("%s, stop decoder!", __FUNCTION__);
        ret = Tvin_StopDecoder(window_sel);
        if (0 == ret) {
            if (TvinApi_ClosePort(window_sel) < 0) {
                ALOGW("%s, close port failed.", __FUNCTION__);
                //return -1;
            }
        } else if (1 == ret) {
            if (TvinApi_ClosePort(window_sel) < 0) {
                ALOGW("%s, close port failed.", __FUNCTION__);
                //return -1;
            }
        } else {
            ALOGW("%s, stop decoder failed.", __FUNCTION__);
            return -1;
        }
    }

    if (curPort >= TVIN_PORT_COMP0 && curPort <= TVIN_PORT_SVIDEO7) {
        // Open Port
        if (TvinApi_OpenPort(window_sel, source_port) < 0) {
            ALOGD("%s, OpenPort failed, window_sel :=%d source_port =%x ", __FUNCTION__, window_sel, source_port);
            return -1;
        }
        Tvin_SetSrcPort(curPort);
    } else if (curPort >= TVIN_PORT_VGA0 && curPort <= TVIN_PORT_VGA7) {
        // Open Port
        if (TvinApi_OpenPort(window_sel, source_port) < 0) {
            ALOGD("%s, OpenPort failed, window_sel :=%d  source_port=%x ", __FUNCTION__, window_sel, source_port);
            return -1;
        }
        Tvin_SetSrcPort(curPort);
    }

    usleep(500*1000); //delay after adc reset for opening port

    return 0;
}
int TvinSetADCAutoCalibration_Result()
{
    if (gTvinApiPara.is_adc_autocalibration_start == false && gTvinApiPara.is_turnon_adc_autocalibration_thread == false
        && gTvinApiPara.adc_autocalibration_status == TV_ADC_AUTO_CALIBRATION_END)
        return 1;
    else if (gTvinApiPara.is_adc_autocalibration_start == false && gTvinApiPara.is_turnon_adc_autocalibration_thread == false
             && gTvinApiPara.adc_autocalibration_status == TV_ADC_AUTO_CALIBRATION_FAILED)
        return -1;
    else
        return 0;

}

void *TvinADCAutoCalibration_TreadRun(void* data)
{
    int cal_cnt = 2;
    prctl(PR_SET_NAME,(unsigned long)"TvinADCAutoCalibration_TreadRun");
    while (gTvinApiPara.is_turnon_adc_autocalibration_thread == true) {
        if (gTvinApiPara.is_adc_autocalibration_start == true) {
            {
                usleep(100 * 1000);
            }
            if (Tvin_IsSrcSwitchExecDone() == true) {
                if (Tvin_IsSigDetectExecDone() == true) {
                    switch (gTvinApiPara.adc_autocalibration_status) {
                    case TV_ADC_AUTO_CALIBRATION_INIT:
                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_YPBPR1;
                        ALOGD("%s, TV_ADC_AUTO_CALIBRATION_INIT. CNT=%d", __FUNCTION__, cal_cnt);
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR1:
                        if (TvinADCAutoCalibration_SelectSource(Tvin_GetInputWindow(), TVIN_PORT_COMP0) < 0) {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_YPBPR1_START;
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR1.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR1_START:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if (ADCAutoCalibration_Para.info.status == TVIN_SIG_STATUS_NOSIG) {
                            TvinApi_ADCAutoCalibration();
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_YPBPR1_WAIT;
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR1_START.", __FUNCTION__);
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                            ALOGW("%s, fail cos there is signal input.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR1_WAIT:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if ((ADCAutoCalibration_Para.flag & TVIN_PARM_FLAG_CAL) == 0) {
                            if (TvinApi_GetYPbPrADCGainOffset(&ADCGainOffset_Ypbpr1) < 0) {
                                gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                ALOGW("%s, TvinApi_GetADCGainOffset in ypbpr failed.", __FUNCTION__);
                            } else {
                                if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_YPBPR1) == true ) {
                                    ALOGW("%s, ypbpr-1 adc cal failed -> invalid value", __FUNCTION__);
                                    gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                    TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
                                    TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR1, ADCGainOffset_Ypbpr1);

                                } else {
                                    if (TvinApi_SetYPbPrADCGainOffset(ADCGainOffset_Ypbpr1) < 0) {
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                        ALOGW("%s, TvinApi_SetADCGainOffset in ypbpr failed.", __FUNCTION__);
                                    } else {
                                        TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR1, ADCGainOffset_Ypbpr1);
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_VGA;
                                        ALOGW("%s, ypbpr1 calibration successful.", __FUNCTION__);
                                    }
                                }
                            }
                        } else {
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR1_WAIT...", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR2:
                        if (TvinADCAutoCalibration_SelectSource(Tvin_GetInputWindow(), TVIN_PORT_COMP1) < 0) {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_YPBPR2_START;
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR2.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR2_START:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if (ADCAutoCalibration_Para.info.status == TVIN_SIG_STATUS_NOSIG) {
                            TvinApi_ADCAutoCalibration();
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_YPBPR2_WAIT;
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR2_START.", __FUNCTION__);
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                            ALOGW("%s, fail cos there is signal input.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_YPBPR2_WAIT:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if ((ADCAutoCalibration_Para.flag & TVIN_PARM_FLAG_CAL) == 0) {
                            if (TvinApi_GetYPbPrADCGainOffset(&ADCGainOffset_Ypbpr2) < 0) {
                                gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                ALOGW("%s, TvinApi_GetYPbPrADCGainOffset in ypbpr2 failed.", __FUNCTION__);
                            } else {
                                if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_YPBPR2) == true ) {
                                    ALOGW("%s, ypbpr-2 adc cal failed -> invalid value", __FUNCTION__);
                                    gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                    TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
                                    TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR2, ADCGainOffset_Ypbpr2);
                                } else {
                                    if (TvinApi_SetYPbPrADCGainOffset(ADCGainOffset_Ypbpr2) < 0) {
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                        ALOGW("%s, TvinApi_SetYPbPrADCGainOffset in ypbpr2 failed.", __FUNCTION__);
                                    } else {
                                        TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR2, ADCGainOffset_Ypbpr2);
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_VGA;
                                        ALOGW("%s, ypbpr2 calibration successful.", __FUNCTION__);
                                    }
                                }
                            }
                        } else {
                            ALOGW("%s, TV_ADC_AUTO_CALIBRATION_YPBPR2_WAIT...", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_VGA:
                        if (TvinADCAutoCalibration_SelectSource(Tvin_GetInputWindow(), TVIN_PORT_VGA0) < 0) {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_VGA_START;
                            ALOGD("%s, TV_ADC_AUTO_CALIBRATION_VGA.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_VGA_START:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if (ADCAutoCalibration_Para.info.status == TVIN_SIG_STATUS_NOSIG) {
                            TvinApi_ADCAutoCalibration();
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_VGA_WAIT;
                            ALOGD("%s, TV_ADC_AUTO_CALIBRATION_VGA_START.", __FUNCTION__);
                        } else {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                            ALOGW("%s, fail cos there is signal input.", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_VGA_WAIT:
                        TvinApi_ADCGetPara(Tvin_GetInputWindow(), &ADCAutoCalibration_Para);
                        if ((ADCAutoCalibration_Para.flag & TVIN_PARM_FLAG_CAL) == 0) {
                            if (TvinApi_GetADCGainOffset(&ADCGainOffset_VGA) < 0) {
                                gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                            } else {
                                if(IsInvalidADCValue(ADC_CALIBRATION_INPUT_VGA) == true ) {
                                    ALOGW("%s, vga adc cal failed -> invalid value", __FUNCTION__);
                                    gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                    TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
                                    TvinADCAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_VGA, ADCGainOffset_VGA);
                                } else {
                                    if (TvinApi_SetADCGainOffset(ADCGainOffset_VGA) < 0) {
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_FAILED;
                                        ALOGW("%s, TvinApi_SetADCGainOffset in vga failed.", __FUNCTION__);
                                    } else {
                                        TvinADCAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_VGA, ADCGainOffset_VGA);
                                        gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_END;
                                        ALOGD("%s, vga calibration successful.", __FUNCTION__);
                                    }
                                }
                            }
                        } else {
                            ALOGD("%s, TV_ADC_AUTO_CALIBRATION_VGA_WAIT...", __FUNCTION__);
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_END:
                        if(--cal_cnt) {
                            gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_INIT;
                        } else {
                            TvinApi_ClosePort(0);
                            gTvinApiPara.is_adc_autocalibration_start = false;
                            gTvinApiPara.is_turnon_adc_autocalibration_thread = false;
                            //sendMessage
                            //android::TvService::getIntance()->SendADCAutoCalibrationState(1);
                            ALOGD("%s, TV_ADC_AUTO_CALIBRATION_END.", __FUNCTION__);
                            if (gTvinConfig.adccal_autoswitchsource != 0x55) {
                                Tvin_SetSrcPort(TVIN_PORT_MPEG0);
                            } else {
                                Tv_SetSourceSwitchInput(0, (tvin_source_input_t)Tvin_GetLastSourceInput(), Tv_GetInputSourceAudioChannelIndex(Tvin_GetSourcePortBySourceInput((tvin_source_input_t)Tvin_GetLastSourceInput())));
                            }
                        }
                        break;
                    case TV_ADC_AUTO_CALIBRATION_FAILED:
                    default:
                        TvinApi_ClosePort(0);
                        gTvinApiPara.is_adc_autocalibration_start = false;
                        gTvinApiPara.is_turnon_adc_autocalibration_thread = false;
                        //sendMessage
                        //android::TvService::getIntance()->SendADCAutoCalibrationState(-3);
                        ALOGD("%s, TV_ADC_AUTO_CALIBRATION_FAILED.", __FUNCTION__);
                        if (gTvinConfig.adccal_autoswitchsource != 0x55) {
                            Tvin_SetSrcPort(TVIN_PORT_MPEG0);
                        } else {
                            Tv_SetSourceSwitchInput(0, (tvin_source_input_t)Tvin_GetLastSourceInput(), Tv_GetInputSourceAudioChannelIndex(Tvin_GetSourcePortBySourceInput((tvin_source_input_t)Tvin_GetLastSourceInput())));
                        }
                        break;
                    }
                } else {
                    //sendMessage
                    //android::TvService::getIntance()->SendADCAutoCalibrationState(-2);
                    ALOGW("%s, IsSigDetectExecDone == false, ADCAutoCalibration failed.", __FUNCTION__);
                }
            } else {
                //sendMessage
                //android::TvService::getIntance()->SendADCAutoCalibrationState(-1);
                ALOGW("%s, isSourceSwitchExecuteDone == false, ADCAutoCalibration failed.", __FUNCTION__);
            }
        }
    }
    return ((void*) 0);
}

int TvinADCAutoCalibration_CreateThread(void)
{
    int ret = -1;
    pthread_attr_t attr;
    struct sched_param param;
    if (gTvinApiPara.is_turnon_adc_autocalibration_thread != false
        || gTvinApiPara.is_turnon_adc_autocalibration_thread != false)
        return ret;
    if (TvinADCAutoCalibration_ThreadId)
        TvinADCAutoCalibration_ThreadId = 0;
    gTvinApiPara.is_adc_autocalibration_start = true;
    gTvinApiPara.is_turnon_adc_autocalibration_thread = true;
    gTvinApiPara.adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_INIT;
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    ret |= pthread_create(&TvinADCAutoCalibration_ThreadId, &attr, &TvinADCAutoCalibration_TreadRun, NULL);
    pthread_attr_destroy(&attr);
    return ret;
}
#if 0
void TvinADCAutoCalibration_KillThread(void)
{
    int i = 0;
    gTvinApiPara.is_turnon_source_switch_thread = false;
    gTvinApiPara.is_source_switch_thread_start = false;
    for (i = 0; i < 4; i++) {
        if (Tvin_IsSrcSwitchExecDone() == false) {
            ALOGW("%s, Source detect thread is busy, please wait 50ms...", __FUNCTION__);
            usleep(50 * 1000);
        } else {
            ALOGW("%s, Source detect thread is idle now, go ahead...", __FUNCTION__);
            break;
        }
    }
    if (i == 4) {
        ALOGW("%s, 200ms delay Timeout, have to go.", __FUNCTION__);
    }
    pthread_join(TvinADCAutoCalibration_ThreadId, NULL);
    TvinADCAutoCalibration_ThreadId = 0;
    ALOGD("%s, done.", __FUNCTION__);
}
#endif

pthread_t TvinSourceSwitch_ThreadId = 0;
tvin_src_switch_para_t gTvinSrcSwitchPara = {
    //.port =
    TVIN_PORT_MPEG0,
    //.input_window =
    0,
    //.audio_channel =
    TV_AUDIO_LINE_IN_1,
};

static void* tryAlsaInitThread(void* arg)
{
    AudioAlsaInit(48000, 48000);
    return NULL;
}

static void tryAlsaInit()
{
    int ret;
    pthread_t id;
    pthread_attr_t attr;
    struct sched_param param;

    ALOGD("[%s:%d] Indirect call to AudioAlsaInit.", __FUNCTION__, __LINE__);
    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(&id, &attr, &tryAlsaInitThread, NULL);
    pthread_attr_destroy(&attr);
}

int TvinSourceSwitch_RealSwitchSource(int window_sel, tvin_port_t source_port, tvin_audio_channel_e audio_channel)
{
    tvin_port_t prePort = gTvinApiPara.tvin_para.port;
    tvin_port_t curPort = source_port;
    int ret = 0;
    int tmp_source_type = SOURCE_MPEG;

    // During booting process, the media service may not be available when this
    // function is called, which will cause a long delay when calling AudioAlsaInit.
    // So, to support fast boot, when this function is called for the first time
    // after booting, we move the call to AudioAlsaInit to a new thread.
    // This is a flag to indicate if it's the first time after booting.
    static bool bFirstAfterBoot = true;

    // Prevent this function to be called by two threads concurrently.
    //Mutex::Autolock _l(source_switch_lock);

    if (prePort == curPort && Tvin_GetTvinStatus() == Tvin_GetTvinPrePlayStatus()
        && (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START || Tvin_GetTvinStatus() == TVIN_STATUS_NORMAL_START)) {
        //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 2);
        ALOGD("%s, ignor same source input switch in preview mode or normal mode, pre_status[%d], status[%d],signal_status[%d].",
              __FUNCTION__, (int)Tvin_GetTvinPrePlayStatus(), (int)Tvin_GetTvinStatus(),(int)Tvin_GetSigStatus());
        return -1;
    }

    gTvinApiPara.tvin_pre_status = gTvinApiPara.tvin_status;

    if (Tvin_StopSigDetect() == 0) {
        ALOGD("%s, StopSigDetectThr success.", __FUNCTION__);
    } else {
        ALOGD("%s, StopSigDetectThr failed.", __FUNCTION__);
    }

    gTvinApiPara.is_source_switch_exec_done = false;

    AudioSetSysMuteStatus(CC_MUTE_ON);

    if (prePort == TVIN_PORT_CVBS0) {
        ATVKillAutoAFCThread();
        ATVMuteAudioStream();
        ATVPowerOff();
    } else if (prePort == TVIN_PORT_MPEG0 && curPort != TVIN_PORT_CVBS0) {
        ALOGD("%s, First power on, when source is not TV, mute TunerAudio", __FUNCTION__);
        ATVMuteAudioStream();
        ATVPowerOff();
    }

    SetAudioVolumeCompensationVal(0);

    ALOGD("%s, source switch start...", __FUNCTION__);
    ALOGD("%s, source_port = %x, audio_channel = %d.", __FUNCTION__, source_port, audio_channel);

    TvinApi_SetBlackOutPolicy(1);
    TvinApi_KeepLastFrame(0);
    if (Tvin_GetTvinStatus() == TVIN_STATUS_NORMAL_START) {
        TvinApi_SetStartDropFrameCn(0);
    }

    if (mTvStopFlag == 1) {
        gTvinApiPara.is_source_switch_exec_done = true;
        ALOGW("%s, tv need to stop, just break source switch 1.", __FUNCTION__);
        return -1;
    }

    // not first call SourceSwitch() function
    if (prePort != TVIN_PORT_MPEG0) {
        ALOGD("%s, Stop decoder!", __FUNCTION__);
        // Uninit Alsa before closing the port.
        AudioAlsaUnInit();

        ret = Tvin_StopDecoder(window_sel);
        if (0 == ret || 1 == ret) {
            TvinApi_ClosePort(window_sel);
        } else {
            ALOGW("%s, stop decoder failed.", __FUNCTION__);
            gTvinApiPara.is_source_switch_exec_done = true;
            return -1;
        }
    }

    // current usage:only one window
    gTvinApiPara.input_window = window_sel;
    gTvinApiPara.tvin_para.port = curPort;

    Tvin_SetAudioChannel(audio_channel);

    Tvin_SetDepthOf2Dto3D(0); // set default depth
    TvinApi_Set2Dto3D(0);
    Tvin_SetPanelParamsFor3D(MODE3D_DISABLE);
    Tvin_BypassModulesFor3D(MODE3D_DISABLE);
    Tvin_Set3DFunction(MODE3D_DISABLE);

    if (curPort == TVIN_PORT_HDMI3) {
        TvinApi_MHL_WorkEnable(1);
        ALOGD("%s, turn ON mhl i2c rw for HDMI3.", __FUNCTION__);
    } else {
        TvinApi_MHL_WorkEnable(0);
        ALOGD("%s, turn OFF mhl i2c rw for non-HDMI3.", __FUNCTION__);
    }


    if (curPort == TVIN_PORT_CVBS0) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_TV);
        ALOGD("%s, SetCurSource TV.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_CVBS1) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_AV1);
        ALOGD("%s, SetCurSource AV1.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_CVBS2) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_AV2);
        ALOGD("%s, SetCurSource AV2.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_COMP0) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_YPBPR1);
        ALOGD("%s, SetCurSource YPBPR1.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_COMP1) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_YPBPR2);
        ALOGD("%s, SetCurSource YPBPR2.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_HDMI0) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI1);
        ALOGD("%s, SetCurSource HDMI1.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_HDMI2) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI2);
        ALOGD("%s, SetCurSource HDMI2.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_HDMI3) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI3);
        ALOGD("%s, SetCurSource HDMI3.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_VGA0) {
        Tvin_SetSourceInput(curPort);
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_VGA);
        TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_START);
        if (gTvinConfig.vag_force_adjust == 1) {
            TvinSetVgaAutoAdjustForceStart(1);
        }
        ALOGD("%s, SetCurSource VGA.", __FUNCTION__);
    } else if (curPort == TVIN_PORT_MPEG0) {
        ALOGD("%s, SetCurSource MPEG.", __FUNCTION__);
    }

    tmp_source_type = (unsigned char) Tvin_GetSourceInput();
    if (tmp_source_type < SOURCE_TV || tmp_source_type >= SOURCE_MAX) {
        tmp_source_type = SOURCE_AV1;
    }

    SSMSaveSourceInput(tmp_source_type);
    SSMSaveLastSelectSourceType(tmp_source_type);

    //sendMessage
    //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 1);

    SSMSave3DLRSwitch(0);
    SSMSave3DTO2D(0);
    Tvin_Set3DStatus(STATUS3D_DISABLE);

    if (mTvStopFlag == 1) {
        gTvinApiPara.is_source_switch_exec_done = true;
        ALOGW("%s, tv need to stop, just break source switch 2.", __FUNCTION__);
        return -1;
    }

    if (curPort >= TVIN_PORT_HDMI0 && curPort <= TVIN_PORT_HDMI7) {
        // hdmi_input

        SetAudioLineInCaptureVolume(curPort);
        AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_HDMI, 1, 1,-1);
        SetAudioMasterVolume(GetAudioMasterVolume());
        SetAudioSourceAVOutBypass(SOURCE_TYPE_HDMI);

        // set audio in source type
        AudioSetAudioInSource(CC_AUDIO_SOURCE_IN_HDMI);

        if (mTvStopFlag == 1) {
            gTvinApiPara.is_source_switch_exec_done = true;
            ALOGW("%s, tv need to stop, just break source switch hdmi.", __FUNCTION__);
            return -1;
        }

        // Open Port
        if (TvinApi_OpenPort(window_sel, source_port) < 0) {
            ALOGE("%s, OpenPort failed, window_sel := %d  source_port= %x ", __FUNCTION__, window_sel, source_port);
        }

        // Alsa Init
        bFirstAfterBoot = false;
        gTvinApiPara.is_hdmi_sr_detect_start = true;
        gTvinApiPara.hdmi_sampling_rate = 0;
    } else if (curPort >= TVIN_PORT_COMP0 && curPort <= TVIN_PORT_SVIDEO7) {
        // line-in_type:YPbPr,AV,S-Video

        if (Tvin_GetSrcPort() == TVIN_PORT_CVBS0) {
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_TV, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            SetAudioSourceAVOutBypass(SOURCE_TYPE_TV);
        } else if (Tvin_GetSrcPort() >= TVIN_PORT_CVBS1 && Tvin_GetSrcPort() <= TVIN_PORT_SVIDEO7) {
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_AV, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            SetAudioSourceAVOutBypass(SOURCE_TYPE_AV);
        } else if (Tvin_GetSrcPort() >= TVIN_PORT_COMP0 && Tvin_GetSrcPort() <= TVIN_PORT_COMP7) {
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_COMP, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            SetAudioSourceAVOutBypass(SOURCE_TYPE_COMPONENT);
        }

        // Select Channel
        AudioLineInSelectChannel(audio_channel);

        // set audio in source type
        AudioSetAudioInSource (CC_AUDIO_SOURCE_IN_AV);

        if (curPort == TVIN_PORT_CVBS0) {
            ATVPowerOn();
            ATVSourceSwitchSetCurrentChannelInfo();
        }

        // Open Port
        if (TvinApi_OpenPort(window_sel, source_port) < 0) {
            ALOGD("%s, OpenPort failed, window_sel :=%d source_port =%x ", __FUNCTION__, window_sel, source_port);
        }

        if (curPort == TVIN_PORT_CVBS0) {
            ATVSourceSwitchSetCurrentChannelCVBSStd();
        }

        // Alsa Init
        if (bFirstAfterBoot) {
            tryAlsaInit();
            bFirstAfterBoot = false;
        } else {
            AudioAlsaInit();
        }

        if (mTvStopFlag == 1) {
            gTvinApiPara.is_source_switch_exec_done = true;
            ALOGW("%s, tv need to stop, just break source switch ypbpr or av.", __FUNCTION__);
            return -1;
        }

    } else if (curPort >= TVIN_PORT_VGA0 && curPort <= TVIN_PORT_VGA7) {
        //VGA input

        SetAudioLineInCaptureVolume(curPort);
        AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_VGA, 1, 1,-1);
        SetAudioMasterVolume(GetAudioMasterVolume());
        SetAudioSourceAVOutBypass(SOURCE_TYPE_VGA);

        // Select Channel
        AudioLineInSelectChannel(audio_channel);

        // set audio in source type
        AudioSetAudioInSource(CC_AUDIO_SOURCE_IN_AV);

        // Open Port
        if (TvinApi_OpenPort(window_sel, source_port) < 0) {
            ALOGW("%s, OpenPort failed, window_sel :=%d  source_port=%x ", __FUNCTION__, window_sel, source_port);
        }

        // Alsa Init
        if (bFirstAfterBoot) {
            tryAlsaInit();
            bFirstAfterBoot = false;
        } else {
            AudioAlsaInit();
        }

        if (mTvStopFlag == 1) {
            gTvinApiPara.is_source_switch_exec_done = true;
            ALOGW("%s, tv need to stop, just break source switch vga.", __FUNCTION__);
            return -1;
        }
    }

    if (curPort == TVIN_PORT_CVBS0 && gTvinConfig.atv_keeplastframe != 0) {
        TvinApi_SetBlackOutPolicy(0);
        ATVCreateAutoAFCThread();
    }

    if (Tvin_GetSourceInput() == SOURCE_TV) {
        TvinApi_SetVdinFlag(MEMP_ATV_WITH_3D);
        if (Tvin_GetTvinStatus() == TVIN_STATUS_NORMAL_START)
            TvinApi_SetStartDropFrameCn(4);
    } else {
        TvinApi_SetVdinFlag(MEMP_VDIN_WITH_3D);
        if (Tvin_GetTvinStatus() == TVIN_STATUS_NORMAL_START)
            TvinApi_SetStartDropFrameCn(0);
    }

	if( Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI ||
		Tvin_GetSrcInputType() == SOURCE_TYPE_COMPONENT ||
		Tvin_GetSrcInputType() == SOURCE_TYPE_VGA){
		SetFileAttrValue("/sys/module/di/parameters/bypass_prog", "1"); 
		ALOGE("%s, bypass prog for HDMI.", __FUNCTION__);
	} else {
		SetFileAttrValue("/sys/module/di/parameters/bypass_prog", "0"); 
	}
	
    Vpp_ResetLastVppSettingsSourceType();
    Tvin_SuspendSourceDetect(false);//after source witch resume source detect

    return 0;
}

int TvinAudioSwitch(tvin_source_input_t source_input)
{
    tvin_audio_channel_e audio_channel = Tv_GetInputSourceAudioChannelIndex(Tvin_GetSourcePortBySourceInput(source_input));
    tvin_port_t curPort;

    switch (source_input) {
    case SOURCE_TV:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_TV);
        break;
    case SOURCE_AV1:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_AV1);
        break;
    case SOURCE_AV2:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_AV2);
        break;
    case SOURCE_YPBPR1:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_YPBPR1);
        break;
    case SOURCE_YPBPR2:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_YPBPR2);
        break;
    case SOURCE_HDMI1:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI1);
        break;
    case SOURCE_HDMI2:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI2);
        break;
    case SOURCE_HDMI3:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_HDMI3);
        break;
    case SOURCE_VGA:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_VGA);
        break;
    case SOURCE_MPEG:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_MPEG);
        break;
    case SOURCE_DTV:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_DTV);
    default:
        Tvin_SetSourceAudioInput(SOURCE_AUDIO_MPEG);
        break;
    }
    if (source_input == SOURCE_HDMI1 || source_input == SOURCE_HDMI2 || source_input == SOURCE_HDMI3) {
        // hdmi_input
        curPort = Tvin_GetSourcePortBySourceInput(source_input);
        SetAudioLineInCaptureVolume(curPort);
        AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_HDMI, 1, 1,-1);
        SetAudioMasterVolume(GetAudioMasterVolume());

        //Stop Dsp
        AudioDspStop();

        // Uninit Alsa
        AudioAlsaUnInit();

        // Stop HDMI in
        AudioStopHDMIIn();

        // set audio in source type
        AudioSetAudioInSource(CC_AUDIO_SOURCE_IN_HDMI);

        // DSP Start
        AudioDspStart();

        // Alsa Init
        AudioAlsaInit();

        // Start HDMI In
        AudioStartHDMIIn();
    } else {
        switch (source_input) {
        case SOURCE_TV:
            curPort = Tvin_GetSourcePortBySourceInput(source_input);
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_TV, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            break;
        case SOURCE_AV1:
        case SOURCE_AV2:
            curPort = Tvin_GetSourcePortBySourceInput(source_input);
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_AV, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            break;
        case SOURCE_YPBPR1:
        case SOURCE_YPBPR2:
            curPort = Tvin_GetSourcePortBySourceInput(source_input);
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_COMP, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            break;
        case SOURCE_VGA:
            curPort = Tvin_GetSourcePortBySourceInput(source_input);
            SetAudioLineInCaptureVolume(curPort);
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_VGA, 1, 1,-1);
            SetAudioMasterVolume(GetAudioMasterVolume());
            break;
        default:
            return -1;
        }

        //Stop Dsp
        AudioDspStop();

        // Uninit Alsa
        AudioAlsaUnInit();

        // Stop Line in
        AudioStopLineIn();

        // Select Channel
        AudioLineInSelectChannel(audio_channel);

        // set audio in source type
        AudioSetAudioInSource(CC_AUDIO_SOURCE_IN_AV);

        // Alsa Init
        AudioAlsaInit();

        // Start Line In
        AudioStartLineIn();

        // DSP Start
        AudioDspStart();
    }
    ALOGD("%s, change audio to source input = %d, audio channel = %d.", __FUNCTION__, source_input, audio_channel);
    return 0;
}

static int SetAudioLineInCaptureVolume(tvin_port_t curPort)
{
    int l_vol = -1, r_vol = -1;
    int set_audio_mute_type = 0;

    set_audio_mute_type = GetAudioSetAudioMuteTypeCFG();
    if (set_audio_mute_type == 1) {
        return 0;//no need set line in capture volume
    }

    if (curPort >= TVIN_PORT_HDMI0 && curPort <= TVIN_PORT_HDMI7) {
        if (GetAudioSrcInputHDMILineInVol(&l_vol, &r_vol) < 0) {
            l_vol = 64, r_vol = 64;
        }
    } else if (curPort >= TVIN_PORT_COMP0 && curPort <= TVIN_PORT_SVIDEO7) {
        if (curPort == TVIN_PORT_CVBS0) {
            if (GetAudioSrcInputTVLineInVol(&l_vol, &r_vol) < 0) {
                l_vol = 64, r_vol = 64;
            }
        } else if (curPort >= TVIN_PORT_CVBS1 && curPort <= TVIN_PORT_SVIDEO7) {
            if (GetAudioSrcInputAVLineInVol(&l_vol, &r_vol) < 0) {
                l_vol = 64, r_vol = 64;
            }
        } else if (curPort >= TVIN_PORT_COMP0 && curPort <= TVIN_PORT_COMP7) {
            if (GetAudioSrcInputCOMPLineInVol(&l_vol, &r_vol) < 0) {
                l_vol = 64, r_vol = 64;
            }
        }
    } else if (curPort >= TVIN_PORT_VGA0 && curPort <= TVIN_PORT_VGA7) {
        if (GetAudioSrcInputVGALineInVol(&l_vol, &r_vol) < 0) {
            l_vol = 64, r_vol = 64;
        }
    } else if (curPort == TVIN_PORT_MPEG0) {
        if (GetAudioSrcInputMPEGLineInVol(&l_vol, &r_vol) < 0) {
            l_vol = 64, r_vol = 64;
        }
    }

    if (l_vol < 0 || r_vol < 0) {
        return -1;
    }

    return AudioSetLineInCaptureVolume(l_vol, r_vol);
}

void *TvinSourceSwitch_TreadRun(void* data)
{
    prctl(PR_SET_NAME,(unsigned long)"TvinSourceSwitch_TreadRun");
    while (gTvinApiPara.is_turnon_source_switch_thread == true) {
        if (gTvinApiPara.is_source_switch_thread_start == true) {
            ALOGD("%s, switch start.", __FUNCTION__);
            if (Tvin_IsSrcSwitchExecDone() == true) {
                if (Tvin_IsSigDetectExecDone() == true) {
                    TvinSourceSwitch_RealSwitchSource(Tvin_GetInputWindow(), Tv_GetSourceSwitchInput(), Tv_GetSourceSwitchAudioChannel());
                } else {
                    ALOGW("%s, isDetectThrDone == false, source swith failed.", __FUNCTION__);
                }
            } else {
                ALOGW("%s, isSourceSwitchExecuteDone == false, source swith failed.", __FUNCTION__);
            }
            gTvinApiPara.is_source_switch_thread_start = false;
            ALOGD("%s, isSourceSwitchExecuteDone == false, source swith thread done.", __FUNCTION__);
        } else {
            usleep(10 * 1000);
        }
    }
    return ((void*) 0);
}

int TvinSourceSwitch_CreateThread(void)
{
    int ret = 0;
    pthread_attr_t attr;
    struct sched_param param;

    gTvinApiPara.is_turnon_source_switch_thread = true;
    gTvinApiPara.is_source_switch_thread_start = false;
    gTvinSrcSwitchPara.port = gTvinApiPara.tvin_para.port;
    gTvinSrcSwitchPara.input_window = 0;
    gTvinSrcSwitchPara.audio_channel = gTvinApiPara.audio_channel;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(&TvinSourceSwitch_ThreadId, &attr, &TvinSourceSwitch_TreadRun, NULL);
    pthread_attr_destroy(&attr);
    return ret;
}

void TvinSourceSwitch_KillThread(void)
{
    int i = 0, dly = 50;
    gTvinApiPara.is_turnon_source_switch_thread = false;
    gTvinApiPara.is_source_switch_thread_start = false;
    for (i = 0; i < 10; i++) {
        if (Tvin_IsSrcSwitchExecDone() == false) {
            ALOGW("%s, source detect thread is busy, please wait %d ms...", __FUNCTION__, (dly*i));
            usleep(dly * 1000);
        } else {
            ALOGD("%s, after %d ms, source switch thread is idle now, let's go ...", __FUNCTION__, (i*dly));
            break;
        }
    }
    if (i == 10) {
        ALOGW("%s, %d ms delay Timeout, have to go.", __FUNCTION__, (dly*i));
    }
    pthread_join(TvinSourceSwitch_ThreadId, NULL);
    TvinSourceSwitch_ThreadId = 0;
    ALOGD("%s, done.", __FUNCTION__);
}

int Tvin_CheckFs(void)
{
    FILE *f;
    char mount_dev[256];
    char mount_dir[256];
    char mount_type[256];
    char mount_opts[256];
    int mount_freq;
    int mount_passno;
    int match;
    int found_ro_fs = 0;
    int data_status = 0;
    int cache_status = 0;
    int atv_status = 0;
    int dtv_status = 0;
    int param_status = 0;
    int cool_reboot = 0;
    int recovery_reboot = 0;

    f = fopen("/proc/mounts", "r");
    if (! f) {
        /* If we can't read /proc/mounts, just give up */
        return 1;
    }

    do {
        match = fscanf(f, "%255s %255s %255s %255s %d %d\n",
                       mount_dev, mount_dir, mount_type,
                       mount_opts, &mount_freq, &mount_passno);
        mount_dev[255] = 0;
        mount_dir[255] = 0;
        mount_type[255] = 0;
        mount_opts[255] = 0;
        if ((match == 6) && (!strncmp(mount_dev, "/dev/block", 10))) {
            ALOGD("%s, %s %s %s %s %d %d!", __FUNCTION__, mount_dev, mount_dir, mount_type, mount_opts, mount_freq, mount_passno);
            if (!strncmp(mount_dir, "/param", 6)) {
                param_status |= 0x01;
            } else if (!strncmp(mount_dir, "/atv", 4)) {
                atv_status |= 0x01;
            } else if (!strncmp(mount_dir, "/dtv", 4)) {
                dtv_status |= 0x01;
            } else if (!strncmp(mount_dir, "/data", 5)) {
                data_status |= 0x01;
            } else if (!strncmp(mount_dir, "/cache", 6)) {
                cache_status |= 0x01;
            }
            if (strstr(mount_opts, "ro")) {
                found_ro_fs += 1;
                if (!strncmp(mount_dir, "/param", 6)) {
                    param_status |= 0x02;
                } else if (!strncmp(mount_dir, "/atv", 4)) {
                    atv_status |= 0x02;
                } else if (!strncmp(mount_dir, "/dtv", 4)) {
                    dtv_status |= 0x02;
                } else if (!strncmp(mount_dir, "/data", 5)) {
                    data_status |= 0x02;
                } else if (!strncmp(mount_dir, "/cache", 6)) {
                    cache_status |= 0x02;
                }
            }
        }
    } while (match != EOF);

    fclose(f);

    switch (param_status) {
    case 0x03:
        ALOGW("%s, param partition is read-only!", __FUNCTION__);
        break;
    case 0x00:
        ALOGW("%s, param partition can not be mounted!", __FUNCTION__);
        break;
    default:
        break;
    }
    switch (atv_status) {
    case 0x03:
        ALOGW("%s, atv partition is read-only!", __FUNCTION__);
        cool_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
        break;
    case 0x00:
        ALOGW("%s, atv partition can not be mounted!", __FUNCTION__);
        recovery_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
    default:
        break;
    }
    switch (dtv_status) {
    case 0x03:
        ALOGW("%s, dtv partition is read-only!", __FUNCTION__);
        //android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
        break;
    case 0x00:
        ALOGW("%s, dtv partition can not be mounted!", __FUNCTION__);
        //android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
    default:
        break;
    }
    switch (data_status) {
    case 0x03:
        ALOGW("%s, data partition is read-only!", __FUNCTION__);
        cool_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
        break;
    case 0x00:
        ALOGW("%s, data partition can not be mounted!", __FUNCTION__);
        recovery_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
        break;
    default:
        break;
    }
    switch (cache_status) {
    case 0x03:
        ALOGW("%s, cache partition is read-only!", __FUNCTION__);
        cool_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
        break;
    case 0x00:
        ALOGW("%s, cache partition can not be mounted!", __FUNCTION__);
        recovery_reboot = 1;
        //android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
        break;
    default:
        break;
    }
    if (cool_reboot == 1) {
        android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
    }
    if (recovery_reboot == 1) {
        android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
    }
    return found_ro_fs;
}

int Tvin_GetTvinConfig(void)
{
    char config_value[64];

    memset(config_value, 0x0, 64);
    config_get("tvin.dvi.audio.vga", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.dvi_audio_vga = 0x55;
    } else {
        gTvinConfig.dvi_audio_vga = 0;
    }
    ALOGD("%s, dvi_audio_vga[0x%x].", __FUNCTION__, gTvinConfig.dvi_audio_vga);

    memset(config_value, 0x0, 64);
    memset(config_3d_pin_enable, 0x0, 64);
    memset(config_3d_pin_disable, 0x0, 64);
    config_get("tvin.3Dpin.ctrl", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        config_get("tvin.3Dpin.ctrl.enable", config_3d_pin_enable, "null");
        if (strcmp(config_3d_pin_enable, "null") == 0) {
            gTvinConfig.pin_ctrl_3D = 0;
        } else {
            config_get("tvin.3Dpin.ctrl.disable", config_3d_pin_disable, "null");
            if (strcmp(config_3d_pin_disable, "null") == 0) {
                gTvinConfig.pin_ctrl_3D = 0;
            } else {
                gTvinConfig.pin_ctrl_3D = 0x55;
            }
        }
    } else {
        gTvinConfig.pin_ctrl_3D = 0;
    }
    ALOGD("%s, pin_ctrl_3D[0x%x], config_3d_pin_enable[%s], config_3d_pin_disable[%s].",
          __FUNCTION__, gTvinConfig.pin_ctrl_3D, config_3d_pin_enable, config_3d_pin_disable);


    memset(config_value, 0x0, 64);
    config_get("tvin.peripheral.3D.6M30", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.peripheral_3D_6M30 = 0x55;
    } else {
        gTvinConfig.peripheral_3D_6M30 = 0;
    }
    ALOGD("%s, peripheral_3D_6M30[0x%x].", __FUNCTION__, gTvinConfig.peripheral_3D_6M30);

    memset(config_value, 0x0, 64);
    config_get("tvin.autoset.displayfreq", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.autoset_displayfreq = 0x55;
    } else {
        gTvinConfig.autoset_displayfreq = 0;
    }
    ALOGD("%s, autoset_displayfreq[0x%x].", __FUNCTION__, gTvinConfig.autoset_displayfreq);

    memset(config_value, 0x0, 64);
    config_get("tvin.hdmi.eq", config_value, "null");
    if (strcmp(config_value, "v1") == 0) {
        gTvinConfig.hdmi_eq = 1;
    } else {
        gTvinConfig.hdmi_eq = 2;
    }
    ALOGD("%s, hdmi_eq v%d.", __FUNCTION__, gTvinConfig.hdmi_eq);

    memset(config_value, 0x0, 64);
    config_get("tvin.atv.keeplastframe", config_value, "null");
    if (strcmp(config_value, "disable") == 0) {
        gTvinConfig.atv_keeplastframe = 0;
    } else {
        gTvinConfig.atv_keeplastframe = 1;
    }
    ALOGD("%s, atv_keeplastframe[%d].", __FUNCTION__, gTvinConfig.atv_keeplastframe);

    memset(config_value, 0x0, 64);
    config_get("tvin.multi.db", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.multi_db = 1;
    } else {
        gTvinConfig.multi_db = 0;
    }
    ALOGD("%s, multi_db[%d].", __FUNCTION__, gTvinConfig.multi_db);

    memset(config_value, 0x0, 64);
    config_get("tvin.overscan.3d", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.overscan_3d = 1;
    } else {
        gTvinConfig.overscan_3d = 0;
    }
    ALOGD("%s, overscan_3d[%d].", __FUNCTION__, gTvinConfig.overscan_3d);

    memset(config_value, 0x0, 64);
    config_get("tvin.tv.out.counter", config_value, "null");
    gTvinConfig.tv_out_counter = (unsigned int)strtol(config_value, NULL, 10);
    if (gTvinConfig.tv_out_counter <= 0 || gTvinConfig.tv_out_counter > 50) {
        gTvinConfig.tv_out_counter = 20;
    }
    ALOGD("%s, tv_out_counter[%d].", __FUNCTION__, gTvinConfig.tv_out_counter);

    memset(config_value, 0x0, 64);
    config_get("tvin.vga.force.adjust", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.vag_force_adjust = 1;
    } else {
        gTvinConfig.vag_force_adjust = 0;
    }
    ALOGD("%s, vag_force_adjust[%d].", __FUNCTION__, gTvinConfig.vag_force_adjust);

    memset(config_value, 0x0, 64);
    config_get("tvin.hdmi.auto3d", config_value, "null");
    if (strcmp(config_value, "disable") == 0) {
        gTvinConfig.hdmi_auto3d = 0x55;
    } else {
        gTvinConfig.hdmi_auto3d = 0;
    }
    ALOGD("%s, hdmi_auto3d[%d].", __FUNCTION__, gTvinConfig.hdmi_auto3d);

    memset(config_value, 0x0, 64);
    config_get("tvin.preview.freescale", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.preview_freescale = 0x55;
    } else {
        gTvinConfig.preview_freescale = 0;
    }
    ALOGD("%s, preview_freescale[%d].", __FUNCTION__, gTvinConfig.preview_freescale);

    memset(config_value, 0x0, 64);
    config_get("tvin.source.pg.lock", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.source_pg_lock = 0x55;
    } else {
        gTvinConfig.source_pg_lock = 0;
    }
    ALOGD("%s, source_pg_lock[%d].", __FUNCTION__, gTvinConfig.source_pg_lock);

    memset(config_value, 0x0, 64);
    config_get("tvin.adccal.autoswitchsource", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.adccal_autoswitchsource = 0x55;
    } else {
        gTvinConfig.adccal_autoswitchsource = 0;
    }
    ALOGD("%s, adccal_autoswitchsource[%d].", __FUNCTION__, gTvinConfig.adccal_autoswitchsource);

    memset(config_value, 0x0, 64);
    config_get("tvin.checkfs", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.checkfs = 0x55;
    } else {
        gTvinConfig.checkfs = 0;
    }
    ALOGD("%s, checkfs[%d].", __FUNCTION__, gTvinConfig.checkfs);

    memset(config_value, 0x0, 64);
    config_get("tvin.userpet", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.userpet = 0x55;
    } else {
        gTvinConfig.userpet = 0;
    }
    ALOGD("%s, userpet[%d].", __FUNCTION__, gTvinConfig.userpet);

    memset(config_value, 0x0, 64);
    config_get("tvin.userpet.timeout", config_value, "null");
    gTvinConfig.userpet_timeout = (unsigned int)strtol(config_value, NULL, 10);
    if (gTvinConfig.userpet_timeout <= 0 || gTvinConfig.userpet_timeout > 100) {
        gTvinConfig.userpet_timeout = 10;
    }
    ALOGD("%s, userpet_timeout[%d].", __FUNCTION__, gTvinConfig.userpet_timeout);

    memset(config_value, 0x0, 64);
    config_get("tvin.userpet.reset", config_value, "null");
    if (strcmp(config_value, "disable") == 0) {
        gTvinConfig.userpet_reset = 0;
    } else {
        gTvinConfig.userpet_reset = 1;
    }
    ALOGD("%s, userpet_reset[%d].", __FUNCTION__, gTvinConfig.userpet_reset);

    memset(config_value, 0x0, 64);
    config_get("tvin.trigger.autoreboot", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.autoreboot = 0x55;
    } else {
        gTvinConfig.autoreboot = 0;
    }
    ALOGD("%s, autoreboot[%d].", __FUNCTION__, gTvinConfig.autoreboot);

    memset(config_value, 0x0, 64);
    config_get("tvin.seria.debug", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.seria_debug = 0x55;
    } else {
        gTvinConfig.seria_debug = 0;
    }
    ALOGD("%s, seria_debug[%d].", __FUNCTION__, gTvinConfig.seria_debug);

    memset(config_value, 0x0, 64);
    config_get("tvin.socket.interface", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.socket_interface = 0x55;
    } else {
        gTvinConfig.socket_interface = 0;
    }
    ALOGD("%s, socket_interface[%d].", __FUNCTION__, gTvinConfig.socket_interface);

    memset(config_value, 0x0, 64);
    config_get("tvin.new_d2d3", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.new_d2d3 = 0x55;
    } else {
        gTvinConfig.new_d2d3 = 0;
    }
    ALOGD("%s, new_d2d3[%d].", __FUNCTION__, gTvinConfig.new_d2d3);

    memset(config_value, 0x0, 64);
    config_get("tvin.new_overscan", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.new_overscan = 0x55;
    } else {
        gTvinConfig.new_overscan = 0;
    }
    ALOGD("%s, new_overscan[%d].", __FUNCTION__, gTvinConfig.new_overscan);

    memset(config_value, 0x0, 64);
    config_get("tvin.new_2d3ddepth", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.new_2d3ddepth = 0x55;
    } else {
        gTvinConfig.new_2d3ddepth = 0;
    }
    ALOGD("%s, new_2d3ddepth[%d].", __FUNCTION__, gTvinConfig.new_2d3ddepth);

    memset(config_value, 0x0, 64);
    config_get("tvin.depth_reverse", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.depth_reverse = 0x55;
    } else {
        gTvinConfig.depth_reverse = 0;
    }
    ALOGD("%s, depth_reverse[%d].", __FUNCTION__, gTvinConfig.depth_reverse);


    memset(config_value, 0x0, 64);
    config_get("tvin.new_3dautodetc", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.new_3dautodetc  = 0x55;
    } else {
        gTvinConfig.new_3dautodetc  = 0;
    }
    ALOGD("%s, new_3dautodetc[%d].", __FUNCTION__, gTvinConfig.new_3dautodetc);

    memset(config_value, 0x0, 64);
    config_get("tvin.new_set3dfunction", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.new_set3dfunction = 0x55;
    } else {
        gTvinConfig.new_set3dfunction = 0;
    }
    ALOGD("%s, new_set3dfunction[%d].", __FUNCTION__, gTvinConfig.new_set3dfunction);

    memset(config_value, 0x0, 64);
    config_get("tvin.source_detectfunction", config_value, "null");
    if (strcmp(config_value, "enable") == 0) {
        gTvinConfig.source_detectfunction = 0x55;
    } else {
        gTvinConfig.source_detectfunction = 0;
    }
    ALOGD("%s, source_detectfunction[%d].", __FUNCTION__, gTvinConfig.source_detectfunction);

    memset(config_preview_video_axis, 0x0, 64);
    config_get("tvin.preview.video.axis", config_preview_video_axis, "null");
    if (strcmp(config_preview_video_axis, "null") == 0) {
        strcpy(config_preview_video_axis, "0 0 1919 1079");
    }
    ALOGD("%s, preview_video_axis[%s].", __FUNCTION__, config_preview_video_axis);

    memset(config_tv_path, 0x0, 64);
    config_get("tvin.manual.set.tvpath", config_tv_path, "null");
    ALOGD("%s, tvin.manual.set.tvpath -> config_tv_path[%s].", __FUNCTION__, config_tv_path);

    memset(config_default_path, 0x0, 64);
    config_get("tvin.manual.set.defaultpath", config_default_path, "null");
    ALOGD("%s, tvin.manual.set.defaultpath -> config_default_path[%s].", __FUNCTION__, config_default_path);

    memset(config_3d_glass_enable,0x0,64);
    memset(config_3d_glass_reset,0x0,64);
    config_get("tvin.3d.glass.enable",config_3d_glass_enable,"null");
    ALOGD("%s, tvin.manual.set.defaultpath -> config_default_path[%s].", __FUNCTION__, config_3d_glass_enable);
    if(strcmp(config_3d_glass_enable,"enable")==0) {
        config_get("tvin.3d.glass.reset",config_3d_glass_reset,"null");
        ALOGD("%s, tvin.3d.glass.reset -> config_default_path[%s].", __FUNCTION__, config_3d_glass_reset);
        if(strcmp(config_3d_glass_reset,"null")==0) {
            gTvinConfig.glass_3d_enable =0;
        } else
            gTvinConfig.glass_3d_enable =0x55;
    } else
        gTvinConfig.glass_3d_enable=0;

    return 0;
}

int Tvin_PreInit_Parameters(tvin_api_para_t *parameters)
{
    int val = SOURCE_MPEG;
    char prop_value[PROPERTY_VALUE_MAX];
    am_regs_t regs;
    int ret =1, paneltype =0, fscheck_ret;

    if (NULL == parameters)
        return -1;

    Tvin_GetTvinConfig();

    if (gTvinConfig.checkfs == 0x55) {
        fscheck_ret = Tvin_CheckFs();
        ALOGD("%s, fscheck_ret[%d].", __FUNCTION__, fscheck_ret);
    }

    parameters->source_input = (tvin_source_input_t)val;
    SSMReadSourceInput(&val);
    if (val < SOURCE_TV || val >= SOURCE_MAX) {
        ALOGW("%s, SSMReadSourceInput[%d] error! Set sourceinput to MPEG!", __FUNCTION__, val);
        val = SOURCE_MPEG;
    }
    parameters->last_source_input = (tvin_source_input_t)val;
    parameters->tvin_para.info.fmt = TVIN_SIG_FMT_NULL;
    parameters->tvin_status = TVIN_STATUS_IDLE;
    parameters->tvin_pre_status = TVIN_STATUS_IDLE;
    parameters->pathid = TV_PATH_DECODER_3D_AMVIDEO;
    parameters->is_mpeg = 1;
    if (gTvinConfig.hdmi_eq == 1) {
        TvinApi_SetHDMIEQConfig(0x207);
    } else {
        TvinApi_SetHDMIEQConfig(0x7);
    }

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.atv", prop_value, "4096");
    if (sscanf(prop_value, "%d", &(parameters->atv_port)) != 1) {
        parameters->atv_port = TVIN_PORT_CVBS0;
        ALOGW("%s, Get prop ro.tv.tvinchannel.atv error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.av1", prop_value, "4097");
    if (sscanf(prop_value, "%d", &(parameters->av1_port)) != 1) {
        parameters->av1_port = TVIN_PORT_CVBS1;
        ALOGW("%s, Get prop ro.tv.tvinchannel.av1 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.av2", prop_value, "4098");
    if (sscanf(prop_value, "%d", &(parameters->av2_port)) != 1) {
        parameters->av2_port = TVIN_PORT_CVBS2;
        ALOGW("%s, Get prop ro.tv.tvinchannel.av2 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.ypbpr1", prop_value, "2048");
    if (sscanf(prop_value, "%d", &(parameters->ypbpr1_port)) != 1) {
        parameters->ypbpr1_port = TVIN_PORT_COMP0;
        ALOGW("%s, Get prop ro.tv.tvinchannel.ypbpr1 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.ypbpr2", prop_value, "2049");
    if (sscanf(prop_value, "%d", &(parameters->ypbpr2_port)) != 1) {
        parameters->ypbpr2_port = TVIN_PORT_COMP1;
        ALOGW("%s, Get prop ro.tv.tvinchannel.ypbpr2 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.hdmi1", prop_value, "16384");
    if (sscanf(prop_value, "%d", &(parameters->hdmi1_port)) != 1) {
        parameters->hdmi1_port = TVIN_PORT_HDMI0;
        ALOGW("%s, Get prop ro.tv.tvinchannel.hdmi1 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.hdmi2", prop_value, "16386");
    if (sscanf(prop_value, "%d", &(parameters->hdmi2_port)) != 1) {
        parameters->hdmi1_port = TVIN_PORT_HDMI2;
        ALOGW("%s, Get prop ro.tv.tvinchannel.hdmi2 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.hdmi3", prop_value, "16387");
    if (sscanf(prop_value, "%d", &(parameters->hdmi3_port)) != 1) {
        parameters->hdmi1_port = TVIN_PORT_HDMI3;
        ALOGW("%s, Get prop ro.tv.tvinchannel.hdmi3 error!\n", __FUNCTION__);
    }
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.tv.tvinchannel.vga", prop_value, "1024");
    if (sscanf(prop_value, "%d", &(parameters->vga_port)) != 1) {
        parameters->vga_port = TVIN_PORT_VGA0;
        ALOGW("%s, Get prop ro.tv.tvinchannel.vga error!\n", __FUNCTION__);
    }

    ALOGD("%s, atv_port = 0x%x.", __FUNCTION__, parameters->atv_port);
    ALOGD("%s, av1_port = 0x%x.", __FUNCTION__, parameters->av1_port);
    ALOGD("%s, av2_port = 0x%x.", __FUNCTION__, parameters->av2_port);
    ALOGD("%s, ypbpr1_port = 0x%x.", __FUNCTION__, parameters->ypbpr1_port);
    ALOGD("%s, ypbpr2_port = 0x%x.", __FUNCTION__, parameters->ypbpr2_port);
    ALOGD("%s, hdmi1_port = 0x%x.", __FUNCTION__, parameters->hdmi1_port);
    ALOGD("%s, hdmi2_port = 0x%x.", __FUNCTION__, parameters->hdmi2_port);
    ALOGD("%s, hdmi3_port = 0x%x.", __FUNCTION__, parameters->hdmi3_port);
    ALOGD("%s, vga_port = 0x%x.\n", __FUNCTION__, parameters->vga_port);

    if (gTvinConfig.multi_db == 1) {
        paneltype = SSMReadPanelType();
        switch(paneltype) {
        case PANEL_42_LG:
            property_set("ro.tv.paneltype", "LG42_PQ1_0:0");
            ret = openDBbyName("pq1_0.db");
            break;
        case PANEL_39_IW:
            property_set("ro.tv.paneltype", "IW39_PQ1_0:3");
            ret = openDBbyName("pq1_0.db");
            break;
        case PANEL_47_LG:
            property_set("ro.tv.paneltype", "LG47_PQ2_0:1");
            ret = openDBbyName("pq2_0.db");
            break;
        case PANEL_42_IW:
            property_set("ro.tv.paneltype", "IW42_PQ2_0:4");
            ret = openDBbyName("pq2_0.db");
            break;
        case PANEL_55_LG:
            property_set("ro.tv.paneltype", "LG55_PQ3_0:2");
            ret = openDBbyName("pq3_0.db");
            break;
        case PANEL_50_IW:
            property_set("ro.tv.paneltype", "IW50_PQ3_0:5");
            ret = openDBbyName("pq3_0.db");
            break;
        case PANEL_39_CM:
            property_set("ro.tv.paneltype", "CM39_PQ1_1:6");
            ret = openDBbyName("pq1_1.db");
            break;
        case PANEL_42_CM:
            property_set("ro.tv.paneltype", "CM42_PQ2_1:7");
            ret = openDBbyName("pq2_1.db");
            break;
        case PANEL_50_CM:
            property_set("ro.tv.paneltype", "CM50_PQ3_1:8");
            ret = openDBbyName("pq3_1.db");
            break;
        case PANEL_42_SL:
            property_set("ro.tv.paneltype", "SL42_PQ2_2:9");
            ret = openDBbyName("pq2_2.db");
            break;
        case PANEL_42_SK:
            property_set("ro.tv.paneltype", "SK42_PQ1_3:10");
            ret = openDBbyName("pq1_3.db");
            break;
        case PANEL_47_SK:
            property_set("ro.tv.paneltype", "SK47_PQ2_3:11");
            ret = openDBbyName("pq2_3.db");
            break;
        case PANEL_55_SK:
            property_set("ro.tv.paneltype", "SK42_PQ3_3:12");
            ret = openDBbyName("pq3_3.db");
            break;
        default:
            property_set("ro.tv.paneltype", "LG47_PQ2_0:1");
            ret = openDBbyName("pq2_0.db");
            break;
        }
        if(ret) {
            ALOGW("openDB failed!");
            openDBbyName("pq1_0.db"); //default db
        } else {
            ALOGD("openDB success!");
        }
    } else {
        if (openDB()) {
            ALOGW("openDB failed!");
        } else {
            ALOGD("openDB success!");
        }
    }
    return 0;
}

void Tvin_GetDbcFunc()
{
    void *pSofile;
    pSofile=dlopen("libtv_dbc.so",RTLD_NOW);
    if(pSofile != NULL) {
        DrvDynaBacklight = (GetHandle)dlsym(pSofile,"_Z17vDrvDynaBacklightv");

        if(DrvDynaBacklight == NULL) {
            dlclose(pSofile);
            ALOGE("dlsym  vDrvDynaBacklight fail");
        }
    } else {
        ALOGE("dlopen tv_dbc.so fail %s",__FUNCTION__);
    }
}

int Tvin_Init_Parameters(tvin_api_para_t *parameters, int mode)
{
    ALOGE("Enter Tvin_Init_Parameters!!!!");
    unsigned char val = 0;
    char prop_value[PROPERTY_VALUE_MAX];

    if (NULL == parameters)
        return -1;

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.build.product", prop_value, "e00ref");
    if (strcmp(prop_value, "e00ref") == 0) {
        parameters->product_id = TV_PRODUCT_E00REF;
    } else if (strcmp(prop_value, "e03ref") == 0) {
        parameters->product_id = TV_PRODUCT_E03REF;
    } else if (strcmp(prop_value, "e04ref") == 0) {
        parameters->product_id = TV_PRODUCT_E04REF;
    } else if (strcmp(prop_value, "e05ref") == 0) {
        parameters->product_id = TV_PRODUCT_E05REF;
    } else if (strcmp(prop_value, "e06ref") == 0) {
        parameters->product_id = TV_PRODUCT_E06REF;
    } else if (strcmp(prop_value, "e07ref") == 0) {
        parameters->product_id = TV_PRODUCT_E07REF;
    } else if (strcmp(prop_value, "e08ref") == 0) {
        parameters->product_id = TV_PRODUCT_E08REF;
    } else if (strcmp(prop_value, "e09ref") == 0) {
        parameters->product_id = TV_PRODUCT_E09REF;
    } else if (strcmp(prop_value, "e10ref") == 0) {
        parameters->product_id = TV_PRODUCT_E10REF;
    } else if (strcmp(prop_value, "e11ref") == 0) {
        parameters->product_id = TV_PRODUCT_E11REF;
    } else if (strcmp(prop_value, "e12ref") == 0) {
        parameters->product_id = TV_PRODUCT_E12REF;
    } else if (strcmp(prop_value, "e13ref") == 0) {
        parameters->product_id = TV_PRODUCT_E13REF;
    } else if (strcmp(prop_value, "e14ref") == 0) {
        parameters->product_id = TV_PRODUCT_E14REF;
    } else if (strcmp(prop_value, "e15ref") == 0) {
        parameters->product_id = TV_PRODUCT_E15REF;
    } else if (strcmp(prop_value, "e16ref") == 0) {
        parameters->product_id = TV_PRODUCT_E16REF;
    } else if (strcmp(prop_value, "e17ref") == 0) {
        parameters->product_id = TV_PRODUCT_E17REF;
    } else if (strcmp(prop_value, "e18ref") == 0) {
        parameters->product_id = TV_PRODUCT_E18REF;
    } else if (strcmp(prop_value, "e19ref") == 0) {
        parameters->product_id = TV_PRODUCT_E19REF;
    } else if (strcmp(prop_value, "e20ref") == 0) {
        parameters->product_id = TV_PRODUCT_E20REF;
    } else if (strcmp(prop_value, "h00ref") == 0) {
        parameters->product_id = TV_PRODUCT_H00REF;
    } else if (strcmp(prop_value, "h01ref") == 0) {
        parameters->product_id = TV_PRODUCT_H01REF;
    } else if (strcmp(prop_value, "h02ref") == 0) {
        parameters->product_id = TV_PRODUCT_H02REF;
    } else if (strcmp(prop_value, "h03ref") == 0) {
        parameters->product_id = TV_PRODUCT_H03REF;
    } else if (strcmp(prop_value, "h04ref") == 0) {
        parameters->product_id = TV_PRODUCT_H04REF;
    } else if (strcmp(prop_value, "h05ref") == 0) {
        parameters->product_id = TV_PRODUCT_H05REF;
    } else {
        parameters->product_id = TV_PRODUCT_E00REF;
    }

    parameters->tvin_para.index = 0;
    parameters->tvin_para.port = TVIN_PORT_MPEG0;
    parameters->tvin_para.info.trans_fmt = TVIN_TFMT_2D;
    parameters->tvin_para.info.fmt = TVIN_SIG_FMT_NULL;
    parameters->tvin_para.info.status = TVIN_SIG_STATUS_UNSTABLE;
    parameters->tvin_para.info.reserved = 0;
    memset(&(parameters->tvin_para.cutwin), 0, sizeof(parameters->tvin_para.cutwin));
    memset(parameters->tvin_para.histgram, 0, sizeof(parameters->tvin_para.histgram));
    parameters->tvin_para.flag = 0;
    parameters->tvin_para.reserved = 0;

    parameters->tvin_status = (tvin_status_t)mode;
    parameters->source_input = parameters->last_source_input;
    parameters->pathid = TV_PATH_DECODER_3D_AMVIDEO;
    parameters->audio_channel = TV_AUDIO_LINE_IN_0;
    parameters->fmt_ratio = RATIO_169;
    parameters->color_fmt = COLOR_YUV422;
    parameters->av_color_system = COLOR_SYSTEM_AUTO;
    parameters->input_window = 0;
    parameters->cut_top = 0;
    parameters->cut_left = 0;
    parameters->mode_3d = MODE3D_DISABLE;
    parameters->status_3d = STATUS3D_DISABLE;
    parameters->is_decoder_start = false;
    parameters->is_turnon_signal_detect_thread = false;
    parameters->is_signal_detect_thread_start = false;
    parameters->is_signal_detect_exec_done = true;
    parameters->is_turnon_source_switch_thread = false;
    parameters->is_source_switch_thread_start = false;
    parameters->is_source_switch_exec_done = true;
    parameters->is_turnon_overscan = true;
    parameters->is_hdmi_sr_detect_start = false;
    parameters->hdmi_sampling_rate = 0;
    parameters->source_switch_donecount = 0;
    parameters->stable_count = 0;
    parameters->check_stable_count = 0;
    parameters->pre_check_stable_count = 0;
    parameters->is_turnon_adc_autocalibration_thread = false;
    parameters->is_adc_autocalibration_start = false;
    parameters->adc_autocalibration_status = TV_ADC_AUTO_CALIBRATION_END;

    SSMReadADCCalibrationFlagValue((1*ADC_CALIBRATION_INPUT_VGA), 1, &val);
    if (val == 0x55) {
        ALOGD("%s, get VGA gain/offset from rom.", __FUNCTION__);
        TvinADCAutoCalibration_GetPara(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
    } else {
        TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
        TvinADCAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_VGA, ADCGainOffset_VGA);
        ALOGD("%s, init VGA gain/offset and save to rom.", __FUNCTION__);
    }
    SSMReadADCCalibrationFlagValue((1*ADC_CALIBRATION_INPUT_YPBPR1), 1, &val);
    if (val == 0x55) {
        ALOGE("%s, get YBPBR1 gain/offset from rom.", __FUNCTION__);
        TvinADCYPbPrAutoCalibration_GetPara(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
    } else {
        TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
        TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR1, ADCGainOffset_Ypbpr1);
        ALOGE("%s, init YBPBR1 gain/offset and save to rom.", __FUNCTION__);
    }
    SSMReadADCCalibrationFlagValue((1*ADC_CALIBRATION_INPUT_YPBPR2), 1, &val);
    if (val == 0x55) {
        ALOGE("%s, get YBPBR2 gain/offset from rom.", __FUNCTION__);
        TvinADCYPbPrAutoCalibration_GetPara(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
    } else {
        TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
        TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR2, ADCGainOffset_Ypbpr2);
        ALOGE("%s, init YBPBR2 gain/offset and save to rom.", __FUNCTION__);
    }
    Tvin_Set3DStatus(STATUS3D_DISABLE);
    SSMSave3DLRSwitch(0);
    SSMSave3DTO2D(0);
    switch(SSMReadPanelType()) {
    case PANEL_39_IW:
    case PANEL_42_IW:
    case PANEL_50_IW:
        int vdac_bk = GetFileAttrIntValue("/sys/module/iw7023/parameters/vdac_2d_backup");
        ALOGD("%s, current_mode = 2d, save vdac_2d(%d) to eeprom.\n",__FUNCTION__,vdac_bk);
        SSMSaveVDac2DValue(vdac_bk);
        break;
    }
    return 0;
}

static int Tv_init_vdin(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    TvinApi_OpenVDINModule(0);
    TvinApi_SetBlackOutPolicy(1);
    Tvin_RemovePath(TV_PATH_TYPE_DEFAULT);

    if (gTvinConfig.preview_freescale == 0x55) {
        if (gTvinApiPara.tvin_status == TVIN_STATUS_PREVIEW_START) {
            Tvin_AddPath(TV_PATH_VDIN_FREESCALE_AMVIDEO);
//            TvinApi_EnableFreeScale(1280,720, 104, 96, 507, 349);
        } else {
            Tvin_RemovePath(TV_PATH_TYPE_TVIN_PREVIEW);
            Tvin_AddPath(TV_PATH_VDIN_NEW3D_AMVIDEO);
        }
    } else {
        if (gTvinConfig.new_d2d3 == 0x55 ) {
            Tvin_AddPath(TV_PATH_VDIN_NEW3D_AMVIDEO);
        } else {
            Tvin_AddPath(TV_PATH_VDIN_3D_AMVIDEO);
        }
    }

    TvinSigDetect_CreateThread();
    //TvinSourceSwitch_CreateThread();
//    if (gTvinConfig.seria_debug == 0x55) {
//        SerialCmd_CreateThread();
//    }
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

static int Tv_init_afe(void)
{
    ALOGE("Enter %s.", __FUNCTION__);
    TvinApi_OpenAFEModule();
    SSMSetVgaEdid();
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

static int Tv_init_vpp(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    return Vpp_Init();
}

static int Tv_init_ppmgr(void)
{
    int ret = -1;
    ALOGD("Enter %s.", __FUNCTION__);
    ret = TvinApi_OpenPPMGRModule();
    ret |= TvinApi_SetPpmgrPlatformType(2);
    return ret;
}

static int Tv_init_audio(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    return 0;
}
static int Tv_init_ssm(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    if (SSMDeviceMarkCheck() < 0) {
        SSMRestoreDeviceMarkValues();
        SSMRestoreDefaultSetting();
    }

    SSMMacAddressCheck();
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

static int Tv_uninit_vdin(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    TvinApi_ClosePort(0);
    TvinApi_CloseVDINModule(0);
    TvinSigDetect_KillThread();
    //TvinSourceSwitch_KillThread();
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

static int Tv_uninit_afe(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    TvinApi_CloseAFEModule();
    return 0;
}

static int Tv_uninit_vpp(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    Vpp_Uninit();
    return 0;
}

static int Tv_uninit_ppmgr(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    return 0;
}

static int Tv_uninit_audio(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    return 0;
}
static int Tv_uninit_eeprom(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    return 0;
}

static int Tv_init_reg_interface(void)
{
    LOGD("Enter %s.", __FUNCTION__);

    SSMRegisterInterface();
    VppRegisterInterface();
    TvinRegisterInterface();
    AudioRegisterInterface();
    ATVRegisterInterface();
    TvMiscRegisterInterface();

    return 0;
}

typedef struct tagTvServerInfo {
    int power_on_off_channel;
    int last_source_select;
    int system_language;
} TvServerInfo;

static int SetTvServerInfoProp(TvServerInfo *tmpInfo)
{
    int power_on_off_channel;
    int last_source_select;
    int system_language;
    int cfg_item_count = 0;
    const char *strDelimit = ",";
    char *token = NULL;
    char prop_value[PROPERTY_VALUE_MAX];
    char show_src_type_str[SOURCE_MAX][32];

    power_on_off_channel = tmpInfo->power_on_off_channel;
    last_source_select = tmpInfo->last_source_select;
    system_language = tmpInfo->system_language;

    strcpy(show_src_type_str[SOURCE_TV], "SOURCE_TV");
    strcpy(show_src_type_str[SOURCE_AV1], "SOURCE_AV1");
    strcpy(show_src_type_str[SOURCE_AV2], "SOURCE_AV2");
    strcpy(show_src_type_str[SOURCE_YPBPR1], "SOURCE_YPBPR1");
    strcpy(show_src_type_str[SOURCE_YPBPR2], "SOURCE_YPBPR2");
    strcpy(show_src_type_str[SOURCE_HDMI1], "SOURCE_HDMI1");
    strcpy(show_src_type_str[SOURCE_HDMI2], "SOURCE_HDMI2");
    strcpy(show_src_type_str[SOURCE_HDMI3], "SOURCE_HDMI3");
    strcpy(show_src_type_str[SOURCE_VGA], "SOURCE_VGA");
    strcpy(show_src_type_str[SOURCE_MPEG], "SOURCE_MPEG");
    strcpy(show_src_type_str[SOURCE_DTV], "SOURCE_DTV");

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);

    config_get("misc.lastselsrc.show.cfg", prop_value, "null");
    if (strcasecmp(prop_value, "null") != 0) {
        cfg_item_count = 0;

        token = strtok(prop_value, strDelimit);
        while (token != NULL) {
            strcpy(show_src_type_str[cfg_item_count], "SOURCE_");
            strncat(show_src_type_str[cfg_item_count], token, 32);

            token = strtok(NULL, strDelimit);
            cfg_item_count += 1;
        }
    }

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);

    if (last_source_select == (int) SOURCE_TV) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_TV], system_language);
    } else if (last_source_select == (int) SOURCE_AV1) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_AV1], system_language);
    } else if (last_source_select == (int) SOURCE_AV2) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_AV2], system_language);
    } else if (last_source_select == (int) SOURCE_YPBPR1) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_YPBPR1], system_language);
    } else if (last_source_select == (int) SOURCE_YPBPR2) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_YPBPR2], system_language);
    } else if (last_source_select == (int) SOURCE_HDMI1) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_HDMI1], system_language);
    } else if (last_source_select == (int) SOURCE_HDMI2) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_HDMI2], system_language);
    } else if (last_source_select == (int) SOURCE_HDMI3) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_HDMI3], system_language);
    } else if (last_source_select == (int) SOURCE_VGA) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_VGA], system_language);
    } else if (last_source_select == (int) SOURCE_MPEG) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_MPEG], system_language);
    } else if (last_source_select == (int) SOURCE_DTV) {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, show_src_type_str[SOURCE_DTV], system_language);
    } else {
        sprintf(prop_value, "%d,%s,%d", power_on_off_channel, "SOURCE_NULL", system_language);
    }

    property_set("rw.tvserver.info", prop_value);

    return 0;
}

static int SetAudioMainVolLutBufName()
{
    int i = 0;
    int get_type_buf[6] = { CC_GET_LUT_TV, CC_GET_LUT_AV, CC_GET_LUT_COMP, CC_GET_LUT_HDMI, CC_GET_LUT_VGA, CC_GET_LUT_MPEG };
    char* end_str_buf[6] = { ".tv", ".av", ".comp", ".hdmi", ".vga", ".mpeg", };
    char tmp_buf[64];
    char value_buf[64];
    char key_buf[64];

    int name_en = GetAudioMainVolLutBufNameModifyEnableCFG();

    if (name_en == 1) {
        int panelty = SSMReadPanelType();
        ALOGD(" %s, audio.amp.mainvol.lutbufname.mod.en = %d ,LCD panel = %d \n", __FUNCTION__, name_en, panelty);

        memset(tmp_buf, '\0', 64);
        memset(value_buf, '\0', 64);
        memset(key_buf, '\0', 64);

        cfg_get_one_item("misc.panel.amp.power", panelty, tmp_buf);

        for (i = 0; i < 6; i++) {
            GetAudioMainVolLutBufNameCFG(get_type_buf[i], key_buf);

            strcpy(value_buf, tmp_buf);
            strcat(value_buf, end_str_buf[i]);
            config_set(key_buf, value_buf);

            ALOGD(" %s, config set string: %s \n", __FUNCTION__, value_buf);
            memset(value_buf, '\0', 64);
        }
    }

    return 0;
}

static int SetAudioSupperBassVolLutBufName()
{
    int i = 0;
    int get_type_buf[6] = { CC_GET_LUT_TV, CC_GET_LUT_AV, CC_GET_LUT_COMP, CC_GET_LUT_HDMI, CC_GET_LUT_VGA, CC_GET_LUT_MPEG };
    char* end_str_buf[6] = { ".tv", ".av", ".comp", ".hdmi", ".vga", ".mpeg", };
    char tmp_buf[64];
    char value_buf[64];
    char key_buf[64];

    int name_en = GetAudioSupperBassVolLutBufNameModifyEnableCFG();

    if (name_en == 1) {
        int panelty = SSMReadPanelType();
        ALOGD(" %s, audio.amp.supbassvol.lutbufname.mod.en = %d ,LCD panel = %d \n", __FUNCTION__, name_en, panelty);

        memset(tmp_buf, '\0', 64);
        memset(value_buf, '\0', 64);
        memset(key_buf, '\0', 64);

        cfg_get_one_item("misc.panel.amp.supperbass.power", panelty, tmp_buf);

        for (i = 0; i < 6; i++) {
            GetAudioSupperBassVolLutBufNameCFG(get_type_buf[i], key_buf);

            strcpy(value_buf, tmp_buf);
            strcat(value_buf, end_str_buf[i]);
            config_set(key_buf, value_buf);

            ALOGD(" %s, config set string: %s \n", __FUNCTION__, value_buf);
            memset(value_buf, '\0', 64);
        }
    }

    return 0;
}

static int Tv_SetTvServerInfo(int clean_flag)
{
    int power_on_off_channel = 0;
    int last_source_select = 0;
    int system_language = 0;
    static int init_done = 0;

    if (Tvin_GetTvProductId() != TV_PRODUCT_E06REF && Tvin_GetTvProductId() != TV_PRODUCT_E12REF
        && Tvin_GetTvProductId() != TV_PRODUCT_E13REF && Tvin_GetTvProductId() != TV_PRODUCT_E15REF
        && Tvin_GetTvProductId() != TV_PRODUCT_E16REF && Tvin_GetTvProductId() != TV_PRODUCT_H02REF) {
        return -1;
    }
    if (init_done != 0) {
        return -1;
    }

    TvServerInfo tmpInfo;

    if (clean_flag) {
        tmpInfo.power_on_off_channel = 0;
        tmpInfo.last_source_select = -1;
        tmpInfo.system_language = 0;
        SetTvServerInfoProp(&tmpInfo);
        init_done = 1;
    } else {
        tmpInfo.power_on_off_channel = SSMReadPowerOnOffChannel();
        tmpInfo.last_source_select = SSMReadLastSelectSourceType();
        tmpInfo.system_language = SSMReadSystemLanguage();
        SetTvServerInfoProp(&tmpInfo);
    }

    return 0;
}

int TvSourceDetectThreadCreate(void)
{
    int ret = 0;
    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 1;
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(&TvSourceDetecthreadId, &attr, &TvinSourceDetect_TreadRun, NULL);
    pthread_attr_destroy(&attr);

    ALOGD("%s, create source detect  thread sucess, TvSourceDetecthreadId(%d)\n", __FUNCTION__, TvSourceDetecthreadId);

    return ret;
}

int Tv_Open(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    Tv_init_reg_interface();
    char prop_value[PROPERTY_VALUE_MAX];
    memset(prop_value, '\0', PROPERTY_VALUE_MAX);

    for(int i=0; i<200; i++) {
        property_get("ro.build.product", prop_value, "init");
        if (strcmp(prop_value, "init") == 0) {
            usleep(100*1000);
        } else {
            ALOGD("%s,prop_value = %s", __FUNCTION__,prop_value);
            break;
        }
    }
    if (strcmp(prop_value, "e00ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E00REF;
    } else if (strcmp(prop_value, "e03ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E03REF;
    } else if (strcmp(prop_value, "e04ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E04REF;
    } else if (strcmp(prop_value, "e05ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E05REF;
    } else if (strcmp(prop_value, "e06ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E06REF;
    } else if (strcmp(prop_value, "e07ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E07REF;
    } else if (strcmp(prop_value, "e08ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E08REF;
    } else if (strcmp(prop_value, "e09ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E09REF;
    } else if (strcmp(prop_value, "e10ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E10REF;
    } else if (strcmp(prop_value, "e11ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E11REF;
    } else if (strcmp(prop_value, "e12ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E12REF;
    } else if (strcmp(prop_value, "e13ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E13REF;
    } else if (strcmp(prop_value, "e14ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E14REF;
    } else if (strcmp(prop_value, "e15ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E15REF;
    } else if (strcmp(prop_value, "e16ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E16REF;
    } else if (strcmp(prop_value, "e17ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E17REF;
    } else if (strcmp(prop_value, "e18ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E18REF;
    } else if (strcmp(prop_value, "e19ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E19REF;
    } else if (strcmp(prop_value, "e20ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_E20REF;
    } else if (strcmp(prop_value, "h00ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H00REF;
    } else if (strcmp(prop_value, "h01ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H01REF;
    } else if (strcmp(prop_value, "h02ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H02REF;
    } else if (strcmp(prop_value, "h03ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H03REF;
    } else if (strcmp(prop_value, "h04ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H04REF;
    } else if (strcmp(prop_value, "h05ref") == 0) {
        gTvinApiPara.product_id = TV_PRODUCT_H05REF;
    } else {
        gTvinApiPara.product_id = TV_PRODUCT_E00REF;
    }
    Tv_init_ssm();
    Tvin_PreInit_Parameters(&gTvinApiPara);
    Tv_init_vpp();
    Tv_init_ppmgr();
    SetAudioSupperBassVolLutBufName();
    SetAudioMainVolLutBufName();
    AudioCtlInitializeLoad();
    //Tv_SetTvServerInfo(0);
    TvMisc_EnableWDT(gTvinConfig.userpet, gTvinConfig.userpet_timeout, gTvinConfig.userpet_reset);
    if (gTvinConfig.socket_interface == 0x55) {
        TvMiscSocketThreadKill();
        TvMiscSocketThreadCreate();
    }
    TvinApi_SetCompPhase();
    TvinApi_SetCompPhaseEnable(1);
    TvinApi_SetRDMA(1);
    TvinApi_SetDIBypasshd(1);
    if (gTvinConfig.source_detectfunction == 0x55) { //source detect for tcl only
        TvSourceDetectThreadCreate();
        Tvin_GetDbcFunc();
    }
    SSMSetHDCPKey();
    system("/system/bin/dec");
    //Tv_init_ssm();
    SSMMacAddressCheck();

    Tv_SetTvServerInfo(0);
	
    ATVInit();
#if(0)
    if (gTvinConfig.seria_debug == 0x55) {
        GetConsoleStatus();
//      SSMReadSerialOnOffFlg();
//      cmdmain();
    }
#endif
    Tvin_RemovePath(TV_PATH_TYPE_DEFAULT);
    Tvin_AddPath(TV_PATH_DECODER_NEW3D_AMVIDEO);
	TvinApi_SetPpmgrView_mode(3);
	
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

int Tv_Close(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
    Tv_uninit_ppmgr();
    Tv_uninit_vpp();
    Tv_uninit_eeprom();
    TvMisc_DisableWDT(gTvinConfig.userpet);
    if (gTvinConfig.socket_interface == 0x55) {
        //    TvMiscSocketThreadKill();
    }
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}
int Tv_StartPlay()
{
    tvin_parm_t tvin_para;
    tvin_para.index = 0;
    tvin_para.port = TVIN_PORT_CVBS0;
    tvin_para.cutwin.hs = 0;
    tvin_para.cutwin.he = 0;
    tvin_para.cutwin.vs = 0;
    tvin_para.cutwin.ve = 0;
    if (TvinApi_StartDec(0, tvin_para) >= 0) {
        LOGD("%s, StartDecoder succeed.", __FUNCTION__);
        return 0;
    } else {
        LOGE("%s, StartDecoder faild.", __FUNCTION__);
    }
    return -1;
}
int Tv_StopPlay()
{
    if (TvinApi_StopDec(0) >= 0) {
        LOGD("%s, StopDecoder ok!", __FUNCTION__);
        return 0;
    } else {
        LOGE("%s, StopDecoder Failed!", __FUNCTION__);
    }
    return -1;
}
int Tv_Start(int mode)
{
    Mutex::Autolock _l(start_Lock);
    ALOGD("Enter %s, start mode [%d].", __FUNCTION__, mode);
    property_set("media.policy.tvInputDev", "true");
    if (mTvStartedFlag == 0) {
        mTvStartedFlag = 1;
        gTvinApiPara.tvin_status = (tvin_status_t)mode;
        Tv_SetTvServerInfo(1);
        Tvin_Init_Parameters(&gTvinApiPara, mode);
        Tv_init_vdin();
        Tv_init_afe();
        AudioCtlInit();
        int_source_mapping_table();
        if ((tvin_status_t)mode == TVIN_STATUS_PREVIEW_START) {
            //TvinApi_SetDIBypasshd(1);
            TvinApi_SetDIBypassAll(1);
            TvinApi_SetVdinHVScale(0,720,480);
        } else {
            TvinApi_SetDIBypasshd(0);
            TvinApi_SetDIBypassAll(0);
            TvinApi_SetVdinHVScale(0,0,0);
        }
        Vpp_SetVppVideoDisable(1);
        TvinApi_SetVdinFlag(MEMP_VDIN_WITHOUT_3D);
        mTvStopFlag = 0;
        ALOGD("%s, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
    } else if (mTvStartedFlag == 1 && mTvStopFlag == 0) {
        if (Tvin_GetTvinStatus() != (tvin_status_t)mode) {
            gTvinApiPara.tvin_status = (tvin_status_t)mode;
            if ((tvin_status_t)mode == TVIN_STATUS_PREVIEW_START) {
                TvinApi_SetDIBypasshd(1);
                gTvinApiPara.last_source_input = gTvinApiPara.source_input;
            } else {
                TvinApi_SetDIBypasshd(0);
            }
            ALOGD("%s, Tv is running, but start mode will be changed.", __FUNCTION__);
        } else {
            ALOGW("%s, Tv is running, you no need start again!", __FUNCTION__);
        }
    } else if (mTvStopFlag == 1) {
        ALOGW("%s, Tv is stoping, you can not start.", __FUNCTION__);
    }

    if (gTvinConfig.autoreboot == 0x55) {
        android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
    }

    char ssm_prop_value[PROPERTY_VALUE_MAX];
    char uboot_prop_value[PROPERTY_VALUE_MAX];
    int uboot_tmp_value = 0;
    int ssm_tmp_value = 0;

    ssm_tmp_value = SSMReadAdbSwitchValue();

    ALOGD("%s, AdbSwitchValue ssm_tmp_value = %d",__FUNCTION__,ssm_tmp_value);

    if(ssm_tmp_value < 0) {
        ALOGE("%s, read adb_switch_value error", __FUNCTION__);
        return -1;
    }

    sprintf(ssm_prop_value, "%d", ssm_tmp_value);
    //ALOGD("%s, ssm_prop_value = %s",__FUNCTION__,ssm_prop_value);

    property_get("persist.service.adb.enable", uboot_prop_value, "null");
    //ALOGD("%s, uboot_prop_value = %s",__FUNCTION__,uboot_prop_value);

    if(strcmp(uboot_prop_value, ssm_prop_value) != 0) {
        property_set("persist.service.adb.enable", ssm_prop_value);
        //ALOGD("%s, The uboot adb switch is [%s] not equal to ssm adb switch [%s]\n", __FUNCTION__, uboot_prop_value, ssm_prop_value);
    }

    ssm_tmp_value = SSMReadSerialCMDSwitchValue();
    //ALOGD("%s, SerialCMDSwitchValue ssm_tmp_value = %d", __FUNCTION__,ssm_tmp_value);

    if(ssm_tmp_value < 0) {
        ALOGE("%s, read SerialCMDSwitchValue error!", __FUNCTION__);
        return -1;
    }

    if(ssm_tmp_value == 1) {
        strcpy(ssm_prop_value, "ttyS0,115200n8");//open
    } else {
        strcpy(ssm_prop_value, "off");//close
    }

    //ALOGD("%s, ssm_prop_value = %s",__FUNCTION__,ssm_prop_value);

    property_get("ubootenv.var.console_debug", uboot_prop_value, "null");

    if(strcmp(uboot_prop_value, ssm_prop_value) != 0) {
        property_set("ubootenv.var.console_debug", ssm_prop_value);
        //ALOGD("The uboot adb switch is [%s] not equal to ssm adb switch [%s]\n", uboot_prop_value, ssm_prop_value);
    }
#if(1)
    if (gTvinConfig.seria_debug == 0x55) {
//        SSMReadSerialOnOffFlg();
        GetConsoleStatus();
//  cmdmain();
    }
#endif

    return 0;
}

int Tv_Stop(void)
{
    ALOGD("Enter %s, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
    if (mTvStopFlag == 0 || mTvStartedFlag != 0) {
        if (gTvinApiPara.tvin_status == TVIN_STATUS_IDLE) {
            ALOGD("Exit %s, never start, so no need stop, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
            return 0;
        }
        acquire_wake_lock(PARTIAL_WAKE_LOCK, "amlogic_atv_stop_wakelock");
        property_set("media.policy.tvInputDev", "false");
        mTvStopFlag = 1;
        mTvStartedFlag = 0;
        gTvinApiPara.tvin_pre_status = gTvinApiPara.tvin_status;
        if (gTvinApiPara.tvin_status == TVIN_STATUS_PREVIEW_START) {
            gTvinApiPara.tvin_status = TVIN_STATUS_PREVIEW_STOP;
        } else {
            gTvinApiPara.tvin_status = TVIN_STATUS_NORMAL_STOP;
        }

        Tvin_TurnOnBlueScreen(2);

        //kill atv auto afc thread before audio mute.
        ATVKillAutoAFCThread();

        //we should stop audio first for audio mute.
        AudioCtlUninit();

        ATVUninit();

        Tvin_StopSigDetect();
        Tvin_StopSourceSwitch();
        TvinApi_SetBlackOutPolicy(1);

        if (gTvinApiPara.is_decoder_start == true) {
            TvinApi_StopDec(0);
            gTvinApiPara.is_decoder_start = false;
        }
        Tvin_SetSigFormat(TVIN_SIG_FMT_NULL);
		
        if (Tvin_GetSourceInput() == SOURCE_DTV) {
			TvinApi_SetRDMA(1);
            ALOGD("%s, current source is set to dtv, no need set back to mpeg.", __FUNCTION__);
        } else {
            Tvin_SetSrcPort(TVIN_PORT_MPEG0);
            Tvin_SetSourceInput(TVIN_PORT_MPEG0);
            SSMSaveSourceInput(SOURCE_MPEG);
			TvinApi_SetRDMA(0);
        }
		
        TvinApi_SetDIBypassAll(0);
        TvinApi_SetDIBypasshd(1);
        Tvin_Set3DMode(MODE3D_DISABLE);
        Tvin_Set3DStatus(STATUS3D_DISABLE);
        Vpp_SetVppVideoDisable(2);
        TvinApi_KeepLastFrame(0);
        Vpp_SetVppVideoCrop(0, 0, 0, 0);
        TvinApi_Set3DOverScan(false);
        TvinApi_SetStartDropFrameCn(0);
        if (gTvinConfig.preview_freescale == 0x55 && gTvinApiPara.tvin_status == TVIN_STATUS_PREVIEW_STOP) {
            Tvin_RemovePath(TV_PATH_TYPE_TVIN_PREVIEW);
        } else {
            Tvin_RemovePath(TV_PATH_TYPE_TVIN);
        }
        if (gTvinConfig.new_d2d3 == 0x55 ) {
            TvinApi_SetD2D3Bypass(1);
            Tvin_AddPath(TV_PATH_DECODER_NEW3D_AMVIDEO);
        } else {
            Tvin_AddPath(TV_PATH_DECODER_3D_AMVIDEO);
        }
        TvinApi_SetVdinFlag(MEMP_DCDR_WITHOUT_3D);
        TvinApi_Send3DCommand(MODE3D_DISABLE);
        if (gTvinConfig.new_3dautodetc == 0x55) {
            TvinApi_SetDI3DDetc(MODE3D_DISABLE);
        }
        Tv_uninit_afe();
        Tv_uninit_vdin();
        AudioSetSysMuteStatus(CC_MUTE_OFF);
        Tv_LoadVppSettings(SOURCE_TYPE_MPEG, TVIN_SIG_FMT_NULL, STATUS3D_DISABLE, TVIN_TFMT_2D);
        gTvinApiPara.tvin_status = TVIN_STATUS_IDLE;
        mTvStopFlag = 2;

        if (gTvinConfig.preview_freescale == 0x55) {
            Vpp_SetVppScreenColor(3, 16, 128, 128);
        }

        ALOGD("%s, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
    } else if (mTvStopFlag == 1) {
        ALOGW("%s, Stopping, please do not stop again, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
    } else {
        ALOGW("%s, Stoped, no need stop, mTvStartedFlag[%d], mTvStopFlag[%d].", __FUNCTION__, mTvStartedFlag, mTvStopFlag);
    }

    //double confirm we set the main volume lut buffer to mpeg
    AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_MPEG, 1, 1, -1);
    SetAudioMasterVolume(GetAudioMasterVolume());

    if (gTvinConfig.autoreboot == 0x55) {
        android_reboot(ANDROID_RB_RESTART2, 0, "cool_reboot");
    }
    release_wake_lock("amlogic_atv_stop_wakelock");
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

int Tv_AudioPause(void)
{
    ALOGD("Enter %s.", __FUNCTION__);

    property_set("media.policy.tvInputDev", "false");
    //Stop Dsp
    AudioDspStop();

    // Uninit Alsa
    AudioAlsaUnInit();

    if (Tvin_GetSrcInputType() == SOURCE_TYPE_HDMI) {
        // Stop HDMI in
        AudioStopHDMIIn();
    } else {
        // Stop Line in
        AudioStopLineIn();
    }

    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
        AudioSetSysMuteStatus(CC_MUTE_OFF);
    }

    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

int Tv_AudioResume(void)
{
    ALOGD("Enter %s.", __FUNCTION__);

    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
        if(Tvin_GetSigStatus() != TVIN_SIG_STATUS_STABLE) {
            AudioSetSysMuteStatus(CC_MUTE_ON);
        }
    }

    property_set("media.policy.tvInputDev", "true");
    return TvinAudioSwitch(Tvin_GetSourceInput());
}

int Tv_SetSourceSwitchInput(int window_sel, tvin_source_input_t source_input, tvin_audio_channel_t audio_channel)
{
    ALOGD("Enter %s.", __FUNCTION__);

    // This is a workaround for a ATV H/W problem: there's background noise when
    // the audio volume is high.
    if (Tvin_GetTvProductId() == TV_PRODUCT_H02REF) {
        if (source_input == SOURCE_TV) {
            ForceEqGain(4, CC_MIN_EQ_GAIN_VAL);
        } else if (Tvin_GetSrcInputType() == SOURCE_TYPE_TV) {
            ForceEqGain(4, INVALID_HPEQ_FORCED_GAIN);
        }
    }

    SetAudioVolumeCompensationVal(0);
    if ((mTvStartedFlag != 1 || mTvStopFlag != 0) && source_input != SOURCE_DTV) {
        if (mTvStartedFlag != 1 && mTvStopFlag != 1) {
            if(window_sel == PREVIEW_WONDOW)
                Tv_Start(TVIN_STATUS_PREVIEW_START);
            else if(window_sel == NORMAL_WONDOW)
                Tv_Start(TVIN_STATUS_NORMAL_START);
            ALOGD("%s, tv is not started[%d], start now!", __FUNCTION__, mTvStartedFlag);
        } else {
            ALOGW("%s, tv is stoppig[%d], you can not set source switch input!", __FUNCTION__, mTvStopFlag);
            return -1;
        }
    }

    if ((source_input < SOURCE_TV || source_input >= SOURCE_MAX) && (source_input != SOURCE_MPEG)) {
        ALOGW("%s, it is not a tvin input!", __FUNCTION__);
        return -1;
    } else if (source_input == SOURCE_MPEG && (gTvinApiPara.tvin_status == TVIN_STATUS_NORMAL_START)) {
        ALOGW("%s, it is not a tvin input [%d]!", __FUNCTION__, source_input);
        Tv_Stop();
        //sendMessage
        //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
        ALOGD("Exit %s.", __FUNCTION__);
        return -1;
    }

    Tvin_SuspendSourceDetect(true);//when source switch suspend source detect first
    Tvin_StopSigDetect();
    Tvin_StopSourceSwitch();

    if (source_input == SOURCE_TV || source_input == SOURCE_DTV) {
        if (source_input == SOURCE_TV) {
            //Tvin_SetSrcPort(TVIN_PORT_CVBS0);
            //Tvin_SetSourceInput(TVIN_PORT_CVBS0);
            //SSMSaveSourceInput(SOURCE_TV);
            //SSMSaveLastSelectSourceType(SOURCE_TV);
        } else if (source_input == SOURCE_DTV) {
            //we should set dtv's volume digit lut buffer and set volume.
            AudioSetVolumeDigitLUTBuf(CC_LUT_SEL_MPEG, 1, 1, -1);
            SetAudioMasterVolume (GetAudioMasterVolume());

            Tv_Stop();
            Tvin_SetSrcPort(TVIN_PORT_DTV);
            Tvin_SetSourceInput(TVIN_PORT_DTV);
            SSMSaveSourceInput(SOURCE_DTV);
            SSMSaveLastSelectSourceType(SOURCE_DTV);
            if (Tvin_GetTvProductId() == TV_PRODUCT_E16REF ||
                Tvin_GetTvProductId() == TV_PRODUCT_H02REF) {
                property_set("hw.videohole.x", "-1");
                property_set("hw.videohole.y", "-1");
                property_set("hw.videohole.width", "-1");
                property_set("hw.videohole.height", "-1");
            }
            //sendMessage
            //android::TvService::getIntance()->SendSourceSwitchState(Tvin_GetSourceInput(), 0);
            ALOGD("%s, set DTV source input!", __FUNCTION__);


            ALOGD("Exit %s.", __FUNCTION__);
            return 0;
        }
        if (Tvin_GetTvProductId() == TV_PRODUCT_E12REF || Tvin_GetTvProductId() == TV_PRODUCT_E13REF) {
            if(SSMReadLocalDimingOnOffFlg()==1) {
                Vpp_SetLocalDimingOnoff(1);
                ALOGD("%s, Turn On DTV Local-diming when full-screen", __FUNCTION__);
            }
        }
        if (gTvinConfig.preview_freescale == 0x55) {
            TvinApi_DisableFreeScale(1);
        }
        ALOGD("%s, set TV[%d] source input!", __FUNCTION__, source_input);
    }

    gTvinSrcSwitchPara.port = Tvin_GetSourcePortBySourceInput(source_input);
    gTvinSrcSwitchPara.input_window = 0;
    gTvinSrcSwitchPara.audio_channel = audio_channel;
    Tvin_StartSourceSwitch();
    ALOGD("Exit %s.", __FUNCTION__);
    return 0;
}

tvin_port_t Tv_GetSourceSwitchInput(void)
{
    return gTvinSrcSwitchPara.port;
}

tvin_audio_channel_t Tv_GetSourceSwitchAudioChannel(void)
{
    return gTvinSrcSwitchPara.audio_channel;
}

int Tv_IsSourceSwtichDone(void)
{
    return (Tvin_IsSrcSwitchExecDone() ? 1 : 0);
}

tvin_source_input_t Tv_GetCurrentSourceInput(void)
{
    return Tvin_GetSourceInput();
}

tvin_source_input_t Tv_GetLastSourceInput(void)
{
    return Tvin_GetLastSourceInput();
}

tvin_info_t Tv_GetCurrentSignalInfo(void)
{
    return Tvin_GetSigInfo();
}

tvin_status_t Tv_GetTvStatus(void)
{
    return Tvin_GetTvinStatus();
}

int Tv_Set3DMode(tvin_3d_mode_t mode)
{
    if(Tv_Set3DModeWithoutSave(mode)<0) {
        ALOGW("%s,Tv_Set3DModeWithoutSave fail !", __FUNCTION__);
        return -1;
    } else {
        if(Tv_Save3DMode(mode)<0) {
            ALOGW("%s,Tv_Save3DModeWithoutSave fail !", __FUNCTION__);
            return -1;
        } else {
            ALOGW("%s,Tv_Save3DModeWithoutSave success !", __FUNCTION__);
            return 0;
        }
    }
}
int Tv_Set3DModeWithoutSave(tvin_3d_mode_t mode)
{
    if (Tvin_GetTvinStatus() == TVIN_STATUS_PREVIEW_START) {
        ALOGW("%s, you can not set 3D mode when 3D functionwithoutssm disabled!", __FUNCTION__);
        return -1;
    }
    return Tvin_Set3DFunction(mode);
}
int Tv_Save3DMode(tvin_3d_mode_t mode)
{
    tvin_3d_mode_t pre_mode = gTvinApiPara.mode_3d;

    if (pre_mode == mode || Tvin_GetSrcInputType() == SOURCE_TYPE_VGA || Tvin_GetSrcInputType() == SOURCE_TYPE_DTV) {
        ALOGW("%s, Set3DMode faild, pre_mode:%d, mode:%d, source_type:%d.", __FUNCTION__, pre_mode, mode, Tvin_GetSrcInputType());
        return -1;
    }
    Tvin_3DSTATUS_SSM((tvin_3d_status_t)mode);
    return 0;
}


tvin_3d_mode_t Tv_Get3DMode(void)
{
    int val = 0;
    SSMRead3DMode(&val);
    if (val < MODE3D_DISABLE || val > MODE3D_BT)
        val = MODE3D_DISABLE;
    return (tvin_3d_mode_t)val;
}
int Tv_Set3DLRSwith(int on_off, tvin_3d_status_t status)
{
    tvin_3d_mode_t mode;
    if (on_off) {
        if(status == STATUS3D_BT) {
            mode = MODE3D_ON_LR_SWITCH_BT;
        } else {
            mode = MODE3D_ON_LR_SWITCH;
        }
    } else {
        if(status == STATUS3D_BT) {
            mode = MODE3D_OFF_LR_SWITCH_BT;
        } else {
            mode = MODE3D_OFF_LR_SWITCH;
        }
    }
    if(Tvin_Set3DFunction(mode)<0) {
        ALOGW("%s,Set3Dfunction fail!!! ", __FUNCTION__);
        return -1;
    } else {
        ALOGW("%s,Set3Dfunction success!!! ", __FUNCTION__);
        Tvin_3DMODE_SSM((tvin_3d_mode_t)mode);
        return 0;
    }
}
int Tv_Get3DLRSwith(void)
{
    int val = 0;
    SSMRead3DLRSwitch(&val);
    if (val < 0 || val > 2)
        val = 0;
    return val;
}
int Tv_Set3DTo2DMode(int mode, tvin_3d_status_t status)
{
    if(Tv_Set3DTo2DModeWithoutSave(mode, status)<0) {
        ALOGW("%s,Set3DTo2DModeWithoutSave fail!!! ", __FUNCTION__);
        return -1;
    } else {
        ALOGW("%s,Set3DTo2DModeWithoutSave success!!! ", __FUNCTION__);
        Tv_Save3DTo2DMode(mode, status);
        return 0;
    }

}
int Tv_Set3DTo2DModeWithoutSave(int mode, tvin_3d_status_t status)
{
    switch (mode) {
    case 1: //L
        if (status == STATUS3D_BT)
            mode = MODE3D_L_3D_TO_2D_BT;
        else
            mode = MODE3D_L_3D_TO_2D;
        break;
    case 2: //R
        if (status == STATUS3D_BT)
            mode = MODE3D_R_3D_TO_2D_BT;
        else
            mode = MODE3D_R_3D_TO_2D;
        break;
    case 0: //Disalbe
    default:
        if (status == STATUS3D_BT)
            mode = MODE3D_OFF_3D_TO_2D_BT;
        else
            mode = MODE3D_OFF_3D_TO_2D;
        break;
    }
    return Tvin_Set3DFunction((tvin_3d_mode_t)mode);
}

int Tv_Save3DTo2DMode(int mode, tvin_3d_status_t status)
{
    switch (mode) {
    case 1: //L
        if (status == STATUS3D_BT)
            mode = MODE3D_L_3D_TO_2D_BT;
        else
            mode = MODE3D_L_3D_TO_2D;
        break;
    case 0: //Disalbe
    default:
        if (status == STATUS3D_BT)
            mode = MODE3D_OFF_3D_TO_2D_BT;
        else
            mode = MODE3D_OFF_3D_TO_2D;
        break;
    }
    Tvin_3DMODE_SSM((tvin_3d_mode_t)mode);
    return 0;
}


int Tv_Get3DTo2DMode(void)
{
    int val = 0;
    SSMRead3DTO2D(&val);
    if (val < 0 || val > 2)
        val = 0;
    return val;
}

int Tv_Set3DTo2DModeforh03(int mode)
{
    Mode3D2Dforh03 = (tvin_3d_2d_for_h03)mode;
    return 0;
}

int Tv_Get3DTo2DModeforh03()
{
    return (int)Mode3D2Dforh03;
}

int Tv_Set3DDepth(int value)
{
    Tv_Set3DDepthWithoutSave(value);
    Tv_Save3DDepth(value);
    return 0;
}

int Tv_Set3DDepthWithoutSave(int value)
{
    if( Tvin_GetTvProductId() == TV_PRODUCT_H02REF ) {
        // -16~16
        if (value > 16)
            value = 16;
        else if (value < -16)
            value = -16;
    }
    Tvin_SetDepthOf2Dto3D(value);
    return 0;
}

int Tv_Save3DDepth(int value)
{
    if(Tvin_GetTvProductId() == TV_PRODUCT_H02REF) {
        // -16~16
        if (value > 16)
            value = 16;
        else if (value < -16)
            value = -16;
    }
    SSMSave3DDepth(value);
    return 0;
}


int Tv_Get3DDepth(void)
{
    int val = 0;
    SSMRead3DDepth(&val);
    if(Tvin_GetTvProductId() == TV_PRODUCT_H02REF) {
        if (val > 16)
            val -= 256;
        if (val < -16)
            val = 0;
    }
    return val;
}

int Tv_Set3DAutoDectect(int on_off)
{
    return  TvinApi_SetDI3DDetc(on_off);
}

int Tv_Get3DAutoDectect()
{
    return    TvinApi_GetDI3DDetc();
}

int Tv_Update3DAutoDectect(int on_off)
{
    auto_detect_3d =on_off;
    return 0;
}

int Tv_RunVGAAutoAdjust(void)
{
    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv first!", __FUNCTION__);
        return -1;
    }
    if (TvinGetVagAutoAdjustStatus() != TV_VGA_AUTO_ADJUST_STATUS_STOP) {
        ALOGW("%s, vga is running auto adjust, please do not run again!", __FUNCTION__);
        return -1;
    }

    TvinSetVgaAutoAdjustForceStart(1);
    TvinSetVagAutoAdjustStatus(TV_VGA_AUTO_ADJUST_STATUS_START);
    return 0;
}

int Tv_GetCurrentVGAAjustPara(tvafe_vga_parm_t *adjparam)
{
    if (adjparam == NULL)
        return -1;
    return TvinApi_GetVGACurTimingAdj(adjparam);
}

int Tv_GetVGAAjustPara(tvafe_vga_parm_t *adjparam, tvin_sig_fmt_t vga_fmt)
{
    unsigned char buff[4];
    if (adjparam == NULL)
        return -1;
    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    if (SSMReadVGAAdjustValue((vga_fmt-1)*4, 4, buff)) {
        return -1;
    }

    adjparam->clk_step = buff[0];
    if (adjparam->clk_step < 0)
        adjparam->clk_step = 0;
    else if (adjparam->clk_step > 100)
        adjparam->clk_step = 100;
    adjparam->clk_step -= 50;

    adjparam->phase = (unsigned short)(buff[1]&0x7f);
    if (adjparam->phase > 100)
        adjparam->phase = 100;

    adjparam->hpos_step = buff[2];
    if (adjparam->hpos_step < 0)
        adjparam->hpos_step = 0;
    else if (adjparam->hpos_step > 100)
        adjparam->hpos_step = 100;
    adjparam->hpos_step -= 50;

    adjparam->vpos_step = buff[3];
    if (adjparam->vpos_step < 0)
        adjparam->vpos_step = 0;
    else if (adjparam->vpos_step > 100)
        adjparam->vpos_step = 100;
    adjparam->vpos_step -= 50;

    ALOGD("%s, ClockData[%d], PhaseData[%d], H-PosData[%d], V-PosData[%d].\n", __FUNCTION__, buff[0], buff[1]&0x7f, buff[2], buff[3]);
    ALOGD("%s, Clock[%d], Phase[%d], H-Pos[%d], V-Pos[%d].\n", __FUNCTION__, adjparam->clk_step, adjparam->phase, adjparam->hpos_step, adjparam->vpos_step);

    return 0;
}

tvafe_vga_parm_t Tv_AdjustParmSet(tvafe_vga_parm_t adjparam, unsigned char buff[])
{

    if (adjparam.clk_step < -50)
        adjparam.clk_step = -50;
    else if (adjparam.clk_step > 50)
        adjparam.clk_step = 50;
    buff[0] = (unsigned char)(adjparam.clk_step+50);

    if (adjparam.phase > 100)
        adjparam.phase = 100;
    buff[1] = (unsigned char)(adjparam.phase|0x80);

    if (adjparam.hpos_step < -50)
        adjparam.hpos_step = -50;
    else if (adjparam.hpos_step > 50)
        adjparam.hpos_step = 50;

    buff[2] = (unsigned char)(adjparam.hpos_step+50);

    if (adjparam.vpos_step < -50)
        adjparam.vpos_step = -50;
    else if (adjparam.vpos_step > 50)
        adjparam.vpos_step = 50;

    buff[3] = (unsigned char)(adjparam.vpos_step+50);

    ALOGD("%s, Clock[%d], Phase[%d], H-Pos[%d], V-Pos[%d].\n", __FUNCTION__, adjparam.clk_step, adjparam.phase, adjparam.hpos_step, adjparam.vpos_step);
    ALOGD("%s, ClockData[%d], PhaseData[%d], H-PosData[%d], V-PosData[%d].\n", __FUNCTION__, buff[0], buff[1]&0x7f, buff[2], buff[3]);

    return adjparam;
}

int Tv_SetVGAAjustPara(tvafe_vga_parm_t adjparam, tvin_sig_fmt_t vga_fmt)
{
    unsigned char buff[4];
    tvafe_vga_parm_t adjparamAfter;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    adjparamAfter = Tv_AdjustParmSet(adjparam, buff);

    if (TvinApi_SetVGACurTimingAdj(adjparamAfter) < 0)
        return -1;

    return SSMSaveVGAAdjustValue((vga_fmt-1)*4, 4, buff);
}

int Tv_SetVGAHPosWithoutSave(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.hpos_step = 50 - value;

    adjparamAfter = Tv_AdjustParmSet(adjparam, buff);

    return TvinApi_SetVGACurTimingAdj(adjparamAfter);
}

int Tv_SaveVGAHPos(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    adjparam.hpos_step = 50 - value;
    Tv_AdjustParmSet(adjparam, buff);

    return SSMSaveVGAAdjustValue((vga_fmt-1)*4, 4, buff);
}

int Tv_SetVGAHPos(int value, tvin_sig_fmt_t vga_fmt)
{
    if (0 == Tv_SetVGAHPosWithoutSave(value, vga_fmt)) {
        return Tv_SaveVGAHPos(value, vga_fmt);
    }
    ALOGE("Tv_SetVGAHPosWithoutSave() Failed !!!");
    return -1;
}

int Tv_GetVGAHPos(tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0) {
        return -1;
    }

    if (adjparam.hpos_step < -50)
        adjparam.hpos_step = -50;
    else if (adjparam.hpos_step > 50)
        adjparam.hpos_step = 50;

    return (50-adjparam.hpos_step);
}

int Tv_SetVGAVPos(int value, tvin_sig_fmt_t vga_fmt)
{
    if (0 == Tv_SetVGAVPosWithoutSave(value, vga_fmt)) {
        return Tv_SaveVGAVPos(value, vga_fmt);
    }
    ALOGE("Tv_SetVGAVPosWithoutSave() Failed !!!");
    return -1;
}

int Tv_GetVGAVPos(tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0) {
        return -1;
    }

    if (adjparam.vpos_step < -50)
        adjparam.vpos_step = -50;
    else if (adjparam.vpos_step > 50)
        adjparam.vpos_step = 50;

    return (adjparam.vpos_step+50);
}

int Tv_SetVGAVPosWithoutSave(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.vpos_step = value - 50;

    adjparamAfter = Tv_AdjustParmSet(adjparam, buff);

    return TvinApi_SetVGACurTimingAdj(adjparamAfter);
}

int Tv_SaveVGAVPos(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.vpos_step = value - 50;

    Tv_AdjustParmSet(adjparam, buff);

    return SSMSaveVGAAdjustValue((vga_fmt-1)*4, 4, buff);
}

int Tv_SetVGAClock(int value, tvin_sig_fmt_t vga_fmt)
{
    if (0 == Tv_SetVGAClockWithoutSave(value,vga_fmt)) {
        return Tv_SaveVGAClock(value, vga_fmt);
    }
    ALOGE("Tv_SetVGAClockWithoutSave() Failed !!!");
    return -1;
}

int Tv_GetVGAClock(tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0) {
        return -1;
    }
    if (adjparam.clk_step < -50)
        adjparam.clk_step = -50;
    else if (adjparam.clk_step > 50)
        adjparam.clk_step = 50;
    return (adjparam.clk_step+50);
}

int Tv_SetVGAClockWithoutSave(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.clk_step = value - 50;

    adjparamAfter = Tv_AdjustParmSet(adjparam,  buff);

    return TvinApi_SetVGACurTimingAdj(adjparamAfter);
}

int Tv_SaveVGAClock(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.clk_step = value - 50;

    adjparamAfter = Tv_AdjustParmSet(adjparam,  buff);

    return SSMSaveVGAAdjustValue((vga_fmt-1)*4, 4, buff);

}
int Tv_SetVGAPhase(int value, tvin_sig_fmt_t vga_fmt)
{
    if(0 == Tv_SetVGAPhaseWithoutSave(value, vga_fmt)) {
        return Tv_SaveVGAPhase(value, vga_fmt);
    }
    ALOGE("Tv_SetVGAPhaseWithoutSave() Failed!!!");
    return -1;
}

int Tv_GetVGAPhase(tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0) {
        return -1;
    }

    return adjparam.phase;
}

int Tv_SetVGAPhaseWithoutSave(int value, tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam, adjparamAfter;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.phase = value;
    adjparamAfter = Tv_AdjustParmSet(adjparam, buff);

    return TvinApi_SetVGACurTimingAdj(adjparamAfter);
}
int Tv_SaveVGAPhase(int value,tvin_sig_fmt_t vga_fmt)
{
    tvafe_vga_parm_t adjparam;
    unsigned char buff[4];

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and switch to vga first!", __FUNCTION__);
        return -1;
    }

    if (value < 0)
        value = 0;
    else if (value > 100)
        value = 100;

    if (Tv_GetVGAAjustPara(&adjparam, vga_fmt) != 0)
        return -1;

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    adjparam.phase = value;
    Tv_AdjustParmSet(adjparam, buff);

    return SSMSaveVGAAdjustValue((vga_fmt-1)*4, 4, buff);
}

int Tv_IsVGAAutoAdjustDone(tvin_sig_fmt_t vga_fmt)
{
    int is_done = 0, index = 0;

    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv and run vga auto adjust first!", __FUNCTION__);
        return -1;
    }

    if (vga_fmt <= TVIN_SIG_FMT_NULL || vga_fmt >= TVIN_SIG_FMT_VGA_MAX)
        return -1;
    index = (int) vga_fmt;
    if (SSMReadVGAAdjustValue((index-1)*4+1, 1, &is_done) == 0) {
        ALOGD("%s, is_done = [%d].\n", __FUNCTION__, is_done);
        return ((is_done&0x80)?1:0);
    }
    return -1;
}

int Tv_RunADCAutoCalibration(void)
{
    if (mTvStartedFlag != 1) {
        ALOGW("%s, please start tv first!", __FUNCTION__);
        return -1;
    }
    return TvinADCAutoCalibration_CreateThread();
}

int Tv_IsADCAutoCalibrationDone(void)
{
    return 0;
}

tvin_vga_auto_adjust_status_t Tv_GetVagAutoAdjustStatus(void)
{
    return TvinGetVagAutoAdjustStatus();
}

int Tv_FactorySetOverscan(tvin_source_input_type_t source_type, tvin_sig_fmt_t fmt, tvin_3d_status_t status, tvin_trans_fmt_t trans_fmt, tvin_cutwin_t cutwin_t)
{
    int ret = -1;
    if (source_type < SOURCE_TYPE_TV || source_type >= SOURCE_TYPE_MAX)
        return ret;
    if (fmt < TVIN_SIG_FMT_NULL || fmt >= TVIN_SIG_FMT_MAX)
        return ret;
    if (trans_fmt < TVIN_TFMT_2D || trans_fmt > TVIN_TFMT_3D_LDGD)
        return ret;
    ALOGD("%s, source_type[%d], fmt[%d], status[%d], trans_fmt[%d], cutwin_t.hs[%d], cutwin_t.he[%d], cutwin_t.vs[%d], cutwin_t.ve[%d].\n",
          __FUNCTION__, source_type, fmt, status, trans_fmt, cutwin_t.hs, cutwin_t.he, cutwin_t.vs, cutwin_t.ve);
#ifdef PQ_ENABLE_DB
    status = STATUS3D_DISABLE;
    trans_fmt = TVIN_TFMT_2D;
    ret = PQ_SetOverscanParams(source_type, fmt, status, trans_fmt, cutwin_t);
    if (ret == 0) {
        ALOGD("%s, PQ_SetOverscanParams sucess.\n", __FUNCTION__);
    } else {
        ALOGW("%s, PQ_SetOverscanParams fail.\n", __FUNCTION__);
    }
#endif
    if(gTvinConfig.new_overscan == 0x55) {
        Vpp_SetVppVideoCrop((int)cutwin_t.vs, (int)cutwin_t.hs, (int)cutwin_t.ve, (int)cutwin_t.he);
    }

    return ret;
}

tvin_cutwin_t Tv_FactoryGetOverscan(tvin_source_input_type_t source_type, tvin_sig_fmt_t fmt, tvin_3d_status_t status, tvin_trans_fmt_t trans_fmt)
{
    int ret = -1;
    tvin_cutwin_t cutwin_t;
    memset(&cutwin_t, 0, sizeof(cutwin_t));

    if (source_type < SOURCE_TYPE_TV || source_type >= SOURCE_TYPE_MAX)
        return cutwin_t;
    if (fmt < TVIN_SIG_FMT_NULL || fmt >= TVIN_SIG_FMT_MAX)
        return cutwin_t;
    if (trans_fmt < TVIN_TFMT_2D || trans_fmt > TVIN_TFMT_3D_LDGD)
        return cutwin_t;
#ifdef PQ_ENABLE_DB
    ret = PQ_GetOverscanParams(source_type, fmt, status, trans_fmt, VPP_DISPLAY_MODE_169, &cutwin_t);
    if (ret == 0) {
        ALOGD("%s, source_type[%d], fmt[%d], status[%d], trans_fmt[%d], cutwin_t.hs[%d], cutwin_t.he[%d], cutwin_t.vs[%d], cutwin_t.ve[%d].\n",
              __FUNCTION__, source_type, fmt, status, trans_fmt, cutwin_t.hs, cutwin_t.he, cutwin_t.vs, cutwin_t.ve);
    } else {
        ALOGW("%s, PQ_GetOverscanParams faild.\n", __FUNCTION__);
    }
#endif
    return cutwin_t;
}

int Tv_SetCVBSStd(tvin_color_system_e std_val)
{
    int ret = -1;
    if (std_val >= COLOR_SYSTEM_AUTO && std_val < COLOR_SYSTEM_MAX) {
        if (TvinApi_SetCVBSStd(std_val) >= 0) {
            Tvin_TurnOnBlueScreen(2);
            Tvin_StopSigDetect();
            usleep(20 * 1000);;
            Tvin_StopDecoder(0);
            usleep(20 * 1000);;
            Tvin_StartSigDetect();
            if (SSMSaveCVBSStd((int) std_val) < 0) {
                ALOGW("%s, write std value to ssm error!\n", __FUNCTION__);
            } else {
                ALOGD("%s, set std_val = [%d].\n", __FUNCTION__, (int)std_val);
                return 0;
            }
        }
    }
    ALOGW("%s, set std_val[%d] faild!\n", __FUNCTION__, (int)std_val);
    return -1;
}

tvin_color_system_e Tv_GetCVBSStd()
{
    int std_val = 0;

    if (SSMReadCVBSStd(&std_val) < 0) {
        ALOGW("%s, read std value from ssm error, return auto default!\n", __FUNCTION__);
        return COLOR_SYSTEM_AUTO;
    }

    if (std_val >= COLOR_SYSTEM_AUTO && std_val < COLOR_SYSTEM_MAX) {
        ALOGD("%s, get std_val = [%d].\n", __FUNCTION__, (int) std_val);
        return (tvin_color_system_e) std_val;
    }

    ALOGW("%s, get std_val[%d] is overflow, return auto default!\n", __FUNCTION__, (int) std_val);
    return COLOR_SYSTEM_AUTO;
}

int Tv_KeepLastFrame(int enable)
{
    if (Tvin_GetSourceInput() == SOURCE_TV) {
        return TvinApi_KeepLastFrame(enable);
    } else {
        ALOGW("%s, you can not keep last frame except TV input!\n", __FUNCTION__);
    }
    return -1;
}

int Tv_SetMpegFlag(int is_mpeg)
{
    gTvinApiPara.is_mpeg = is_mpeg;
    return 0;
}
int Tv_GetMpegPortxScaleOffset(void)
{
    return gTvinApiPara.mpeg_port_xscale_offset;
}

void Tv_SetMpegPortxScaleOffset(int offset)
{
    gTvinApiPara.mpeg_port_xscale_offset = offset;
}

int Tv_GetMpegPortyScaleOffset(void)
{
    return gTvinApiPara.mpeg_port_yscale_offset;
}

void Tv_SetMpegPortyScaleOffset(int offset)
{
    gTvinApiPara.mpeg_port_yscale_offset = offset;
}

int Tv_SetTvinParamDefault(void)
{
    ALOGE("Enter Tv_SetTvinParamDefault!!!");
    int ret = -1;
    ret = TvinResetVgaAjustParam();
    ret |= TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
    ret |= TvinADCAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_VGA, ADCGainOffset_VGA);
    ret |= TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
    ret |= TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR1, ADCGainOffset_Ypbpr1);
    ret |= TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
    ret |= TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR2, ADCGainOffset_Ypbpr2);
    ret |= SSMSave3DMode(STATUS3D_DISABLE);
    ret |= SSMSave3DLRSwitch(0);
    ret |= SSMSave3DTO2D(0);
    ret |= SSMSave3DDepth(0);
    ret |= SSMSaveDisable3D(0);
    return ret;
}

int TvinSSMRestoreDefault()
{
    TvinResetVgaAjustParam();
    TvinADCGainOffsetInit(ADC_CALIBRATION_INPUT_VGA, &ADCGainOffset_VGA);
    TvinADCAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_VGA, ADCGainOffset_VGA);
    TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR1, &ADCGainOffset_Ypbpr1);
    TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR1, ADCGainOffset_Ypbpr1);
    TvinADCYPbPrGainOffsetInit(ADC_CALIBRATION_INPUT_YPBPR2, &ADCGainOffset_Ypbpr2);
    TvinADCYPbPrAutoCalibration_SavePara(ADC_CALIBRATION_INPUT_YPBPR2, ADCGainOffset_Ypbpr2);
    SSMSaveCVBSStd(0);
    SSMSaveLastSelectSourceType(SOURCE_AV1);
    SSMSavePanelType(0);
    return 0;
}


void GetConsoleStatus(void)
{
    char prop_value[PROPERTY_VALUE_MAX];

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ubootenv.var.console_debug",prop_value,"NULL");

    ALOGD("serialcmd = %s",prop_value);
    if (strcmp(prop_value, "ttyS0,115200n8") == 0) {
        SSMSaveSerialOnOffFlg(0);
    } else {
        SSMReadSerialOnOffFlg();
    }
}


int GetSkyModuleName(void)
{
    char prop_value[PROPERTY_VALUE_MAX];
    int ret = 0;

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("ro.build.skytype",prop_value,"E580");

    ALOGD("sky module = %s",prop_value);
    if (strcmp(prop_value, "E580") == 0) {
        ret = 1;
    } else if (strcmp(prop_value, "E750B") == 0)
        ret = 2;
    else
        ret = 0;

    return ret;
}

