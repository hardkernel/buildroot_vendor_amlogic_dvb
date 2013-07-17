#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include <jni.h>
#include <cutils/properties.h>
#include <cutils/log.h>
#include <sys/mman.h>
#include "tvin_api.h"
#include <linux/tvin/tvin.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "TvinApi"
#endif

#define ALOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__); 
#define ALOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGW(...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__);
 
#define LOGD_VDIN
//#define LOGD_VDIN_START_DEC
//#define LOGD_VDIN_STOP_DEC
//#define LOGD_VDIN_GET_SIGNAL_INFO
//#define LOGD_VDIN_SET_SIGNAL_PARAM
//#define LOGD_VDIN_GET_SIGNAL_PARAM
//#define LOGD_3D_FUNCTION

#define LOGD_AFE
//#define LOGD_AFE_EDID
//#define LOGD_AFE_ADC_AUTO_ADJ
//#define LOGD_AFE_ADC_ADJUSTMENT
//#define LOGD_AFE_ADC_CALIBRATION

#define VDIN_0_DEV_PATH "/dev/vdin0"
#define VDIN_1_DEV_PATH "/dev/vdin1"
#define AFE_DEV_PATH    "/dev/tvafe0"

#define CC_SEL_VDIN_DEV_0   (0)
#define CC_SEL_VDIN_DEV_1   (1)

#define JNI_RETURN(x)           JNIEXPORT x JNICALL
#define JNI_PARAM               JNIEnv* env, jobject thiz

#define PPMGR_IOC_MAGIC         'P'
#define PPMGR_IOC_2OSD0         _IOW(PPMGR_IOC_MAGIC, 0x00, unsigned int)
#define PPMGR_IOC_ENABLE_PP     _IOW(PPMGR_IOC_MAGIC, 0X01, unsigned int)
#define PPMGR_IOC_CONFIG_FRAME  _IOW(PPMGR_IOC_MAGIC, 0X02, unsigned int)
#define PPMGR_IOC_VIEW_MODE     _IOW(PPMGR_IOC_MAGIC, 0X03, unsigned int)

#define MODE_3D_DISABLE         0x00000000
#define MODE_3D_ENABLE          0x00000001
#define MODE_AUTO               0x00000002
#define MODE_2D_TO_3D           0x00000004
#define MODE_LR                 0x00000008
#define MODE_BT                 0x00000010
#define MODE_LR_SWITCH          0x00000020
#define MODE_FIELD_DEPTH        0x00000040
#define MODE_3D_TO_2D_L         0x00000080
#define MODE_3D_TO_2D_R         0x00000100
#define LR_FORMAT_INDICATOR     0x00000200
#define BT_FORMAT_INDICATOR     0x00000400

/* ADC calibration pattern & format define */
/* default 100% 8 color-bar */
//#define VGA_SOURCE_RGB444
#define VGA_H_ACTIVE    (1024)
#define VGA_V_ACTIVE    (768)
#define COMP_H_ACTIVE   (1280)
#define COMP_V_ACTIVE   (720)
#define CVBS_H_ACTIVE   (720)
#define CVBS_V_ACTIVE   (480)

#define  FBIOPUT_OSD_FREE_SCALE_ENABLE     0x4504
#define  FBIOPUT_OSD_FREE_SCALE_WIDTH      0x4505
#define  FBIOPUT_OSD_FREE_SCALE_HEIGHT  0x4506

typedef enum {
    VIEWMODE_NULL = 0,
    VIEWMODE_4_3,
    VIEWMODE_16_9
} view_mode_t;

 int vdin0_dev_fd = -1;
 int vdin1_dev_fd = -1;
 int afe_dev_fd = -1;
 int ppmgr_dev_fd = -1;

 pthread_mutex_t vdin_video_path_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_dev_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_afe_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_ppmgr_op_mutex = PTHREAD_MUTEX_INITIALIZER;

 pthread_mutex_t vdin_vscaler_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_display_mode_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_ppmgr_depth_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_ppmgr_axis_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_ppmgr_platform_type_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_ppmgr_view_mode_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_mvc_view_mode_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_buf_mgr_mode_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_det3d_en_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_det3d_get_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_config_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_black_bar_en_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_hdmi_edid_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_keep_out_frame_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_black_out_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_video_freeze_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_bypasshd_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_bypassall_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_di_bypasspost_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t aml_debug_reg_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_hdmi_eq_config_default_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_flag_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t rdma_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t new_d2d3_op_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_mutex_t vdin_source_detect_mutex = PTHREAD_MUTEX_INITIALIZER;

 pthread_mutex_t vdin_comp_phase_op_mutex = PTHREAD_MUTEX_INITIALIZER;

 tvin_parm_t gTvinVDINParam;
 tvin_info_t gTvinVDINSignalInfo;
 tvin_parm_t gTvinAFEParam;
 tvin_info_t gTvinAFESignalInfo;

int SetFileAttrValue(const char *fp, const char value[])
{
    int fd = -1;

    fd = open(fp,O_RDWR);

    if (fd < 0) {
        ALOGE("open %s ERROR(%s)!!\n", fp, strerror(errno));
        return -1;
    }

    return write(fd, value, strlen(value));
}

//VDIN
 int VDIN_AddVideoPath(const char *videopath)
{
    FILE *fp = NULL;
    int ret = -1;

    pthread_mutex_lock(&vdin_video_path_op_mutex);

    fp = fopen("/sys/class/vfm/map", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/vfm/map error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_video_path_op_mutex);
        return -1;
    }

    ret = fprintf(fp, "%s", videopath);
    if (ret < 0) {
        ALOGW("Add VideoPath error(%s)!\n", strerror(errno));
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_video_path_op_mutex);
    return ret;
}

 int VDIN_RmDefPath(void)
{
    int fd = -1, ret;
    char str[] = "rm default";

    pthread_mutex_lock(&vdin_video_path_op_mutex);

    fd = open("/sys/class/vfm/map", O_RDWR);

    if (fd < 0) {
        ALOGW("Open /sys/class/vfm/map error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_video_path_op_mutex);
        return -1;
    }

    ret = write(fd, str, sizeof(str));
    if (ret < 0) {
        ALOGW("Rm default path error(%s)!\n", strerror(errno));
    }

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_video_path_op_mutex);
    return ret;
}

 int VDIN_RmTvPath(void)
{
    int fd, ret;
    char str[] = "rm tvpath";

    pthread_mutex_lock(&vdin_video_path_op_mutex);

    fd = open("/sys/class/vfm/map", O_RDWR);
    if (fd < 0) {
        ALOGW("Open /sys/class/vfm/map error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_video_path_op_mutex);
        return -1;
    }

    ret = write(fd, str, sizeof(str));
    if (ret < 0) {
        ALOGW("Rm tv path error(%s)!\n", strerror(errno));
    }

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_video_path_op_mutex);
    return ret;
}

 int VDIN_AddTvPath(int selPath)
{
    int ret = -1;
    char prop_value[PROPERTY_VALUE_MAX];
    int no_di_path = 0;

   
    switch (selPath) {
    case 0:
        ret = VDIN_AddVideoPath("add tvpath vdin0 amvideo");
        break;
    case 1:
        if (no_di_path == 1) {
            ret = VDIN_AddVideoPath("add tvpath vdin0 amvideo");
        } else {
            ret = VDIN_AddVideoPath("add tvpath vdin0 deinterlace amvideo");
        }
        break;
    case 2:
        ret = VDIN_AddVideoPath("add tvpath vdin0 ppmgr amvideo");
        break;
    case 3:
        ret = VDIN_AddVideoPath("add tvpath vdin0 deinterlace ppmgr d2d3 amvideo");
        break;
    case 4:
        ret = VDIN_AddVideoPath("add default decoder ppmgr deinterlace amvideo");
        break;
    case 5:
        ret = VDIN_AddVideoPath("add default decoder deinterlace amvideo");
        break;
    case 6:
        ret = VDIN_AddVideoPath("add previewpath vdin0 freescale amvideo");
        break;
    case 7:
        ret = VDIN_AddVideoPath("add default decoder deinterlace ppmgr d2d3 amvideo");
        break;
    }
    return ret;
}

 int VDIN_RmPreviewPath(void)
{
    int fd, ret;
    char str[] = "rm previewpath";

    pthread_mutex_lock(&vdin_video_path_op_mutex);

    fd = open("/sys/class/vfm/map", O_RDWR);
    if (fd < 0) {
        ALOGW("Open /sys/class/vfm/map error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_video_path_op_mutex);
        return -1;
    }

    ret = write(fd, str, sizeof(str));
    if (ret < 0) {
        ALOGW("Rm tv path error(%s)!\n", strerror(errno));
    }

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_video_path_op_mutex);
    return ret;
}

int EnableFreeScale(const int osdWidth, const int osdHeight, const int previewX0, const int previewY0, const int previewX1, const int previewY1)
{
    int fd0 = -1, fd1 = -1;
    int fd_daxis = -1, fd_vaxis = -1;
    int fd_freescale = -1,fd_freescale_rect = -1 ,fd_freescale_disp = -1;
    int fd_fs_axis = -1;
    int fd_video = -1;
    int fd0_osdx2 = -1, fd1_osdx2 = -1;
    int ret = -1;
    int temp = 0;
    char set_str[32];

    //================================
    //  osd
    //================================
    if((fd0 = open("/dev/graphics/fb0", O_RDWR)) < 0) {
        ALOGW("open /dev/graphics/fb0 fail.");
        goto exit;
    }
    if((fd1 = open("/dev/graphics/fb1", O_RDWR)) < 0) {
        ALOGW("open /dev/graphics/fb1 fail.");
        goto exit;
    }

    if((fd0_osdx2 = open("/sys/class/graphics/fb0/request2XScale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb0/request2XScale fail.");
        goto exit;
    }
    if((fd1_osdx2 = open("/sys/class/graphics/fb1/request2XScale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb0/request2XScale fail.");
        goto exit;
    }

    //================================
    //  display / video axis
    //================================
    if((fd_vaxis = open("/sys/class/video/axis", O_RDWR)) < 0) {
        ALOGW("open /sys/class/video/axis fail.");
        goto exit;
    }
    if((fd_daxis = open("/sys/class/display/axis", O_RDWR)) < 0) {
        ALOGW("open /sys/class/display/axis fail.");
        goto exit;
    }

    //================================
    //  disable video
    //================================
    if((fd_video = open("/sys/class/video/disable_video", O_RDWR)) < 0) {
        ALOGW("open /sys/class/video/disable_video fail.");
        goto exit;
    }

    //================================
    //  freescale
    //================================
    if((fd_freescale = open("/sys/class/freescale/ppscaler", O_RDWR)) < 0) {
        ALOGW("open /sys/class/freescale/ppscaler fail.");
        goto exit;
    }
    if((fd_freescale_rect = open("/sys/class/freescale/ppscaler_rect", O_RDWR)) < 0) {
        ALOGW("open /sys/class/freescale/ppscaler_rect fail.");
        goto exit;
    }
    if((fd_freescale_disp = open("/sys/class/freescale/disp", O_RDWR)) < 0) {
        ALOGW("open /sys/class/freescale/disp fail.");
        goto exit;
    }
    if((fd_fs_axis = open("/sys/class/graphics/fb0/free_scale_axis",O_RDWR)) < 0) {
        ALOGW("open /sys/class/graphics/fb0/free_scale_axis fail.");
        goto exit;
    }

    memset(set_str, 0, 32);
    if(fd0_osdx2>=0) {
        int ret_len = read(fd0_osdx2, set_str, sizeof(set_str));
        if(ret_len>0) {
            if(sscanf(set_str,"%d",&temp)>0) {
                if(temp == 2) {
                    ALOGD("already enable freescale mode: return!!!!");
                    ret = 0;
                    goto exit;
                } else {
                    ALOGD("fd0_osdx2 = %d", temp);
                }
            }
        }
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d %d %d", 0, 0, 1279, 719);
    if(fd_fs_axis >=0) {
        write(fd_fs_axis, set_str, strlen(set_str));
    }

    if(fd0_osdx2 >=0) {
        write(fd0_osdx2, "2", strlen("2"));
    }
    if(fd1_osdx2 >=0) {
        write(fd1_osdx2, "2", strlen("2"));
    }

    if (fd_freescale >= 0) {
        write(fd_freescale, "0", strlen("0"));
    }
    if (fd_video >= 0)  {
        write(fd_video, "1", 1);
    }
    if (fd_freescale >= 0) {
        write(fd_freescale, "1", strlen("1"));
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d %d %d %d", previewX0, previewY0, previewX1, previewY1, 1);
    //sprintf(set_str, "%d %d %d %d %d", 104, 456, 507, 709, 1);
    if(fd_freescale_rect >= 0) {
        write(fd_freescale_rect, set_str, strlen(set_str));
        ALOGD("freescale_rect :: %s \n", set_str);
    }

    if(fd_vaxis >= 0) {
        //write(fd_vaxis, "104 456 507 709", strlen("104 456 507 709"));
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d", osdWidth, osdHeight);
    //sprintf(set_str, "%d %d", 1280, 720);
    if(fd_freescale_disp >=0) {
        write(fd_freescale_disp, set_str, strlen(set_str));
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d %d %d", 0, 0, osdWidth, osdHeight);  //strcpy(daxis_str, "0 0 1280 720");
    if(fd_daxis >=0) {
        write(fd_daxis, set_str, strlen(set_str));
        //write(fd_daxis, daxis_str, strlen(daxis_str));
    }

    ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
    ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE, 0);
    ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_WIDTH, osdWidth);
    ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_HEIGHT, osdHeight);
    ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_WIDTH, osdWidth);
    ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_HEIGHT, osdHeight);
    ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
    ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE, 1);
    /*
    if((fd_freescale >= 0)&&(find_flag)){
        write(fd_vaxis, vaxis_str, strlen(vaxis_str));
    }*/
    if ((fd_video >= 0)&&(fd_freescale >= 0))   {
        write(fd_video, "0", strlen("0"));
    }

    memset(set_str, 0, 32);
    sprintf(set_str, "%d %d %d %d %d", previewX0, previewY0, previewX1, previewY1, 1);
    if(fd_freescale_rect >= 0) {
        //usleep(100*1000);
        //ALOGD("freescale_rect :: %s \n", set_str);
        write(fd_freescale_rect, set_str, strlen(set_str));
    }

    ret = 0;

exit:
    close(fd0);
    close(fd1);
    close(fd0_osdx2);
    close(fd1_osdx2);
    close(fd_vaxis);
    close(fd_daxis);
    close(fd_fs_axis);
    close(fd_freescale);
    close(fd_video);
    close(fd_freescale_rect);
    close(fd_freescale_disp);

    return ret;

}

 int DisableFreeScale(int mode)
{
    int fd0 = -1, fd1 = -1;
    int fd_freescale = -1;
    int fd_daxis = -1;
    int fd_vaxis = -1;
    int fd0_saxis = -1;
    int fd0_freescale = -1, fd1_freescale = -1;
    int fd0_osdx2enable = -1;
    int fd0_osdx2 = -1, fd1_osdx2 = -1;
    int ret = -1;
    char charbuf[1];

    if((fd0 = open("/dev/graphics/fb0", O_RDWR)) < 0) {
        goto exit;
    }
    if((fd1 = open("/dev/graphics/fb1", O_RDWR)) < 0) {
        goto exit;
    }
    if((fd_freescale = open("/sys/class/freescale/ppscaler", O_RDWR)) < 0) {
        ALOGW("open /sys/class/freescale/ppscaler fail.");
        goto exit;
    }
    if((fd_daxis = open("/sys/class/display/axis", O_RDWR)) < 0) {
        ALOGW("open /sys/class/display/axis fail.");
        goto exit;
    }
    if((fd_vaxis = open("/sys/class/video/axis", O_RDWR)) < 0) {
        goto exit;
    }
    if((fd0_saxis = open("/sys/class/graphics/fb0/scale_axis", O_RDWR)) < 0) {
        ALOGW("open /sys/class/graphics/fb0/scale_axis fail.");
        goto exit;
    }

    if((fd0_freescale = open("/sys/class/graphics/fb0/free_scale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb0/free_scale fail.");
        goto exit;
    }
    if((fd1_freescale = open("/sys/class/graphics/fb1/free_scale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb1/free_scale fail.");
        goto exit;
    }
    if((fd0_osdx2 = open("/sys/class/graphics/fb0/request2XScale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb0/request2XScale fail.");
        goto exit;
    }
    if((fd1_osdx2 = open("/sys/class/graphics/fb1/request2XScale",O_RDWR))<0) {
        ALOGW("/sys/class/graphics/fb1/request2XScale fail.");
        goto exit;
    }
    if((fd0_osdx2enable = open("/sys/class/graphics/fb0/scale",O_RDWR))<0) {
        ALOGW("/sys/devices/platform/mesonfb.0/graphics/fb0/scale fail.");
        goto exit;
    }


    if(mode == 0) { //get osd0x2's current value
        if(fd0_osdx2>=0) {
            int ret_len = read(fd0_osdx2, charbuf, sizeof(charbuf));
            if(ret_len>0) {
                if(sscanf(charbuf,"%d",&ret)>0) {
                    goto exit;
                }
            }
        }
    }

    ioctl(fd0,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
    ioctl(fd1,FBIOPUT_OSD_FREE_SCALE_ENABLE,0);
    if (fd_freescale >= 0)  write(fd_freescale, "0", strlen("0"));

    if(fd0_freescale >=0) {
        write(fd0_freescale, "0", strlen("0"));
    }
    if(fd1_freescale >=0) {
        write(fd1_freescale, "0", strlen("0"));
    }

    if(fd0_osdx2enable >=0) {
        write(fd0_osdx2enable, "0x0000", strlen("0x0000"));
        write(fd0_osdx2enable, "0x10000", strlen("0x10000"));
    }

    if(fd0_osdx2 >=0) {
        write(fd0_osdx2, "2", strlen("2"));
        write(fd0_osdx2, "8", strlen("8"));
    }
    if(fd1_osdx2 >=0) {
        write(fd1_osdx2, "2", strlen("2"));
        write(fd1_osdx2, "8", strlen("8"));
    }
    if(fd_daxis >=0) {
        write(fd_daxis, "0 0 1920 1080 0 0 64 64 ", strlen("0 0 1920 1080 0 0 64 64 "));
    }
    if(fd0_saxis >=0) {
        write(fd0_saxis, "0 0 959 1079 ", strlen("0 0 959 1079 "));
    }

    ret =0;

exit:
    close(fd0);
    close(fd1);
    close(fd_daxis);
    close(fd_vaxis);
    close(fd_freescale);
    close(fd0_saxis);
    close(fd0_freescale);
    close(fd1_freescale);
    close(fd0_osdx2enable);
    close(fd0_osdx2);
    close(fd1_osdx2);

    return ret;
}

 int PPMGR_OpenModule(void)
{
    pthread_mutex_lock(&vdin_ppmgr_op_mutex);

    if (ppmgr_dev_fd < 0) {
        ppmgr_dev_fd = open("/dev/ppmgr", O_RDWR);
        if (ppmgr_dev_fd < 0) {
            ALOGW("Open /dev/ppmgr error(%s)!\n", strerror(errno));
            pthread_mutex_unlock(&vdin_ppmgr_op_mutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&vdin_ppmgr_op_mutex);
    return ppmgr_dev_fd;
}

 int PPMGR_CloseModule(void)
{
    pthread_mutex_lock(&vdin_ppmgr_op_mutex);

    if (ppmgr_dev_fd >= 0) {
        close(ppmgr_dev_fd);
        ppmgr_dev_fd = -1;
    }

    pthread_mutex_unlock(&vdin_ppmgr_op_mutex);

    return 0;
}

 int PPMGR_DeviceIOCtl(int request, ...)
{
    int tmp_ret = -1;
    va_list ap;
    void * arg;

    pthread_mutex_lock(&vdin_ppmgr_op_mutex);

    if (ppmgr_dev_fd >= 0) {
        va_start(ap, request);
        arg = va_arg(ap, void *);
        va_end(ap);

        tmp_ret = ioctl(ppmgr_dev_fd, request, arg);

        pthread_mutex_unlock(&vdin_ppmgr_op_mutex);
        return tmp_ret;
    }

    pthread_mutex_unlock(&vdin_ppmgr_op_mutex);
    return -1;
}

 int VDIN_OpenModule(const unsigned char selVDIN)
{
    int tmp_fd = -1;
    const char *file_name = NULL;

    pthread_mutex_lock(&vdin_dev_op_mutex);
    if (selVDIN == CC_SEL_VDIN_DEV_0) {
        if (vdin0_dev_fd >= 0) {
            pthread_mutex_unlock(&vdin_dev_op_mutex);
            return -1;
        }

        file_name = VDIN_0_DEV_PATH;
    } else if (selVDIN == CC_SEL_VDIN_DEV_1) {
        if (vdin1_dev_fd >= 0) {
            pthread_mutex_unlock(&vdin_dev_op_mutex);
            return -1;
        }

        file_name = VDIN_1_DEV_PATH;
    }

    tmp_fd = open(file_name, O_RDWR);
    if (tmp_fd < 0) {
        ALOGW("Open %s error(%s)!\n", file_name, strerror(errno));
        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return -1;
    }

    memset(&gTvinVDINParam, 0, sizeof(gTvinVDINParam));
    memset(&gTvinVDINSignalInfo, 0, sizeof(gTvinVDINSignalInfo));

#ifdef LOGD_VDIN
    ALOGD("Open vdin module=[%d] return vdin%d_dev_fd = [%d]", selVDIN, selVDIN, tmp_fd);
#endif

    if (selVDIN == CC_SEL_VDIN_DEV_0) {
        vdin0_dev_fd = tmp_fd;
        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return vdin0_dev_fd;
    } else if (selVDIN == CC_SEL_VDIN_DEV_1) {
        vdin1_dev_fd = tmp_fd;
        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return vdin1_dev_fd;
    }

    pthread_mutex_unlock(&vdin_dev_op_mutex);
    return -1;
}

 int VDIN_CloseModule(const unsigned char selVDIN)
{
#ifdef LOGD_VDIN
    ALOGD("Close vdin[%d].", selVDIN);
#endif

    pthread_mutex_lock(&vdin_dev_op_mutex);

    if (selVDIN == CC_SEL_VDIN_DEV_0) {
        if (vdin0_dev_fd >= 0) {
            close(vdin0_dev_fd);
            vdin0_dev_fd = -1;
        }

        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return 0;
    } else if (selVDIN == CC_SEL_VDIN_DEV_1) {
        if (vdin1_dev_fd >= 0) {
            close(vdin1_dev_fd);
            vdin1_dev_fd = -1;
        }

        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return 0;
    }

    pthread_mutex_unlock(&vdin_dev_op_mutex);
    return -1;
}

 int VDIN_DeviceIOCtl(const unsigned char selVDIN, int request, ...)
{
    int tmp_ret = -1, tmp_fd = -1;
    va_list ap;
    void * arg;

    pthread_mutex_lock(&vdin_dev_op_mutex);

    if (selVDIN == CC_SEL_VDIN_DEV_0) {
        tmp_fd = vdin0_dev_fd;
    } else if (selVDIN == CC_SEL_VDIN_DEV_1) {
        tmp_fd = vdin1_dev_fd;
    }

    if (tmp_fd >= 0) {
        va_start(ap, request);
        arg = va_arg(ap, void *);
        va_end(ap);

        tmp_ret = ioctl(tmp_fd, request, arg);

        pthread_mutex_unlock(&vdin_dev_op_mutex);
        return tmp_ret;
    }

    pthread_mutex_unlock(&vdin_dev_op_mutex);
    return -1;
}

 int VDIN_GetDeviceFileHandle(const unsigned char selVDIN)
{
    int tmp_fd = -1;

    pthread_mutex_lock(&vdin_dev_op_mutex);

    if (selVDIN == CC_SEL_VDIN_DEV_0) {
        tmp_fd = vdin0_dev_fd;
    } else if (selVDIN == CC_SEL_VDIN_DEV_1) {
        tmp_fd = vdin1_dev_fd;
    }

    pthread_mutex_unlock(&vdin_dev_op_mutex);

    return tmp_fd;
}

 int VDIN_OpenPort(const unsigned char selVDIN, const struct tvin_parm_s *vdinParam)
{
    int rt = -1;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_OPEN, vdinParam);

    if (rt < 0) {
        ALOGW("Vdin open port[%d], error(%s)!", selVDIN, strerror(errno));
    }

    return rt;
}

 int VDIN_ClosePort(const unsigned char selVDIN)
{
    int rt = -1;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_CLOSE);

    if (rt < 0) {
        ALOGW("Vdin close port, error(%s)!", strerror(errno));
    }

    return rt;
}

 int VDIN_StartDec(const unsigned char selVDIN, const struct tvin_parm_s *vdinParam)
{
    int rt = -1;

    if (vdinParam == NULL) {
        return -1;
    }

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_START_DEC, vdinParam);

#ifdef LOGD_VDIN_START_DEC
    ALOGD("VDIN_StartDec:\n");
    ALOGD("index = [%d]\n", vdinParam->index);
    ALOGD("port = [0x%x]\n", (unsigned int) vdinParam->port);
    ALOGD("format = [0x%x]\n", (unsigned int) (vdinParam->info.fmt));
    ALOGD("cutwin.hs = [%d]\n", vdinParam->cutwin.hs);
    ALOGD("cutwin.he = [%d]\n", vdinParam->cutwin.he);
    ALOGD("cutwin.vs = [%d]\n", vdinParam->cutwin.vs);
    ALOGD("cutwin.ve = [%d]\n", vdinParam->cutwin.ve);
#endif

    if (rt < 0) {
        ALOGW("Vdin start decode, error(%s)!\n", strerror(errno));
    }

    return rt;
}

 int VDIN_StopDec(const unsigned char selVDIN)
{
    int rt = -1;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_STOP_DEC);

#ifdef LOGD_VDIN_STOP_DEC
    ALOGD("Vdin stop dec!");
#endif

    if (rt < 0) {
        ALOGW("Vdin stop decode, error(%s)", strerror(errno));
    }

    return rt;
}

 int VDIN_GetSignalInfo(const unsigned char selVDIN, struct tvin_info_s *SignalInfo)
{
    int rt = -1;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_G_SIG_INFO, SignalInfo);

#ifdef LOGD_VDIN_GET_SIGNAL_INFO
    ALOGD("=VDIN CPP=> vdin get signal info: trans_fmt(%d), fmt(%d), status(%d)\n",
          SignalInfo->trans_fmt, SignalInfo->fmt, SignalInfo->status);
#endif

    if (rt < 0) {
        ALOGW("Vdin get signal info, error(%s), ret = %d.\n", strerror(errno), rt);
    }

    return rt;
}

 int VDIN_SetVdinParam(const unsigned char selVDIN, const struct tvin_parm_s *vdinParam)
{
    int rt = -1, i = 0;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_S_PARM, vdinParam);

#ifdef LOGD_VDIN_SET_SIGNAL_PARAM
    ALOGD("Vdin set para:\n");
    ALOGD("VdinSigParam->index=[%d]\n", vdinParam->index);
    ALOGD("VdinSigParam->port=[%x]\n", vdinParam->port);
    ALOGD("VdinSigParam->trans_fmt=[%d]\n", vdinParam->info.trans_fmt);
    ALOGD("VdinSigParam->fmt=[%d]\n", vdinParam->info.fmt);
    ALOGD("VdinSigParam->status=[%d]\n", vdinParam->info.status);
    ALOGD("VdinSigParam->cutwin=hs[%d],he[%d],vs[%d],ve[%d]\n",
          vdinParam->cutwin.hs, vdinParam->cutwin.he, vdinParam->cutwin.vs, vdinParam->cutwin.ve);
    for (i=0; i<8; i++)
        ALOGD("VdinSigParam->histgram<%d~%d]>:[%d],[%d],[%d],[%d],[%d],[%d],[%d],[%d]\n", i*8, i*8+7,
              vdinParam->histgram[i*8], vdinParam->histgram[i*8+1], vdinParam->histgram[i*8+2], vdinParam->histgram[i*8+3],
              vdinParam->histgram[i*8+4], vdinParam->histgram[i*8+5], vdinParam->histgram[i*8+6], vdinParam->histgram[i*8+7]);
#endif

    if (rt < 0) {
        ALOGW("Vdin set signal param, error(%s)\n", strerror(errno));
    }

    return rt;
}

 int VDIN_GetVdinParam(const unsigned char selVDIN, const struct tvin_parm_s *vdinParam)
{
    int rt = -1, i = 0;

    rt = VDIN_DeviceIOCtl(selVDIN, TVIN_IOC_G_PARM, vdinParam);

#ifdef LOGD_VDIN_GET_SIGNAL_PARAM
    ALOGD("Vdin get para:\n");
    ALOGD("VdinSigParam->index=[%d]\n", vdinParam->index);
    ALOGD("VdinSigParam->port=[%x]\n", vdinParam->port);
    ALOGD("VdinSigParam->trans_fmt=[%d]\n", vdinParam->info.trans_fmt);
    ALOGD("VdinSigParam->fmt=[%d]\n", vdinParam->info.fmt);
    ALOGD("VdinSigParam->status=[%d]\n", vdinParam->info.status);
    ALOGD("VdinSigParam->cutwin=hs[%d],he[%d],vs[%d],ve[%d]\n",
          vdinParam->cutwin.hs, vdinParam->cutwin.he, vdinParam->cutwin.vs, vdinParam->cutwin.ve);
    for (i=0; i<8; i++)
        ALOGD("VdinSigParam->histgram<%d~%d]>:[%d],[%d],[%d],[%d],[%d],[%d],[%d],[%d]\n", i*8, i*8+7,
              vdinParam->histgram[i*8], vdinParam->histgram[i*8+1], vdinParam->histgram[i*8+2], vdinParam->histgram[i*8+3],
              vdinParam->histgram[i*8+4], vdinParam->histgram[i*8+5], vdinParam->histgram[i*8+6], vdinParam->histgram[i*8+7]);
#endif

    if (rt < 0) {
        ALOGW("Vdin get signal param, error(%s)\n", strerror(errno));
    }

    return rt;
}

 int VDIN_OnoffVScaler(int isOn)
{
    FILE *fp = NULL;

    if (isOn == 1) {
        isOn = 1;
    } else {
        isOn = 0;
    }

    pthread_mutex_lock(&vdin_vscaler_op_mutex);

    fp = fopen("/sys/class/video/vscaler", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/video/vscaler error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_vscaler_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", (int) isOn);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_vscaler_op_mutex);
    return 0;
}

 int VDIN_SetDisplayVFreq(int freq)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_display_mode_op_mutex);

    fp = fopen("/sys/class/display/mode", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/display/mode error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_display_mode_op_mutex);
        return -1;
    }

    if (freq == 50) {
        fprintf(fp, "%s", "lvds1080p50hz");
    } else {
        fprintf(fp, "%s", "lvds1080p");
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_display_mode_op_mutex);
    return 0;
}

 int VDIN_SetDepthOfField(int halfpixcount)
{
    FILE *fp = NULL;
    unsigned int w_data = 0;

    if (halfpixcount <= 0) {
        w_data = (0 - halfpixcount) * 128;
    } else if (halfpixcount > 0) {
        w_data = (halfpixcount * 128) | 0x10000000;
    }

    pthread_mutex_lock(&vdin_ppmgr_depth_op_mutex);

    fp = fopen("/sys/class/ppmgr/depth", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/ppmgr/depth ERROR(%s)!!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_ppmgr_depth_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", w_data);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_ppmgr_depth_op_mutex);
    return 0;
}

 int VDIN_Set2D3DDepth(int count)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_ppmgr_depth_op_mutex);

    fp = fopen("/sys/module/d2d3/parameters/depth", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/d2d3/parameters/depth ERROR(%s)!!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_ppmgr_depth_op_mutex);
        return -1;
    }

    if( count >=-127 && count <=127 ) {
#ifdef LOGD_3D_FUNCTION
        ALOGD("set depth value (%d).\n", count);
#endif
    } else {
        count = 8*12;
        ALOGE("set depth value ERROR!! set default depth.\n");
    }

    fprintf(fp, "%d", count);
    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_ppmgr_depth_op_mutex);
    return 0;
}


 int VDIN_SetPpmgrViewMode(int mode)
{
    view_mode_t view_mode;
    view_mode = (view_mode_t) mode;

    return PPMGR_DeviceIOCtl(PPMGR_IOC_VIEW_MODE, view_mode);
}

 int VDIN_Set2Dto3D(int on_off)
{
    struct tvin_parm_s VdinParam;
    VDIN_GetVdinParam(0, &VdinParam);
    VdinParam.flag &= (~TVIN_PARM_FLAG_2D_TO_3D);
    VdinParam.flag |= (on_off) ? (TVIN_PARM_FLAG_2D_TO_3D) : (0);
    return VDIN_SetVdinParam(0, &VdinParam);
}

 int VDIN_Set3DCmd(int cmd)
{
    int ret = -1;

    switch (cmd) {
    case 0:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, MODE_3D_DISABLE);
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (0: Disalbe!)\n");
#endif
        break;
    case 1:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_AUTO));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (1: Auto!)\n");
#endif
        break;
    case 2:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_2D_TO_3D));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (2: 2D->3D!)\n");
#endif
        break;
    case 3:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_LR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (3: L/R!)\n");
#endif
        break;
    case 4:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_BT));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (4: B/T!)\n");
#endif
        break;
    case 5:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_LR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (5: LR SWITCH OFF!)\n");
#endif
        break;
    case 6:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_LR_SWITCH));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (6: LR SWITCH!)\n");
#endif
        break;
    case 7:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_FIELD_DEPTH));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (7: FIELD_DEPTH!)\n");
#endif
        break;
    case 8:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_LR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (8: 3D_TO_2D_TURN_OFF!)\n");
#endif
        break;
    case 9:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_3D_TO_2D_L));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (9: 3D_TO_2D_L!)\n");
#endif
        break;
    case 10:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_3D_TO_2D_R));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (10: 3D_TO_2D_R!)\n");
#endif
        break;
    case 11:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_BT|BT_FORMAT_INDICATOR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (11: BT SWITCH OFF!)\n");
#endif
        break;
    case 12:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_LR_SWITCH|BT_FORMAT_INDICATOR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (12: BT SWITCH!)\n");
#endif
        break;
    case 13:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_BT));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D fucntion (13: 3D_TO_2D_TURN_OFF_BT!)\n");
#endif
        break;
    case 14:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_3D_TO_2D_L|BT_FORMAT_INDICATOR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (14: 3D TO 2D L BT!)\n");
#endif
        break;
    case 15:
        ret = PPMGR_DeviceIOCtl(PPMGR_IOC_ENABLE_PP, (MODE_3D_ENABLE|MODE_3D_TO_2D_R|BT_FORMAT_INDICATOR));
#ifdef LOGD_3D_FUNCTION
        ALOGD("3D function (15: 3D TO 2D R BT!)\n");
#endif
        break;
    }

    if (ret < 0) {
        ALOGW("Set 3D function error\n");
    }

    pthread_mutex_unlock(&vdin_ppmgr_op_mutex);
    return ret;
}

 int VDIN_SetPpmgrPlatformType(int mode)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_ppmgr_platform_type_op_mutex);

    fp = fopen("/sys/class/ppmgr/platform_type", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/ppmgr/platform_type error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_ppmgr_platform_type_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", mode);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_ppmgr_platform_type_op_mutex);
    return 0;
}

 int VDIN_SetPpmgrView_mode(int mode)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_ppmgr_view_mode_op_mutex);

    fp = fopen("/sys/class/ppmgr/view_mode", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/ppmgr/view_mode error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_ppmgr_view_mode_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", mode);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_ppmgr_view_mode_op_mutex);
    return 0;
}

 int VDIN_Set3DOverscan(int top, int left)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_ppmgr_axis_op_mutex);

    fp = fopen("/sys/class/ppmgr/axis", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/ppmgr/axis error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_ppmgr_axis_op_mutex);
        return -1;
    }

    fprintf(fp, "%d %d", top, left);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_ppmgr_axis_op_mutex);
    return 0;
}

 int VDIN_GetHistgram(int *hisgram)
{
    int i = 0;
    struct tvin_parm_s vdinParam;
    if (NULL == hisgram)
        return -1;
    if (0 == VDIN_GetVdinParam(0, &vdinParam)) {
        for (i = 0; i < CC_HIST_GRAM_BUF_SIZE; i++)
            hisgram[i] = (int) vdinParam.histgram[i];
    } else
        return -1;
    return 0;
}

 int VDIN_SetMVCViewMode(int mode)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_mvc_view_mode_op_mutex);

    fp = fopen("/sys/module/amvdec_h264mvc/parameters/view_mode", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/amvdec_h264mvc/parameters/view_mode error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_mvc_view_mode_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", (int) mode);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_mvc_view_mode_op_mutex);
    return 0;
}

 int VDIN_GetMVCViewMode(void)
{
    FILE *fp = NULL;
    int ret = 0;
    int mode = 0;

    pthread_mutex_lock(&vdin_mvc_view_mode_op_mutex);

    fp = fopen("/sys/module/amvdec_h264mvc/parameters/view_mode", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/amvdec_h264mvc/parameters/view_mode ERROR(%s)!!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_mvc_view_mode_op_mutex);
        return -1;
    }

    ret = fread(&mode, 1, 1, fp);
    ALOGD("fread /sys/module/amvdec_h264mvc/parameters/view_mode = [%d]", mode);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_mvc_view_mode_op_mutex);
    return mode;
}

 int VDIN_SetDIBuffMgrMode(int mgr_mode)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_buf_mgr_mode_op_mutex);

    fp = fopen("sys/module/di/parameters/buf_mgr_mode", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/di/parameters/buf_mgr_mode error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_buf_mgr_mode_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", mgr_mode);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_buf_mgr_mode_op_mutex);
    return 0;
}

 int VDIN_SetDICFG(int cfg)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_config_op_mutex);

    fp = fopen("sys/class/deinterlace/di0/config", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/deinterlace/di0/config error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_config_op_mutex);
        return -1;
    }

    if (0 == cfg) {
        fprintf(fp, "%s", "disable");
    } else {
        fprintf(fp, "%s", "enable");
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_config_op_mutex);
    return 0;
}

 int VDIN_SetDI3DDetc(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_det3d_en_op_mutex);

    fp = fopen("/sys/module/di/parameters/det3d_en", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/di/parameters/det3d_en error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_det3d_en_op_mutex);
        return -1;
    }

    if (0 == enable) {
        fprintf(fp, "%s", "disable");
    } else {
        fprintf(fp, "%s", "enable");
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_det3d_en_op_mutex);
    return 0;
}

 int VDIN_Get3DDetc(void)
{
    int fd = -1;
    int ret = -1;
    char buf[10];

    pthread_mutex_lock(&vdin_di_det3d_get_mutex);

    fd = open("/sys/module/di/parameters/det3d_en", O_RDWR);
    if (fd < 0) {
        ALOGW("Open /sys/module/di/parameters/det3d_en error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_det3d_get_mutex);
        return -1;
    }

    ret = read(fd, buf, sizeof(buf));

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_di_det3d_get_mutex);

    if(strcmp("enable",buf)==0)
        return 1;
    else
        return 0;
}


 int VDIN_GetVscalerStatus(void)
{
    int fd = -1;
    int ret = -1;
    char buf[7];

    pthread_mutex_lock(&vdin_vscaler_op_mutex);

    fd = open("/sys/class/video/vscaler", O_RDWR);
    if (fd < 0) {
        ALOGW("Open /sys/class/video/vscaler error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_vscaler_op_mutex);
        return -1;
    }

    ret = read(fd, buf, sizeof(buf));

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_vscaler_op_mutex);

#if 1
    ALOGD("GetVscalerStatus, buf size(%d), value(%s)\n", ret, buf);
#endif

    sscanf(buf, "%d", &ret);

    ret = ((ret & 0x40000) == 0) ? 1 : 0;

    if (ret == 1) {
        sleep(1);
    }

    return ret;
}

 int VDIN_TurnOnBlackBarDetect(int isEnable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_black_bar_en_op_mutex);

    fp = fopen("/sys/module/tvin_vdin/parameters/black_bar_enable", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/tvin_vdin/parameters/black_bar_enable error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_black_bar_en_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", isEnable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_black_bar_en_op_mutex);
    return 0;
}

 int VDIN_LoadHdcpKey(unsigned char *hdcpkey_buff)
{
    unsigned char testHdcp[368] = { 0x53, 0x4B, 0x59, 0x01, 0x00, 0x10, 0x0D, 0x15, 0x3A, 0x8E, 0x99, 0xEE, 0x2A, 0x55, 0x58, 0xEE, 0xED, 0x4B, 0xBE, 0x00, 0x74, 0xA9, 0x00, 0x10, 0x0A, 0x21, 0xE3,
                                    0x30, 0x66, 0x34, 0xCE, 0x9C, 0xC7, 0x8B, 0x51, 0x27, 0xF9, 0x0B, 0xAD, 0x09, 0x5F, 0x4D, 0xC2, 0xCA, 0xA2, 0x13, 0x06, 0x18, 0x8D, 0x34, 0x82, 0x46, 0x2D, 0xC9, 0x4B, 0xB0, 0x1C, 0xDE,
                                    0x3D, 0x49, 0x39, 0x58, 0xEF, 0x2B, 0x68, 0x39, 0x71, 0xC9, 0x4D, 0x25, 0xE9, 0x75, 0x4D, 0xAC, 0x62, 0xF5, 0xF5, 0x87, 0xA0, 0xB2, 0x4A, 0x60, 0xD3, 0xF1, 0x09, 0x3A, 0xB2, 0x3E, 0x19,
                                    0x4F, 0x3B, 0x1B, 0x2F, 0x85, 0x14, 0x28, 0x44, 0xFC, 0x69, 0x6F, 0x50, 0x42, 0x81, 0xBF, 0x7C, 0x2B, 0x3A, 0x17, 0x2C, 0x15, 0xE4, 0x93, 0x77, 0x74, 0xE8, 0x1F, 0x1C, 0x38, 0x54, 0x49,
                                    0x10, 0x64, 0x5B, 0x7D, 0x90, 0x3D, 0xA0, 0xE1, 0x8B, 0x67, 0x5C, 0x19, 0xE6, 0xCA, 0x9D, 0xE9, 0x68, 0x5A, 0xB5, 0x62, 0xDF, 0xA1, 0x28, 0xBC, 0x68, 0x82, 0x9A, 0x22, 0xC4, 0xDC, 0x48,
                                    0x85, 0x0F, 0xF1, 0x3E, 0x05, 0xDD, 0x1B, 0x2D, 0xF5, 0x49, 0x3A, 0x15, 0x29, 0xE7, 0xB6, 0x0B, 0x2A, 0x40, 0xE3, 0xB0, 0x89, 0xD5, 0x75, 0x84, 0x2E, 0x76, 0xE7, 0xBC, 0x63, 0x67, 0xE3,
                                    0x57, 0x67, 0x86, 0x81, 0xF4, 0xD7, 0xEA, 0x4D, 0x89, 0x8E, 0x37, 0x95, 0x59, 0x1C, 0x8A, 0xCD, 0x79, 0xF8, 0x4F, 0x82, 0xF2, 0x6C, 0x7E, 0x7F, 0x79, 0x8A, 0x6B, 0x90, 0xC0, 0xAF, 0x4C,
                                    0x8D, 0x43, 0x47, 0x1F, 0x9A, 0xF1, 0xBB, 0x88, 0x64, 0x49, 0x14, 0x50, 0xD1, 0xC3, 0xDF, 0xA6, 0x87, 0xA0, 0x15, 0x98, 0x51, 0x81, 0xF5, 0x97, 0x55, 0x10, 0x4A, 0x99, 0x30, 0x54, 0xA4,
                                    0xFC, 0xDA, 0x0E, 0xAC, 0x6A, 0xFA, 0x90, 0xEE, 0x12, 0x70, 0x69, 0x74, 0x63, 0x46, 0x63, 0xFB, 0xE6, 0x1F, 0x72, 0xEC, 0x43, 0x5D, 0x50, 0xFF, 0x03, 0x4F, 0x05, 0x33, 0x88, 0x36, 0x93,
                                    0xE4, 0x72, 0xD5, 0xCC, 0x34, 0x52, 0x96, 0x15, 0xCE, 0xD0, 0x32, 0x52, 0x41, 0x4F, 0xBC, 0x2D, 0xDF, 0xC5, 0xD6, 0x7F, 0xD5, 0x74, 0xCE, 0x51, 0xDC, 0x10, 0x5E, 0xF7, 0xAA, 0x4A, 0x2D,
                                    0x20, 0x9A, 0x17, 0xDD, 0x30, 0x89, 0x71, 0x82, 0x36, 0x50, 0x09, 0x1F, 0x7C, 0xF3, 0x12, 0xE9, 0x43, 0x10, 0x5F, 0x51, 0xBF, 0xB8, 0x45, 0xA8, 0x5A, 0x8D, 0x3F, 0x77, 0xE5, 0x96, 0x73,
                                    0x68, 0xAB, 0x73, 0xE5, 0x4C, 0xFB, 0xE5, 0x98, 0xB9, 0xAE, 0x74, 0xEB, 0x51, 0xDB, 0x91, 0x07, 0x7B, 0x66, 0x02, 0x9B, 0x79, 0x03, 0xC5, 0x34, 0x1C, 0x58, 0x13, 0x31, 0xD2, 0x4A, 0xEC
                                  };
    int ret = -1;
    int fd = -1;

    pthread_mutex_lock(&vdin_hdmi_edid_op_mutex);

    fd = open("/sys/class/hdmirx/hdmirx0/edid", O_RDWR);
    if (fd < 0) {
        ALOGW("Open hdmi hdcp key error(%s)!!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_hdmi_edid_op_mutex);
        return -1;
    }

    ret = write(fd, testHdcp, 368);
    if (ret < 0) {
        ALOGD("Write hdmi hdcp key error(%s)!!\n", strerror(errno));
    }

    close(fd);
    fd = -1;

    pthread_mutex_unlock(&vdin_hdmi_edid_op_mutex);
    return ret;
}

 int VDIN_KeepLastFrame(int enable)
{
    FILE *fp = NULL;

    return 0;

    pthread_mutex_lock(&vdin_keep_out_frame_op_mutex);

    fp = fopen("/sys/module/amvideo/parameters/keep_old_frame", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/amvideo/parameters/keep_old_frame error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_keep_out_frame_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_keep_out_frame_op_mutex);
    return 0;
}

 int VDIN_SetBlackOut(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_black_out_op_mutex);

    fp = fopen("/sys/class/video/blackout_policy", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/video/blackout_policy error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_black_out_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_black_out_op_mutex);
    return 0;
}

 int VDIN_SetVideoFreeze(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_video_freeze_op_mutex);

    fp = fopen("/sys/class/vdin/vdin0/attr", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/vdin/vdin0/attr error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_video_freeze_op_mutex);
        return -1;
    }

    if (enable == 1) {
        fprintf(fp, "freeze");
    } else {
        fprintf(fp, "unfreeze");
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_video_freeze_op_mutex);
    return 0;
}

 int VDIN_SetDIBypasshd(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_bypasshd_op_mutex);

    fp = fopen("/sys/module/di/parameters/bypass_hd", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/di/parameters/bypass_hd error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_bypasshd_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_bypasshd_op_mutex);
    return 0;
}

 int VDIN_SetDIBypassAll(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_bypassall_op_mutex);

    fp = fopen("/sys/module/di/parameters/bypass_all", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/di/parameters/bypass_all error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_bypassall_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_bypassall_op_mutex);
    return 0;
}

 int VDIN_SetDIBypassPost(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_di_bypasspost_op_mutex);

    fp = fopen("/sys/module/di/parameters/bypass_post", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/di/parameters/bypass_post error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_di_bypasspost_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_di_bypasspost_op_mutex);
    return 0;
}

 int VDIN_SetD2D3Bypass(int enable)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&new_d2d3_op_mutex);

    fp = fopen("/sys/class/d2d3/d2d3/debug", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/d2d3/d2d3/debug error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&new_d2d3_op_mutex);
        return -1;
    }

    if( enable == 1 ) {
        fprintf(fp, "%s", "bypass");
    } else {
        fprintf(fp, "%s", "enable");
    }
    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&new_d2d3_op_mutex);
    return 0;
}


 int VDIN_SetHDMIEQConfig(int config)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&vdin_hdmi_eq_config_default_op_mutex);

    fp = fopen("/sys/module/tvin_hdmirx/parameters/eq_config_default", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/tvin_hdmirx/parameters/eq_config_default_hd error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_hdmi_eq_config_default_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", config);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_hdmi_eq_config_default_op_mutex);
    return 0;
}

 int VDIN_SetVdinFlag(int flag)
{
    FILE *fp = NULL;
    int freq = 1200000;
    pthread_mutex_lock(&vdin_flag_op_mutex);

    fp = fopen("/sys/class/vdin/memp", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/class/vdin/memp error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_flag_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", flag);
    fclose(fp);
    fp = NULL;

    if (flag <= 1) {
        freq = 888000;
    } else {
        freq = 1200000;
    }
    fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vdin_flag_op_mutex);
        return -1;
    }/* else {
        fread(&freq, 1, 1, fp);
    }*/

    fprintf(fp, "%d", freq);
    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vdin_flag_op_mutex);

    /*
      // to save local playback bandwidth
      pthread_mutex_lock(&vdin_di_bypasshd_op_mutex);

      fp = fopen("/sys/module/di/parameters/bypass_hd", "w");
      if (fp == NULL) {
          ALOGW("Open /sys/module/di/parameters/bypass_hd error(%s)!\n", strerror(errno));
          pthread_mutex_unlock(&vdin_di_bypasshd_op_mutex);
          return -1;
      }

      if (flag > 1)
          fprintf(fp, "%d", 1);
      else
          fprintf(fp, "%d", 0);

      fclose(fp);
      fp = NULL;

      pthread_mutex_unlock(&vdin_di_bypasshd_op_mutex);
     */
    return 0;
}

 int VDIN_EnableRDMA(int enable)
{
    FILE *fp = NULL;
    pthread_mutex_lock(&rdma_op_mutex);

    fp = fopen("/sys/module/rdma/parameters/enable", "w");
    if (fp == NULL) {
        ALOGW("Open /sys/module/rdma/parameters/enable error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&rdma_op_mutex);
        return -1;
    }

    fprintf(fp, "%d", enable);
    fclose(fp);
    fp = NULL;
    pthread_mutex_unlock(&rdma_op_mutex);
    return 0;
}

// AFE
 int AFE_OpenModule(void)
{
    pthread_mutex_lock(&vdin_afe_op_mutex);

    if (afe_dev_fd < 0) {
        afe_dev_fd = open(AFE_DEV_PATH, O_RDWR);
        if (afe_dev_fd < 0) {
            ALOGW("Open tvafe module, error(%s).\n", strerror(errno));
            pthread_mutex_unlock(&vdin_afe_op_mutex);
            return -1;
        }
    }

#ifdef LOGD_AFE
    ALOGD("Open tvafe module=%d.\n", afe_dev_fd);
#endif

    pthread_mutex_unlock(&vdin_afe_op_mutex);

    return afe_dev_fd;
}

 void AFE_CloseModule(void)
{
#ifdef LOGD_AFE
    ALOGD("Close tvafe module.\n");
#endif

    pthread_mutex_lock(&vdin_afe_op_mutex);

    if (afe_dev_fd >= 0) {
        close(afe_dev_fd);
        afe_dev_fd = -1;
    }

    pthread_mutex_unlock(&vdin_afe_op_mutex);
    return;
}

int AFE_DeviceIOCtl(int request, ...)
{
    int tmp_ret = -1;
    va_list ap;
    void * arg;

    pthread_mutex_lock(&vdin_afe_op_mutex);

    if (afe_dev_fd >= 0) {
        va_start(ap, request);
        arg = va_arg(ap, void *);
        va_end(ap);

        tmp_ret = ioctl(afe_dev_fd, request, arg);

        pthread_mutex_unlock(&vdin_afe_op_mutex);
        return tmp_ret;
    }

    pthread_mutex_unlock(&vdin_afe_op_mutex);
    return -1;
}

 int AFE_GetDeviceFileHandle()
{
    int tmp_fd = -1;

    pthread_mutex_lock(&vdin_afe_op_mutex);

    tmp_fd = afe_dev_fd;

    pthread_mutex_unlock(&vdin_afe_op_mutex);

    return tmp_fd;
}


 

 int AFE_SetADCTimingAdjust(const struct tvafe_vga_parm_s *timingadj)
{
    int rt = -1;

    if (timingadj == NULL)
        return rt;

#ifdef LOGD_AFE_ADC_ADJUSTMENT
    ALOGD("AFE_SetADCTimingAdjust:Clock[%d],Phase[%d],H-Pos[%d],V-Pos[%d].\n",
          timingadj->clk_step, timingadj->phase, timingadj->hpos_step, timingadj->vpos_step);
#endif

    rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_VGA_PARM, timingadj);
    if (rt < 0)
        ALOGW("AFE_SetADCTimingAdjust, error(%s)!\n", strerror(errno));

    return rt;
}

 int AFE_GetADCCurrentTimingAdjust(struct tvafe_vga_parm_s *timingadj)
{
    int rt = -1;

    if (timingadj == NULL)
        return rt;

    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_VGA_PARM, timingadj);

    if (rt < 0) {
        ALOGW("AFE_GetADCCurrentTimingAdjust, error(%s)!\n", strerror(errno));
        return -1;
    }

#ifdef LOGD_AFE_ADC_ADJUSTMENT
    ALOGD("AFE_GetADCTimingAdjust:Clock[%d],Phase[%d],H-Pos[%d],V-Pos[%d].\n",
          timingadj->clk_step, timingadj->phase, timingadj->hpos_step, timingadj->vpos_step);
#endif

    return 0;
}

 int AFE_VGAAutoAdjust(struct tvafe_vga_parm_s *timingadj)
{
    enum tvafe_cmd_status_e CMDStatus = TVAFE_CMD_STATUS_PROCESSING;
    struct tvin_parm_s tvin_para;
    int rt = -1, i = 0;

    if (timingadj == NULL)
        return -1;

#ifdef LOGD_AFE_ADC_AUTO_ADJ
    ALOGD("AFE_VGAAutoAdjust, start.\n");
    ALOGD("===================================================\n");
#endif

    for (i = 0, CMDStatus == TVAFE_CMD_STATUS_PROCESSING; i < 50; i++) {
        rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_CMD_STATUS, &CMDStatus);
        if (rt < 0)
            ALOGD("get afe CMD status, error(%s), fd(%d), return(%d).\n", strerror(errno), AFE_GetDeviceFileHandle(), rt);
        if ((CMDStatus == TVAFE_CMD_STATUS_IDLE) || (CMDStatus == TVAFE_CMD_STATUS_SUCCESSFUL)) {
#ifdef LOGD_AFE_ADC_AUTO_ADJ
            ALOGD("CMD status OK, =[%d]\n", CMDStatus);
#endif
            break;
        }
        usleep(10*1000);
    }

    if ((CMDStatus == TVAFE_CMD_STATUS_PROCESSING) || (CMDStatus == TVAFE_CMD_STATUS_FAILED)) {
#ifdef LOGD_AFE_ADC_AUTO_ADJ
        ALOGD("===================================================\n");
        ALOGD("AFE_VGAAutoAdjust, CMD status fail, out of auto adjustment.\n");
        ALOGW("AFE_VGAAutoAdjust, failed!\n");
#endif
        return -1;
    }

    for (i = 0; i < 100; i++) {
        rt = VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_PARM, &tvin_para);
        if (tvin_para.info.status == TVIN_SIG_STATUS_STABLE) {
            break;
        }
        usleep(10*1000);
    }

#ifdef LOGD_AFE_ADC_AUTO_ADJ
    if (tvin_para.info.status == TVIN_SIG_STATUS_STABLE) {
        ALOGD("vdin signal status = STABLE.\n");
    } else {
        ALOGD("vdin signal status = NOT STABLE!\n");
    }
#endif

    rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_VGA_AUTO);
    if (rt < 0) {
        timingadj->clk_step = 0;
        timingadj->phase = 0;
        timingadj->hpos_step = 0;
        timingadj->vpos_step = 0;
        AFE_DeviceIOCtl(TVIN_IOC_S_AFE_VGA_PARM, timingadj);
#ifdef LOGD_AFE_ADC_AUTO_ADJ
        ALOGD("===================================================\n");
        ALOGW("AFE_VGAAutoAdjust, error(%s), fd(%d), return(%d).\n", strerror(errno), AFE_GetDeviceFileHandle(), rt);
        ALOGW("AFE_VGAAutoAdjust, failed!\n");
#endif
        return rt;
    } else {
        ;//AFE_DeviceIOCtl(TVIN_IOC_G_AFE_VGA_PARM, timingadj);
    }

    for (i = 0; i < 10; i++) {
        sleep(1);

        rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_CMD_STATUS, &CMDStatus);
        if (rt < 0) {
#ifdef LOGD_AFE_ADC_AUTO_ADJ
            ALOGD("===================================================\n");
            ALOGW("get afe CMD status, error(%s) fd(%d) return(%d)\n", strerror(errno), AFE_GetDeviceFileHandle(), rt);
            ALOGW("AFE_VGAAutoAdjust, failed!\n");
#endif
            return rt;
        } else {
            if (CMDStatus == TVAFE_CMD_STATUS_SUCCESSFUL) {
#ifdef LOGD_AFE_ADC_AUTO_ADJ
                ALOGD("CMD status = SUCCESSFULL!");
#endif
                usleep(100*1000);
                AFE_GetADCCurrentTimingAdjust(timingadj);
                ALOGD("===================================================\n");
                ALOGW("AFE_VGAAutoAdjust, successfull!\n");
                return 0;
            }
        }
    }
#ifdef LOGD_AFE_ADC_AUTO_ADJ
    ALOGW("CMD status = FAIL!");
    ALOGD("===================================================\n");
    ALOGW("AFE_VGAAutoAdjust, failed!\n");
#endif
    return -1;
}

 int AFE_SetVGAAutoAjust(void)
{
    int rt = -1;
    tvafe_vga_parm_t timingadj;
    tvafe_cmd_status_t Status;
    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_CMD_STATUS, &Status);
    if ((Status == TVAFE_CMD_STATUS_IDLE) || (Status == TVAFE_CMD_STATUS_SUCCESSFUL)) {
        ;
    } else {
        ALOGW("AFE_SetVGAAutoAjust, TVIN_IOC_G_AFE_CMD_STATUS failed!\n");
        return -1;
    }
    rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_VGA_AUTO);
    if (rt < 0) {
        timingadj.clk_step = 0;
        timingadj.phase = 0;
        timingadj.hpos_step = 0;
        timingadj.vpos_step = 0;
        AFE_DeviceIOCtl(TVIN_IOC_S_AFE_VGA_PARM, &timingadj);
#ifdef LOGD_AFE_ADC_AUTO_ADJ
        ALOGW("AFE_SetVGAAutoAjust, error(%s), fd(%d), return(%d).\n", strerror(errno), AFE_GetDeviceFileHandle(), rt);
#endif
        return rt;
    }
    return 0;
}

 int AFE_GetVGAAutoAdjustCMDStatus(tvafe_cmd_status_t *Status)
{
    int rt = -1;
    if (Status == NULL)
        return rt;
    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_CMD_STATUS, Status);
    if (rt < 0) {
#ifdef LOGD_AFE_ADC_AUTO_ADJ
        ALOGW("AFE_GetVGAAutoAdjustStatus, get status, error(%s) fd(%d) return(%d)\n", strerror(errno), AFE_GetDeviceFileHandle(), rt);
#endif
        return rt;
    }
    return 0;
}

 int AFE_GetADCGainOffset(struct tvafe_adc_cal_s *adccalvalue)
{
    int rt = -1;

    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_ADC_CAL, adccalvalue);

#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGD("AFE_GetADCGainOffset:\n");
    ALOGD("===================================================\n");
    ALOGD("A analog clamp = [0x%x]", adccalvalue->a_analog_clamp);
    ALOGD("A analog gain = [0x%x]", adccalvalue->a_analog_gain);
    ALOGD("A digital offset1 = [0x%x]", adccalvalue->a_digital_offset1);
    ALOGD("A digital gain = [0x%x]", adccalvalue->a_digital_gain);
    ALOGD("A digital offset2 = [0x%x]", adccalvalue->a_digital_offset2);
    ALOGD("---------------------------------------------------\n");
    ALOGD("B analog clamp = [0x%x]", adccalvalue->b_analog_clamp);
    ALOGD("B analog gain = [0x%x]", adccalvalue->b_analog_gain);
    ALOGD("B digital offset1 = [0x%x]", adccalvalue->b_digital_offset1);
    ALOGD("B digital gain = [0x%x]", adccalvalue->b_digital_gain);
    ALOGD("B digital offset2 = [0x%x]", adccalvalue->b_digital_offset2);
    ALOGD("---------------------------------------------------\n");
    ALOGD("C analog clamp = [0x%x]", adccalvalue->c_analog_clamp);
    ALOGD("C analog gain = [0x%x]", adccalvalue->c_analog_gain);
    ALOGD("C digital offset1 = [0x%x]", adccalvalue->c_digital_offset1);
    ALOGD("C digital gain = [0x%x]", adccalvalue->c_digital_gain);
    ALOGD("C digital offset2 = [0x%x]", adccalvalue->c_digital_offset2);
    ALOGD("===================================================\n");
#endif

    if (rt < 0)
        ALOGW("AFE_GetADCGainOffset, error(%s)!\n", strerror(errno));

    return rt;
}

 int AFE_SetADCGainOffset(struct tvafe_adc_cal_s *adccalvalue)
{
    int rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, adccalvalue);

#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGD("AFE_SetADCGainOffset:\n");
    ALOGD("===================================================\n");
    ALOGD("A analog clamp = [0x%x]", adccalvalue->a_analog_clamp);
    ALOGD("A analog gain = [0x%x]", adccalvalue->a_analog_gain);
    ALOGD("A digital offset1 = [0x%x]", adccalvalue->a_digital_offset1);
    ALOGD("A digital gain = [0x%x]", adccalvalue->a_digital_gain);
    ALOGD("A digital offset2 = [0x%x]", adccalvalue->a_digital_offset2);
    ALOGD("---------------------------------------------------\n");
    ALOGD("B analog clamp = [0x%x]", adccalvalue->b_analog_clamp);
    ALOGD("B analog gain = [0x%x]", adccalvalue->b_analog_gain);
    ALOGD("B digital offset1 = [0x%x]", adccalvalue->b_digital_offset1);
    ALOGD("B digital gain = [0x%x]", adccalvalue->b_digital_gain);
    ALOGD("B digital offset2 = [0x%x]", adccalvalue->b_digital_offset2);
    ALOGD("---------------------------------------------------\n");
    ALOGD("C analog clamp = [0x%x]", adccalvalue->c_analog_clamp);
    ALOGD("C analog gain = [0x%x]", adccalvalue->c_analog_gain);
    ALOGD("C digital offset1 = [0x%x]", adccalvalue->c_digital_offset1);
    ALOGD("C digital gain = [0x%x]", adccalvalue->c_digital_gain);
    ALOGD("C digital offset2 = [0x%x]", adccalvalue->c_digital_offset2);
    ALOGD("---------------------------------------------------\n");
    ALOGD("D analog clamp = [0x%x]", adccalvalue->d_analog_clamp);
    ALOGD("D analog gain = [0x%x]", adccalvalue->d_analog_gain);
    ALOGD("D digital offset1 = [0x%x]", adccalvalue->d_digital_offset1);
    ALOGD("D digital gain = [0x%x]", adccalvalue->d_digital_gain);
    ALOGD("D digital offset2 = [0x%x]", adccalvalue->d_digital_offset2);
    ALOGD("===================================================\n");
#endif

    if (rt < 0)
        ALOGW("AFE_SetADCGainOffset, error(%s)!", strerror(errno));

    return rt;
}

 int AFE_GetYPbPrADCGainOffset(struct tvafe_adc_comp_cal_s *adccalvalue)
{
    int rt = -1;

    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_ADC_COMP_CAL, adccalvalue);
#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGW("AFE_GetYPbPrADCGainOffset:\n");
    ALOGW("=====================start comp_cal_val[0]===========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[0].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[0].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[0].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_offset2);
    ALOGW("========================End comp_cal_val[0]==========================\n");
    ALOGW("=====================================================================\n");
    ALOGW("=======================Start comp_cal_val[1]=========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[1].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[1].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[1].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_offset2);
    ALOGW("========================End comp_cal_val[1]==========================\n");
    ALOGW("=====================================================================\n");
    ALOGW("=======================Start comp_cal_val[2]=========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[2].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[2].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[2].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_offset2);
    ALOGW("========================End comp_cal_val[2]==========================\n");
    ALOGW("=====================================================================\n");

#endif

    if (rt < 0)
        ALOGW("AFE_GetYPbPrADCGainOffset, error(%s)!\n", strerror(errno));

    return rt;
}

 int AFE_SetYPbPrADCGainOffset(struct tvafe_adc_comp_cal_s *adccalvalue)
{
    int rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_COMP_CAL, adccalvalue);

#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGW("AFE_SetYPbPrADCGainOffset:\n");
    ALOGW("=====================start comp_cal_val[0]===========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[0].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[0].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[0].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[0].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[0].c_digital_offset2);
    ALOGW("========================End comp_cal_val[0]==========================\n");
    ALOGW("=====================================================================\n");
    ALOGW("=======================Start comp_cal_val[1]=========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[1].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[1].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[1].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[1].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[1].c_digital_offset2);
    ALOGW("========================End comp_cal_val[1]==========================\n");
    ALOGW("=====================================================================\n");
    ALOGW("=======================Start comp_cal_val[2]=========================\n");
    ALOGW("A analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].a_analog_clamp);
    ALOGW("A analog gain = [0x%x]", adccalvalue->comp_cal_val[2].a_analog_gain);
    ALOGW("A digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_offset1);
    ALOGW("A digital gain = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_gain);
    ALOGW("A digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].a_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("B analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].b_analog_clamp);
    ALOGW("B analog gain = [0x%x]", adccalvalue->comp_cal_val[2].b_analog_gain);
    ALOGW("B digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_offset1);
    ALOGW("B digital gain = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_gain);
    ALOGW("B digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].b_digital_offset2);
    ALOGW("---------------------------------------------------\n");
    ALOGW("C analog clamp = [0x%x]", adccalvalue->comp_cal_val[2].c_analog_clamp);
    ALOGW("C analog gain = [0x%x]", adccalvalue->comp_cal_val[2].c_analog_gain);
    ALOGW("C digital offset1 = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_offset1);
    ALOGW("C digital gain = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_gain);
    ALOGW("C digital offset2 = [0x%x]", adccalvalue->comp_cal_val[2].c_digital_offset2);
    ALOGW("========================End comp_cal_val[2]==========================\n");
    ALOGW("=====================================================================\n");

#endif

    if (rt < 0)
        ALOGW("AFE_SetYPbPrADCGainOffset, error(%s)!", strerror(errno));

    return rt;
}
 int AFE_GetYPbPrWSSinfo(struct tvafe_comp_wss_s *wssinfo)
{
    int rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_COMP_WSS, wssinfo);

#ifdef LOGD_AFE_YPBPR_WSSINFO
    int i = 0;
    ALOGD("AFE_GetYPbPrWSSinfo:\n");
    ALOGD("===================================================\n");
    for (i=0; i<5; i++)
        ALOGD("wss11[%d] = [0x%x].\n", wssinfo->wss1[i]);
    for (i=0; i<5; i++)
        ALOGD("wss12[%d] = [0x%x].\n", wssinfo->wss2[i]);
    ALOGD("===================================================\n");
#endif

    if (rt < 0)
        ALOGW("AFE_GetYPbPrWSSinfo, error(%s)!", strerror(errno));
    return rt;
}

#define RGB444      3
#define YCBCR422    2
#define YCBCR444    3
#define Y422_POS    0
#define CB422_POS   1
#define CR422_POS   3
#define Y444_POS    0
#define CB444_POS   1
#define CR444_POS   2
#define R444_POS    0
#define G444_POS    1
#define B444_POS    2

//=========== VGA =====================
#define VGA_BUF_WID         (VGA_H_ACTIVE)

#ifdef PATTERN_7_COLOR_BAR
#define VGA_BAR_WID     (VGA_H_ACTIVE/7)
#define VGA_H_CUT_WID   (10)
#else
#define VGA_BAR_WID     (VGA_H_ACTIVE/8)
#define VGA_H_CUT_WID   (10)
#endif

#define VGA_V_CUT_WID       (40)
#define VGA_V_CAL_WID       (200+VGA_V_CUT_WID)

#define VGA_WHITE_HS        (VGA_BAR_WID*0+VGA_H_CUT_WID)
#define VGA_WHITE_HE        (VGA_BAR_WID*1-VGA_H_CUT_WID-1)
#define VGA_WHITE_VS        (VGA_V_CUT_WID)
#define VGA_WHITE_VE        (VGA_V_CAL_WID-1)
#define VGA_WHITE_SIZE      ((VGA_WHITE_HE-VGA_WHITE_HS+1)*(VGA_WHITE_VE-VGA_WHITE_VS+1))
#ifdef PATTERN_7_COLOR_BAR
#define VGA_BLACK_HS    (VGA_BAR_WID*6+VGA_H_CUT_WID)
#define VGA_BLACK_HE    (VGA_BAR_WID*7-VGA_H_CUT_WID-1)
#define VGA_BLACK_VS        (768-140)
#define VGA_BLACK_VE    (768-40-1)
#define VGA_BLACK_SIZE  ((VGA_BLACK_HE-VGA_BLACK_HS+1)*(VGA_BLACK_VE-VGA_BLACK_VS+1))
#else
#define VGA_BLACK_HS    (VGA_BAR_WID*7+VGA_H_CUT_WID)
#define VGA_BLACK_HE    (VGA_BAR_WID*8-VGA_H_CUT_WID-1)
#define VGA_BLACK_VS    (VGA_V_CUT_WID)
#define VGA_BLACK_VE    (VGA_V_CAL_WID-1)
#define VGA_BLACK_SIZE  ((VGA_BLACK_HE-VGA_BLACK_HS+1)*(VGA_BLACK_VE-VGA_BLACK_VS+1))
#endif

//=========== YPBPR =====================
#define COMP_BUF_WID        (COMP_H_ACTIVE)

#define COMP_BAR_WID        (COMP_H_ACTIVE/8)
#define COMP_H_CUT_WID      (20)
#define COMP_V_CUT_WID      (100)
#define COMP_V_CAL_WID      (200+COMP_V_CUT_WID)

#define COMP_WHITE_HS       (COMP_BAR_WID*0+COMP_H_CUT_WID)
#define COMP_WHITE_HE       (COMP_BAR_WID*1-COMP_H_CUT_WID-1)
#define COMP_WHITE_VS       (COMP_V_CUT_WID)
#define COMP_WHITE_VE       (COMP_V_CAL_WID-1)
#define COMP_WHITE_SIZE     ((COMP_WHITE_HE-COMP_WHITE_HS+1)*(COMP_WHITE_VE-COMP_WHITE_VS+1))
#define CB_WHITE_SIZE       ((COMP_WHITE_HE-COMP_WHITE_HS+1)*(COMP_WHITE_VE-COMP_WHITE_VS+1)/2)
#define CR_WHITE_SIZE       ((COMP_WHITE_HE-COMP_WHITE_HS+1)*(COMP_WHITE_VE-COMP_WHITE_VS+1)/2)

#define COMP_YELLOW_HS      (COMP_BAR_WID*1+COMP_H_CUT_WID)
#define COMP_YELLOW_HE      (COMP_BAR_WID*2-COMP_H_CUT_WID-1)
#define COMP_YELLOW_VS      (COMP_V_CUT_WID)
#define COMP_YELLOW_VE      (COMP_V_CAL_WID-1)
#define COMP_YELLOW_SIZE    ((COMP_YELLOW_HE-COMP_YELLOW_HS+1)*(COMP_YELLOW_VE-COMP_YELLOW_VS+1)/2)

#define COMP_CYAN_HS        (COMP_BAR_WID*2+COMP_H_CUT_WID)
#define COMP_CYAN_HE        (COMP_BAR_WID*3-COMP_H_CUT_WID-1)
#define COMP_CYAN_VS        (COMP_V_CUT_WID)
#define COMP_CYAN_VE        (COMP_V_CAL_WID-1)
#define COMP_CYAN_SIZE      ((COMP_CYAN_HE-COMP_CYAN_HS+1)*(COMP_CYAN_VE-COMP_CYAN_VS+1)/2)

#define COMP_RED_HS         (COMP_BAR_WID*5+COMP_H_CUT_WID)
#define COMP_RED_HE         (COMP_BAR_WID*6-COMP_H_CUT_WID-1)
#define COMP_RED_VS         (COMP_V_CUT_WID)
#define COMP_RED_VE         (COMP_V_CAL_WID-1)
#define COMP_RED_SIZE       ((COMP_RED_HE-COMP_RED_HS+1)*(COMP_RED_VE-COMP_RED_VS+1)/2)

#define COMP_BLUE_HS        (COMP_BAR_WID*6+COMP_H_CUT_WID)
#define COMP_BLUE_HE        (COMP_BAR_WID*7-COMP_H_CUT_WID-1)
#define COMP_BLUE_VS        (COMP_V_CUT_WID)
#define COMP_BLUE_VE        (COMP_V_CAL_WID-1)
#define COMP_BLUE_SIZE      ((COMP_BLUE_HE-COMP_BLUE_HS+1)*(COMP_BLUE_VE-COMP_BLUE_VS+1)/2)

#define COMP_BLACK_HS       (COMP_BAR_WID*7+COMP_H_CUT_WID)
#define COMP_BLACK_HE       (COMP_BAR_WID*8-COMP_H_CUT_WID-1)
#define COMP_BLACK_VS       (COMP_V_CUT_WID)
#define COMP_BLACK_VE       (COMP_V_CAL_WID-1)
#define COMP_BLACK_SIZE     ((COMP_BLACK_HE-COMP_BLACK_HS+1)*(COMP_BLACK_VE-COMP_BLACK_VS+1))
#define CB_BLACK_SIZE       ((COMP_BLACK_HE-COMP_BLACK_HS+1)*(COMP_BLACK_VE-COMP_BLACK_VS+1)/2)
#define CR_BLACK_SIZE       ((COMP_BLACK_HE-COMP_BLACK_HS+1)*(COMP_BLACK_VE-COMP_BLACK_VS+1)/2)

//=========== CVBS =====================
#define CVBS_BUF_WID        (CVBS_H_ACTIVE)
#define CVBS_BAR_WID        (CVBS_H_ACTIVE/8)
#define CVBS_H_CUT_WID      (20)

#define CVBS_V_CUT_WID      (40)
#define CVBS_V_CAL_WID      (140+CVBS_V_CUT_WID)

#define CVBS_WHITE_HS       (CVBS_BAR_WID*0+CVBS_H_CUT_WID)
#define CVBS_WHITE_HE       (CVBS_BAR_WID*1-CVBS_H_CUT_WID-1)
#define CVBS_WHITE_VS       (CVBS_V_CUT_WID)
#define CVBS_WHITE_VE       (CVBS_V_CAL_WID-1)
#define CVBS_WHITE_SIZE     ((CVBS_WHITE_HE-CVBS_WHITE_HS+1)*(CVBS_WHITE_VE-CVBS_WHITE_VS+1))

#define CVBS_BLACK_HS       (CVBS_BAR_WID*7+CVBS_H_CUT_WID)
#define CVBS_BLACK_HE       (CVBS_BAR_WID*8-CVBS_H_CUT_WID-1)
#define CVBS_BLACK_VS       (CVBS_V_CUT_WID)
#define CVBS_BLACK_VE       (CVBS_V_CAL_WID-1)
#define CVBS_BLACK_SIZE     ((CVBS_BLACK_HE-CVBS_BLACK_HS+1)*(CVBS_BLACK_VE-CVBS_BLACK_VS+1))

#define COMP_CAP_SIZE       (COMP_H_ACTIVE*COMP_V_ACTIVE*YCBCR422)
#ifdef VGA_SOURCE_RGB444
#define VGA_CAP_SIZE    (VGA_H_ACTIVE*VGA_V_ACTIVE*RGB444)
#else
#define VGA_CAP_SIZE    (VGA_H_ACTIVE*VGA_V_ACTIVE*YCBCR444)
#endif
#define CVBS_CAP_SIZE       (CVBS_H_ACTIVE*CVBS_V_ACTIVE)

#define PRE_0       -16
#define PRE_1       -128
#define PRE_2       -128
#define COEF_00     1.164
#define COEF_01     0
#define COEF_02     1.793
#define COEF_10     1.164
#define COEF_11     -0.213
#define COEF_12     -0.534
#define COEF_20     1.164
#define COEF_21     2.115
#define COEF_22     0
#define POST_0      0
#define POST_1      0
#define POST_2      0

 unsigned int data_limit(float data)
{
    if (data < 0)
        return (0);
    else if (data > 255)
        return (255);
    else
        return ((unsigned int) data);
}

 void matrix_convert_yuv709_to_rgb(unsigned int y, unsigned int u, unsigned int v, unsigned int *r, unsigned int *g, unsigned int *b)
{
    *r = data_limit(((float) y + PRE_0) * COEF_00 + ((float) u + PRE_1) * COEF_01 + ((float) v + PRE_2) * COEF_02 + POST_0 + 0.5);
    *g = data_limit(((float) y + PRE_0) * COEF_10 + ((float) u + PRE_1) * COEF_11 + ((float) v + PRE_2) * COEF_12 + POST_1 + 0.5);
    *b = data_limit(((float) y + PRE_0) * COEF_20 + ((float) u + PRE_1) * COEF_21 + ((float) v + PRE_2) * COEF_22 + POST_2 + 0.5);
}

typedef enum adc_cal_type_e {
    CAL_YPBPR = 0,
    CAL_VGA,
    CAL_CVBS,
} adc_cal_type_t;

typedef enum signal_range_e {
    RANGE100 = 0,
    RANGE75,
} signal_range_t;

typedef struct adc_cal_s {
    unsigned int rcr_max;
    unsigned int rcr_min;
    unsigned int g_y_max;
    unsigned int g_y_min;
    unsigned int bcb_max;
    unsigned int bcb_min;
    unsigned int cr_white;
    unsigned int cb_white;
    unsigned int cr_black;
    unsigned int cb_black;
} adc_cal_t;

 void re_order(unsigned int *a, unsigned int *b)
{
    unsigned int c = 0;

    if (*a > *b) {
        c = *a;
        *a = *b;
        *b = c;
    }
}

 char *get_cap_addr(enum adc_cal_type_e calType)
{
    int n;
    char * dp;

    for (n = 0; n < 0x00ff; n++) {
        if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_SIG_INFO, &gTvinAFESignalInfo) < 0) {
            ALOGW("get_cap_addr, get signal info, error(%s),fd(%d).\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
            return NULL;
        } else {
            if (gTvinAFESignalInfo.status == TVIN_SIG_STATUS_STABLE) {
                gTvinAFEParam.info.fmt = gTvinAFESignalInfo.fmt;
                break;
            }
        }
    }

    if (gTvinAFESignalInfo.status != TVIN_SIG_STATUS_STABLE) {
        ALOGD("get_cap_addr, signal isn't stable, out of calibration!\n");
        return NULL;
    } else {
        if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_STOP_DEC) < 0) {
            ALOGW("get_cap_addr, stop vdin, error (%s).\n", strerror(errno));
            return NULL;
        }

        usleep(1000);

        if (calType == CAL_YPBPR)
            dp = (char *) mmap(NULL, COMP_CAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0), 0);
        else
            dp = (char *) mmap(NULL, VGA_CAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0), 0);

        if (dp < 0)
            ALOGD("get_cap_addr, mmap failed!\n");

        return dp;
    }
    return NULL;
}

inline unsigned char get_mem_data(char *dp, unsigned int addr)
{
    return (*(dp + (addr ^ 7)));
}

 int get_frame_average(enum adc_cal_type_e calType, struct adc_cal_s *mem_data)
{
    unsigned int y = 0, cb = 0, cr = 0;
    unsigned int r = 0, g = 0, b = 0;
    unsigned long n;
    unsigned int i = 0, j = 0;
    char *dp = get_cap_addr(calType);

    if (calType == CAL_YPBPR) {
        for (j = COMP_WHITE_VS; j <= COMP_WHITE_VE; j++) {
            for (i = COMP_WHITE_HS; i <= COMP_WHITE_HE; i++) {
                mem_data->g_y_max += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422));
            }
        }
        mem_data->g_y_max /= COMP_WHITE_SIZE;

        for (j = COMP_WHITE_VS; j <= COMP_WHITE_VE; j++) {
            for (i = COMP_WHITE_HS; i <= COMP_WHITE_HE;) {
                mem_data->cb_white += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CB422_POS));
                mem_data->cr_white += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CR422_POS));
                i = i + 2;
            }
        }
        mem_data->cb_white /= CB_WHITE_SIZE;
        mem_data->cr_white /= CR_WHITE_SIZE;

        for (j = COMP_RED_VS; j <= COMP_RED_VE; j++) {
            for (i = COMP_RED_HS; i <= COMP_RED_HE;) {
                mem_data->rcr_max += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CR422_POS));
                i = i + 2;
            }
        }
        mem_data->rcr_max /= COMP_RED_SIZE;

        for (j = COMP_BLUE_VS; j <= COMP_BLUE_VE; j++) {
            for (i = COMP_BLUE_HS; i <= COMP_BLUE_HE;) {
                mem_data->bcb_max += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CB422_POS));
                i = i + 2;
            }
        }
        mem_data->bcb_max /= COMP_BLUE_SIZE;

        for (j = COMP_BLACK_VS; j <= COMP_BLACK_VE; j++) {
            for (i = COMP_BLACK_HS; i <= COMP_BLACK_HE; i++) {
                mem_data->g_y_min += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422));
            }
        }
        mem_data->g_y_min /= COMP_BLACK_SIZE;

        for (j = COMP_BLACK_VS; j <= COMP_BLACK_VE; j++) {
            for (i = COMP_BLACK_HS; i <= COMP_BLACK_HE;) {
                mem_data->cb_black += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CB422_POS));
                mem_data->cr_black += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CR422_POS));
                i = i + 2;
            }
        }
        mem_data->cb_black /= CB_BLACK_SIZE;
        mem_data->cr_black /= CR_BLACK_SIZE;
        /*
         for(j=COMP_BLACK_VS; j<=COMP_BLACK_VE; j++) {
         for (i=COMP_BLACK_HS; i<=COMP_BLACK_HE;) {
         //mem_data->cb_black += get_mem_data(dp, ((COMP_BUF_WID*j+i)*YCBCR422+CB422_POS));
         mem_data->cr_black += get_mem_data(dp, ((COMP_BUF_WID*j+i)*YCBCR422+CR422_POS));
         i = i+2;
         }
         }
         mem_data->cr_black /= CR_BLACK_SIZE;
         */
        for (j = COMP_CYAN_VS; j <= COMP_CYAN_VE; j++) {
            for (i = COMP_CYAN_HS; i <= COMP_CYAN_HE;) {
                mem_data->rcr_min += get_mem_data(dp, ((COMP_BUF_WID * j + i) * YCBCR422 + CR422_POS));
                i = i + 2;
            }
        }
        mem_data->rcr_min /= COMP_CYAN_SIZE;

        for (j = COMP_YELLOW_VS; j <= COMP_YELLOW_VE; j++) {
            for (i = COMP_YELLOW_HS; i <= COMP_YELLOW_HE;) {
                mem_data->bcb_min += get_mem_data(dp, (COMP_BUF_WID * j + i) * YCBCR422 + CB422_POS);
                i = i + 2;
            }
        }
        mem_data->bcb_min /= COMP_YELLOW_SIZE;

    } else if (calType == CAL_VGA) {
        for (j = VGA_WHITE_VS; j <= VGA_WHITE_VE; j++) {
            for (i = VGA_WHITE_HS; i <= VGA_WHITE_HE; i++) {
#ifdef VGA_SOURCE_RGB444
                r = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+R444_POS));
                g = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+G444_POS));
                b = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+B444_POS));
#else
                y = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + Y444_POS));
                cb = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + CB444_POS));
                cr = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + CR444_POS));
                matrix_convert_yuv709_to_rgb(y, cb, cr, &r, &g, &b);
#endif
                mem_data->rcr_max = mem_data->rcr_max + r;
                mem_data->g_y_max = mem_data->g_y_max + g;
                mem_data->bcb_max = mem_data->bcb_max + b;
            }
        }
        mem_data->rcr_max = mem_data->rcr_max / VGA_WHITE_SIZE;
        mem_data->g_y_max = mem_data->g_y_max / VGA_WHITE_SIZE;
        mem_data->bcb_max = mem_data->bcb_max / VGA_WHITE_SIZE;

        for (j = VGA_BLACK_VS; j <= VGA_BLACK_VE; j++) {
            for (i = VGA_BLACK_HS; i <= VGA_BLACK_HE; i++) {
#ifdef VGA_SOURCE_RGB444
                r = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+R444_POS));
                g = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+G444_POS));
                b = get_mem_data(dp, ((VGA_BUF_WID*j+i)*RGB444+B444_POS));
#else
                y = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + Y444_POS));
                cb = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + CB444_POS));
                cr = get_mem_data(dp, ((VGA_BUF_WID * j + i) * YCBCR444 + CR444_POS));
                matrix_convert_yuv709_to_rgb(y, cb, cr, &r, &g, &b);
#endif
                mem_data->rcr_min = mem_data->rcr_min + r;
                mem_data->g_y_min = mem_data->g_y_min + g;
                mem_data->bcb_min = mem_data->bcb_min + b;
            }
        }
        mem_data->rcr_min = mem_data->rcr_min / VGA_BLACK_SIZE;
        mem_data->g_y_min = mem_data->g_y_min / VGA_BLACK_SIZE;
        mem_data->bcb_min = mem_data->bcb_min / VGA_BLACK_SIZE;

    } else { //CVBS
        for (j = CVBS_WHITE_VS; j <= CVBS_WHITE_VE; j++) {
            for (i = CVBS_WHITE_HS; i <= CVBS_WHITE_HE; i++) {
                mem_data->g_y_max += mem_data->g_y_max + get_mem_data(dp, ((CVBS_BUF_WID * j + i) * YCBCR422));
            }
        }
        mem_data->g_y_max /= COMP_WHITE_SIZE;

        for (j = CVBS_BLACK_VS; j <= CVBS_BLACK_VE; j++) {
            for (i = CVBS_BLACK_HS; i <= CVBS_BLACK_HE; i++) {
                mem_data->g_y_min += mem_data->g_y_min + get_mem_data(dp, ((CVBS_BUF_WID * j + i) * YCBCR422));
            }
        }
        mem_data->g_y_min /= CVBS_BLACK_SIZE;
    }

    if (calType == CAL_YPBPR)
        munmap(dp, COMP_CAP_SIZE);
    else if (calType == CAL_VGA)
        munmap(dp, VGA_CAP_SIZE);
    else
        munmap(dp, CVBS_CAP_SIZE);

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_START_DEC, &gTvinAFEParam) < 0) {
        ALOGW("get_frame_average, get vdin signal info, error(%s),fd(%d).\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
        return NULL;
    } else {
        ;
    }
    /*
     ALOGD("get_frame_average, FRAME AVERAGE:\n");
     ALOGD("get_frame_average, MAX :\n Cr->%d \n G->%d \n Cb->%d\n",mem_data->rcr_max,mem_data->g_y_max,mem_data->bcb_max);
     ALOGD("get_frame_average, MIN :\n Cr->%d \n Y->%d \n Cb->%d\n",mem_data->rcr_min,mem_data->g_y_min,mem_data->bcb_min);
     */
    return 0;
}

#define ADC_CAL_FRAME_QTY_ORDER 2 //NOTE:  MUST >=2!!
 struct adc_cal_s get_n_frame_average(enum adc_cal_type_e calType) {
    struct adc_cal_s mem_data = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned int rcrmax[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int rcrmin[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int g_ymax[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int g_ymin[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int bcbmax[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int bcbmin[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int cbwhite[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int crwhite[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int cbblack[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int crblack[1 << ADC_CAL_FRAME_QTY_ORDER];
    unsigned int i = 0, j = 0;

    for (i = 0; i < (1 << ADC_CAL_FRAME_QTY_ORDER); i++) {
        get_frame_average(calType, &mem_data);
        rcrmax[i] = mem_data.rcr_max;
        rcrmin[i] = mem_data.rcr_min;
        g_ymax[i] = mem_data.g_y_max;
        g_ymin[i] = mem_data.g_y_min;
        bcbmax[i] = mem_data.bcb_max;
        bcbmin[i] = mem_data.bcb_min;
        cbwhite[i] = mem_data.cb_white;
        crwhite[i] = mem_data.cr_white;
        cbblack[i] = mem_data.cb_black;
        crblack[i] = mem_data.cr_black;
    }

    for (i = 0; i < (1 << ADC_CAL_FRAME_QTY_ORDER) - 1; i++) {
        for (j = 1; j < (1 << ADC_CAL_FRAME_QTY_ORDER); j++) {
            re_order(&(rcrmax[i]), &(rcrmax[j]));
            re_order(&(rcrmin[i]), &(rcrmin[j]));
            re_order(&(g_ymax[i]), &(g_ymax[j]));
            re_order(&(g_ymin[i]), &(g_ymin[j]));
            re_order(&(bcbmax[i]), &(bcbmax[j]));
            re_order(&(bcbmin[i]), &(bcbmin[j]));
            re_order(&(cbwhite[i]), &(cbwhite[j]));
            re_order(&(crwhite[i]), &(crwhite[j]));
            re_order(&(cbblack[i]), &(cbblack[j]));
            re_order(&(crblack[i]), &(crblack[j]));
        }
    }

    /*
     for(i=0; i<8; i++) {
     ALOGD("get_n_frame_average:\n");
     ALOGD("===================================================\n");
     ALOGD("mem_data.y_max=%d\n", g_ymax[i]);
     ALOGD("mem_data.y_min=%d\n", g_ymin[i]);
     ALOGD("---------------------------------------------------\n");
     ALOGD("mem_data.cb_max=%d\n", bcbmax[i]);
     ALOGD("mem_data.cb_min=%d\n", bcbmin[i]);
     ALOGD("---------------------------------------------------\n");
     ALOGD("mem_data.cr_max=%d\n", rcrmax[i]);
     ALOGD("mem_data.cr_min=%d\n", rcrmin[i]);
     ALOGD("===================================================\n");
     }
     */

    memset(&mem_data, 0, sizeof(mem_data));
    for (i = 0; i < (1 << (ADC_CAL_FRAME_QTY_ORDER - 1)); i++) {//(1<<(ADC_CAL_FRAME_QTY_ORDER-1))
        mem_data.rcr_max += rcrmax[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.rcr_min += rcrmin[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.g_y_max += g_ymax[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.g_y_min += g_ymin[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.bcb_max += bcbmax[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.bcb_min += bcbmin[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.cb_white += cbwhite[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.cr_white += crwhite[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.cb_black += cbblack[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
        mem_data.cr_black += crblack[i + (1 << (ADC_CAL_FRAME_QTY_ORDER - 2))];
    }

    /*
     ALOGD("get_n_frame_average:\n");
     ALOGD("===================================================\n");
     ALOGD("mem_data.y_max=%d\n", g_ymax[i]);
     ALOGD("mem_data.y_min=%d\n", g_ymin[i]);
     ALOGD("---------------------------------------------------\n");
     ALOGD("mem_data.cb_max=%d\n", bcbmax[i]);
     ALOGD("mem_data.cb_min=%d\n", bcbmin[i]);
     ALOGD("---------------------------------------------------\n");
     ALOGD("mem_data.cr_max=%d\n", rcrmax[i]);
     ALOGD("mem_data.cr_min=%d\n", rcrmin[i]);
     ALOGD("===================================================\n");
     */

    mem_data.rcr_max >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.rcr_min >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.g_y_max >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.g_y_min >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.bcb_max >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.bcb_min >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.cb_white >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.cr_white >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.cb_black >>= (ADC_CAL_FRAME_QTY_ORDER - 1);
    mem_data.cr_black >>= (ADC_CAL_FRAME_QTY_ORDER - 1);

    /*
     ALOGD("get_n_frame_average, TOTAL FRAME AVERAGE:\n");
     ALOGD("===================================================\n");
     ALOGD("get_n_frame_average, MAX :\n  Cr->%d \n Y->%d \n Cb->%d\n", mem_data.rcr_max, mem_data.g_y_max, mem_data.bcb_max);
     ALOGD("get_n_frame_average, MIN :\n  Cr->%d \n Y->%d \n Cb->%d\n", mem_data.rcr_min, mem_data.g_y_min, mem_data.bcb_min);
     ALOGD("===================================================\n");
     */
    return mem_data;
}

#define COUNT_NUM       8
#define ADC_WIN_MIN     0
#define ADC_WIN_MAX     1023
 struct tvafe_adc_cal_s AFE_ADCAutoCalibration_Old(enum adc_cal_type_e calType, enum signal_range_e sigRange) {
    struct tvafe_adc_cal_s tvafe_adc_cal;
    struct adc_cal_s mem_data = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    unsigned int finetune, offsetune, gaintune;
    unsigned int count = 0;
    unsigned int step_clamp = 32;
    unsigned int step_gain = 64;
    unsigned int Y_G_MIN, CB_B_MIN, CR_R_MIN;
    unsigned int Y_G_MAX, CB_B_MAX, CR_R_MAX;

    if (calType == CAL_YPBPR) {
        if (sigRange == RANGE100) {
            Y_G_MIN = 16;
            CB_B_MIN = 16;
            CR_R_MIN = 16;
            Y_G_MAX = 235;
            CB_B_MAX = 240;
            CR_R_MAX = 240;
        } else {
            Y_G_MIN = 16;
            CB_B_MIN = 44;
            CR_R_MIN = 44;
            Y_G_MAX = 180;
            CB_B_MAX = 212;
            CR_R_MAX = 212;
        }
    } else {
        if (sigRange == RANGE100) {
            Y_G_MIN = 0;
            CB_B_MIN = 0;
            CR_R_MIN = 0;
            Y_G_MAX = 255;
            CB_B_MAX = 255;
            CR_R_MAX = 255;
        } else {
            Y_G_MIN = 0;
            CB_B_MIN = 0;
            CR_R_MIN = 0;
            Y_G_MAX = 191;
            CB_B_MAX = 191;
            CR_R_MAX = 191;
        }
    }

#ifdef LOGD_AFE_ADC_CALIBRATION
    get_frame_average(calType, &mem_data);
    ALOGW("AFE_ADCAutoCalibration_Old, Start ->MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
    ALOGD("AFE_ADCAutoCalibration_Old, Start ->MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white,
          mem_data.cr_black);
#endif

    AFE_GetADCGainOffset(&tvafe_adc_cal);

    tvafe_adc_cal.a_analog_clamp = 63;
    tvafe_adc_cal.a_analog_gain = 255;
    tvafe_adc_cal.a_digital_offset1 = 0;
    tvafe_adc_cal.a_digital_gain = 0x400;//x1.0
    tvafe_adc_cal.a_digital_offset2 = 0;

    tvafe_adc_cal.b_analog_clamp = 63;
    tvafe_adc_cal.b_analog_gain = 255;
    tvafe_adc_cal.b_digital_offset1 = 0;
    tvafe_adc_cal.b_digital_gain = 0x400;
    tvafe_adc_cal.b_digital_offset2 = 0;

    tvafe_adc_cal.c_analog_clamp = 63;
    tvafe_adc_cal.c_analog_gain = 255;
    tvafe_adc_cal.c_digital_offset1 = 0;
    tvafe_adc_cal.c_digital_gain = 0x400;
    tvafe_adc_cal.c_digital_offset2 = 0;

    if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
        ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));

    usleep(100*1000);

    while (count < (COUNT_NUM - 1 - 1)) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration_Old, XXXXXXXXX [counter=%d],[step_clamp=%d] XXXXXXXXXXXXX\n", count, step_clamp);
#endif

        mem_data = get_n_frame_average(calType);

#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration_Old, MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
        ALOGD("AFE_ADCAutoCalibration_Old, MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white, mem_data.cr_black);
#endif

        if (calType == CAL_YPBPR) {
            if (mem_data.g_y_min < Y_G_MIN) {
                tvafe_adc_cal.a_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)a analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.a_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)a analog clamp \n", step_clamp);
            }

            if (mem_data.cb_white < 128) {
                tvafe_adc_cal.b_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)b analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.b_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)b analog clamp \n", step_clamp);
            }

            if (mem_data.cr_black < 128) {
                tvafe_adc_cal.c_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)c analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.c_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)c analog clamp \n", step_clamp);
            }
        } else {
            if (mem_data.g_y_min < Y_G_MIN) {
                tvafe_adc_cal.a_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)a analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.a_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)a analog clamp \n", step_clamp);
            }

            if (mem_data.bcb_min < CB_B_MIN) {
                tvafe_adc_cal.b_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)b analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.b_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)b analog clamp \n", step_clamp);
            }

            if (mem_data.rcr_min < CR_R_MIN) {
                tvafe_adc_cal.c_analog_clamp += step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)c analog clamp \n", step_clamp);
            } else {
                tvafe_adc_cal.c_analog_clamp -= step_clamp;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)c analog clamp \n", step_clamp);
            }
        }

        step_clamp >>= 1;
        count++;

        if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
            ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));

        usleep(100*1000);
    }
    /*
     get_frame_average(calType,&mem_data);
     if(calType == CAL_YPBPR){
     if(mem_data.g_y_min >Y_G_MIN)
     tvafe_adc_cal.a_analog_clamp--;
     if(mem_data.cb_white>128)
     tvafe_adc_cal.b_analog_clamp--;
     if(mem_data.cr_black>128)
     tvafe_adc_cal.c_analog_clamp--;
     }else{
     if(mem_data.g_y_min >Y_G_MIN)
     tvafe_adc_cal.a_analog_clamp--;
     if(mem_data.cb_white>CB_B_MIN)
     tvafe_adc_cal.b_analog_clamp--;
     if(mem_data.cr_black>CR_R_MIN)
     tvafe_adc_cal.c_analog_clamp--;
     }

     if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
     ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));
     usleep(100*1000);
     */

#ifdef LOGD_AFE_ADC_CALIBRATION
    get_frame_average(calType, &mem_data);
    ALOGW("AFE_ADCAutoCalibration_Old, after clamp tune->MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
    ALOGW("AFE_ADCAutoCalibration_Old, after clamp tune->MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white,
          mem_data.cr_black);
#endif

    AFE_GetADCGainOffset(&tvafe_adc_cal);
    count = 0;
    tvafe_adc_cal.a_analog_gain = 128;
    tvafe_adc_cal.b_analog_gain = 128;
    tvafe_adc_cal.c_analog_gain = 128;

    if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
        ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));

    usleep(100*1000);

    while (count < (COUNT_NUM - 1)) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration_Old, XXXXXXXX [counter=%d], ,[step_gain=%d] XXXXXXXXXXXXXXXXXXX\n", count, step_gain);
#endif

        mem_data = get_n_frame_average(calType);

#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration_Old, MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
        ALOGD("AFE_ADCAutoCalibration_Old, MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white, mem_data.cr_black);
#endif

        if (calType == CAL_YPBPR) {
            if ((mem_data.g_y_max > Y_G_MAX)) {//|| (mem_data.g_y_min <= Y_G_MIN)){
                tvafe_adc_cal.a_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)a analog: gain \n", step_gain);
            } else {
                tvafe_adc_cal.a_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)a analog gain \n", step_gain);
            }

            if ((mem_data.bcb_max > CB_B_MAX) || (mem_data.bcb_min < CB_B_MIN)) {
                tvafe_adc_cal.b_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)b analog gain \n", step_gain);
            } else {
                tvafe_adc_cal.b_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)b analog gain \n", step_gain);
            }

            if ((mem_data.rcr_max > CR_R_MAX) || (mem_data.rcr_min < CR_R_MIN)) { // touching ADC input window
                tvafe_adc_cal.c_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)c analog gain \n", step_gain);
            } else {
                tvafe_adc_cal.c_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)c analog gain \n", step_gain);
            }
        } else {
            if ((mem_data.g_y_max > Y_G_MAX)) {//|| (mem_data.g_y_min < 0x00)){
                tvafe_adc_cal.a_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)a analog: gain \n", step_gain);
            } else {
                tvafe_adc_cal.a_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)a analog gain \n", step_gain);
            }

            if ((mem_data.bcb_max > CB_B_MAX)) {// || (mem_data.bcb_min <= 0x00)){
                tvafe_adc_cal.b_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)b analog gain \n", step_gain);
            } else {
                tvafe_adc_cal.b_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)b analog gain \n", step_gain);
            }

            if ((mem_data.rcr_max > CR_R_MIN)) {// || (mem_data.rcr_min <= 0x00)){ // touching ADC input window
                tvafe_adc_cal.c_analog_gain -= step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (-%d)c analog gain \n", step_gain);
            } else {
                tvafe_adc_cal.c_analog_gain += step_gain;
                ALOGD("AFE_ADCAutoCalibration_Old, (+%d)c analog gain \n", step_gain);
            }
        }
        step_gain >>= 1;
        count++;

        AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal);

        usleep(100*1000);
    }
    /*
     get_frame_average(calType,&mem_data);
     if(calType == CAL_YPBPR){
     if(mem_data.g_y_min <Y_G_MAX)
     tvafe_adc_cal.a_analog_gain ++;
     if(mem_data.bcb_max <CB_B_MAX ||mem_data.bcb_min>CB_B_MIN)
     tvafe_adc_cal.b_analog_gain++;
     if(mem_data.rcr_max<CR_R_MAX || mem_data.rcr_min>CR_R_MIN)
     tvafe_adc_cal.c_analog_gain ++;
     }else{
     if(mem_data.g_y_min <Y_G_MAX)
     tvafe_adc_cal.a_analog_gain ++;
     if(mem_data.cb_white<CB_B_MAX)
     tvafe_adc_cal.b_analog_gain++;
     if(mem_data.cr_black<CR_R_MAX)
     tvafe_adc_cal.c_analog_gain ++;
     }

     if(AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
     ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));
     usleep(100*1000);
     */
    mem_data = get_n_frame_average(calType);
#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGW("AFE_ADCAutoCalibration_Old, After gain tune MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
    ALOGW("AFE_ADCAutoCalibration_Old, After gain tune MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white,
          mem_data.cr_black);
#endif

    for (finetune = 0; finetune < 4; finetune++) {
        for (offsetune = 0; offsetune < 2; offsetune++) {
            get_frame_average(CAL_YPBPR, &mem_data);
            if (calType == CAL_YPBPR) {
                if (mem_data.g_y_min > Y_G_MIN)
                    tvafe_adc_cal.a_analog_clamp--;
                else if (mem_data.g_y_min < Y_G_MIN)
                    tvafe_adc_cal.a_analog_clamp++;
                if (mem_data.cb_white > 128)
                    tvafe_adc_cal.b_analog_clamp--;
                else if (mem_data.cb_white < 128)
                    tvafe_adc_cal.b_analog_clamp++;
                if (mem_data.cr_black > 128)
                    tvafe_adc_cal.c_analog_clamp--;
                else if (mem_data.cb_black < 128)
                    tvafe_adc_cal.c_analog_clamp++;
            } else {
                if (mem_data.g_y_min > Y_G_MIN)
                    tvafe_adc_cal.a_analog_clamp--;
                else if (mem_data.g_y_min < Y_G_MIN)
                    tvafe_adc_cal.a_analog_clamp++;
                if (mem_data.cb_white > CB_B_MIN)
                    tvafe_adc_cal.b_analog_clamp--;
                else if (mem_data.cb_white < CB_B_MIN)
                    tvafe_adc_cal.b_analog_clamp++;
                if (mem_data.cr_black > CR_R_MIN)
                    tvafe_adc_cal.c_analog_clamp--;
                else if (mem_data.cb_black < CR_R_MIN)
                    tvafe_adc_cal.c_analog_clamp++;
            }
            if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
                ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)!\n", strerror(errno));
            usleep(100*1000);
        }

        for (gaintune = 0; gaintune < 2; gaintune++) {
            get_frame_average(CAL_YPBPR, &mem_data);
            if (calType == CAL_YPBPR) {
                if (mem_data.g_y_max > Y_G_MAX)
                    tvafe_adc_cal.a_analog_gain--;
                else if (mem_data.g_y_max < Y_G_MAX)
                    tvafe_adc_cal.a_analog_gain++;
                if (mem_data.bcb_max > CB_B_MAX || mem_data.bcb_min < CB_B_MIN)
                    tvafe_adc_cal.b_analog_gain--;
                else if (mem_data.bcb_max < CB_B_MAX || mem_data.bcb_min > CB_B_MIN)
                    tvafe_adc_cal.b_analog_gain++;
                if (mem_data.rcr_max > CR_R_MAX || mem_data.rcr_min < CR_R_MIN)
                    tvafe_adc_cal.c_analog_gain--;
                else if (mem_data.rcr_max < CR_R_MAX || mem_data.rcr_min > CR_R_MIN)
                    tvafe_adc_cal.c_analog_gain++;
            } else {
                if (mem_data.g_y_min > Y_G_MIN)
                    tvafe_adc_cal.a_analog_gain--;
                else if (mem_data.g_y_min < Y_G_MIN)
                    tvafe_adc_cal.a_analog_gain++;
                if (mem_data.cb_white > CB_B_MIN)
                    tvafe_adc_cal.b_analog_gain--;
                else if (mem_data.cb_white < CB_B_MIN)
                    tvafe_adc_cal.b_analog_gain++;
                if (mem_data.cr_black > CR_R_MIN)
                    tvafe_adc_cal.c_analog_gain--;
                else if (mem_data.cb_black < CR_R_MIN)
                    tvafe_adc_cal.c_analog_gain++;
            }
            if (AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal) < 0)
                ALOGW("AFE_ADCAutoCalibration_Old, set adc calibration value, error(%s)\n", strerror(errno));
            usleep(100*1000);
        }
    }

#ifdef LOGD_AFE_ADC_CALIBRATION
    mem_data = get_n_frame_average(calType);
    ALOGD("AFE_ADCAutoCalibration_Old, After fine-tune MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
    ALOGD("AFE_ADCAutoCalibration_Old, After fine-tune MIN :\n Y->%d \n Cb->%d \n Cr->%d\n CbWhite ->%d\n CrBlack->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min, mem_data.cb_white,
          mem_data.cr_black);
#endif

#if 1
    tvafe_adc_cal.a_digital_offset1 = (0xfffff800 - 4 * mem_data.g_y_min) & 0x000007ff;
    tvafe_adc_cal.b_digital_offset1 = (0xfffff800 - 4 * mem_data.bcb_min) & 0x000007ff;
    tvafe_adc_cal.c_digital_offset1 = (0xfffff800 - 4 * mem_data.rcr_min) & 0x000007ff;
    //ALOGD("AFE_ADCAutoCalibration_Old, a offset=0x%x,\n", tvafe_adc_cal.a_digital_offset1);
    //ALOGD("AFE_ADCAutoCalibration_Old, b offset=0x%x.\n", tvafe_adc_cal.b_digital_offset1);
    mem_data.g_y_max -= mem_data.g_y_min;
    mem_data.bcb_max -= mem_data.bcb_min;
    mem_data.rcr_max -= mem_data.rcr_min;
    mem_data.g_y_min = 0;
    mem_data.bcb_min = 0;
    mem_data.rcr_min = 0;

    if (calType == CAL_YPBPR) {
        tvafe_adc_cal.a_digital_gain = (879 << 10) / (4 * mem_data.g_y_max);
        tvafe_adc_cal.b_digital_gain = (899 << 10) / (4 * mem_data.bcb_max);
        tvafe_adc_cal.c_digital_gain = (899 << 10) / (4 * mem_data.rcr_max);
    } else {
        tvafe_adc_cal.a_digital_gain = (1023 << 10) / (4 * mem_data.g_y_max);
        tvafe_adc_cal.b_digital_gain = (1023 << 10) / (4 * mem_data.bcb_max);
        tvafe_adc_cal.c_digital_gain = (1023 << 10) / (4 * mem_data.rcr_max);
    }
#if 0
    if (calType == CAL_YPBPR) {
        mem_data.g_y_max = 879;
        mem_data.bcb_max = 899;
        mem_data.rcr_max = 899;
        mem_data.g_y_min = 0;
        mem_data.bcb_min = 0;
        mem_data.rcr_min = 0;
    } else { // CAL_VGA
        mem_data.g_y_max = 1023;
        mem_data.bcb_max = 1023;
        mem_data.rcr_max = 1023;
        mem_data.g_y_min = 0;
        mem_data.bcb_min = 0;
        mem_data.rcr_min = 0;
    }
#endif
    if (calType == CAL_YPBPR) {
        tvafe_adc_cal.a_digital_offset2 = 64;
        tvafe_adc_cal.b_digital_offset2 = 64;
        tvafe_adc_cal.c_digital_offset2 = 64;
    } else {
        tvafe_adc_cal.a_digital_offset2 = 0;
        tvafe_adc_cal.b_digital_offset2 = 0;
        tvafe_adc_cal.c_digital_offset2 = 0;
    }
    // calculate data window after digital offset2
    /*
     if (calType == CAL_YPBPR) {
     mem_data.g_y_max = 943; // 943 = 879+64
     mem_data.bcb_max = 963; // 963 = 899+64
     mem_data.rcr_max = 963; // 963 = 899+64
     mem_data.g_y_min = 64; // 64 = 16<<2
     mem_data.bcb_min = 64; // 64 = 16<<2
     mem_data.rcr_min = 64; // 64 = 16<<2
     } else { // CAL_VGA
     mem_data.g_y_max = 1023;
     mem_data.bcb_max = 1023;
     mem_data.rcr_max = 1023;
     mem_data.g_y_min  =   0;
     mem_data.bcb_min  =   0;
     mem_data.rcr_min  =   0;
     }
     */
#endif
    AFE_DeviceIOCtl(TVIN_IOC_S_AFE_ADC_CAL, &tvafe_adc_cal);
    usleep(100*1000);
#ifdef LOGD_AFE_ADC_CALIBRATION
    get_frame_average(calType, &mem_data);
    ALOGW("AFE_ADCAutoCalibration_Old, Finish MAX :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_max, mem_data.bcb_max, mem_data.rcr_max);
    ALOGW("AFE_ADCAutoCalibration_Old, Finish MIN :\n Y->%d \n Cb->%d \n Cr->%d\n", mem_data.g_y_min, mem_data.bcb_min, mem_data.rcr_min);
#endif
    return tvafe_adc_cal;
}

 int AFE_GetMemData(int typeSel, struct adc_cal_s *mem_data)
{
    int rt = -1;
    if (VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0) < 0 || mem_data == NULL) {
        ALOGW("AFE_GetMemData, didn't open vdin fd, return!\n");
        return -1;
    }

    memset(&gTvinAFEParam, 0, sizeof(gTvinAFEParam));
    memset(&gTvinAFESignalInfo, 0, sizeof(gTvinAFESignalInfo));

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_PARM, &gTvinAFEParam) < 0) {
        ALOGW("AFE_GetMemData, get vdin param, error(%s), fd(%d)!\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
        return -1;
    }

    gTvinAFEParam.flag = gTvinAFEParam.flag | TVIN_PARM_FLAG_CAP;

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam) < 0) {
        ALOGW("AFE_GetMemData, set vdin param error(%s)!\n", strerror(errno));
        return -1;
    }

    if (typeSel == 0)
        get_frame_average(CAL_YPBPR, mem_data);
    else if (typeSel == 1)
        get_frame_average(CAL_VGA, mem_data);
    else
        *mem_data = get_n_frame_average(CAL_CVBS);

    gTvinAFEParam.flag &= 0x11111110;

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam) < 0) {
        ALOGW("AFE_GetMemData, set vdin param error(%s)\n", strerror(errno));
        return -1;
    }

    ALOGD("AFE_GetMemData, MAX ======> :\n Y(White)->%d \n Cb(Blue)->%d \n Cr(Red)->%d\n", mem_data->g_y_max, mem_data->bcb_max, mem_data->rcr_max);
    ALOGD("AFE_GetMemData, MIN ======>:\n Y(Black)->%d \n Cb(Yellow)->%d \n Cr(Cyan)->%d\n Cb(White) ->%d\n Cb(Black)->%d\n Cr(Black)->%d\n", mem_data->g_y_min, mem_data->bcb_min, mem_data->rcr_min,
          mem_data->cb_white, mem_data->cb_black, mem_data->cr_black);
    return 0;
}

 int Vdin0ColorMatrix(unsigned char onoff)
{
    FILE *fp = NULL;

    pthread_mutex_lock(&aml_debug_reg_op_mutex);

    fp = fopen("/sys/class/amdbg/reg", "w");
    if (fp == NULL) {
        ALOGD("Open /sys/class/amdbg/reg error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&aml_debug_reg_op_mutex);
        return -1;
    }

    if (onoff == 1) {
        fprintf(fp, "%s", "wc 0x1210 1");
    } else {
        fprintf(fp, "%s", "wc 0x1210 0");
    }

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&aml_debug_reg_op_mutex);
    return 0;
}

 int AFE_ADCAutoCalibration(void)
{
    int i;
    struct tvafe_adc_cal_s tvafe_adc_cal;
    memset(&gTvinAFEParam, 0, sizeof(gTvinAFEParam));

#if 0
    ALOGD("AFE_ADCAutoCalibration, start!\n");
    if (VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0) < 0) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGW("AFE_ADCAutoCalibration, didn't open vdin fd, return!\n");
#endif
        return -1;
    }

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_PARM, &gTvinAFEParam) < 0) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGW("AFE_ADCAutoCalibration, get vdin param 1, error(%s), fd(%d)!\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
#endif
        return -1;
    }

    if (gTvinAFEParam.info.status == TVIN_SIG_STATUS_NOSIG || gTvinAFEParam.info.status == TVIN_SIG_STATUS_NULL) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration, NO_SIGNAL, go to do auto calibration!");
#endif
    } else {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration, HAVE_SIGNAL, get out of auto-calibration!");
#endif
        return -1;
    }
#endif

    gTvinAFEParam.flag = gTvinAFEParam.flag | TVIN_PARM_FLAG_CAL;

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam) < 0) {
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGW("AFE_ADCAutoCalibration, set vdin param(TVIN_PARM_FLAG_CAL), error(%s)!\n", strerror(errno));
#endif
        return -1;
    }

#ifdef LOGD_AFE_ADC_CALIBRATION
    ALOGD("AFE_ADCAutoCalibration, set vdin FLAY_CAL = %d.\n", gTvinAFEParam.flag);
#endif

#if 0
    sleep(1);

    for (i=0; i<100; i++) {
        if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_PARM, &gTvinAFEParam) < 0) {
#ifdef LOGD_AFE_ADC_CALIBRATION
            ALOGW("AFE_ADCAutoCalibration, get vdin param 2, error(%s), fd(%d)!\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
#endif
            return -1;
        }
#ifdef LOGD_AFE_ADC_CALIBRATION
        ALOGD("AFE_ADCAutoCalibration, get vdin param 2, flag(%d).\n", gTvinAFEParam.flag);
#endif
        if ((gTvinAFEParam.flag & TVIN_PARM_FLAG_CAL) == 0) {
#ifdef LOGD_AFE_ADC_CALIBRATION
            ALOGD("AFE_ADCAutoCalibration, flag(%d).\n", gTvinAFEParam.flag);
#endif
            break;
        }
        usleep(300*1000);
    }
    ALOGD("AFE_ADCAutoCalibration, finish!\n");
#endif
    return 0;
}

 int AFE_GetCVBSLockStatus(enum tvafe_cvbs_video_e *cvbs_lock_status)
{
    int rt = -1;

    rt = AFE_DeviceIOCtl(TVIN_IOC_G_AFE_CVBS_LOCK, cvbs_lock_status);

    if (rt < 0) {
        ALOGD("AFE_GetCVBSLockStatus, error: return(%d), error(%s)!\n", rt, strerror(errno));
    } else {
        ALOGD("AFE_GetCVBSLockStatus, value=%d.\n", *cvbs_lock_status);
    }
    return *cvbs_lock_status;
}

 int AFE_SetCVBSStd(unsigned int sig_fmt)
{
    int rt = -1;
    enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

    switch (sig_fmt) {
    case COLOR_SYSTEM_AUTO:
        fmt = TVIN_SIG_FMT_NULL;
        break;
    case COLOR_SYSTEM_PAL:
        fmt = TVIN_SIG_FMT_CVBS_PAL_I;
        break;
    case COLOR_SYSTEM_NTSC:
        fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
        break;
    case COLOR_SYSTEM_SECAM:
        fmt = TVIN_SIG_FMT_CVBS_SECAM;
        break;
    default:
        fmt = TVIN_SIG_FMT_NULL;
        break;
    }
    ALOGD("AFE_SetCVBSStd, sig_fmt = %d\n", fmt);
    rt = AFE_DeviceIOCtl(TVIN_IOC_S_AFE_CVBS_STD, &fmt);

    if (rt < 0) {
        ALOGD("AFE_SetCVBSStd, error: return(%d), error(%s)!\n", rt, strerror(errno));
    }
    return rt;
}

int TvinApi_OpenPPMGRModule(void)
{
    return PPMGR_OpenModule();
}

int TvinApi_ClosePPMGRModule(void)
{
    return PPMGR_CloseModule();
}

int TvinApi_OnoffVScaler(int onOff)
{
    if (onOff == 0)
        return VDIN_OnoffVScaler(0);
    else
        return VDIN_OnoffVScaler(1);
}

int TvinApi_Send3DCommand(int commd)
{
    int ret = -1;

    ret = VDIN_Set3DCmd(commd);

    return ret;
}

int TvinApi_SetPpmgrPlatformType(int mode)
{
    return VDIN_SetPpmgrPlatformType(mode);
}

int TvinApi_SetPpmgrView_mode(int mode)
{
    return VDIN_SetPpmgrView_mode(mode);
}


int TvinApi_Set3DOverscan(int top, int left)
{
    return VDIN_Set3DOverscan(top, left);
}

int TvinApi_OpenVDINModule(int selVDIN)
{
    return VDIN_OpenModule((unsigned char) selVDIN);
}

int TvinApi_CloseVDINModule(int selVDIN)
{
    return VDIN_CloseModule((unsigned char) selVDIN);
}

int TvinApi_IsVDINModuleOpen(int selVDIN)
{
    if (VDIN_GetDeviceFileHandle(selVDIN) < 0) {
        return -1;
    }

    return 0;
}

int TvinApi_OpenPort(int selVDIN, int sourceId)
{
    gTvinVDINParam.index = selVDIN;
    gTvinVDINParam.port = (enum tvin_port_e) sourceId;
    return VDIN_OpenPort((unsigned char) selVDIN, &gTvinVDINParam);
}

int TvinApi_ClosePort(int selVDIN)
{
    return VDIN_ClosePort((unsigned char) selVDIN);
}

int TvinApi_StartDec(int selVDIN, tvin_parm_t TvinVDINParam)
{
    return VDIN_StartDec((unsigned char) selVDIN, &TvinVDINParam);
}

int TvinApi_GetSignalInfo(int selVDIN, tvin_info_t *SignalInfo)
{
    if (SignalInfo == NULL)
        return -1;
    return VDIN_GetSignalInfo((unsigned char) selVDIN, SignalInfo);
}

int TvinApi_GetHistgram(int *histgram_buf)
{
    if (histgram_buf == NULL)
        return -1;
    return VDIN_GetHistgram(histgram_buf);
}

int TvinApi_StopDec(int selVDIN)
{
    return VDIN_StopDec((unsigned char) selVDIN);
}

int TvinApi_ManualSetPath(char *videopath)
{
    return VDIN_AddVideoPath(videopath);
}

int TvinApi_AddTvPath(int selPath)
{
    return VDIN_AddTvPath((int) selPath);
}

int TvinApi_RmTvPath(void)
{
    return VDIN_RmTvPath();
}

int TvinApi_RmDefPath(void)
{
    return VDIN_RmDefPath();
}

int TvinApi_RmPreviewPath()
{
    return VDIN_RmPreviewPath();
}

int TvinApi_EnableFreeScale(const int osdWidth, const int osdHeight, const int previewX0, const int previewY0, const int previewX1,  const int previewY1)
{
    return EnableFreeScale(osdWidth, osdHeight, previewX0, previewY0, previewX1, previewY1);
}

int TvinApi_DisableFreeScale(int mode)
{
    return DisableFreeScale(mode);
}


int TvinApi_SetDICFG(int cfg)
{
    return VDIN_SetDICFG((int) cfg);
}

int TvinApi_GetDI3DDetc()
{
    return VDIN_Get3DDetc();
}

int TvinApi_SetDI3DDetc(int enable)
{
    return VDIN_SetDI3DDetc(enable);
}

int TvinApi_SetPpmgrMode(int mode)
{
    return VDIN_SetPpmgrViewMode((int) mode);
}

int TvinApi_SetMVCMode(int mode)
{
    return VDIN_SetMVCViewMode((int) mode);
}

int TvinApi_GetMVCMode(void)
{
    return VDIN_GetMVCViewMode();
}

int TvinApi_GetVscalerStatus(void)
{
    return VDIN_GetVscalerStatus();
}

int TvinApi_Set3DOvserScan(int top, int left)
{
    return VDIN_Set3DOverscan((int) top, (int) left);
}

int TvinApi_TurnOnBlackBarDetect(int isEnable)
{
    return VDIN_TurnOnBlackBarDetect((int) isEnable);
}

int TvinApi_LoadHdcpKey(unsigned char *hdcpkeybuff)
{
    //if (hdcpkeybuff == NULL)
    //return -1;
    return VDIN_LoadHdcpKey(hdcpkeybuff);
}

int TvinApi_OpenAFEModule(void)
{
    return AFE_OpenModule();
}

void TvinApi_CloseAFEModule(void)
{
    AFE_CloseModule();
}





int TvinApi_SetVGACurTimingAdj(tvafe_vga_parm_t adjparam)
{
    return AFE_SetADCTimingAdjust(&adjparam);
}

int TvinApi_GetVGACurTimingAdj(tvafe_vga_parm_t *adjparam)
{
    if (adjparam == NULL)
        return -1;
    return AFE_GetADCCurrentTimingAdjust(adjparam);
}

int TvinApi_VGAAutoAdj(tvafe_vga_parm_t *adjparam)
{
    if (adjparam == NULL)
        return -1;
    return AFE_VGAAutoAdjust(adjparam);
}

int TvinApi_SetVGAAutoAdjust(void)
{
    return AFE_SetVGAAutoAjust();
}

int TvinApi_GetVGAAutoAdjustCMDStatus(tvafe_cmd_status_t *Status)
{
    if (Status == NULL)
        return -1;
    return AFE_GetVGAAutoAdjustCMDStatus(Status);
}

int TvinApi_GetYPbPrWSSInfo(tvafe_comp_wss_t *wssinfo)
{
    if (wssinfo == NULL)
        return -1;
    return AFE_GetYPbPrWSSinfo(wssinfo);
}



int TvinApi_SetADCGainOffset(tvafe_adc_cal_t adc_cal_parm)
{
    return AFE_SetADCGainOffset(&adc_cal_parm);
}

int TvinApi_GetYPbPrADCGainOffset(tvafe_adc_comp_cal_t *adc_cal_parm)
{
    if (adc_cal_parm == NULL)
        return -1;
    return AFE_GetYPbPrADCGainOffset(adc_cal_parm);
}

int TvinApi_SetYPbPrADCGainOffset(tvafe_adc_comp_cal_t adc_cal_parm)
{
    return AFE_SetYPbPrADCGainOffset(&adc_cal_parm);
}
int TvinApi_ADCAutoCalibration_Old(int typeSel, int rangeSel)
{
    int rt = -1;

    if (VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0) < 0) {
        ALOGW("TvinApi_ADCAutoCalibration_Old, didn't open vdin fd, return!\n");
        return -1;
    }

    memset(&gTvinAFEParam, 0, sizeof(gTvinAFEParam));
    memset(&gTvinAFESignalInfo, 0, sizeof(gTvinAFESignalInfo));

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_G_PARM, &gTvinAFEParam) < 0) {
        ALOGW("TvinApi_ADCAutoCalibration_Old, get vdin param, error(%s),fd(%d)!\n", strerror(errno), VDIN_GetDeviceFileHandle(CC_SEL_VDIN_DEV_0));
        return -1;
    }

    gTvinAFEParam.flag = gTvinAFEParam.flag | TVIN_PARM_FLAG_CAP;

    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam) < 0) {
        ALOGW("TvinApi_ADCAutoCalibration_Old, set vdin param error(%s)!\n", strerror(errno));
        return -1;
    }

    if (typeSel == 0) {
        if (rangeSel == 0)
            AFE_ADCAutoCalibration_Old(CAL_YPBPR, RANGE100);
        else
            AFE_ADCAutoCalibration_Old(CAL_YPBPR, RANGE75);

        gTvinAFEParam.flag &= 0x11111110;
        rt = VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam);
        if (rt < 0) {
            ALOGW("TvinApi_ADCAutoCalibration_Old, set vdin param error(%s)", strerror(errno));
        }
    } else {
        if (rangeSel == 0)
            AFE_ADCAutoCalibration_Old(CAL_VGA, RANGE100);
        else
            AFE_ADCAutoCalibration_Old(CAL_VGA, RANGE75);

        gTvinAFEParam.flag &= 0x11111110;

        rt = VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_S_PARM, &gTvinAFEParam);
        if (rt < 0)
            ALOGW("TvinApi_ADCAutoCalibration_Old, set vdin param error(%s)", strerror(errno));
    }
    return rt;
}

int TvinApi_GetMemData(int typeSel)
{
    struct adc_cal_s mem_data = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    return AFE_GetMemData((int) typeSel, &mem_data);
}

int TvinApi_CVBSLockStatus(void)
{
    enum tvafe_cvbs_video_e cvbs_lock_status;
    return AFE_GetCVBSLockStatus(&cvbs_lock_status);
}

int TvinApi_SetCVBSStd(int fmt)
{
    return AFE_SetCVBSStd((unsigned int) fmt);
}

int TvinApi_ADCAutoCalibration(void)
{
    return AFE_ADCAutoCalibration();
}

int TvinApi_ADCGetPara(unsigned char selwin, struct tvin_parm_s *para)
{
    if (selwin > 1 || para == NULL)
        return -1;
    return VDIN_GetVdinParam(selwin, para);
}

int TvinApi_KeepLastFrame(int enable)
{
    return VDIN_KeepLastFrame(enable);
}

int TvinApi_SetBlackOutPolicy(int enable)
{
    return VDIN_SetBlackOut(enable);
}

int TvinApi_SetVideoFreeze(int enable)
{
    return VDIN_SetVideoFreeze(enable);
}

int TvinApi_SetDIBypasshd(int enable)
{
    return VDIN_SetDIBypasshd(enable);
}

int TvinApi_SetDIBypassAll(int enable)
{
    return VDIN_SetDIBypassAll(enable);
}

int TvinApi_SetDIBypassPost(int enable)
{
    return VDIN_SetDIBypassPost(enable);
}

int TvinApi_SetD2D3Bypass(int enable)
{
    return VDIN_SetD2D3Bypass(enable);
}

int TvinApi_SetHDMIEQConfig(int config)
{
    return VDIN_SetHDMIEQConfig(config);
}

int TvinApi_SetVdinFlag(int flag)
{
    return VDIN_SetVdinFlag(flag);
}

int TvinApi_SetRDMA(int enable)
{
    return VDIN_EnableRDMA(enable);
}

int TvinApi_SetStartDropFrameCn(int count)
{
    int ret = -1;
    char set_str[4];

    memset(set_str, 0, 4);
    sprintf(set_str, "%d", count);
    return SetFileAttrValue("/sys/module/di/parameters/start_frame_drop_count", set_str);
}




int TvinApi_SetVdinHVScale(int vdinx, int hscale, int vscale)
{
    int ret = -1;
    char set_str[32];

    memset(set_str, 0, 32);
    sprintf(set_str, "%s %d %d", "hvscaler", hscale, vscale);

    if( vdinx == 0)
        ret = SetFileAttrValue("/sys/class/vdin/vdin0/attr",set_str);
    else
        ret = SetFileAttrValue("/sys/class/vdin/vdin1/attr",set_str);

    return ret;
}

int TvinApi_SetCompPhaseEnable(int enable)
{
    int ret = -1;
    if (enable == 1) {
        ret = SetFileAttrValue("/sys/module/tvin_afe/parameters/enable_dphase","Y");
        ALOGD("%s, enable TvinApi_SetCompPhase.", __FUNCTION__);
    }
    return ret;
}
 int VDIN_GetVdinPortSignal(int port)
{
    int status = 0;
    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_CALLMASTER_SET, &port) < 0) {
        ALOGW("TVIN_IOC_CALLMASTER_SET error(%s)\n", strerror(errno));
        return 0;
    }
    if (VDIN_DeviceIOCtl(CC_SEL_VDIN_DEV_0, TVIN_IOC_CALLMASTER_GET, &status) < 0) {
        ALOGW("TVIN_IOC_CALLMASTER_GET error(%s)\n", strerror(errno));
        return 0;
    }
    //ALOGD("%s, port:%x,status:%d", __FUNCTION__,port,status);

    return status;
}



int TvinApi_GetVdinPortSignal(int port)
{
    return VDIN_GetVdinPortSignal(port);
}


