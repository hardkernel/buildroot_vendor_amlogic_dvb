#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:

 */
/**\file
 * \brief 解扰器测试程序
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-10-08: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 5

#include <am_debug.h>
#include <am_dsc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "fcntl.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

#define PID_PATH "/sys/class/amstream/ports"

/****************************************************************************
 * API functions
 ***************************************************************************/

int main(int argc, char **argv)
{
	AM_DSC_OpenPara_t dsc_para;
	int dsccv, dscca;
	int vpid=0, apid=0;
	char buf[1024];
	char *p = buf;
	int fd = open(PID_PATH, O_RDONLY);
	int dsc = 0, src = 0;
	int ret;
	int aes = 0, des = 0;
	int odd_type = AM_DSC_KEY_TYPE_ODD;
	int even_type = AM_DSC_KEY_TYPE_EVEN;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "vid", 3))
			sscanf(argv[i], "vid=%i", &vpid);
		else if (!strncmp(argv[i], "aid", 3))
			sscanf(argv[i], "aid=%i", &apid);
		else if (!strncmp(argv[i], "dsc", 3))
			sscanf(argv[i], "dsc=%i", &dsc);
		else if (!strncmp(argv[i], "src", 3))
			sscanf(argv[i], "src=%i", &src);
		else if (!strncmp(argv[i], "aes", 3))
			aes = 1;
		else if (!strncmp(argv[i], "des", 3))
			des = 1;
		else if (!strncmp(argv[i], "help", 4)) {
			printf("Usage: %s [vid=pid] [aid=pid] [dsc=n] [src=n] [aes|des]\n", argv[0]);
			printf("\t if no v/a specified, will set to current running v/a\n");
			exit(0);
		}
	}

	printf("use dsc[%d] src[%d]\n", dsc, src);
	if (aes) {
		printf("aes mode\n");
		odd_type = AM_DSC_KEY_TYPE_AES_ODD;
		even_type = AM_DSC_KEY_TYPE_AES_EVEN;
	} else if (des) {
		printf("des mode\n");
		odd_type = AM_DSC_KEY_TYPE_DES_ODD;
		even_type = AM_DSC_KEY_TYPE_DES_EVEN;
	} else {
		printf("csa mode\n");
	}

	memset(&dsc_para, 0, sizeof(dsc_para));
	AM_TRY(AM_DSC_Open(dsc, &dsc_para));

	printf("DSC [%d] Set Source [%d]\n", dsc, src);

	ret = AM_DSC_SetSource(dsc, src);
	if(src==AM_DSC_SRC_BYPASS)
		goto end;

	if (!vpid && !apid) {
		if(fd<0) {
			printf("Can not open "PID_PATH);
			goto end;
		}
		read(fd, buf, 1024);
		p = strstr(buf, "amstream_mpts");
		while (p >= buf && p < (buf+1024))
		{
			while((p[0]!='V') && (p[0]!='A'))
				p++;
			if(p[0]=='V' && p[1]=='i' && p[2]=='d' && p[3]==':')
				sscanf(&p[4], "%d", &vpid);
			else if(p[0]=='A' && p[1]=='i' && p[2]=='d' && p[3]==':')
				sscanf(&p[4], "%d", &apid);
			if(vpid>0 && apid>0)
				break;
			p++;
		}
	}

	if(vpid>0 || apid>0) {
		char aes_key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
		char des_key[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
		/*char csa_key[8] = {0x11, 0x22, 0x33, 0x66, 0x55, 0x66, 0x77, 0x32};*/
		char csa_key[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
		char *key = NULL;

		if (aes) {
			key = aes_key;
			printf("use AES key\n");
		} else if (des) {
			key = des_key;
			printf("use DES key\n");
		} else {
			key = csa_key;
			printf("use CSA key\n");
		}

#define AM_CHECK_ERR(_x) do {\
            AM_ErrorCode_t ret = (_x);\
            if (ret != AM_SUCCESS)\
                printf("ERROR (0x%x) %s\n", ret, #_x);\
        } while(0)

		if(vpid>0) {
			AM_CHECK_ERR(AM_DSC_AllocateChannel(dsc, &dsccv));
			AM_CHECK_ERR(AM_DSC_SetChannelPID(dsc, dsccv, vpid));
			AM_CHECK_ERR(AM_DSC_SetKey(dsc,dsccv,odd_type, (const uint8_t*)key));
			AM_CHECK_ERR(AM_DSC_SetKey(dsc,dsccv,even_type, (const uint8_t*)key));
			printf("set default key for pid[%d]\n", vpid);
		}
		if(apid>0) {
			AM_CHECK_ERR(AM_DSC_AllocateChannel(dsc, &dscca));
			AM_CHECK_ERR(AM_DSC_SetChannelPID(dsc, dscca, apid));
			AM_CHECK_ERR(AM_DSC_SetKey(dsc,dscca,odd_type, (const uint8_t*)key));
			AM_CHECK_ERR(AM_DSC_SetKey(dsc,dscca,even_type, (const uint8_t*)key));
			printf("set default key for pid[%d]\n", apid);
		}

		while(fgets(buf, 256, stdin))
		{
			if(!strncmp(buf, "quit", 4))
			{
				goto end;
			}
		}
	} else { 		
		printf("No A/V playing.\n");
	}

end:
	AM_DSC_Close(dsc);

	return 0;
}
