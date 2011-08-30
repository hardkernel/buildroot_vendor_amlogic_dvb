/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-06-30: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_time.h>
#include <am_mem.h>
#include "am_smc_internal.h"
#include "../am_adp_internal.h"
#include <assert.h>
#include <unistd.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define SMC_STATUS_SLEEP_TIME (1000)

/****************************************************************************
 * Static data
 ***************************************************************************/

#ifdef EMU_SMC
extern const AM_SMC_Driver_t emu_drv;
#else
extern const AM_SMC_Driver_t aml_smc_drv;
#endif

static AM_SMC_Device_t smc_devices[] =
{
#ifdef EMU_SMC
{
.drv = &emu_drv
}
#else
{
.drv = &aml_smc_drv
}
#endif
};

/**\brief 智能卡设备数*/
#define SMC_DEV_COUNT    AM_ARRAY_SIZE(smc_devices)

/****************************************************************************
 * Static functions
 ***************************************************************************/

/**\brief 根据设备号取得设备结构*/
static AM_INLINE AM_ErrorCode_t smc_get_dev(int dev_no, AM_SMC_Device_t **dev)
{
	if((dev_no<0) || (dev_no>=SMC_DEV_COUNT))
	{
		AM_DEBUG(1, "invalid smartcard device number %d, must in(%d~%d)", dev_no, 0, SMC_DEV_COUNT-1);
		return AM_SMC_ERR_INVALID_DEV_NO;
	}
	
	*dev = &smc_devices[dev_no];
	return AM_SUCCESS;
}

/**\brief 根据设备号取得设备结构并检查设备是否已经打开*/
static AM_INLINE AM_ErrorCode_t smc_get_openned_dev(int dev_no, AM_SMC_Device_t **dev)
{
	AM_TRY(smc_get_dev(dev_no, dev));
	
	if(!(*dev)->openned)
	{
		AM_DEBUG(1, "smartcard device %d has not been openned", dev_no);
		return AM_SMC_ERR_NOT_OPENNED;
	}
	
	return AM_SUCCESS;
}

/**\brief 从智能卡读取数据*/
static AM_ErrorCode_t smc_read(AM_SMC_Device_t *dev, uint8_t *buf, int len, int timeout)
{
	uint8_t *ptr = buf;
	int left = len;
	int now, end = 0, diff, cnt = 0;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	if(timeout>=0)
	{
		AM_TIME_GetClock(&now);
		end = now + timeout;
	}
	
	while (left)
	{
		int tlen = left;
		int ms;
		
		if(timeout>=0)
		{
			ms = end-now;
		}
		else
		{
			ms = -1;
		}
		
		ret = dev->drv->read(dev, ptr, &tlen, ms);
		
		if(ret<0)
		{
			break;
		}
		
		ptr  += tlen;
		left -= tlen;
		cnt  += tlen;
		
		AM_TIME_GetClock(&now);
		diff = now-end;
		if(diff>=0)
		{
			AM_DEBUG(1, "read %d bytes timeout", len);
			ret = AM_SMC_ERR_TIMEOUT;
			break;
		}
	}
	
	return ret;
}

/**\brief 向智能卡发送数据*/
static AM_ErrorCode_t smc_write(AM_SMC_Device_t *dev, const uint8_t *buf, int len, int timeout)
{
	const uint8_t *ptr = buf;
	int left = len;
	int now, end = 0, diff, cnt = 0;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	if(timeout>=0)
	{
		AM_TIME_GetClock(&now);
		end = now + timeout;
	}
	
	while (left)
	{
		int tlen = left;
		int ms;
		
		if(timeout>=0)
		{
			ms = end-now;
		}
		else
		{
			ms = -1;
		}
		
		ret = dev->drv->write(dev, ptr, &tlen, ms);
		
		if(ret<0)
		{
			break;
		}
		
		ptr  += tlen;
		left -= tlen;
		cnt  += tlen;
		
		AM_TIME_GetClock(&now);
		diff = now-end;
		if(diff>=0)
		{
			AM_DEBUG(1, "write %d bytes timeout", len);
			ret = AM_SMC_ERR_TIMEOUT;
			break;
		}
	}
	
	return ret;
}

/**\brief status monitor thread*/
static void*
smc_status_thread(void *arg)
{
	AM_SMC_Device_t *dev = (AM_SMC_Device_t*)arg;
	AM_SMC_CardStatus_t old_status = AM_SMC_CARD_OUT;
	
	while(dev->enable_thread)
	{
		AM_SMC_CardStatus_t status;
		AM_ErrorCode_t ret;
		
		pthread_mutex_lock(&dev->lock);
		
		ret = dev->drv->get_status(dev, &status);
		
		pthread_mutex_unlock(&dev->lock);
		
		if(status!=old_status)
		{
			old_status = status;
			
			pthread_mutex_lock(&dev->lock);
			dev->flags |= SMC_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);
			
			if(dev->cb)
			{
				dev->cb(dev->dev_no, status, dev->user_data);
			}
			
			AM_EVT_Signal(dev->dev_no, (status==AM_SMC_CARD_OUT)?AM_SMC_EVT_CARD_OUT:AM_SMC_EVT_CARD_IN, NULL);
			
			pthread_mutex_lock(&dev->lock);
			dev->flags &= ~SMC_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);
			pthread_cond_broadcast(&dev->cond);
			
			usleep(SMC_STATUS_SLEEP_TIME*1000);
		}
	}
	
	return NULL;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 打开智能卡设备
 * \param dev_no 智能卡设备号
 * \param[in] para 智能卡设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Open(int dev_no, const AM_SMC_OpenPara_t *para)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(smc_get_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);
	
	if(dev->openned)
	{
		AM_DEBUG(1, "smartcard device %d has already been openned", dev_no);
		ret = AM_SMC_ERR_BUSY;
		goto final;
	}
	
	dev->dev_no = dev_no;
	
	if(dev->drv->open)
	{
		AM_TRY_FINAL(dev->drv->open(dev, para));
	}
	
	pthread_mutex_init(&dev->lock, NULL);
	pthread_cond_init(&dev->cond, NULL);
	
	dev->enable_thread = para->enable_thread;
	dev->openned = AM_TRUE;
	dev->flags = 0;
	
	if(dev->enable_thread)
	{
		if(pthread_create(&dev->status_thread, NULL, smc_status_thread, dev))
		{
			AM_DEBUG(1, "cannot create the status monitor thread");
			pthread_mutex_destroy(&dev->lock);
			pthread_cond_destroy(&dev->cond);
			if(dev->drv->close)
			{
				dev->drv->close(dev);
			}
			ret = AM_SMC_ERR_CANNOT_CREATE_THREAD;
			goto final;
		}
	}
	else
	{
		dev->status_thread = (pthread_t)-1;
	}
final:
	pthread_mutex_unlock(&am_gAdpLock);
	
	return ret;
}

/**\brief 关闭智能卡设备
 * \param dev_no 智能卡设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Close(int dev_no)
{
	AM_SMC_Device_t *dev;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);
	
	if(dev->enable_thread)
	{
		dev->enable_thread = AM_FALSE;
		pthread_join(dev->status_thread, NULL);
	}
	
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

/**\brief 得到当前的智能卡插入状态
 * \param dev_no 智能卡设备号
 * \param[out] status 返回智能卡插入状态
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_GetCardStatus(int dev_no, AM_SMC_CardStatus_t *status)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(status);
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_status)
	{
		AM_DEBUG(1, "driver do not support get_status");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->get_status(dev, status);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

/**\brief 复位智能卡
 * \param dev_no 智能卡设备号
 * \param[out] atr 返回智能卡的ATR数据
 * \param[in,out] len 输入ATR缓冲区大小，返回实际的ATR长度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Reset(int dev_no, uint8_t *atr, int *len)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	uint8_t atr_buf[AM_SMC_MAX_ATR_LEN];
	int len_buf;
	
	if(atr && len)
	{
		if(*len<AM_SMC_MAX_ATR_LEN)
		{
			AM_DEBUG(1, "ATR buffer < %d", AM_SMC_MAX_ATR_LEN);
			return AM_SMC_ERR_BUF_TOO_SMALL;
		}
	}
	else
	{
		if(!atr)
			atr = atr_buf;
		if(!len)
			len = &len_buf;
	}
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->reset)
	{
		AM_DEBUG(1, "driver do not support reset");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->reset(dev, atr, len);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

/**\brief 从智能卡读取数据
 *直接从智能卡读取数据，调用函数的线程会阻塞，直到读取到期望数目的数据，或到达超时时间。
 * \param dev_no 智能卡设备号
 * \param[out] data 数据缓冲区
 * \param[in] len 希望读取的数据长度
 * \param timeout 读取超时时间，以毫秒为单位，<0表示永久等待。
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Read(int dev_no, uint8_t *data, int len, int timeout)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(data);
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	ret = smc_read(dev, data, len, timeout);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 向智能卡发送数据
 *直接向智能卡发送数据，调用函数的线程会阻塞，直到全部数据被写入，或到达超时时间。
 * \param dev_no 智能卡设备号
 * \param[in] data 数据缓冲区
 * \param[in] len 希望发送的数据长度
 * \param timeout 读取超时时间，以毫秒为单位，<0表示永久等待。
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Write(int dev_no, const uint8_t *data, int len, int timeout)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(data);
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	ret = smc_write(dev, data, len, timeout);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 按T0协议传输数据
 * \param dev_no 智能卡设备号
 * \param[in] send 发送数据缓冲区
 * \param[in] slen 待发送的数据长度
 * \param[out] recv 接收数据缓冲区
 * \param[out] rlen 返回接收数据的长度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_TransferT0(int dev_no, const uint8_t *send, int slen, uint8_t *recv, int *rlen)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	uint8_t byte;
	uint8_t *dst;
	int left;
	AM_Bool_t sent = AM_FALSE;
	
	assert(send && recv && rlen && (slen>=5));
	
	dst  = recv;
	left = *rlen;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	AM_TRY_FINAL(smc_write(dev, send, 5, 1000));
	
	while(1)
	{
		AM_TRY_FINAL(smc_read(dev, &byte, 1, 1000));
		if(byte==0x60)
		{
			continue;
		}
		else if(((byte&0xF0)==0x60) || ((byte&0xF0)==0x90))
		{
			if(left<2)
			{
				AM_DEBUG(1, "receive buffer must >= %d", 2);
				ret = AM_SMC_ERR_BUF_TOO_SMALL;
				goto final;
			}
			dst[0] = byte;
			AM_TRY_FINAL(smc_read(dev, &dst[1], 1, 1000));
			dst += 2;
			left -= 2;
			break;
		}
		else if(byte==send[1])
		{
			if(!sent)
			{
				int cnt = slen - 5;
			
				if(cnt)
				{
					AM_TRY_FINAL(smc_write(dev, send+5, cnt, 5000));
				}
				else
				{
					cnt = send[4];
					if(!cnt) cnt = 256;
					
					if(left<cnt+2)
					{
						AM_DEBUG(1, "receive buffer must >= %d", cnt+2);
						ret = AM_SMC_ERR_BUF_TOO_SMALL;
						goto final;
					}
					
					AM_TRY_FINAL(smc_read(dev, dst, cnt, 5000));
					dst  += cnt;
					left -= cnt;
				}
				
				sent = AM_TRUE;
			}
			else
			{
				ret = AM_SMC_ERR_IO;
				break;
			}
		}
		else
		{
			ret = AM_SMC_ERR_IO;
			break;
		}
	}
final:
	pthread_mutex_unlock(&dev->lock);
	
	*rlen = dst-recv;
	
	return ret;
}

/**\brief 取得当前的智能卡状态回调函数
 * \param dev_no 智能卡设备号
 * \param[out] cb 返回回调函数指针
 * \param[out] data 返回用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_GetCallback(int dev_no, AM_SMC_StatusCb_t *cb, void **data)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	if(cb)
	{
		*cb = dev->cb;
	}
	
	if(data)
	{
		*data = dev->user_data;
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 设定智能卡状态回调函数
 * \param dev_no 智能卡设备号
 * \param[in] cb 回调函数指针
 * \param[in] data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_SetCallback(int dev_no, AM_SMC_StatusCb_t cb, void *data)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	while((dev->flags&SMC_FL_RUN_CB) && (pthread_self()!=dev->status_thread))
	{
		pthread_cond_wait(&dev->cond, &dev->lock);
	}
	
	dev->cb = cb;
	dev->user_data = data;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 激活智能卡设备
 * \param dev_no 智能卡设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Active(int dev_no)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->active)
	{
		AM_DEBUG(1, "driver do not support active");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->active(dev);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

/**\brief 取消激活智能卡设备
 * \param dev_no 智能卡设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_Deactive(int dev_no)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->deactive)
	{
		AM_DEBUG(1, "driver do not support deactive");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->deactive(dev);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

/**\brief 获取智能卡参数
 * \param dev_no 智能卡设备号
 * \param[out] para 返回智能卡参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_GetParam(int dev_no, AM_SMC_Param_t *para)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->get_param)
	{
		AM_DEBUG(1, "driver do not support get_param");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->get_param(dev, para);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

/**\brief 设定智能卡参数
 * \param dev_no 智能卡设备号
 * \param[in] para 智能卡参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_smc.h)
 */
AM_ErrorCode_t AM_SMC_SetParam(int dev_no, const AM_SMC_Param_t *para)
{
	AM_SMC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(smc_get_openned_dev(dev_no, &dev));
	
	if(!dev->drv->set_param)
	{
		AM_DEBUG(1, "driver do not support set_param");
		ret = AM_SMC_ERR_NOT_SUPPORTED;
	}
	
	if(ret==0)
	{
		pthread_mutex_lock(&dev->lock);
		ret = dev->drv->set_param(dev, para);
		pthread_mutex_unlock(&dev->lock);
	}
	
	return ret;
}

