/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 解扰器模块
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-06: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_mem.h>
#include "am_dsc_internal.h"
#include "../am_adp_internal.h"
#include <assert.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define DSC_DEV_COUNT      (1)

/****************************************************************************
 * Static data
 ***************************************************************************/

#ifdef EMU_DSC
extern const AM_DSC_Driver_t emu_dsc_drv;
#else
extern const AM_DSC_Driver_t aml_dsc_drv;
#endif

static AM_DSC_Device_t dsc_devices[DSC_DEV_COUNT] =
{
#ifdef EMU_DSC
{
.drv = &emu_dsc_drv
}
#else
{
.drv = &aml_dsc_drv
}
#endif
};

/****************************************************************************
 * Static functions
 ***************************************************************************/
 
/**\brief 根据设备号取得设备结构指针*/
static AM_INLINE AM_ErrorCode_t dsc_get_dev(int dev_no, AM_DSC_Device_t **dev)
{
	if((dev_no<0) || (dev_no>=DSC_DEV_COUNT))
	{
		AM_DEBUG(1, "invalid dsc device number %d, must in(%d~%d)", dev_no, 0, DSC_DEV_COUNT-1);
		return AM_DSC_ERR_INVALID_DEV_NO;
	}
	
	*dev = &dsc_devices[dev_no];
	return AM_SUCCESS;
}

/**\brief 根据设备号取得设备结构并检查设备是否已经打开*/
static AM_INLINE AM_ErrorCode_t dsc_get_openned_dev(int dev_no, AM_DSC_Device_t **dev)
{
	AM_TRY(dsc_get_dev(dev_no, dev));
	
	if(!(*dev)->openned)
	{
		AM_DEBUG(1, "dsc device %d has not been openned", dev_no);
		return AM_DSC_ERR_INVALID_DEV_NO;
	}
	
	return AM_SUCCESS;
}

/**\brief 根据ID取得对应解扰通道，并检查通道是否在使用*/
static AM_INLINE AM_ErrorCode_t dsc_get_used_chan(AM_DSC_Device_t *dev, int chan_id, AM_DSC_Channel_t **pchan)
{
	AM_DSC_Channel_t *chan;
	
	if((chan_id<0) || (chan_id>=DSC_CHANNEL_COUNT))
	{
		AM_DEBUG(1, "invalid channel id, must in %d~%d", 0, DSC_CHANNEL_COUNT-1);
		return AM_DSC_ERR_INVALID_ID;
	}
	
	chan = &dev->channels[chan_id];
	
	if(!chan->used)
	{
		AM_DEBUG(1, "channel %d has not been allocated", chan_id);
		return AM_DSC_ERR_NOT_ALLOCATED;
	}
	
	*pchan = chan;
	return AM_SUCCESS;
}

/**\brief 释放解扰通道*/
static AM_ErrorCode_t dsc_free_chan(AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan)
{
	if(!chan->used)
		return AM_SUCCESS;
	
	if(dev->drv->free_chan)
		dev->drv->free_chan(dev, chan);
	
	chan->used = AM_FALSE;
	return AM_SUCCESS;
}

/****************************************************************************
 * API functions
 ***************************************************************************/

/**\brief 打开解扰器设备
 * \param dev_no 解扰器设备号
 * \param[in] para 解扰器设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_Open(int dev_no, const AM_DSC_OpenPara_t *para)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(para);
	
	AM_TRY(dsc_get_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);
	
	if(dev->openned)
	{
		AM_DEBUG(1, "dsc device %d has already been openned", dev_no);
		ret = AM_DSC_ERR_BUSY;
		goto final;
	}
	
	dev->dev_no = dev_no;
	
	if(dev->drv->open)
	{
		ret = dev->drv->open(dev, para);
	}
	
	if(ret==AM_SUCCESS)
	{
		pthread_mutex_init(&dev->lock, NULL);
		dev->openned = AM_TRUE;
	}
final:
	pthread_mutex_unlock(&am_gAdpLock);
	
	return ret;
}

/**\brief 分配一个解扰通道
 * \param dev_no 解扰器设备号
 * \param[out] chan_id 返回解扰通道ID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_AllocateChannel(int dev_no, int *chan_id)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_DSC_Channel_t *chan = NULL;
	int i;
	
	assert(chan_id);
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	for(i=0; i<DSC_CHANNEL_COUNT; i++)
	{
		if(!dev->channels[i].used)
		{
			chan = &dev->channels[i];
			break;
		}
	}
	
	if(i>=DSC_CHANNEL_COUNT)
	{
		AM_DEBUG(1, "too many channels allocated");
		ret = AM_DSC_ERR_NO_FREE_CHAN;
	}
	
	if(ret==AM_SUCCESS)
	{
		chan->id   = i;
		chan->pid  = 0xFFFF;
		chan->used = AM_TRUE;
		
		if(dev->drv->alloc_chan)
		{
			ret = dev->drv->alloc_chan(dev, chan);
			if(ret!=AM_SUCCESS)
			{
				chan->used = AM_FALSE;
			}
		}
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	*chan_id = i;
	
	return ret;
}

/**\brief 设定解扰通道对应流的PID值
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \param pid 流的PID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_SetChannelPID(int dev_no, int chan_id, uint16_t pid)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_DSC_Channel_t *chan;
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dsc_get_used_chan(dev, chan_id, &chan);
	
	if(ret==AM_SUCCESS)
	{
		if(dev->drv->set_pid)
			ret = dev->drv->set_pid(dev, chan, pid);
	}
	
	if(ret==AM_SUCCESS)
		chan->pid = pid;
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 设定解扰通道的控制字
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \param type 控制字类型
 * \param[in] key 控制字
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_SetKey(int dev_no, int chan_id, AM_DSC_KeyType_t type, const uint8_t *key)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_DSC_Channel_t *chan;
	
	assert(key);
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dsc_get_used_chan(dev, chan_id, &chan);
	
	if(ret==AM_SUCCESS)
	{
		if(dev->drv->set_key)
			ret = dev->drv->set_key(dev, chan, type, key);
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 释放一个解扰通道
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_FreeChannel(int dev_no, int chan_id)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	AM_DSC_Channel_t *chan;
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	ret = dsc_get_used_chan(dev, chan_id, &chan);
	
	if(ret==AM_SUCCESS)
	{
		dsc_free_chan(dev, chan);
		chan->used = AM_FALSE;
	}
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

/**\brief 关闭解扰器设备
 * \param dev_no 解扰器设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_Close(int dev_no)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	int i;
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&am_gAdpLock);
	
	for(i=0; i<DSC_CHANNEL_COUNT; i++)
	{
		dsc_free_chan(dev, &dev->channels[i]);
	}
	
	if(dev->drv->close)
	{
		dev->drv->close(dev);
	}
	
	pthread_mutex_destroy(&dev->lock);
	dev->openned = AM_FALSE;
	
	pthread_mutex_unlock(&am_gAdpLock);
	
	return ret;
}

/**\brief 设定解扰器设备的输入源
 * \param dev_no 解扰器设备号
 * \param src 输入源
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
AM_ErrorCode_t AM_DSC_SetSource(int dev_no, AM_DSC_Source_t src)
{
	AM_DSC_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_TRY(dsc_get_openned_dev(dev_no, &dev));
	
	pthread_mutex_lock(&dev->lock);
	
	if(dev->drv->set_source)
		ret = dev->drv->set_source(dev, src);
	
	pthread_mutex_unlock(&dev->lock);
	
	return ret;
}

