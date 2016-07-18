#ifndef MESON_AM_KL
#define MESON_AM_KL

#define MESON_KL_DEV "/dev/keyladder"
#define MESON_KL_NUM 0
#define MESON_KL_LEVEL 3
#define MESON_KL_IOC_MAGIC 'k'
#define MESON_KL_RUN              _IOW(MESON_KL_IOC_MAGIC, 13, \
	struct meson_kl_run_args)
#define MESON_KL_CR               _IOWR(MESON_KL_IOC_MAGIC, 14, \
	struct meson_kl_cr_args)

struct meson_kl_config{
	unsigned char ekey5[16];
	unsigned char ekey4[16];
	unsigned char ekey3[16];
	unsigned char ekey2[16];
	unsigned char ekey1[16];
	unsigned char ecw[16];
	int kl_levels;
};

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

struct meson_kl_cr_args {
	__u8 kl_num;
	__u8 __padding[7];
	/* in: challenge-nonce, out:response-dnonce */
	__u8 cr[16];
	/* ekn-1 (e.g. ek2 for 3-key ladder) */
	__u8 ekn1[16];
};

#endif  //end define MESON_AM_KL
