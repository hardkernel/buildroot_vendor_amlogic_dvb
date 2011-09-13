/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_scan_internal.h
 * \brief 搜索模块内部数据
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-10-27: create the document
 ***************************************************************************/

#ifndef _AM_SCAN_INTERNAL_H
#define _AM_SCAN_INTERNAL_H

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/


/****************************************************************************
 * Type definitions
 ***************************************************************************/

typedef struct AM_SCAN_Scanner_s AM_SCAN_Scanner_t ;

/*当前搜索各表接收状态*/
enum
{
	AM_SCAN_RECVING_COMPLETE 	= 0,
	AM_SCAN_RECVING_PAT 		= 0x01,
	AM_SCAN_RECVING_PMT 		= 0x02,
	AM_SCAN_RECVING_CAT 		= 0x04,
	AM_SCAN_RECVING_SDT 		= 0x08,
	AM_SCAN_RECVING_NIT			= 0x10,
	AM_SCAN_RECVING_BAT			= 0x20,
	AM_SCAN_RECVING_MGT			= 0x40,
	AM_SCAN_RECVING_VCT			= 0x80,
	AM_SCAN_RECVING_WAIT_FEND	= 0x100,
};

/*SCAN内部事件类型*/
enum
{
	AM_SCAN_EVT_FEND 		= 0x01, /**< 前端事件*/
	AM_SCAN_EVT_PAT_DONE 	= 0x02, /**< PAT表已经接收完毕*/
	AM_SCAN_EVT_PMT_DONE 	= 0x04, /**< PMT表已经接收完毕*/
	AM_SCAN_EVT_CAT_DONE 	= 0x08, /**< CAT表已经接收完毕*/
	AM_SCAN_EVT_SDT_DONE 	= 0x10, /**< SDT表已经接收完毕*/
	AM_SCAN_EVT_NIT_DONE 	= 0x20, /**< NIT表已经接收完毕*/
	AM_SCAN_EVT_BAT_DONE 	= 0x40, /**< BAT表已经接收完毕*/
	AM_SCAN_EVT_MGT_DONE 	= 0x80, /**< MGT表已经接收完毕*/
	AM_SCAN_EVT_VCT_DONE 	= 0x100, /**< VCT表已经接收完毕*/
	AM_SCAN_EVT_QUIT		= 0x200, /**< 退出搜索事件*/
	AM_SCAN_EVT_START		= 0x400,/**< 开始搜索事件*/
};

/*SCAN 所在阶段*/
enum
{
	AM_SCAN_STAGE_WAIT_START,
	AM_SCAN_STAGE_NIT,
	AM_SCAN_STAGE_TS,
	AM_SCAN_STAGE_DONE
};

/*sqlite3 stmts for DVB*/
enum
{
	QUERY_NET,
	INSERT_NET,
	UPDATE_NET,
	QUERY_TS,
	INSERT_TS,
	UPDATE_TS,
	DELETE_TS_SRVS,
	QUERY_SRV,
	INSERT_SRV,
	UPDATE_SRV,
	QUERY_SRV_BY_TYPE,
	UPDATE_CHAN_NUM,
	DELETE_TS_EVTS,
	QUERY_TS_BY_FREQ_ORDER,
	DELETE_SRV_GRP,
	INSERT_SUBTITLE,
	INSERT_TELETEXT,
	DELETE_TS_SUBTITLES,
	DELETE_TS_TELETEXTS,
	MAX_STMT
};

/**\brief 子表接收控制*/
typedef struct
{
	uint16_t		ext;
	uint8_t			ver;
	uint8_t			sec;
	uint8_t			last;
	uint8_t			mask[32];
}AM_SCAN_SubCtl_t;

/**\brief 表接收控制，有多个子表时使用*/
typedef struct
{
	int				fid;
	int				recv_flag;
	int				evt_flag;
	struct timespec	end_time;
	int				timeout;
	int				data_arrive_time;	/**< 数据到达时间，用于多子表时收齐判断*/
	int				repeat_distance;/**< 多子表时允许的最小数据重复间隔，用于多子表时收齐判断*/
	uint16_t		pid;
	uint8_t 		tid;
	char			tname[10];
	void 			(*done)(struct AM_SCAN_Scanner_s *);
	
	uint16_t subs;				/**< 子表个数*/
	AM_SCAN_SubCtl_t *subctl; 	/**< 子表控制数据*/
}AM_SCAN_TableCtl_t;


/**\brief 搜索中间数据*/
struct AM_SCAN_Scanner_s
{
	AM_SCAN_TableCtl_t				patctl;			/**< PAT接收控制*/
	AM_SCAN_TableCtl_t				pmtctl;			/**< PMT接收控制*/
	AM_SCAN_TableCtl_t				catctl;			/**< CAT接收控制*/
	AM_SCAN_TableCtl_t				sdtctl;			/**< SDT接收控制*/
	AM_SCAN_TableCtl_t				nitctl;			/**< NIT接收控制*/
	AM_SCAN_TableCtl_t				batctl;			/**< BAT接收控制*/
	/*ATSC tables*/
	AM_SCAN_TableCtl_t				mgtctl;			/**< MGT接收控制*/
	AM_SCAN_TableCtl_t				vctctl;			/**< VCT接收控制*/

	int								standard;
	int								evt_flag;		/**< 事件标志*/
	int								recv_status;	/**< 接收状态*/
	int								stage;			/**< 执行阶段*/
	int								fend_dev;		/**< FEND 设备号*/
	int								dmx_dev;		/**< DMX设备号*/
	pthread_mutex_t     			lock;   		/**< 保护互斥体*/
	pthread_cond_t      			cond;    		/**< 条件变量*/
	pthread_t          				thread;         /**< 状态监控线程*/
	int								hsi;			/**< SI解析句柄*/
	int								curr_freq;		/**< 当前正在搜索的频点*/ 
	int								start_freqs_cnt;/**< 需要搜索的频点个数*/
	struct dvb_frontend_event		fe_evt;			/**< 前段事件*/
	struct dvb_frontend_parameters 	*start_freqs;	/**< 需要搜索的频点列表*/ 
	AM_SCAN_TS_t					*curr_ts;		/**< 当前正在搜索的TS数据*/
	dvbpsi_pat_program_t			*cur_prog;		/**< 当前正在接收PMT的Program*/
	AM_SCAN_StoreCb 				store_cb;		/**< 存储回调*/
	AM_SCAN_Result_t				result;			/**< 搜索结果*/
	int							end_code;		/**< 搜索结束码*/
	void						*user_data;		/**< 用户数据*/
	AM_Bool_t					store;			/**< 是否存储*/
};

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/


#ifdef __cplusplus
}
#endif

#endif

