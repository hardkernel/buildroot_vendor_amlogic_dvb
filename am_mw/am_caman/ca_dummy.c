#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#define AM_DEBUG_LEVEL 1

#include "am_debug.h"

#include "ca_dummy.h"


struct dummy_ca_s {
	char *send_name;
	int (*send_msg)(char *name, AM_CA_Msg_t *msg);
} dummy;

static int dummy_ca_open(void *arg, AM_CAMAN_Ts_t *ts)
{
	AM_DEBUG(1, "dummy ca open");
	return 0;
}

static int dummy_ca_close(void *arg)
{
	AM_DEBUG(1, "dummy ca close");
	return 0;
}

static int dummy_ca_camatch(void *arg, unsigned int caid)
{
	AM_DEBUG(1, "dummy ca camatch");
	return 0;
}

static int dummy_ca_new_cat(void *arg, unsigned char *cat, unsigned int size)
{
	AM_DEBUG(1, "dummy ca new cat");
	return 0;
}


static int dummy_ca_startpmt(void *arg, int service_id, unsigned char *pmt, unsigned int size)
{
	AM_DEBUG(1, "dummy ca start capmt srv[%d]", service_id);
	return 0;
}

static int dummy_ca_stoppmt(void *arg, int service_id)
{
	AM_DEBUG(1, "dummy ca stop capmt srv[%d]", service_id);
	return 0;
}

static int dummy_ca_ts(void *arg)
{
	AM_DEBUG(1, "dummy ca ts changed");
	return 0;
}

static int dummy_ca_enable(void *arg, int enable)
{
	AM_DEBUG(1, "dummy ca enable [%s]", enable? "ENABLE" : "DISABLE");
	return 0;
}

static int dummy_ca_register_msg_send(void *arg, char *name, int (*send_msg)(char *name, AM_CA_Msg_t *msg))
{
	AM_DEBUG(1, "dummy ca msg func [%p] ", send_msg);
	dummy.send_name = name;
	dummy.send_msg = send_msg;
	return 0;
}

static int dummy_ca_free_msg(void *arg, AM_CA_Msg_t *msg)
{
	AM_DEBUG(1, "dummy ca free msg");
	return 0;
}

static int dummy_ca_msg_receive(void *arg, AM_CA_Msg_t *msg)
{
	AM_DEBUG(1, "dummy ca msg receive");
	return 0;
}


AM_CA_t dummy_ca = {
	.type = AM_CA_TYPE_CA,

	.arg = (void*)&dummy_ca,
	
	.ops = {
		.open = dummy_ca_open,
		.close= dummy_ca_close,
		.camatch = dummy_ca_camatch,

		.ts_changed = dummy_ca_ts,

		.new_cat = dummy_ca_new_cat,

		.start_pmt = dummy_ca_startpmt,
		.stop_pmt = dummy_ca_stoppmt,

		.enable = dummy_ca_enable,
		
		.register_msg_send = dummy_ca_register_msg_send,
		.free_msg = dummy_ca_free_msg,
		.msg_receive = dummy_ca_msg_receive,
	}
};

