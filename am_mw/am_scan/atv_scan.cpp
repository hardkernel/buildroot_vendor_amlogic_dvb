/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file atv_scan.c
 * \brief Analog SCAN模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-11-30: create the document
 ***************************************************************************/
 
#define AM_DEBUG_LEVEL 2
#include <Tv.h>
#include <binder/Parcel.h>
#include <tvcmd.h>
#include <am_debug.h>

using namespace android;

class AMSCANTvListener: public TvListener
{
public:
	AMSCANTvListener(void *para) 
	{
		mDTVPara = para;
		mTV = NULL;
	}
	~AMSCANTvListener() {}
	virtual void notify(int32_t msgType, const Parcel &p);
	void setTv(sp<Tv> tv) {mTV = tv;}
	sp<Tv> getTv(){return mTV;}
	void setDtvPara(void *para){mDTVPara = para;}
	int sendTvCmd(int cmd, int arg);
private:
	void *mDTVPara;
	sp<Tv> mTV;
};


static sp<AMSCANTvListener> tvListener = NULL;
	
extern "C" {void am_scan_notify_from_atv(const int *ms_pdu, void *para);}



void AMSCANTvListener::notify(int32_t msgType, const Parcel &p)
{
	int msg_pdu[256];
	int i;
	int loop_count = p.readInt32();
	
	if (loop_count > 256)
		loop_count = 256;
		
	for (i = 0; i < loop_count; i++) 
	{
		msg_pdu[i] = p.readInt32();
	}
	AM_DEBUG(1, "AMSCANTvListener:notify--> %d %d", loop_count, msg_pdu[0]);
	if (loop_count > 0)
	{
		AM_DEBUG(1, "AMSCANTvListener:notify-->mDTVPara %p", mDTVPara);
		am_scan_notify_from_atv(msg_pdu, mDTVPara);
	}
}

int AMSCANTvListener::sendTvCmd(int cmd, int arg)
{
	Parcel p;
	Parcel r;
	
	p.writeInt32(ATV_DETECT_FREQUENCY);
	if (cmd == ATV_DETECT_FREQUENCY)
	{
		p.writeInt32(arg);
	}
	p.setDataPosition(0);
	getTv()->processCmd(p, &r);
	
	return r.readInt32();
}

extern "C" int am_scan_start_atv_search(void *dtv_para)
{
	sp<Tv> tv = Tv::connect();

	if (tv == NULL) 
	{
		AM_DEBUG(1,"Fail to connect to tv service");
		return -1;
	}

	// make sure tv amlogic is alive
	if (tv->getStatus() != NO_ERROR) 
	{
		AM_DEBUG(1,"Tv initialization failed");
		return -1;
	}

	tvListener = new AMSCANTvListener(dtv_para);
	tv->setListener(tvListener);
	tvListener->setTv(tv);
	
	return tvListener->sendTvCmd(START_TV,  0);
}

extern "C" int am_scan_atv_detect_frequency(int freq)
{
	if (tvListener == NULL)
	{
		AM_DEBUG(1, ">>>Cannot detect frequency, has not connected to tv");
		return -1;
	}
		
	AM_DEBUG(1,"Searching Analog frequency %d...", freq);
	
	tvListener->sendTvCmd(ATV_DETECT_FREQUENCY,  freq);
	
	return 0;
}

extern "C" int am_scan_stop_atv_search()
{
	// clean up if release has not been called before
	if (tvListener != NULL) 
	{
		sp<Tv> tv = tvListener->getTv();
		AM_DEBUG(1,"ATV Release");

		//to avoid notify
		tvListener->setDtvPara(NULL);
		tvListener->sendTvCmd(STOP_TV,  0);
		
		// clear callbacks
		if (tv != NULL) 
		{
			//tv.clear();
			tv->disconnect();
		}
	}
	
	return 0;
}

