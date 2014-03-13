#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "pthread.h"
#include "memwatch.h"
#include "semaphore.h"
#include <am_ad.h>
#include <am_misc.h>
#include <am_dmx.h>
#include <am_time.h>
#include <am_debug.h>

typedef struct{
	AM_AD_Para_t para;
	int          filter;
}AM_AD_Data_t;

static void ad_callback (int dev_no, int fhandle, const uint8_t *data, int len, void *user_data)
{
	AM_AD_Data_t *ad = (AM_AD_Data_t*)user_data;
	AM_DEBUG(1, "ad data %d", len);
}

AM_ErrorCode_t
AM_AD_Create(AM_AD_Handle_t *handle, AM_AD_Para_t *para)
{
	AM_AD_Data_t *ad = NULL;
	AM_Bool_t dmx_openned = AM_FALSE;
	AM_DMX_OpenPara_t dmx_para;
	struct dmx_pes_filter_params pes;
	AM_ErrorCode_t ret;

	if(!handle || !para){
		ret = AM_AD_ERR_INVALID_PARAM;
		goto error;
	}

	ad = (AM_AD_Data_t*)malloc(sizeof(AM_AD_Data_t));
	if(!ad){
		ret = AM_AD_ERR_NO_MEM;
		goto error;
	}

	ad->para   = *para;
	ad->filter = -1;

	memset(&dmx_para, 0, sizeof(dmx_para));

	ret = AM_DMX_Open(para->dmx_id, &dmx_para);
	if(ret != AM_SUCCESS){
		goto error;
	}
	dmx_openned = AM_TRUE;

	memset(&pes, 0, sizeof(pes));
	pes.pid = para->pid;
	pes.output = DMX_OUT_TAP;
	pes.pes_type = DMX_PES_TELETEXT0;

	ret = AM_DMX_AllocateFilter(para->dmx_id, &ad->filter);
	if(ret != AM_SUCCESS)
		goto error;

	ret = AM_DMX_SetPesFilter(para->dmx_id, ad->filter, &pes);
	if(ret != AM_SUCCESS)
		goto error;

	ret = AM_DMX_SetCallback(para->dmx_id, ad->filter, ad_callback, ad);
	if(ret != AM_SUCCESS)
		goto error;

	*handle = (AM_AD_Handle_t)ad;
	return AM_SUCCESS;
error:
	if(ad && (ad->filter != -1))
		AM_DMX_FreeFilter(para->dmx_id, ad->filter);
	if(dmx_openned)
		AM_DMX_Close(para->dmx_id);
	if(ad)
		free(ad);
	return ret;
}

AM_ErrorCode_t
AM_AD_Destroy(AM_AD_Handle_t handle)
{
	AM_AD_Data_t *ad = (AM_AD_Data_t*)handle;

	if(!ad)
		return AM_AD_ERR_INVALID_HANDLE;

	AM_DMX_FreeFilter(ad->para.dmx_id, ad->filter);
	AM_DMX_Close(ad->para.dmx_id);
	free(ad);
	return AM_SUCCESS;
}

AM_ErrorCode_t
AM_AD_Start(AM_AD_Handle_t handle)
{
	AM_AD_Data_t *ad = (AM_AD_Data_t*)handle;

	if(!ad)
		return AM_AD_ERR_INVALID_HANDLE;

	AM_DMX_StartFilter(ad->para.dmx_id, ad->filter);

	return AM_SUCCESS;
}

AM_ErrorCode_t
AM_AD_Stop(AM_AD_Handle_t handle)
{
	AM_AD_Data_t *ad = (AM_AD_Data_t*)handle;

	if(!ad)
		return AM_AD_ERR_INVALID_HANDLE;

	AM_DMX_StopFilter(ad->para.dmx_id, ad->filter);

	return AM_SUCCESS;
}

AM_ErrorCode_t
AM_AD_SetVolume(AM_AD_Handle_t handle, int vol)
{
	AM_AD_Data_t *ad = (AM_AD_Data_t*)handle;

	if(!ad)
		return AM_AD_ERR_INVALID_HANDLE;

	return AM_SUCCESS;
}

