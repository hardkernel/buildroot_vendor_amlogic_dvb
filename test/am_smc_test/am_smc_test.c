/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 智能卡测试程序
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-07-05: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 0

#include <am_debug.h>
#include <am_smc.h>
#include <string.h>
#include <unistd.h>

#define SMC_DEV_NO (0)

static void smc_cb(int dev_no, AM_SMC_CardStatus_t status, void *data)
{
	if(status==AM_SMC_CARD_IN)
	{
		printf("cb: card in!\n");
	}
	else
	{
		printf("cb: card out!\n");
	}
}

static int smc_test(AM_Bool_t sync)
{
	AM_SMC_OpenPara_t para;
	uint8_t atr[AM_SMC_MAX_ATR_LEN];
	int i, len;
	AM_SMC_CardStatus_t status;
	uint8_t sbuf[5]={0x80, 0x44, 0x00, 0x00, 0x08};
	uint8_t rbuf[256];
	int rlen = sizeof(rbuf);
	
	memset(&para, 0, sizeof(para));
	para.enable_thread = !sync;
	AM_TRY(AM_SMC_Open(SMC_DEV_NO, &para));
	
	if(!sync)
	{
		AM_SMC_SetCallback(SMC_DEV_NO, smc_cb, NULL);
	}
	
	printf("please insert a card\n");
	do {
		AM_TRY(AM_SMC_GetCardStatus(SMC_DEV_NO, &status));
		usleep(100000);
	} while(status==AM_SMC_CARD_OUT);
	
	printf("card in\n");
	
	len = sizeof(atr);
	AM_TRY(AM_SMC_Reset(SMC_DEV_NO, atr, &len));
	printf("ATR: ");
	for(i=0; i<len; i++)
	{
		printf("%02x ", atr[i]);
	}
	printf("\n");
	
	AM_TRY(AM_SMC_TransferT0(SMC_DEV_NO, sbuf, sizeof(sbuf), rbuf, &rlen));
	printf("send: ");
	for(i=0; i<sizeof(sbuf); i++)
	{
		printf("%02x ", sbuf[i]);
	}
	printf("\n");
	
	printf("recv: ");
	for(i=0; i<rlen; i++)
	{
		printf("%02x ", rbuf[i]);
	}
	printf("\n");
	
	AM_TRY(AM_SMC_Close(SMC_DEV_NO));
	
	return 0;
}

int main(int argc, char **argv)
{
	printf("sync mode test\n");
	smc_test(AM_TRUE);
	
	printf("async mode test\n");
	smc_test(AM_FALSE);
	return 0;
}

