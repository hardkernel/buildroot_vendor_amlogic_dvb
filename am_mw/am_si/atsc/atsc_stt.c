#include "atsc_types.h"
#include "atsc_stt.h"

#define MAKE_SHORT_HL(exp)		            ((INT16U)((exp##_hi<<8)|exp##_lo))
#define MAKE_WORD_HML(exp)			((INT32U)(((exp##_hi)<<24)|((exp##_mh)<<16)|((exp##_ml)<<8)|exp##_lo))
#define secs_Between_1Jan1970_6Jan1980 	(315964800)

#if (ATSC_EPG_TIME_RELATE)  // for atsc epg
static INT32U g_gps_seconds = 0;
static INT32U g_gps_seconds_offset = 0;

INT32U GetCurrentGPSSeconds(void)
{
	return g_gps_seconds;
}

INT32U GetCurrentGPSOffset(void)
{
	return g_gps_seconds_offset;
}
#endif

#if 0
INT32S get_timezone_secondes(void)
{
	INT8S hour = 0;
	INT8S minute = 0;
	INT8S second = 0;
	INT32S total_seconds = 0;
	
	app_time_get_offset(&hour, &minute, &second);
	total_seconds = hour*3600 + minute*60 + second;
	
	return total_seconds;
}
#endif

stt_section_info_t *atsc_psip_new_stt_info(void)
{
	stt_section_info_t *tmp = NULL;

    tmp = AMMem_malloc(sizeof(stt_section_info_t));
    if(tmp)
    {
        tmp->utc_time = 0;
        tmp->p_next = NULL;
    }

    return tmp;
}

void   atsc_psip_free_stt_info(stt_section_info_t *info)
{
	stt_section_info_t *stt_sect_info = NULL;
	stt_section_info_t *tmp_stt_sect_info = NULL;
	if (info)
	{
		tmp_stt_sect_info = info;
		while(tmp_stt_sect_info)
		{
			stt_sect_info = tmp_stt_sect_info->p_next;
			AMMem_free(tmp_stt_sect_info);
			tmp_stt_sect_info = stt_sect_info;
		}
	}
    return ;
}

INT32S atsc_psip_parse_stt(INT8U* data, INT32U length, stt_section_info_t *info)
{
    stt_section_t *sect = NULL;
    INT32U tmp_time;
    INT8U GPS_UTC_offset;
	
    if(data && info)
    {
        sect = (stt_section_t*)data;

        if(sect->table_id == ATSC_PSIP_STT_TID)
        {
	     tmp_time = MAKE_WORD_HML(sect->system_time);
	     GPS_UTC_offset = sect->GPS_UTC_offset;
#if 1
	     info->utc_time = tmp_time + secs_Between_1Jan1970_6Jan1980 - GPS_UTC_offset;
#else	
		 info->utc_time = tmp_time + secs_Between_1Jan1970_6Jan1980 - GPS_UTC_offset + get_timezone_secondes();
#endif
	#if ATSC_EPG_TIME_RELATE
            g_gps_seconds = tmp_time;
	    g_gps_seconds_offset = GPS_UTC_offset;
	#endif
            return 0;
        }
    }

    return -1;
}

void atsc_psip_dump_stt_time(INT32U utc_time)
{
#ifndef __ROM_
    if(utc_time)
    {
        AM_TRACE("\r\n============= STT INFO =============\r\n");

        AM_TRACE("%d, 0x%08x", utc_time, utc_time);
        
        AM_TRACE("\r\n================ END ===============\r\n");
    }
#endif

    return ;
}
