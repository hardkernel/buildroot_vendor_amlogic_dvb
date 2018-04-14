#ifndef __ATV_FRONTEND_H__
#define __ATV_FRONTEND_H__

#include <linux/videodev2.h>

#define V4L2_FE_DEV    "/dev/v4l2_frontend"

enum v4l2_status {
	V4L2_HAS_SIGNAL  = 0x01, /* found something above the noise level */
	V4L2_HAS_CARRIER = 0x02, /* found a DVB signal */
	V4L2_HAS_VITERBI = 0x04, /* FEC is stable  */
	V4L2_HAS_SYNC    = 0x08, /* found sync bytes  */
	V4L2_HAS_LOCK    = 0x10, /* everything's working... */
	V4L2_TIMEDOUT    = 0x20, /* no lock within the last ~2 seconds */
	V4L2_REINIT      = 0x40, /* frontend was reinitialized, */
};							 /* application is recommended to reset */
							 /* DiSEqC, tone and parameters */

struct v4l2_analog_parameters {
	unsigned int frequency;
	unsigned int audmode;	/* audio mode standard */
	unsigned int soundsys;	/*A2,BTSC,EIAJ,NICAM */
	v4l2_std_id std;	/* v4l2 analog video standard */
	unsigned int flag;
	unsigned int afc_range;
	unsigned int reserved;
};

struct v4l2_frontend_event {
	enum v4l2_status status;
	struct v4l2_analog_parameters parameters;
};

#define V4L2_SET_FRONTEND    _IOW('V', 105, struct v4l2_analog_parameters)
#define V4L2_GET_FRONTEND    _IOR('V', 106, struct v4l2_analog_parameters)
#define V4L2_GET_EVENT       _IOR('V', 107, struct v4l2_frontend_event)
#define V4L2_SET_MODE        _IO('V', 108) /* 1 : entry atv, 0 : leave atv */
#define V4L2_READ_STATUS     _IOR('V', 109, enum v4l2_status)

 /* audmode */


typedef void (*ATV_FEND_Callback_t) (int dev_no, struct v4l2_frontend_event *evt, void *user_data);

AM_ErrorCode_t ATV_FEND_Open(int dev_no);
AM_ErrorCode_t ATV_FEND_Close(int dev_no);
AM_ErrorCode_t ATV_FEND_SetCallback(int dev_no, ATV_FEND_Callback_t cb, void *user_data);
AM_ErrorCode_t ATV_FEND_SetProp (int dev_no, const struct v4l2_analog_parameters *para);
AM_ErrorCode_t ATV_FEND_GetProp (int dev_no, struct v4l2_analog_parameters *para);

#endif /* __V4L2_FRONTEND_H__ */
