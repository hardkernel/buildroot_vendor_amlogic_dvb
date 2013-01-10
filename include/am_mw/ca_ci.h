#ifndef _CA_CI_H_
#define _CA_CI_H_

#include "am_caman.h"
#include "am_ci.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ca_ci_msg_s ca_ci_msg_t;


typedef struct ca_ci_appinfo_s ca_ci_appinfo_t;
typedef struct ca_ci_cainfo_s ca_ci_cainfo_t;

typedef struct ca_ci_mmi_close_s ca_ci_mmi_close_t;
typedef struct ca_ci_mmi_display_control_s ca_ci_mmi_display_control_t;
typedef struct ca_ci_mmi_enq_s ca_ci_mmi_enq_t;

typedef struct ca_ci_answer_enq_s ca_ci_answer_enq_t;
typedef struct ca_ci_answer_menu_s ca_ci_answer_menu_t;
typedef struct ca_ci_close_mmi_s ca_ci_close_mmi_t;
typedef struct ca_ci_set_pmt_s ca_ci_set_pmt_t;

enum ca_ci_msg_type
{
	/*msgs from ca to app*/
	ca_ci_msg_type_appinfo,
	ca_ci_msg_type_cainfo,
	ca_ci_msg_type_mmi_close,
	ca_ci_msg_type_mmi_display_control,
	ca_ci_msg_type_mmi_enq,
	ca_ci_msg_type_mmi_menu,
	ca_ci_msg_type_mmi_list,

	/*msgs from app to ca*/
	ca_ci_msg_type_enter_menu,
	ca_ci_msg_type_answer,
	ca_ci_msg_type_answer_menu,
	ca_ci_msg_type_close_mmi,
	ca_ci_msg_type_set_pmt,
	ca_ci_msg_type_get_cainfo,
	ca_ci_msg_type_get_appinfo,
};


struct ca_ci_appinfo_s {
	uint8_t application_type;
	uint16_t application_manufacturer;
	uint16_t manufacturer_code;
	uint8_t menu_string_length;
	uint8_t menu_string[0];
};

struct ca_ci_cainfo_s {
	uint32_t ca_id_count;
	uint16_t ca_ids[0];
};

struct ca_ci_mmi_close_s {
	uint8_t cmd_id;
	uint8_t delay;
};

struct ca_ci_mmi_display_control_s {
	uint8_t cmd_id;
	uint8_t mmi_mode;
};

struct ca_ci_mmi_enq_s {
	uint8_t blind_answer;
	uint8_t expected_answer_length;
	uint32_t text_size;
	uint8_t text[0];
};

/*parse as this struct definition
 !!byte by byte, not aligned!!
struct ca_ci_mmi_text_s {
	unsigned int text_size;
	unsigned char text[0];
};

struct ca_ci_mmi_menu_list_s {
	ca_ci_mmi_text_t title;
	ca_ci_mmi_text_t sub_title;
	ca_ci_mmi_text_t bottom;
	uint32_t item_count;
	ca_ci_mmi_text_t items[0];
	uint32_t item_raw_length;
	uint8_t items_raw[0];
};
*/

struct ca_ci_answer_enq_s {
	int answer_id;
	int size;
	char answer[0];
};

struct ca_ci_answer_menu_s {
	int select;
};

struct ca_ci_close_mmi_s {
	int cmd_id;
	int delay;
};

struct ca_ci_set_pmt_s {
	unsigned int list_management;
	unsigned int cmd_id;
	unsigned int length;
	unsigned char pmt[0];
};

struct ca_ci_msg_s
{
	int type;
	unsigned char msg[0];
};

extern AM_CA_t ca_ci;

#ifdef __cplusplus
}
#endif

#endif

