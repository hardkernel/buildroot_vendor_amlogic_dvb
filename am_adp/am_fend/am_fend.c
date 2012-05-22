/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief DVB前端设备
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-07: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_mem.h>
#include "am_fend_internal.h"
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <math.h>
#include "../am_adp_internal.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define FEND_DEV_COUNT      (2)
#define FEND_WAIT_TIMEOUT   (500)

#define M_BS_START_FREQ			(950)				/*The start RF frequency, 950MHz*/
#define M_BS_STOP_FREQ				(2150)				/*The stop RF frequency, 2150MHz*/
#define M_TUNERMAXLPF_100KHZ		(440)				/*The tuner lpf, 440 100KHz. difference tuner value is not same. this is AV2011 tuner*/

/****************************************************************************
 * Static data
 ***************************************************************************/

#ifdef LINUX_DVB_FEND
extern const AM_FEND_Driver_t linux_dvb_fend_drv;
#endif
#ifdef EMU_FEND
extern const AM_FEND_Driver_t emu_fend_drv;
#endif

static AM_FEND_Device_t fend_devices[FEND_DEV_COUNT] =
{
#ifdef EMU_FEND
	[0] = {
		.dev_no = 0,
		.drv = &emu_fend_drv,
	},
#if  FEND_DEV_COUNT > 1
	[1] = {
		.dev_no = 1,
		.drv = &emu_fend_drv,
	},
#endif
#elif defined LINUX_DVB_FEND
	[0] = {
		.dev_no = 0,
		.drv = &linux_dvb_fend_drv,
	},
#if  FEND_DEV_COUNT > 1
	[1] = {
		.dev_no = 1,
		.drv = &linux_dvb_fend_drv,
	},
#endif
#endif
};

/****************************************************************************
 * Static functions
 ***************************************************************************/

/**\brief 根据设备号取得设备结构指针*/
static AM_INLINE AM_ErrorCode_t fend_get_dev(int dev_no, AM_FEND_Device_t **dev)
{
	if((dev_no<0) || (dev_no>=FEND_DEV_COUNT))
	{
		AM_DEBUG(1, "invalid frontend device number %d, must in(%d~%d)", dev_no, 0, FEND_DEV_COUNT-1);
		return AM_FEND_ERR_INVALID_DEV_NO;
	}
	
	*dev = &fend_devices[dev_no];
	return AM_SUCCESS;
}

/**\brief 根据设备号取得设备结构并检查设备是否已经打开*/
static AM_INLINE AM_ErrorCode_t fend_get_openned_dev(int dev_no, AM_FEND_Device_t **dev)
{
	AM_TRY(fend_get_dev(dev_no, dev));
	
	if(!(*dev)->openned)
	{
		AM_DEBUG(1, "frontend device %d has not been openned", dev_no);
		return AM_FEND_ERR_INVALID_DEV_NO;
	}
	
	return AM_SUCCESS;
}

/**\brief 检查两个参数是否相等*/
static AM_Bool_t fend_para_equal(fe_type_t type, const struct dvb_frontend_parameters *p1, const struct dvb_frontend_parameters *p2)
{
	if(p1->frequency!=p2->frequency)
		return AM_FALSE;
	
	switch(type)
	{
		case FE_QPSK:
			if(p1->u.qpsk.symbol_rate!=p2->u.qpsk.symbol_rate)
				return AM_FALSE;
		break;
		case FE_QAM:
			if(p1->u.qam.symbol_rate!=p2->u.qam.symbol_rate)
				return AM_FALSE;
			if(p1->u.qam.modulation!=p2->u.qam.modulation)
				return AM_FALSE;
		break;
		case FE_OFDM:
		break;
		case FE_ATSC:
			if(p1->u.vsb.modulation!=p2->u.vsb.modulation)
				return AM_FALSE;
		break;
		default:
			return AM_FALSE;
		break;
	}
	
	return AM_TRUE;
}

/**\brief 前端设备监控线程*/
static void* fend_thread(void *arg)
{
	AM_FEND_Device_t *dev = (AM_FEND_Device_t*)arg;
	struct dvb_frontend_event evt;
	AM_ErrorCode_t ret = AM_FAILURE;
	
	while(dev->enable_thread)
	{
		
		if(dev->drv->wait_event)
		{
			ret = dev->drv->wait_event(dev, &evt, FEND_WAIT_TIMEOUT);
		}
		
		if(dev->enable_thread)
		{
			pthread_mutex_lock(&dev->lock);
			dev->flags |= FEND_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);
		
			if(ret==AM_SUCCESS)
			{
				if(dev->cb)
				{
					dev->cb(dev->dev_no, &evt, dev->user_data);
				}
				
				AM_EVT_Signal(dev->dev_no, AM_FEND_EVT_STATUS_CHANGED, &evt);
			}
		
			pthread_mutex_lock(&dev->lock);
			dev->flags &= ~FEND_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);
			pthread_cond_broadcast(&dev->cond);
		}
	}
	
	return NULL;
}

/**\brief Initializes the blind scan parameters.*/
static AM_ErrorCode_t AM_FEND_IBlindScanAPI_Initialize(int dev_no)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);

	dev->bs_setting.m_uiScan_Start_Freq_MHz = M_BS_START_FREQ;     /*Default Set Blind scan start frequency*/
	dev->bs_setting.m_uiScan_Stop_Freq_MHz = M_BS_STOP_FREQ;     /*Default Set Blind scan stop frequency*/
	dev->bs_setting.m_uiScan_Next_Freq_100KHz = 10 * (dev->bs_setting.m_uiScan_Start_Freq_MHz);

	dev->bs_setting.m_uiScan_Max_Symbolrate_MHz = 45;  /*Set MAX symbol rate*/
	dev->bs_setting.m_uiScan_Min_Symbolrate_MHz = 2;   /*Set MIN symbol rate*/
	
	dev->bs_setting.m_uiTuner_MaxLPF_100kHz = M_TUNERMAXLPF_100KHZ;

	dev->bs_setting.m_uiScan_Bind_No = 0;
	dev->bs_setting.m_uiScan_Progress_Per = 0;
	dev->bs_setting.m_uiChannelCount = 0;
	
	dev->bs_setting.m_eSpectrumMode = INVERSION_OFF;  /*Set spectrum mode*/

	dev->bs_setting.BS_Mode = DVBSx_BS_Slow_Mode; /*1: Freq Step forward is 10MHz        0: Freq Step firmware is 20.7MHz*/
	dev->bs_setting.m_uiScaning = 0;
	dev->bs_setting.m_uiScan_Center_Freq_Step_100KHz = 100;  /*only valid when scan_algorithmic set to 1 and would be ignored when scan_algorithmic set to 0.*/

	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Configures the device to indicate whether the tuner inverts the received signal spectrum.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_SetSpectrumMode(int dev_no, fe_spectral_inversion_t SpectrumMode)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);	
	
	dev->bs_setting.m_eSpectrumMode = SpectrumMode;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Sets the blind scan mode.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_SetScanMode(int dev_no, enum AM_FEND_DVBSx_BlindScanAPI_Mode Scan_Mode)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);	
	
	dev->bs_setting.BS_Mode = Scan_Mode;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Sets the start frequency and stop frequency.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_SetFreqRange(int dev_no, unsigned short StartFreq_MHz, unsigned short EndFreq_MHz)
{	
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);

	dev->bs_setting.m_uiScan_Start_Freq_MHz = StartFreq_MHz;     /*Change default start frequency*/
	dev->bs_setting.m_uiScan_Stop_Freq_MHz = EndFreq_MHz;        /*Change default end frequency*/
	dev->bs_setting.m_uiScan_Next_Freq_100KHz = 10 * (dev->bs_setting.m_uiScan_Start_Freq_MHz);

	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Sets the max low pass filter bandwidth of the tuner.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_SetMaxLPF(int dev_no, unsigned short MaxLPF)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);	
	
	dev->bs_setting.m_uiTuner_MaxLPF_100kHz = MaxLPF;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Performs a blind scan operation. Call the function ::AM_FEND_IBlindScanAPI_GetCurrentScanStatus to check the status of the blind scan operation.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_Start(int dev_no, unsigned int *freq)
{
	assert(freq);
	
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;

	fend_get_openned_dev(dev_no, &dev);
	
	if(!dev->drv->blindscan_scan)
	{
		AM_DEBUG(1, "fronend %d no not support blindscan_scan", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}	

	pthread_mutex_lock(&dev->lock);	
	
	struct dvbsx_blindscanpara * pbsPara = &(dev->bs_setting.bsPara);

	/*?set spectrummode*/ 

	if(dev->bs_setting.BS_Mode)
	{
		pbsPara->m_uifrequency_100khz = 10 * dev->bs_setting.m_uiScan_Start_Freq_MHz + dev->bs_setting.m_uiTuner_MaxLPF_100kHz + (dev->bs_setting.m_uiScan_Bind_No) * dev->bs_setting.m_uiScan_Center_Freq_Step_100KHz;
		pbsPara->m_uistartfreq_100khz = pbsPara->m_uifrequency_100khz - dev->bs_setting.m_uiTuner_MaxLPF_100kHz;
		pbsPara->m_uistopfreq_100khz =  pbsPara->m_uifrequency_100khz + dev->bs_setting.m_uiTuner_MaxLPF_100kHz;
	}
	else
	{
		pbsPara->m_uistartfreq_100khz = dev->bs_setting.m_uiScan_Next_Freq_100KHz;
		pbsPara->m_uistopfreq_100khz = dev->bs_setting.m_uiScan_Next_Freq_100KHz + dev->bs_setting.m_uiTuner_MaxLPF_100kHz * 2;
		pbsPara->m_uifrequency_100khz = (pbsPara->m_uistartfreq_100khz + pbsPara->m_uistopfreq_100khz)/2;
	}

	pbsPara->m_uitunerlpf_100khz = dev->bs_setting.m_uiTuner_MaxLPF_100kHz;
	pbsPara->m_uimaxsymrate_khz = 1000 * (dev->bs_setting.m_uiScan_Max_Symbolrate_MHz);
	pbsPara->m_uiminsymrate_khz = 1000 * (dev->bs_setting.m_uiScan_Min_Symbolrate_MHz);	

	/*driver need to set in blindscan mode*/
	ret = dev->drv->blindscan_scan(dev, pbsPara);
	if(ret != AM_SUCCESS)
	{
		ret = AM_FEND_ERR_BLINDSCAN;
		pthread_mutex_unlock(&dev->lock);
		return ret;	
	}
	
	dev->bs_setting.m_uiScaning = 1;

	/*khz*/
	*freq = (unsigned int)(pbsPara->m_uifrequency_100khz * 100);

	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Queries the blind scan status.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_GetCurrentScanStatus(int dev_no)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->blindscan_getscanstatus)
	{
		AM_DEBUG(1, "fronend %d no not support blindscan_getscanstatus", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}

	pthread_mutex_lock(&dev->lock);

	struct dvbsx_blindscaninfo * pbsInfo = &(dev->bs_setting.bsInfo);
	struct dvbsx_blindscanpara * pbsPara = &(dev->bs_setting.bsPara);	
    
	ret = dev->drv->blindscan_getscanstatus(dev, pbsInfo);  /*Query the internal blind scan procedure information.*/
	if(ret != AM_SUCCESS)
	{
		ret = AM_FEND_ERR_BLINDSCAN;
		pthread_mutex_unlock(&dev->lock);
		return ret;
	}
	
	if(100 == pbsInfo->m_uiProgress)
	{
		dev->bs_setting.m_uiScan_Next_Freq_100KHz = pbsInfo->m_uiNextStartFreq_100kHz;
		dev->bs_setting.m_uiScan_Progress_Per = AM_MIN(100, ((10 * (pbsPara->m_uistopfreq_100khz - 10 * dev->bs_setting.m_uiScan_Start_Freq_MHz))/(dev->bs_setting.m_uiScan_Stop_Freq_MHz - dev->bs_setting.m_uiScan_Start_Freq_MHz)));
		dev->bs_setting.m_uiScan_Bind_No++;	
		dev->bs_setting.m_uiScaning = 0;

		/*driver need to set in demod mode*/
	}
	else
	{
		ret = AM_FEND_ERR_BLINDSCAN_INRUNNING;
	}

	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Reads the channels found during a particular scan from the firmware and stores the new channels found in the scan and filters out the duplicate ones.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_Adjust(int dev_no)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->blindscan_readchannelinfo)
	{
		AM_DEBUG(1, "fronend %d no not support blindscan_readchannelinfo", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}

	pthread_mutex_lock(&dev->lock);

	struct dvbsx_blindscaninfo * pbsInfo = &(dev->bs_setting.bsInfo);
	unsigned short Indext = dev->bs_setting.m_uiChannelCount;
	unsigned short i,j,flag;
	struct dvb_frontend_parameters *pTemp;
	struct dvb_frontend_parameters *pValid;
	unsigned int uiSymbolRate_Hz;
	unsigned int ui_SR_offset;

	if(pbsInfo->m_uiChannelCount > 0)
	{
		ret = dev->drv->blindscan_readchannelinfo(dev, dev->bs_setting.channels_Temp);
		if(ret != AM_SUCCESS)
		{
			ret = AM_FEND_ERR_BLINDSCAN;
			pthread_mutex_unlock(&dev->lock);
			return ret;
		}
	}

	for(i=0; i<pbsInfo->m_uiChannelCount; i++)
	{
		pTemp = &(dev->bs_setting.channels_Temp[i]);
		flag = 0;
		for(j = 0; j < dev->bs_setting.m_uiChannelCount; j++)
		{
			pValid = &(dev->bs_setting.channels[j]);
			if( (AM_ABSSUB(pValid->frequency, pTemp->frequency) * 833) < AM_MIN(pValid->u.qpsk.symbol_rate, pTemp->u.qpsk.symbol_rate) )
			{
				flag = 1;
				break;
			}				
		}

		if(0 == flag)
		{
			dev->bs_setting.channels[Indext].u.qpsk.symbol_rate = pTemp->u.qpsk.symbol_rate;
			dev->bs_setting.channels[Indext].frequency = 1000*((pTemp->frequency+500)/1000);

			uiSymbolRate_Hz = dev->bs_setting.channels[Indext].u.qpsk.symbol_rate;
			/*----------------------------adjust symbol rate offset------------------------------------------------------------*/
			ui_SR_offset = ((uiSymbolRate_Hz%10000)>5000)?(10000-(uiSymbolRate_Hz%10000)):(uiSymbolRate_Hz%10000);
			if( ((uiSymbolRate_Hz>10000000) && (ui_SR_offset<3500)) || ((uiSymbolRate_Hz>5000000) && (ui_SR_offset<2000)) )
				uiSymbolRate_Hz = (uiSymbolRate_Hz%10000<5000)?(uiSymbolRate_Hz - ui_SR_offset):(uiSymbolRate_Hz + ui_SR_offset);

			ui_SR_offset = ((uiSymbolRate_Hz%1000)>500)?(1000-(uiSymbolRate_Hz%1000)):(uiSymbolRate_Hz%1000);
			if( (uiSymbolRate_Hz<5000000) && (ui_SR_offset< 500) )
				uiSymbolRate_Hz = (uiSymbolRate_Hz%1000<500)?(uiSymbolRate_Hz - ui_SR_offset):(uiSymbolRate_Hz + ui_SR_offset);
	
			dev->bs_setting.channels[Indext].u.qpsk.symbol_rate = 1000*(uiSymbolRate_Hz/1000);
			/*-------------------------------------------------------------------------------------------------------------*/
			Indext++;
		}
	}
	
	dev->bs_setting.m_uiChannelCount = Indext;

	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief Stops blind scan process.*/
static AM_ErrorCode_t  AM_FEND_IBlindScanAPI_Exit(int dev_no)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if((!dev->drv->blindscan_getscanstatus) && (!dev->drv->blindscan_cancel))
	{
		AM_DEBUG(1, "fronend %d no not support blindscan_getscanstatus or blindscan_cancel", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}	

	pthread_mutex_lock(&dev->lock);

	struct dvbsx_blindscaninfo * pbsInfo = &(dev->bs_setting.bsInfo);
	
	if(dev->bs_setting.m_uiScaning == 1)
	{
		do
		{
			usleep(50 * 1000);			
			ret = dev->drv->blindscan_getscanstatus(dev, pbsInfo);  /*Query the internal blind scan procedure information.*/
			if(ret != AM_SUCCESS)
			{
				break;
			}
		}while(100 != pbsInfo->m_uiProgress);
	}

	if(ret != AM_SUCCESS)
	{
		pthread_mutex_unlock(&dev->lock);
		return ret;
	}
	
	/*driver need to set in demod mode*/
	ret = dev->drv->blindscan_cancel(dev);

	usleep(10 * 1000);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;	
}

/**\brief Gets the progress of blind scan process based on current scan step's start frequency.*/
static unsigned short AM_FEND_IBlindscanAPI_GetProgress(int dev_no)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	unsigned short process = 0;
		
	AM_TRY(fend_get_openned_dev(dev_no, &dev));

	pthread_mutex_lock(&dev->lock);

	process = dev->bs_setting.m_uiScan_Progress_Per;

	pthread_mutex_unlock(&dev->lock);
	
	return process;
}

/**\brief 前端设备盲扫处理线程*/
static void* fend_blindscan_thread(void *arg)
{
	int dev_no = (int)arg;
	AM_FEND_Device_t *dev = NULL;
	AM_FEND_BlindEvent_t evt;
	AM_ErrorCode_t ret = AM_FAILURE;
	unsigned short index = 0;
	enum AM_FEND_DVBSx_BlindScanAPI_Status BS_Status = DVBSx_BS_Status_Init;

	fend_get_openned_dev(dev_no, &dev);

	while(BS_Status != DVBSx_BS_Status_Exit)
	{
		if(!dev->enable_blindscan_thread)
		{
			BS_Status = DVBSx_BS_Status_Cancel;
		}
		
		switch(BS_Status)
		{
			case DVBSx_BS_Status_Init:			
				{
					BS_Status = DVBSx_BS_Status_Start;
					AM_DEBUG(1, "fend_blindscan_thread %d", DVBSx_BS_Status_Init);					
					break;
				}

			case DVBSx_BS_Status_Start:		
				{			
					ret = AM_FEND_IBlindScanAPI_Start(dev_no, &evt.freq);
					AM_DEBUG(1, "fend_blindscan_thread AM_FEND_IBlindScanAPI_Start %d", ret);
					if(ret != AM_SUCCESS)
					{
						BS_Status = DVBSx_BS_Status_Exit;
					}
					else
					{	
						if(dev->blindscan_cb)
						{
							evt.status = AM_FEND_BLIND_START;
							dev->blindscan_cb(dev->dev_no, &evt, dev->blindscan_cb_user_data);
						}
						BS_Status = DVBSx_BS_Status_Wait;
					}
					break;
				}

			case DVBSx_BS_Status_Wait: 		
				{
					ret = AM_FEND_IBlindScanAPI_GetCurrentScanStatus(dev_no);
					AM_DEBUG(1, "fend_blindscan_thread AM_FEND_IBlindScanAPI_GetCurrentScanStatus %d", ret);
					if(ret == AM_SUCCESS)
					{
						BS_Status = DVBSx_BS_Status_Adjust;
					}
					
					if(ret == AM_FEND_ERR_BLINDSCAN)
					{
						BS_Status = DVBSx_BS_Status_Exit;
					}

					if(ret == AM_FEND_ERR_BLINDSCAN_INRUNNING)
					{
						usleep(100 * 1000);
					}
					break;
				}

			case DVBSx_BS_Status_Adjust:		
				{
					ret = AM_FEND_IBlindScanAPI_Adjust(dev_no);
					AM_DEBUG(1, "fend_blindscan_thread AM_FEND_IBlindScanAPI_Adjust %d", ret);
					if(ret != AM_SUCCESS)
					{
						BS_Status = DVBSx_BS_Status_Exit;
					}
					else
					{
						BS_Status = DVBSx_BS_Status_User_Process;
					}
					break;
				}

			case DVBSx_BS_Status_User_Process:	
				{
					evt.process = (unsigned int)AM_FEND_IBlindscanAPI_GetProgress(dev_no);
					/*
					------------Custom code start-------------------
					customer can add the callback function here such as adding TP information to TP list or lock the TP for parsing PSI
					Add custom code here; Following code is an example
					*/
					AM_DEBUG(1, "fend_blindscan_thread custom cb");
					if(dev->blindscan_cb)
					{
						evt.status = AM_FEND_BLIND_UPDATE;
						dev->blindscan_cb(dev->dev_no, &evt, dev->blindscan_cb_user_data);
					}

					/*------------Custom code end -------------------*/
					if ( (evt.process < 100))
						BS_Status = DVBSx_BS_Status_Start;
					else											
						BS_Status = DVBSx_BS_Status_WaitExit;
					
					break;
				}

			case DVBSx_BS_Status_WaitExit:
				{
					usleep(200*1000);
					break;
				}

			case DVBSx_BS_Status_Cancel:		
				{ 
					ret = AM_FEND_IBlindScanAPI_Exit(dev_no);
					BS_Status = DVBSx_BS_Status_Exit;

					AM_DEBUG(1, "AM_FEND_IBlindScanAPI_Exit");
					break;
				}

			default:						    
				{
					BS_Status = DVBSx_BS_Status_Cancel;
					break;
				}
		}
	}
	
	return NULL;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 打开一个DVB前端设备
 * \param dev_no 前端设备号
 * \param[in] para 设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_Open(int dev_no, const AM_FEND_OpenPara_t *para)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	int rc;
	
	AM_TRY(fend_get_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);
	
	if(dev->openned)
	{
		AM_DEBUG(1, "frontend device %d has already been openned", dev_no);
		ret = AM_FEND_ERR_BUSY;
		goto final;
	}
	
	if(dev->drv->open)
	{
		AM_TRY_FINAL(dev->drv->open(dev, para));
	}
	
	pthread_mutex_init(&dev->lock, NULL);
	pthread_cond_init(&dev->cond, NULL);
	
	dev->dev_no = dev_no;
	dev->openned = AM_TRUE;
	dev->enable_thread = AM_TRUE;
	dev->flags = 0;
	
	rc = pthread_create(&dev->thread, NULL, fend_thread, dev);
	if(rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));
		
		if(dev->drv->close)
		{
			dev->drv->close(dev);
		}
		pthread_mutex_destroy(&dev->lock);
		pthread_cond_destroy(&dev->cond);
		dev->openned = AM_FALSE;
		
		ret = AM_FEND_ERR_CANNOT_CREATE_THREAD;
		goto final;
	}
final:	
	pthread_mutex_unlock(&am_gAdpLock);

	return ret;
}

/**\brief 关闭一个DVB前端设备
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_Close(int dev_no)
{
	AM_FEND_Device_t *dev;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));

	pthread_mutex_lock(&am_gAdpLock);
	
	/*Stop the thread*/
	dev->enable_thread = AM_FALSE;
	pthread_join(dev->thread, NULL);
	
	/*Release the device*/
	if(dev->drv->close)
	{
		dev->drv->close(dev);
	}
	
	pthread_mutex_destroy(&dev->lock);
	pthread_cond_destroy(&dev->cond);
	dev->openned = AM_FALSE;
	
	pthread_mutex_unlock(&am_gAdpLock);
	
	return AM_SUCCESS;
}

/**\brief 取得一个DVB前端设备的相关信息
 * \param dev_no 前端设备号
 * \param[out] info 返回前端信息数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetInfo(int dev_no, struct dvb_frontend_info *info)
{
	AM_FEND_Device_t *dev;
	
	assert(info);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	*info = dev->info;
	
	pthread_mutex_unlock(&dev->lock);
	
	return AM_SUCCESS;;
}

/**\brief 设定前端参数
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_SetPara(int dev_no, const struct dvb_frontend_parameters *para)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_para)
	{
		AM_DEBUG(1, "fronend %d no not support set_para", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->set_para(dev, para);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

AM_ErrorCode_t AM_FEND_SetProp(int dev_no, const struct dtv_properties *prop)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(prop);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_prop)
	{
		AM_DEBUG(1, "fronend %d no not support set_prop", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->set_prop(dev, prop);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 取得当前端设备设定的参数
 * \param dev_no 前端设备号
 * \param[out] para 前端设置参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetPara(int dev_no, struct dvb_frontend_parameters *para)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_para)
	{
		AM_DEBUG(1, "fronend %d no not support get_para", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_para(dev, para);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

AM_ErrorCode_t AM_FEND_GetProp(int dev_no, struct dtv_properties *prop)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(prop);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_prop)
	{
		AM_DEBUG(1, "fronend %d no not support get_prop", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_prop(dev, prop);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}


/**\brief 取得前端设备当前的锁定状态
 * \param dev_no 前端设备号
 * \param[out] status 返回前端设备的锁定状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetStatus(int dev_no, fe_status_t *status)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(status);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_status)
	{
		AM_DEBUG(1, "fronend %d no not support get_status", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_status(dev, status);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 取得前端设备当前的SNR值
 * \param dev_no 前端设备号
 * \param[out] snr 返回SNR值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetSNR(int dev_no, int *snr)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(snr);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_snr)
	{
		AM_DEBUG(1, "fronend %d no not support get_snr", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_snr(dev, snr);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 取得前端设备当前的BER值
 * \param dev_no 前端设备号
 * \param[out] ber 返回BER值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetBER(int dev_no, int *ber)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(ber);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_ber)
	{
		AM_DEBUG(1, "fronend %d no not support get_ber", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_ber(dev, ber);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 取得前端设备当前的信号强度值
 * \param dev_no 前端设备号
 * \param[out] strength 返回信号强度值
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetStrength(int dev_no, int *strength)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(strength);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_strength)
	{
		AM_DEBUG(1, "fronend %d no not support get_strength", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dev->drv->get_strength(dev, strength);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 取得当前注册的前端状态监控回调函数
 * \param dev_no 前端设备号
 * \param[out] cb 返回注册的状态回调函数
 * \param[out] user_data 返回状态回调函数的参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_GetCallback(int dev_no, AM_FEND_Callback_t *cb, void **user_data)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	if(cb)
	{
		*cb = dev->cb;
	}
	
	if(user_data)
	{
		*user_data = dev->user_data;
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 注册前端设备状态监控回调函数
 * \param dev_no 前端设备号
 * \param[in] cb 状态回调函数
 * \param[in] user_data 状态回调函数的参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_SetCallback(int dev_no, AM_FEND_Callback_t cb, void *user_data)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	if(cb!=dev->cb)
	{
		if(dev->enable_thread && (dev->thread!=pthread_self()))
		{
			/*等待回调函数执行完*/
			while(dev->flags&FEND_FL_RUN_CB)
			{
				pthread_cond_wait(&dev->cond, &dev->lock);
			}
		}
		
		dev->cb = cb;
		dev->user_data = user_data;
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief AM_FEND_Lock的回调函数参数*/
typedef struct {
	const struct dvb_frontend_parameters *para;
	fe_status_t                          *status;
	AM_FEND_Callback_t                    old_cb;
	void                                 *old_data;
} fend_lock_para_t;

/**\brief AM_FEND_Lock的回调函数*/
static void fend_lock_cb(int dev_no, struct dvb_frontend_event *evt, void *user_data)
{
	AM_FEND_Device_t *dev = NULL;
	fend_lock_para_t *para = (fend_lock_para_t*)user_data;
	
	fend_get_openned_dev(dev_no, &dev);
	
	if(!fend_para_equal(dev->info.type, &evt->parameters, para->para))
		return;
	
	if(!evt->status)
		return;
	
	*para->status = evt->status;
	
	pthread_mutex_lock(&dev->lock);
	dev->flags &= ~FEND_FL_LOCK;
	pthread_mutex_unlock(&dev->lock);
	
	if(para->old_cb)
	{
		para->old_cb(dev_no, evt, para->old_data);
	}
	
	pthread_cond_broadcast(&dev->cond);
}

/**\brief 设定前端设备参数，并等待参数设定完成
 * \param dev_no 前端设备号
 * \param[in] para 前端设置参数
 * \param[out] status 返回前端设备状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */ 
AM_ErrorCode_t AM_FEND_Lock(int dev_no, const struct dvb_frontend_parameters *para, fe_status_t *status)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	fend_lock_para_t lockp;
	
	assert(para && status);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_para)
	{
		AM_DEBUG(1, "fronend %d no not support set_para", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_Lock in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	/*等待回调函数执行完*/
	while(dev->flags&FEND_FL_RUN_CB)
	{
		pthread_cond_wait(&dev->cond, &dev->lock);
	}
	
	lockp.old_cb   = dev->cb;
	lockp.old_data = dev->user_data;
	lockp.para   = para;
	lockp.status = status;
	
	dev->cb = fend_lock_cb;
	dev->user_data = &lockp;
	dev->flags |= FEND_FL_LOCK;
	
	ret = dev->drv->set_para(dev, para);
	
	if(ret==AM_SUCCESS)
	{
		/*等待回调函数执行完*/
		while((dev->flags&FEND_FL_RUN_CB) || (dev->flags&FEND_FL_LOCK))
		{
			pthread_cond_wait(&dev->cond, &dev->lock);
		}
	}
	
	dev->cb = lockp.old_cb;
	dev->user_data = lockp.old_data;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 设定前端管理线程的检测间隔
 * \param dev_no 前端设备号
 * \param delay 间隔时间(单位为毫秒)，0表示没有间隔，<0表示前端管理线程暂停工作
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_SetThreadDelay(int dev_no, int delay)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_para)
	{
		AM_DEBUG(1, "fronend %d no not support set_para", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_Lock in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->set_delay)
		ret = dev->drv->set_delay(dev, delay);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 重置数字卫星设备控制
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_DiseqcResetOverload(int dev_no)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->diseqc_reset_overload)
	{
		AM_DEBUG(1, "fronend %d no not support diseqc_reset_overload", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_DiseqcResetOverload in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->diseqc_reset_overload)
		ret = dev->drv->diseqc_reset_overload(dev);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 发送数字卫星设备控制命令
 * \param dev_no 前端设备号 
 * \param[in] cmd 数字卫星设备控制命令
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_DiseqcSendMasterCmd(int dev_no, struct dvb_diseqc_master_cmd* cmd)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(cmd);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->diseqc_send_master_cmd)
	{
		AM_DEBUG(1, "fronend %d no not support diseqc_send_master_cmd", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_DiseqcSendMasterCmd in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->diseqc_send_master_cmd)
		ret = dev->drv->diseqc_send_master_cmd(dev, cmd);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 接收数字卫星设备控制2.0命令回应
 * \param dev_no 前端设备号 
 * \param[out] reply 数字卫星设备控制回应
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_DiseqcRecvSlaveReply(int dev_no, struct dvb_diseqc_slave_reply* reply)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(reply);
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->diseqc_recv_slave_reply)
	{
		AM_DEBUG(1, "fronend %d no not support diseqc_recv_slave_reply", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_DiseqcRecvSlaveReply in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->diseqc_recv_slave_reply)
		ret = dev->drv->diseqc_recv_slave_reply(dev, reply);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;
}

/**\brief 发送数字卫星设备控制tone burst
 * \param dev_no 前端设备号 
 * \param tone burst控制方式

 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_DiseqcSendBurst(int dev_no, fe_sec_mini_cmd_t minicmd)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
 	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->diseqc_send_burst)
	{
		AM_DEBUG(1, "fronend %d no not support diseqc_send_burst", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_DiseqcSendBurst in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->diseqc_send_burst)
		ret = dev->drv->diseqc_send_burst(dev, minicmd);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;
}

/**\brief 设置卫星设备tone模式
 * \param dev_no 前端设备号 
 * \param tone 卫星设备tone模式
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_SetTone(int dev_no, fe_sec_tone_mode_t tone)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
 	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_tone)
	{
		AM_DEBUG(1, "fronend %d no not support set_tone", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_SetTone in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->set_tone)
		ret = dev->drv->set_tone(dev, tone);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;
}

/**\brief 设置卫星设备控制电压
 * \param dev_no 前端设备号 
 * \param voltage 卫星设备控制电压 
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_SetVoltage(int dev_no, fe_sec_voltage_t voltage)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
 	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_voltage)
	{
		AM_DEBUG(1, "fronend %d no not support set_voltage", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_SetVoltage in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->set_voltage)
		ret = dev->drv->set_voltage(dev, voltage);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;
}

/**\brief 控制卫星设备LNB高电压
 * \param dev_no 前端设备号 
 * \param arg 0表示禁止，!=0表示允许
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_EnableHighLnbVoltage(int dev_no, long arg)
{
	AM_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
 	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->enable_high_lnb_voltage)
	{
		AM_DEBUG(1, "fronend %d no not support enable_high_lnb_voltage", dev_no);
		return AM_FEND_ERR_NOT_SUPPORTED;
	}
	
	if(dev->thread==pthread_self())
	{
		AM_DEBUG(1, "cannot invoke AM_FEND_EnableHighLnbVoltage in callback");
		return AM_FEND_ERR_INVOKE_IN_CB;
	}
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->enable_high_lnb_voltage)
		ret = dev->drv->enable_high_lnb_voltage(dev, arg);
	
	pthread_mutex_unlock(&dev->lock);

	return ret;
}

/**\brief 转化信号强度dBm值为百分比(NorDig)
 * \param rf_power_dbm dBm值
 * \param constellation 调制模式
 * \param code_rate 码率
 * \return 百分比值
 */
int AM_FEND_CalcTerrPowerPercentNorDig(short rf_power_dbm, fe_modulation_t constellation, fe_code_rate_t code_rate)
{
	int order = 0;
	int cr=0;
	int constl=0;   
	int P_ref,P_rel,SSI;
	short table[15] = {-93 ,-91 ,-90 ,-89 ,-88 ,-87 ,-85 ,-84 ,-83 ,-82 ,-82 ,-80 ,-78 ,-77 ,-76 };

	// tps_constell 0 is QPSK, 1 is 16Qam, 2 is 64QAM
	// tps_HP_cr 0 is 1/2 , 1 is 2/3 , 2 is 3/4, 3 is 5/6 , 4 is 7/8
	// 0x78[9] : 0 is HP, 1 is LP
	constl = (constellation==QPSK)? 0 : 
		(constellation==QAM_16)? 1 :
		(constellation==QAM_64)? 2 :
		0;

	cr     = (code_rate==FEC_1_2)? 0 : 
		(code_rate==FEC_2_3)? 1 :
		(code_rate==FEC_3_4)? 2 :
		(code_rate==FEC_5_6)? 3 :
		(code_rate==FEC_7_8)? 4 :
		0;
 
	constl = (constl==3)?0:constl;
	cr     = (cr     >4)?0:cr;
	order = 5*constl + cr;
	P_ref = table[order];
	P_rel = rf_power_dbm - P_ref;

	if (P_rel < -15)                                 SSI = 0;
	else if ((P_rel >= -15)&&(P_rel<0))  SSI = (P_rel+15)*2/3;
	else if ((P_rel >=   0)&&(P_rel<20))  SSI = 4 * P_rel +10;
	else if ((P_rel >=  20)&&(P_rel<35))  SSI = (P_rel -20)*2/3 + 90;
	else                                                 SSI = 100;
	  
	return SSI;
}

/**\brief 转化C/N值为百分比(NorDig)
 * \param cn C/N值
 * \param constellation 调制模式
 * \param code_rate 码率
 * \param hierarchy 等级调制参数
 * \param isLP 低优先级模式
 * \return 百分比值
 */
int AM_FEND_CalcTerrCNPercentNorDig(float cn, int ber, fe_modulation_t constellation, fe_code_rate_t code_rate, fe_hierarchy_t hierarchy, int isLP)
{
	int tps_constell, tps_cr;
	int order = 0;
	int cr=0;
	int constl=0;
	int CN_ref,CN_rel,SQI,BER_SQI;
	float table[15] = {5.1,6.9,7.9,8.9,9.7,10.8,13.1,14.6,15.6,16.0,16.5,18.7,20.2,21.6,22.5};

	// Read TPS info 
	tps_constell = (constellation==QPSK)? 0 : 
		(constellation==QAM_16)? 1 :
		(constellation==QAM_64)? 2 :
		0;

	tps_cr = (code_rate==FEC_1_2)? 0 : 
		(code_rate==FEC_2_3)? 1 :
		(code_rate==FEC_3_4)? 2 :
		(code_rate==FEC_5_6)? 3 :
		(code_rate==FEC_7_8)? 4 :
		0;
	  
	  // tps_constell 0 is QPSK, 1 is 16Qam, 2 is 64QAM
	  // tps_HP_cr 0 is 1/2 , 1 is 2/3 , 2 is 3/4, 3 is 5/6 , 4 is 7/8
	  // 0x78[9] : 0 is HP, 1 is LP
	  if (hierarchy==1){
	  	  constl = tps_constell;
	  	  cr     = tps_cr;
	  }
	  else if (isLP){
	      constl = (tps_constell>0)?tps_constell - 1: 0; 	  
	      cr     = tps_cr;
	  }
	  else {
	  	  constl = 0;
	  	  cr     = tps_cr;
	  }
  
	  constl = (constl==3)?0:constl;
	  cr     = (cr     >4)?0:cr;
	  order = 5*constl + cr;
	  CN_ref = table[order];
	  CN_rel = cn - CN_ref;
	  
	  // BER unit is 10^-7
	  if (ber > 10000)	  BER_SQI = 0;
	  else if (ber > 1)  BER_SQI = 100 - 20*log10(ber);
	  else               BER_SQI = 100;
	  
	  if (CN_rel < -7)            	                      SQI = 0;
	  else if ((CN_rel >=  -7)&&(CN_rel< 3))    SQI = ((CN_rel-3)/10.0 + 1)*BER_SQI;
	  else                                                        SQI = BER_SQI;
	  
	  return SQI;
}

/**\brief 卫星盲扫开始  
 * \param dev_no 前端设备号
 * \param[in] cb 盲扫回调函数
 * \param[in] user_data 状态回调函数的参数
 * \param start_freq 开始频点 unit HZ
 * \param stop_freq 结束频点 unit HZ
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_BlindScan(int dev_no, AM_FEND_BlindCallback_t cb, void *user_data, unsigned int start_freq, unsigned int stop_freq)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	int rc;

	if(start_freq == stop_freq)
	{
		AM_DEBUG(1, "AM_FEND_BlindScan start_freq equal stop_freq\n");
		ret = AM_FEND_ERR_BLINDSCAN;
		return ret;
	}

	/*this function set the parameters blind scan process needed.*/	
	AM_FEND_IBlindScanAPI_Initialize(dev_no);

	AM_FEND_IBlindScanAPI_SetFreqRange(dev_no, start_freq/(1000 * 1000), stop_freq/(1000 * 1000));

	AM_FEND_IBlindScanAPI_SetScanMode(dev_no, DVBSx_BS_Slow_Mode);

	/*Default set is INVERSION_OFF, it must be set correctly according Board HW configuration*/
	AM_FEND_IBlindScanAPI_SetSpectrumMode(dev_no, INVERSION_OFF); 

	/*Set Tuner max LPF value, this value will difference according tuner type*/
	AM_FEND_IBlindScanAPI_SetMaxLPF(dev_no, M_TUNERMAXLPF_100KHZ); 

	/*blindscan handle thread*/	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);

	if(cb!=dev->blindscan_cb)
	{
		dev->blindscan_cb = cb;
		dev->blindscan_cb_user_data = user_data;
	}

	dev->enable_blindscan_thread = AM_TRUE;
	
	rc = pthread_create(&dev->blindscan_thread, NULL, fend_blindscan_thread, (void *)dev_no);
	if(rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));
		ret = AM_FEND_ERR_BLINDSCAN;
	}

	pthread_mutex_unlock(&am_gAdpLock);
	
	return ret;
}

/**\brief 卫星盲扫结束
 * \param dev_no 前端设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_BlindExit(int dev_no)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(fend_get_openned_dev(dev_no, &dev));

	pthread_mutex_lock(&am_gAdpLock);
	
	/*Stop the thread*/
	dev->enable_blindscan_thread = AM_FALSE;
	pthread_join(dev->blindscan_thread, NULL);
	
	pthread_mutex_unlock(&am_gAdpLock);

	return ret;
}


/**\brief 卫星盲扫进度 
 * \param dev_no 前端设备号
 * \param[out] process 盲扫进度0-100
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_BlindGetProcess(int dev_no, unsigned int *process)
{
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(process);

	*process = (unsigned int)AM_FEND_IBlindscanAPI_GetProgress(dev_no);

	return ret;
}

/**\brief 卫星盲扫信息 
 * \param dev_no 前端设备号
 * \param[out] para 盲扫频点信息缓存区
 * \param[in out] para in 盲扫频点信息缓存区大小，out 盲扫频点个数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_fend.h)
 */
AM_ErrorCode_t AM_FEND_BlindGetTPInfo(int dev_no, struct dvb_frontend_parameters *para, unsigned int *count)
{
	AM_FEND_Device_t *dev = NULL;
	AM_ErrorCode_t ret = AM_SUCCESS;

	assert(para);
	assert(count);
		
	AM_TRY(fend_get_openned_dev(dev_no, &dev));

	pthread_mutex_lock(&dev->lock);

	if(*count > dev->bs_setting.m_uiChannelCount)
	{
		*count = (unsigned int)(dev->bs_setting.m_uiChannelCount);
	}
	
	memcpy(para, dev->bs_setting.channels, (*count) * sizeof(struct dvb_frontend_parameters));

	pthread_mutex_unlock(&dev->lock);

	return ret;
}


