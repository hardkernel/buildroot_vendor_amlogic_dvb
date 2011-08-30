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

