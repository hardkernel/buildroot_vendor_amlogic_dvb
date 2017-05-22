#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief USER DATA 测试程序
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-12-10: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
/****************************************************************************
 * Macro definitions
 ***************************************************************************/

static void dump_user_data(const uint8_t *data, int len)
{
	int i;
	
	fprintf(stderr, "-------------------------------------------\n");
	fprintf(stderr, "read %d bytes data:\n", len);	
	for(i=0;i<len;i++)
	{
		fprintf(stderr, "%02x ", data[i]);
		if(((i+1)%16)==0) 
			fprintf(stderr, "\n");
	}
	
	fprintf(stderr, "\n-------------------------------------------\n");
	fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
	int fd, cnt;
	struct pollfd fds;
	uint8_t buf[5*1024];


	fd = open("/dev/amstream_userdata", O_RDONLY);
	if (fd < 0){
		fprintf(stderr, "Cannot open amstream_userdata: %s\n", strerror(errno));
		return -1;
	}

	fds.events = POLLIN | POLLERR;
	fds.fd = fd;

	while(1){
		if (poll(&fds, 1, 1000) > 0){
			cnt = read(fd, buf, sizeof(buf));
			if (cnt > 0){
				dump_user_data(buf, cnt);
			}
		}else{
			fprintf(stderr, "no userdata available, is the video playing?\n");
		}
	}

	close(fd);
	return 0;
}
