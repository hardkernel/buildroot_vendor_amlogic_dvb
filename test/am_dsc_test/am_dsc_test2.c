#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
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
	char buf[512];
	char *p = buf;
	int fd = open(PID_PATH, O_RDONLY);
	int dsc = 0, src = 0;
	int ret;

	if(argc>1)
		sscanf(argv[1], "%d", &dsc);
	if(argc>2)
		sscanf(argv[2], "%d", &src);

	memset(&dsc_para, 0, sizeof(dsc_para));
	AM_TRY(AM_DSC_Open(dsc, &dsc_para));

	printf("DSC [%d] Set Source [%d]\n", dsc, src);

	ret = AM_DSC_SetSource(dsc, src);
	if(src==AM_DSC_SRC_BYPASS)
		goto end;

	if(fd<0) {
		printf("Can not open "PID_PATH);
		goto end;
	}
	read(fd, buf, 512);
	while((p[0]!='V') || (p[0]!='A'))
	{
		if(p[0]=='V' && p[1]=='i' && p[2]=='d' && p[3]==':')
			sscanf(&p[4], "%d", &vpid);
		else if(p[0]=='A' && p[1]=='i' && p[2]=='d' && p[3]==':')
			sscanf(&p[4], "%d", &apid);
		if(vpid>0 && apid>0)
			break;
		if(p++ == &buf[512])
			break;
	}

	if(vpid>0 || apid>0) {
		char key[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

		//printf("keys(Default/key[format-HH HH HH HH HH HH HH HH]):");
		//p = fgets(buf, sizeof(buf), stdin);
		//if(p[0]!='\n') {
		//}
		if(vpid>0) {
			ret=AM_DSC_AllocateChannel(dsc, &dsccv);
			ret=AM_DSC_SetChannelPID(dsc, dsccv, vpid);
			ret=AM_DSC_SetKey(dsc,dsccv,AM_DSC_KEY_TYPE_ODD, (const uint8_t*)key);
			ret=AM_DSC_SetKey(dsc,dsccv,AM_DSC_KEY_TYPE_EVEN, (const uint8_t*)key);
			printf("set default key for pid[%d]\n", vpid);
		}
		if(apid>0) {
			ret=AM_DSC_AllocateChannel(dsc, &dscca);
			ret=AM_DSC_SetChannelPID(dsc, dscca, apid);
			ret=AM_DSC_SetKey(dsc,dscca,AM_DSC_KEY_TYPE_ODD, (const uint8_t*)key);
			ret=AM_DSC_SetKey(dsc,dscca,AM_DSC_KEY_TYPE_EVEN, (const uint8_t*)key);
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
