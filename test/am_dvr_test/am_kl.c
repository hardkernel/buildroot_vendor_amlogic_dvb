#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <am_types.h>
#define MESON_KL_IOC_MAGIC 'k'
#define MESON_KL_RUN              _IOW(MESON_KL_IOC_MAGIC, 13, \
		struct meson_kl_run_args)

enum meson_kl_dest {
	MESON_KL_DEST_OUTPUT_DNONCE,
	MESON_KL_DEST_DESCRAMBLER_EVEN,
	MESON_KL_DEST_DESCRAMBLER_ODD,
	MESON_KL_DEST_CRYPTO_THREAD0,
	MESON_KL_DEST_CRYPTO_THREAD1,
	MESON_KL_DEST_CRYPTO_THREAD2,
	MESON_KL_DEST_CRYPTO_THREAD3,
};

struct meson_kl_run_args {
	__u32 dest;
	__u8 kl_num;
	__u8 kl_levels;
	__u8 __padding[6];
	__u8 keys[7][16];
};

int set_keyladder(unsigned char key2[16], unsigned char key1[16], unsigned char ecw[16])
{
	struct meson_kl_run_args arg;
	int fd;
	int ret;
	fd = open("/dev/keyladder", O_RDWR);
	if (fd <= 0)
		return AM_FAILURE;
	/* Calculate root key and generate the key for even */
	//set keys
	memcpy(arg.keys[2], ecw, 16);
	memcpy(arg.keys[1], key2, 16);
	memcpy(arg.keys[0], key1, 16);
	arg.kl_num = 0; // use the klc0
	arg.kl_levels = 3;
	ret = ioctl(fd, MESON_KL_RUN, &arg);
	close(fd);
	return AM_SUCCESS;
}
