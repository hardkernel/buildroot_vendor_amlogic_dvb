/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief AMLogic 解扰器驱动
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-06: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <am_debug.h>
#include <am_mem.h>
#include "../am_dsc_internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/amdsc.h>
#include <string.h>
#include <errno.h>

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define DEV_NAME "/dev/dvb0.dsc0"

/****************************************************************************
 * Static data
 ***************************************************************************/

static AM_ErrorCode_t aml_open (AM_DSC_Device_t *dev, const AM_DSC_OpenPara_t *para);
static AM_ErrorCode_t aml_alloc_chan (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan);
static AM_ErrorCode_t aml_free_chan (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan);
static AM_ErrorCode_t aml_set_pid (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan, uint16_t pid);
static AM_ErrorCode_t aml_set_key (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan, AM_DSC_KeyType_t type, const uint8_t *key);
static AM_ErrorCode_t aml_set_source (AM_DSC_Device_t *dev, AM_DSC_Source_t src);
static AM_ErrorCode_t aml_close (AM_DSC_Device_t *dev);

AM_DSC_Driver_t aml_dsc_drv =
{
.open        = aml_open,
.alloc_chan  = aml_alloc_chan,
.free_chan   = aml_free_chan,
.set_pid     = aml_set_pid,
.set_key     = aml_set_key,
.set_source  = aml_set_source,
.close       = aml_close
};

/****************************************************************************
 * API functions
 ***************************************************************************/

static AM_ErrorCode_t aml_open (AM_DSC_Device_t *dev, const AM_DSC_OpenPara_t *para)
{
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_alloc_chan (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan)
{
	int fd;
	
	fd = open(DEV_NAME, O_RDWR);
	if(fd==-1)
	{
		AM_DEBUG(1, "cannot open \"%s\" (%d:%s)", DEV_NAME, errno, strerror(errno));
		return AM_DSC_ERR_CANNOT_OPEN_DEV;
	}
	else
	{
		AM_DEBUG(2, "open DSC %d", chan->id);
	}
	
	chan->drv_data = (void*)fd;
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_free_chan (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan)
{
	int fd = (int)chan->drv_data;
	
	AM_DEBUG(2, "close DSC %d", chan->id);
	close(fd);
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_set_pid (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan, uint16_t pid)
{
	int fd = (int)chan->drv_data;
	
	if(ioctl(fd, AMDSC_IOC_SET_PID, pid)==-1)
	{
		AM_DEBUG(1, "set pid failed \"%s\"", strerror(errno));
		return AM_DSC_ERR_SYS;
	}
	else
	{
		AM_DEBUG(2, "SET DSC %d PID %d", chan->id, pid);
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_set_key (AM_DSC_Device_t *dev, AM_DSC_Channel_t *chan, AM_DSC_KeyType_t type, const uint8_t *key)
{
	struct am_dsc_key dkey;
	int fd = (int)chan->drv_data;
	
	dkey.type = type;
	memcpy(dkey.key, key, sizeof(dkey.key));
	
	if(ioctl(fd, AMDSC_IOC_SET_KEY, &dkey)==-1)
	{
		AM_DEBUG(1, "set key failed \"%s\"", strerror(errno));
		return AM_DSC_ERR_SYS;
	}
	else
	{
		AM_DEBUG(2, "SET DSC %d KEY %d, %02x %02x %02x %02x %02x %02x %02x %02x", chan->id, type, key[0], key[1], key[2], key[3],
				key[4], key[5], key[6], key[7]);
	}
	
	return AM_SUCCESS;
}

static AM_ErrorCode_t aml_set_source (AM_DSC_Device_t *dev, AM_DSC_Source_t src)
{
	char *cmd;
	
	switch(src)
	{
		case AM_DSC_SRC_DMX0:
			cmd = "dmx0";
		break;
		case AM_DSC_SRC_DMX1:
			cmd = "dmx1";
		break;
		case AM_DSC_SRC_DMX2:
			cmd = "dmx2";
		break;

		default:
			return AM_DSC_ERR_NOT_SUPPORTED;
		break;
	}
	
	return AM_FileEcho("/sys/class/stb/dsc_source", cmd);
}

static AM_ErrorCode_t aml_close (AM_DSC_Device_t *dev)
{
	return AM_SUCCESS;
}

