

#ifndef _AM_CI_H_
#define _AM_CI_H_

#include "am_caman.h"

#ifdef __cplusplus
extern "C" {
#endif

enum AM_CI_ErrorCode
{
	AM_CI_ERROR_BASE=AM_ERROR_BASE(AM_MOD_CI),
	AM_CI_ERROR_INVALID_DEV_NO,
	AM_CI_ERROR_BAD_PARAM,
	AM_CI_ERROR_NOT_OPEN,
	AM_CI_ERROR_NOT_START,
	AM_CI_ERROR_ALREADY_OPEN,
	AM_CI_ERROR_ALREADY_START,
	AM_CI_ERROR_CANNOT_CREATE_THREAD,
	AM_CI_ERROR_USED_BY_CAMAN,
	AM_CI_ERROR_PROTOCOL,
	AM_CI_ERROR_BAD_PMT,
	AM_CI_ERROR_MAX_DEV,
	AM_CI_ERROR_BAD_CAM,
	AM_CI_ERROR_UNAVAILABLE,
	AM_CI_ERROR_UNKOWN,
};

enum AM_CI_CBID
{
	CBID_AI_CALLBACK,
	CBID_CA_INFO_CALLBACK ,
	CBID_MMI_CLOSE_CALLBACK ,
	CBID_MMI_DISPLAY_CONTROL_CALLBACK ,
	CBID_MMI_ENQ_CALLBACK ,
	CBID_MMI_MENU_CALLBACK ,
	CBID_MMI_LIST_CALLBACK ,
};

enum AM_CI_MMI_ANSWER_ID
{
	AM_CI_MMI_ANSW_CANCEL = 0x00,
	AM_CI_MMI_ANSW_ANSWER = 0x01,
};

enum AM_CI_MMI_CLOSE_MMI_CMD_ID
{
	AM_CI_MMI_CLOSE_MMI_CMD_ID_IMMEDIATE = 0x00,
	AM_CI_MMI_CLOSE_MMI_CMD_ID_DELAY = 0x01,
};

enum AM_CI_CA_LIST_MANAGEMENT
{
	AM_CI_CA_LIST_MANAGEMENT_MORE =    0x00,
	AM_CI_CA_LIST_MANAGEMENT_FIRST =   0x01,
	AM_CI_CA_LIST_MANAGEMENT_LAST =    0x02,
	AM_CI_CA_LIST_MANAGEMENT_ONLY =    0x03,
	AM_CI_CA_LIST_MANAGEMENT_ADD =     0x04,
	AM_CI_CA_LIST_MANAGEMENT_UPDATE =  0x05,
};

enum AM_CI_CA_PMT_CMD_ID
{
	AM_CI_CA_PMT_CMD_ID_OK_DESCRAMBLING =  0x01,
	AM_CI_CA_PMT_CMD_ID_OK_MMI =           0x02,
	AM_CI_CA_PMT_CMD_ID_QUERY =            0x03,
	AM_CI_CA_PMT_CMD_ID_NOT_SELECTED =     0x04,
};

typedef struct
{
	int foo;
} AM_CI_OpenPara_t;

typedef struct  {
	uint8_t *text;
	uint32_t text_size;
} app_mmi_text_t;

typedef int (*ai_callback)(void *arg, uint8_t slot_id, uint16_t session_number,
			     uint8_t application_type, uint16_t application_manufacturer,
			     uint16_t manufacturer_code, uint8_t menu_string_length,
			     uint8_t *menu_string);
typedef int (*ca_info_callback)(void *arg, uint8_t slot_id, uint16_t session_number, uint32_t ca_id_count, uint16_t *ca_ids);
typedef int (*mmi_close_callback)(void *arg, uint8_t slot_id, uint16_t session_number, uint8_t cmd_id, uint8_t delay);
typedef int (*mmi_display_control_callback)(void *arg, uint8_t slot_id, uint16_t session_number,
					      uint8_t cmd_id, uint8_t mmi_mode);
typedef int (*mmi_enq_callback)(void *arg, uint8_t slot_id, uint16_t session_number,
				  uint8_t blind_answer, uint8_t expected_answer_length,
				  uint8_t *text, uint32_t text_size);
typedef int (*mmi_menu_callback)(void *arg, uint8_t slot_id, uint16_t session_number,
				   app_mmi_text_t *title,
				   app_mmi_text_t *sub_title,
				   app_mmi_text_t *bottom,
				   uint32_t item_count, app_mmi_text_t *items,
				   uint32_t item_raw_length, uint8_t *items_raw);

typedef struct {
	union{
		ai_callback ai_cb;
		ca_info_callback ca_info_cb;
		mmi_close_callback mmi_close_cb;
		mmi_display_control_callback mmi_display_control_cb;
		mmi_enq_callback mmi_enq_cb;
		mmi_menu_callback mmi_menu_cb;
		mmi_menu_callback mmi_list_cb;
	}u;
}AM_CI_CB_t;

extern AM_ErrorCode_t AM_CI_Open(int dev_no, int slot_no, const AM_CI_OpenPara_t *para, int *handle);
extern AM_ErrorCode_t AM_CI_Close(int handle);
extern AM_ErrorCode_t AM_CI_Start(int handle);
extern AM_ErrorCode_t AM_CI_Stop(int handle);

/*set callbacks before start() or you may miss callbacks*/
extern AM_ErrorCode_t AM_CI_SetCallback(int handle, int cbid, AM_CI_CB_t *cb, void *arg);


extern AM_ErrorCode_t AM_CI_App_ca_pmt(int handle, unsigned char *capmt, unsigned int size);
extern AM_ErrorCode_t AM_CI_App_ai_entermenu(int handle);
/*answer_id see:AM_CI_MMI_ANSWER_ID*/
extern AM_ErrorCode_t AM_CI_App_mmi_answ(int handle, int answer_id, char *answer, int size);
extern AM_ErrorCode_t AM_CI_App_mmi_menu_answ(int handle, int select);
extern AM_ErrorCode_t AM_CI_App_mmi_close(int handle, int cmd_id, int delay);

/*infomations enquired will be repled in the callback*/
extern AM_ErrorCode_t AM_CI_App_ca_info_enq(int handle);
extern AM_ErrorCode_t AM_CI_App_ai_enquiry(int handle);

/*user need to free the generated capmt with free() in the end */
extern AM_ErrorCode_t AM_CI_GenerateCAPMT(unsigned char *pmt, unsigned int pmt_size, 
											unsigned char **capmt, unsigned int *capmt_size, 
											int ca_list_management, int ca_pmt_cmd_id, 
											int moveca);

extern AM_ErrorCode_t AM_CI_MatchCAID(int handle, unsigned int caid, int *match);
extern AM_ErrorCode_t AM_CI_CAMAN_getCA(int handle, AM_CA_t **ca);



#ifdef __cplusplus
}
#endif

#endif/*_AM_CI_H_*/
