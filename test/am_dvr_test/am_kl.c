#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <am_types.h>
#include "am_kl.h"

static int m_kl_fd = -1;

/* keyladder_init
 * open keyladder device
 *  ret:
 *  	Success:	AM_SUCCESS
 *           Failure:	AM_FAILURE
 */
int keyladder_init()
{
	if(m_kl_fd > 0)
		goto SUCCESS;

	m_kl_fd = open(MESON_KL_DEV, O_RDWR);
	if(m_kl_fd < 0)
		return AM_FAILURE;
SUCCESS:
	return AM_SUCCESS;
}

/* keyladder_release
 * release keyladder device
 */
void keyladder_release()
{
	if(m_kl_fd >= 0){
		close(m_kl_fd);	
		m_kl_fd = -1;
	}
}

/*  set_keyladder:
 *  Currently support 3 levels keyladder, use this function
 *  	to set kl regs, the cw will set to descrambler automatically.
 *  key2:  for 3 levels kl, it means ekn1 
 *  key1:  ekn2
 *  ecw:    ecw
 *  ret:
 *  	Success:	AM_SUCCESS
 *           Failure:	AM_FAILURE
 */
int set_keyladder(struct meson_kl_config *kl_config)
{
	struct meson_kl_run_args arg;
	int ret;	
	if(m_kl_fd<0)
		return AM_FAILURE;
	/* Calculate root key */
	//set keys
	if(kl_config->kl_levels != 3){
		printf("Only support 3 levels keyladder by now\n");
		return AM_FAILURE;
	}else{
		memcpy(arg.keys[2], kl_config->ecw, 16);
		memcpy(arg.keys[1], kl_config->ekey2, 16);
		memcpy(arg.keys[0], kl_config->ekey1, 16);
	}
	arg.kl_num = MESON_KL_NUM; // use the klc0
	arg.kl_levels = kl_config->kl_levels;
	ret = ioctl(m_kl_fd, MESON_KL_RUN, &arg);
	if(ret != -1)
		return AM_SUCCESS;
	else
		return AM_FAILURE;
}

/*  set_keyladder:
 *  Currently support 3 levels keyladder, use this function
 *  	to set kl regs, the cw will set to descrambler automatically.
 *  key2:  for 3 levels kl, it means ekn1
 *  nounce: it is used for challenge
 *  dnounce: response of cr
 *  ret:
 *  	Success:	AM_SUCCESS
 *           Failure:	AM_FAILURE
 */
int set_keyladder_cr(unsigned char key2[16], unsigned nounce[16], unsigned dnounce[16])
{
	int i, ret;
	struct meson_kl_cr_args arg;

	if(m_kl_fd<0)
		return AM_FAILURE;

	memset(&arg, 0, sizeof(arg));
	arg.kl_num = MESON_KL_NUM;
	memcpy(arg.cr, nounce, sizeof(nounce));
	memcpy(arg.ekn1, key2, sizeof(key2));
	ret = ioctl(m_kl_fd, MESON_KL_CR, &arg);
	memcpy(dnounce, arg.cr, 16);
	if(ret != -1)
		return AM_SUCCESS;
	else
		return AM_FAILURE;
}
