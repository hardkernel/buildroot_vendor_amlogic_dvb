#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <am_types.h>
#include <android/log.h>
#include <cutils/log.h>
#include <cutils/log.h>
#include <tvin/tvin_api.h>
#include <errno.h>
#include <sigdetect.h>

#ifndef AM_SUCCESS
#define AM_SUCCESS			(0)
#endif

#ifndef AM_FAILURE
#define AM_FAILURE			(-1)
#endif

#ifndef AM_TRUE
#define AM_TRUE			    (1)
#endif

#ifndef AM_FALSE
#define AM_FALSE			(0)
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "signal"

#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef	 unsigned char 	BOOLEAN; 

typedef struct
{
	BOOLEAN          running;
	pthread_mutex_t    lock;
	pthread_cond_t     cond;
	pthread_t          thread;
}AM_Tvin_Sig_t;

AM_Tvin_Sig_t *tvin_sig_t = NULL;
 
 struct tvin_parm_s   m_tvin_parm_s;
 int decode_start_flag ;
 static int Tvin_StopDecoder(int windowSel)
{
    ALOGD("%s, doing...", __FUNCTION__);
    if (decode_start_flag == AM_TRUE) {
        if (TvinApi_StopDec(windowSel) >= 0) {
            ALOGD("%s, StopDecoder ok!", __FUNCTION__);
            decode_start_flag = AM_FALSE;
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

 static void Tvin_StartDecoder(int windowSel)
 {
    if ((decode_start_flag == AM_FALSE) && (m_tvin_parm_s.info.fmt != TVIN_SIG_FMT_NULL) && (m_tvin_parm_s.info.status == TVIN_SIG_STATUS_STABLE)) 
        if (TvinApi_StartDec(windowSel, m_tvin_parm_s) >= 0) {
            ALOGD("%s, StartDecoder succeed.", __FUNCTION__);
            decode_start_flag  = AM_TRUE;
	    }
 }
 
 static pthread_mutex_t vpp_test_screen_op_mutex = PTHREAD_MUTEX_INITIALIZER;
static int VPP_SetVideoScreenColor(int vdin_blending_mask, int y, int u, int v)
{
    FILE *fp = NULL;
    unsigned long value = 0;

    pthread_mutex_lock(&vpp_test_screen_op_mutex);

    value = vdin_blending_mask << 24;

    value |= (unsigned int) (y << 16) | (unsigned int) (u << 8) | (unsigned int) (v);


    fp = fopen("/sys/class/video/test_screen", "w");
    if (fp == NULL) {
        ALOGE("Open /sys/class/video/test_screen error(%s)!\n", strerror(errno));
        pthread_mutex_unlock(&vpp_test_screen_op_mutex);
        return -1;
    }

    fprintf(fp, "0x%lx", (unsigned long) value);

    fclose(fp);
    fp = NULL;

    pthread_mutex_unlock(&vpp_test_screen_op_mutex);
    return 0;
}
 
static void Tvin_TurnOnBlueScreen(int type)
{
    
        if (type == 0) {
            VPP_SetVideoScreenColor(3, 16, 128, 128); // Show black with vdin0, postblending enabled
        } else if (type == 1) {
            VPP_SetVideoScreenColor(0, 41, 240, 110); // Show blue with vdin0, postblending disabled
        } else {
            VPP_SetVideoScreenColor(0, 16, 128, 128); // Show black with vdin0, postblending disabled
        }
    
}


 
 static void* signal_thread(void *arg)
{
	AM_Tvin_Sig_t *signal = (AM_Tvin_Sig_t*)arg;

	//pthread_mutex_lock(&signal->lock);
	ALOGD("***************signal_thread  signal->running = %d\n",signal->running);   
	while(signal->running)
	{
		TvinApi_GetSignalInfo(0, &(m_tvin_parm_s.info));
		ALOGD("***************signal_thread  m_tvin_parm_s.info.status = %d",m_tvin_parm_s.info.status);   
		if( m_tvin_parm_s.info.status == TVIN_SIG_STATUS_STABLE)
		{
		    Tvin_StartDecoder(0);
		}else
		if (m_tvin_parm_s.info.status== TVIN_SIG_STATUS_UNSTABLE) 
		{
		    Tvin_TurnOnBlueScreen(1);
			Tvin_StopDecoder(0);
		}
		
		if (m_tvin_parm_s.info.status== TVIN_SIG_STATUS_NOSIG) 
		{
		   Tvin_TurnOnBlueScreen(1);
		   Tvin_StopDecoder(0);
		}
		 usleep(50 * 1000);
	}
	//pthread_mutex_unlock(&signal->lock);

	return NULL;
}

typedef enum tvin_path_type_e {
    TV_PATH_TYPE_DEFAULT,
    TV_PATH_TYPE_TVIN,
    TV_PATH_TYPE_TVIN_PREVIEW,
    TV_PATH_TYPE_MAX,
} tvin_path_type_t;

typedef enum tvin_path_id_e {
    TV_PATH_VDIN_AMVIDEO,
    TV_PATH_VDIN_DEINTERLACE_AMVIDEO,
    TV_PATH_VDIN_3D_AMVIDEO,
    TV_PATH_DECODER_3D_AMVIDEO,
    TV_PATH_DECODER_AMVIDEO,
    TV_PATH_VDIN_FREESCALE_AMVIDEO,
    TV_PATH_MAX,
} tvin_path_id_t;

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
                ALOGE("%s, remove default path faild, %d ms gone.", __FUNCTION__, (dly*i));
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
                ALOGE("%s, remove tvin path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
    } else if(pathtype == TV_PATH_TYPE_TVIN_PREVIEW) {
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmPreviewPath();
            if (ret >= 0) {
                ALOGD("%s, remove preview path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGE("%s, remove preview path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }

    }
}

void Tvin_AddPath(tvin_path_id_t pathid)
{
    int ret = -1;
    int i = 0, dly = 10;

    if (pathid >= TV_PATH_VDIN_AMVIDEO && pathid <= TV_PATH_VDIN_3D_AMVIDEO) {
  
        for (i = 0; i < 50; i++) {
            ret = TvinApi_RmTvPath();
            if (ret >= 0) {
                ALOGD("%s, remove tvin path ok, %d ms gone.", __FUNCTION__, (dly*i));
                break;
            } else {
                ALOGE("%s, remove tvin path faild, %d ms gone.", __FUNCTION__, (dly*i));
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
                ALOGE("%s, remove default path faild, %d ms gone.", __FUNCTION__, (dly*i));
                usleep(dly * 1000);
            }
        }
    }

    for (i = 0; i < 50; i++) {
        ret = TvinApi_AddTvPath(pathid);
        if (ret >= 0) {
            ALOGD("%s, add pathid[%d] ok, %d ms gone.", __FUNCTION__, pathid, i);
            break;
        } else {
            ALOGE("%s, add pathid[%d] faild, %d ms gone.", __FUNCTION__, pathid, i);
            usleep(dly * 1000);
        }
    }
}

static int Tv_init_para()
{
     ALOGD("Enter %s.", __FUNCTION__);
    tvin_sig_t = (AM_Tvin_Sig_t*)malloc(sizeof(AM_Tvin_Sig_t));	
	if(!tvin_sig_t)
	{	
		ALOGD("%s******************AM_FAILURE \n", __FUNCTION__);
		return AM_FAILURE;
	}
	memset(tvin_sig_t, 0, sizeof(AM_Tvin_Sig_t));
	tvin_sig_t->running = AM_TRUE;

	pthread_mutex_init(&tvin_sig_t->lock, NULL);
	pthread_cond_init(&tvin_sig_t->cond, NULL);
	
	decode_start_flag = AM_FALSE;
	return AM_SUCCESS;
}

static int Tv_init_vdin()
{

    ALOGD("Enter %s.", __FUNCTION__);
    TvinApi_OpenVDINModule(0);
    Tvin_RemovePath(TV_PATH_TYPE_DEFAULT);
    Tvin_AddPath(TV_PATH_VDIN_DEINTERLACE_AMVIDEO);
	
	struct tvin_parm_s vdin_para;
	memset(&vdin_para, 0, sizeof(vdin_para));
	vdin_para.port = TVIN_PORT_CVBS0;
	
    ALOGD("Enter %s.VDIN_OpenPort", __FUNCTION__);
	VDIN_OpenPort(0,&vdin_para);
	
	return AM_SUCCESS;
}

static int Tv_uninit_vdin(void)
{
    ALOGD("Enter %s.", __FUNCTION__);
	Tvin_StopDecoder(0);
	usleep(10 * 1000);
	TvinApi_ClosePort(0);
    TvinApi_CloseVDINModule(0);  
    return AM_SUCCESS;
}



  AM_ErrorCode_t TvinSigDetect_CreateThread()
{
	ALOGD("******************TvinSigDetect_CreateThread11 \n");
	int ret = AM_SUCCESS;

	Tv_init_para();
    Tv_init_vdin();

	ALOGD("******************running \n");
	if(pthread_create(&tvin_sig_t->thread, NULL, signal_thread, tvin_sig_t))
	{
		ALOGD("******************pthread_create fail \n");
		tvin_sig_t->running = AM_FALSE;
		ret = AM_FAILURE;
	}


	return AM_SUCCESS;
}


/********************************************************/
 AM_ErrorCode_t TvinSigDetect_Stop()
{
    ALOGD("Enter %s.", __FUNCTION__);
	int ret = AM_SUCCESS;
	pthread_t th;
	int wait = AM_FAILURE;

	if(!tvin_sig_t)
	{
		ALOGD("%s.******************fail ",__FUNCTION__);
		return AM_FAILURE;
	}

	pthread_mutex_lock(&tvin_sig_t->lock);
     ALOGD("%s*******tvin_sig_t->running=%d\n",__FUNCTION__,tvin_sig_t->running);
	if(tvin_sig_t->running)
	{
	    ALOGD("%s********running true \n",__FUNCTION__);
		tvin_sig_t->running = AM_FALSE;
		wait = AM_TRUE;
		th = tvin_sig_t->thread;
	}
	pthread_mutex_unlock(&tvin_sig_t->lock);
	pthread_cond_signal(&tvin_sig_t->cond);

	if(wait)
	{
		pthread_join(th, NULL);
	}
    Tv_uninit_vdin();
	return AM_SUCCESS;
}

