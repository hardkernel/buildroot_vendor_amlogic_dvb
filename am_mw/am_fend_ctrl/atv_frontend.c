#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_mem.h>
#include <am_misc.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "am_fend.h"
#include "atv_frontend.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
struct AM_ATV_FEND_Device
{
	int 			   dev_no;		  /**< 设备号*/
	int 			   fd;
	int 			   open_count;	  /**< 设备打开次数计数*/
	AM_Bool_t		   enable_thread; /**< 状态监控线程是否运行*/
	pthread_t		   thread;		  /**< 状态监控线程*/
	pthread_mutex_t    lock;		  /**< 设备数据保护互斥体*/
	pthread_cond_t	   cond;		  /**< 状态监控线程控制条件变量*/
	int 			   flags;		  /**< 状态监控线程标志*/
	ATV_FEND_Callback_t cb;			  /**< 状态监控回调函数*/
	int 			   curr_mode;	  /**< 当前解调模式*/
	void			  *user_data;	  /**< 回调函数参数*/
	AM_Bool_t		  enable_cb;	  /**< 允许或者禁止状态监控回调函数*/
};

#define ATV_FEND_FL_RUN_CB        (1)
#define ATV_FEND_WAIT_TIMEOUT  	  (500)

#define ATV_FEND_DEV_COUNT      (1)

typedef struct AM_ATV_FEND_Device AM_ATV_FEND_Device_t;

static AM_ATV_FEND_Device_t atv_fend_device[ATV_FEND_DEV_COUNT];


/****************************************************************************
 * Static functions
 ***************************************************************************/

static AM_INLINE AM_ErrorCode_t atv_fend_get_dev(int dev_no, AM_ATV_FEND_Device_t **dev)
{
	if ((dev_no < 0) || (dev_no >= ATV_FEND_DEV_COUNT))
	{
		AM_DEBUG(1, "invalid atv frontend device number %d, must in(%d~%d)", dev_no, 0, ATV_FEND_DEV_COUNT-1);
		return AM_FEND_ERR_INVALID_DEV_NO;
	}

	*dev = &atv_fend_device[dev_no];
	return AM_SUCCESS;
}

static AM_INLINE AM_ErrorCode_t atv_fend_get_openned_dev(int dev_no, AM_ATV_FEND_Device_t **dev)
{
	AM_TRY(atv_fend_get_dev(dev_no, dev));

	if ((*dev)->open_count <= 0)
	{
		AM_DEBUG(1, "atv frontend device %d has not been openned", dev_no);
		return AM_FEND_ERR_CANNOT_OPEN;
	}
	return AM_SUCCESS;
}
static AM_ErrorCode_t atv_fend_wait_event (AM_ATV_FEND_Device_t *dev, struct v4l2_frontend_event *evt, int timeout)
{
	struct pollfd pfd;
	int ret;

	pfd.fd = dev->fd;
	pfd.events = POLLIN;

	ret = poll(&pfd, 1, timeout);
	if (ret != 1)
	{
		return AM_FEND_ERR_TIMEOUT;
	}
	if (ioctl(dev->fd, V4L2_GET_EVENT, evt) == -1)
	{
		AM_DEBUG(1, "ioctl FE_GET_EVENT failed, error:%s", strerror(errno));
		return AM_FAILURE;
	}
	return AM_SUCCESS;
}

static void* atv_fend_thread(void *arg)
{
	AM_ATV_FEND_Device_t *dev = (AM_ATV_FEND_Device_t*)arg;
	struct v4l2_frontend_event evt;
	AM_ErrorCode_t ret = AM_FAILURE;

	while (dev->enable_thread)
	{
		ret = atv_fend_wait_event(dev, &evt, ATV_FEND_WAIT_TIMEOUT);

		if (dev->enable_thread)
		{
			pthread_mutex_lock(&dev->lock);
			dev->flags |= ATV_FEND_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);

			if (ret == AM_SUCCESS)
			{
				AM_DEBUG(1, "atv_fend_thread wait evt: %x\n", evt.status);
				if (dev->cb && dev->enable_cb)
				{
					dev->cb(dev->dev_no, &evt, dev->user_data);
				}
			}

			pthread_mutex_lock(&dev->lock);
			dev->flags &= ~ATV_FEND_FL_RUN_CB;
			pthread_mutex_unlock(&dev->lock);
			pthread_cond_broadcast(&dev->cond);
		}
	}
	return NULL;
}


/****************************************************************************
 * Functions
 ***************************************************************************/
AM_ErrorCode_t ATV_FEND_Open(int dev_no)
{
	char name[PATH_MAX];
	int ret;
	AM_ATV_FEND_Device_t *dev;
	int rc;

	AM_TRY(atv_fend_get_dev(dev_no, &dev));

	if (dev->open_count > 0)
	{
		AM_DEBUG(1, "atv frontend device %d has already been openned", dev_no);
		dev->open_count++;
		return AM_SUCCESS;
	}

	snprintf(name, sizeof(name), V4L2_FE_DEV);

	dev->fd = open(name, O_RDWR);
	if (dev->fd == -1)
	{
		AM_DEBUG(1, "cannot open %s, error:%s", name, strerror(errno));
		return AM_FEND_ERR_CANNOT_OPEN;
	}
	pthread_mutex_init(&dev->lock, NULL);
	pthread_cond_init(&dev->cond, NULL);

	dev->dev_no = dev_no;
	dev->open_count = 1;
	dev->enable_thread = AM_TRUE;
	dev->flags = 0;
	dev->enable_cb = AM_TRUE;
	dev->cb = NULL;
//	dev->curr_mode = para->mode;

	rc = pthread_create(&dev->thread, NULL, atv_fend_thread, dev);
	if (rc)
	{
		AM_DEBUG(1, "%s", strerror(rc));

		pthread_mutex_destroy(&dev->lock);
		pthread_cond_destroy(&dev->cond);
		dev->open_count = 0;

		ret = AM_FEND_ERR_CANNOT_CREATE_THREAD;
		return ret;
	}

	ioctl(dev->fd, V4L2_SET_MODE, 1);
	dev->open_count++;

	return AM_SUCCESS;
}

AM_ErrorCode_t ATV_FEND_Close(int dev_no)
{
	AM_ATV_FEND_Device_t *dev;

	AM_TRY(atv_fend_get_openned_dev(dev_no, &dev));

	if (dev->open_count == 1)
	{
		int err = 0;

		dev->enable_cb = AM_FALSE;
		/*Stop the thread*/
		dev->enable_thread = AM_FALSE;
		err = pthread_kill(dev->thread, SIGALRM);
		if (err != 0)
			AM_DEBUG(1, "kill fail, err:%d", err);
		pthread_join(dev->thread, NULL);

		pthread_mutex_destroy(&dev->lock);
		pthread_cond_destroy(&dev->cond);

		ioctl(dev->fd, V4L2_SET_MODE, 0);

		close(dev->fd);
		dev->fd = -1;
	}
	dev->open_count--;

	return AM_SUCCESS;
}

AM_ErrorCode_t ATV_FEND_SetCallback(int dev_no, ATV_FEND_Callback_t cb, void *user_data)
{
	AM_ATV_FEND_Device_t *dev;
	AM_ErrorCode_t ret = AM_SUCCESS;

	AM_TRY(atv_fend_get_openned_dev(dev_no, &dev));

	pthread_mutex_lock(&dev->lock);

	if (cb != dev->cb || user_data != dev->user_data)
	{
		if (dev->enable_thread && (dev->thread != pthread_self()))
		{
			while (dev->flags&ATV_FEND_FL_RUN_CB)
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


AM_ErrorCode_t ATV_FEND_SetProp (int dev_no, const struct v4l2_analog_parameters *para)
{
	AM_ATV_FEND_Device_t *dev;

	AM_TRY(atv_fend_get_openned_dev(dev_no, &dev));

	AM_DEBUG(1, "ioctl V4L2_SET_FRONTEND \n");
	if (ioctl(dev->fd, V4L2_SET_FRONTEND, para) == -1)
	{
		AM_DEBUG(1, "ioctl V4L2_SET_FRONTEND failed, error:%s", strerror(errno));
		return -1;
	}
	AM_DEBUG(1, "ioctl V4L2_SET_FRONTEND success \n");

	return AM_SUCCESS;
}

AM_ErrorCode_t ATV_FEND_GetProp (int dev_no, struct v4l2_analog_parameters *para)
{
	AM_ATV_FEND_Device_t *dev;

	AM_TRY(atv_fend_get_openned_dev(dev_no, &dev));

	if (ioctl(dev->fd, V4L2_GET_FRONTEND, para) == -1)
	{
		AM_DEBUG(1, "V4L2_GET_FRONTEND failed, error:%s", strerror(errno));
		return AM_FAILURE;
	}

	return AM_SUCCESS;
}

#if 0
void test_Callback(int dev_no, struct v4l2_frontend_event *evt, void *user_data)
{
	if (!evt || (evt->status == 0))	{
		printf("evt==NULL or evt->status == 0");
		return;
	}
	printf("callback status:%d\n",evt->status);
}

void test_main(void)
{
	static int std, afc, afc_range;
	struct v4l2_analog_parameters para;
	struct v4l2_frontend_event evt;
	int timeout = 500;
	int ret = 0;
	int freq = 0;

	typedef __u64 v4l2_std_id;

	/* one bit for each */
	#define V4L2_STD_PAL_B          ((v4l2_std_id)0x00000001)
	#define V4L2_STD_PAL_B1         ((v4l2_std_id)0x00000002)
	#define V4L2_STD_PAL_G          ((v4l2_std_id)0x00000004)
	#define V4L2_STD_PAL_H          ((v4l2_std_id)0x00000008)
	#define V4L2_STD_PAL_I          ((v4l2_std_id)0x00000010)
	#define V4L2_STD_PAL_D          ((v4l2_std_id)0x00000020)
	#define V4L2_STD_PAL_D1         ((v4l2_std_id)0x00000040)
	#define V4L2_STD_PAL_K          ((v4l2_std_id)0x00000080)

	#define V4L2_STD_PAL_M          ((v4l2_std_id)0x00000100)
	#define V4L2_STD_PAL_N          ((v4l2_std_id)0x00000200)
	#define V4L2_STD_PAL_Nc         ((v4l2_std_id)0x00000400)
	#define V4L2_STD_PAL_60         ((v4l2_std_id)0x00000800)

	#define V4L2_STD_NTSC_M         ((v4l2_std_id)0x00001000)	/* BTSC */
	#define V4L2_STD_NTSC_M_JP      ((v4l2_std_id)0x00002000)	/* EIA-J */
	#define V4L2_STD_NTSC_443       ((v4l2_std_id)0x00004000)
	#define V4L2_STD_NTSC_M_KR      ((v4l2_std_id)0x00008000)	/* FM A2 */

	#define V4L2_STD_SECAM_B        ((v4l2_std_id)0x00010000)
	#define V4L2_STD_SECAM_D        ((v4l2_std_id)0x00020000)
	#define V4L2_STD_SECAM_G        ((v4l2_std_id)0x00040000)
	#define V4L2_STD_SECAM_H        ((v4l2_std_id)0x00080000)
	#define V4L2_STD_SECAM_K        ((v4l2_std_id)0x00100000)
	#define V4L2_STD_SECAM_K1       ((v4l2_std_id)0x00200000)
	#define V4L2_STD_SECAM_L        ((v4l2_std_id)0x00400000)
	#define V4L2_STD_SECAM_LC       ((v4l2_std_id)0x00800000)

	/* ATSC/HDTV */
	#define V4L2_STD_ATSC_8_VSB     ((v4l2_std_id)0x01000000)
	#define V4L2_STD_ATSC_16_VSB    ((v4l2_std_id)0x02000000)

	/*COLOR MODULATION TYPE*/
	#define V4L2_COLOR_STD_PAL	((v4l2_std_id)0x04000000)
	#define V4L2_COLOR_STD_NTSC	((v4l2_std_id)0x08000000)
	#define V4L2_COLOR_STD_SECAM	((v4l2_std_id)0x10000000)

	if (ATV_FEND_Open(0) != 0)
	{
		printf("Open atv device fail");
		return ;
	}

	ATV_FEND_SetCallback(0, test_Callback, NULL);

	printf("please input freq:\n");
	scanf("%d",&freq);

	/*para2 is finetune data */
	printf("std(-1=default): ");
	scanf("%d", &std);
	printf("afc_range(0=off, -1=default): ");
	scanf("%d", &afc_range);
	if (std == -1)
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_D | V4L2_STD_PAL_D1  | V4L2_STD_PAL_K;

	if (afc_range == -1)
		afc_range = 1000000;
	afc = 0;
	if (afc_range == 0)
		afc &= ~ANALOG_FLAG_ENABLE_AFC;
	else
		afc |= ANALOG_FLAG_ENABLE_AFC;


	para.std = std;
	para.afc_range = afc_range;
	para.flag = afc;
	para.frequency = freq;

    printf("freq=%dHz, std=%#X, flag=%x, afc_range=%d\n", freq, std, afc, afc_range);

	ret = ATV_FEND_SetProp(0, &para);
	if (ret != 0)
	{
		printf("ATV_FEND_SetProp fail\n");
		return ;
	}

	while (1)
	{
		sleep(100);
	}
}
#endif
