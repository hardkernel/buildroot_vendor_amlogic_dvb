/***************************************************************************
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
/**\file
 * \brief file tools
 *
 * \author
 * \date
 ***************************************************************************/

#ifndef _AM_TFILE_H
#define _AM_TFILE_H

#include "am_types.h"

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
enum AM_TFile_EventType
{
	AM_TFILE_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_TFILE),
	AM_TFILE_EVT_RATE_CHANGED,
	AM_TFILE_EVT_SIZE_CHANGED,
	AM_TFILE_EVT_START_TIME_CHANGED,
	AM_TFILE_EVT_END_TIME_CHANGED,
	AM_TFILE_EVT_DURATION_CHANGED,
	AM_TFILE_EVT_END
};

typedef struct AM_TFileData_s AM_TFileData_t;
typedef struct AM_TFile_Sub_s AM_TFile_Sub_t;

typedef AM_TFileData_t * AM_TFile_t;

struct AM_TFile_Sub_s
{
	int rfd;
	int wfd;
	int findex;
	loff_t size;
	struct AM_TFile_Sub_s *next;
};

/**\brief Timeshift File*/
struct AM_TFileData_s
{
	int		opened;	/**< open flag*/
	loff_t	size;	/**< size of the file*/
	loff_t	avail;	/**< avail data*/
	loff_t	total;	/**< total written*/
	loff_t	start;	/**< start offset*/
	loff_t	read;	/**< read offset*/
	loff_t	write;	/**< write offset*/
	pthread_mutex_t lock;	/*r/w lock*/
	pthread_cond_t		cond;
	AM_Bool_t	loop;	/*loop mode*/
	AM_Bool_t	is_timeshift;
	char	*name;	/*name*/
	int		duration;

	/*statistics*/
	loff_t	rtotal;
	int		rlast; /**< the latest time read*/
	int		rrate;
	loff_t	wtotal;
	int		wlast;/**< the latest time write*/
	int		rate;

	/* sub files control */
	int last_sub_index;
	loff_t sub_file_size;
	AM_TFile_Sub_t *sub_files;
	AM_TFile_Sub_t *cur_rsub_file;
	AM_TFile_Sub_t *cur_wsub_file;

	void *timer;

	int delete_on_close;
};

/****************************************************************************
 * API function prototypes
 ***************************************************************************/

/**\brief Open a TFile
 * \param[out] tfile tfile handler
 * \param[in] file_name name of the tfile
 * \param[in] loop
 * \param[in] duration
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern AM_ErrorCode_t AM_TFile_Open(AM_TFile_t *tfile, const char *file_name, AM_Bool_t loop, int duration_max, loff_t size_max);

/**\brief Close a TFile
 * \param[in] tfile handler of the TFile
 * \retval AM_SUCCESS on success
 * \return Error code
 */
extern AM_ErrorCode_t AM_TFile_Close(AM_TFile_t tfile);

/**\brief Read from a TFile
 * \param[in] tfile tfile handler
 * \param[in] buf read data will be stored
 * \param[in] size the length to read
 * \param[in] timeout in ms
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern ssize_t AM_TFile_Read(AM_TFile_t tfile, uint8_t *buf, size_t size, int timeout);

/**\brief Write to a TFile
 * \param[out] tfile tfile handler
 * \param[in] buf data to be written
 * \param[in] size length to write
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern ssize_t AM_TFile_Write(AM_TFile_t tfile, uint8_t *buf, size_t size);

/**\brief Seek a TFile
 * \param[out] tfile tfile handler
 * \param[in] offset the offset to seek
 * \retval AM_SUCCESS On success
 * \return Error code
 */
extern int AM_TFile_Seek(AM_TFile_t tfile, loff_t offset);

extern loff_t AM_TFile_Tell(AM_TFile_t tfile);

extern int AM_TFile_TimeStart(AM_TFile_t tfile);

extern int AM_TFile_TimeSeek(AM_TFile_t tfile, int offset_ms);

extern int AM_TFile_TimeGetReadNow(AM_TFile_t tfile);

extern int AM_TFile_TimeGetStart(AM_TFile_t tfile);

extern int AM_TFile_TimeGetEnd(AM_TFile_t tfile);

extern loff_t AM_TFile_GetAvailable(AM_TFile_t tfile);

#ifdef __cplusplus
}
#endif

#endif


