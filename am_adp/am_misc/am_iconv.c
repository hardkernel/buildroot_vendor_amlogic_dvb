#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief iconv functions
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2014-03-18: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 1

#include <am_iconv.h>

UConverter* (*am_ucnv_open_ptr)(const char *converterName, UErrorCode *err);
void (*am_ucnv_close_ptr)(UConverter * converter);
void (*am_ucnv_convertEx_ptr)(UConverter *targetCnv, UConverter *sourceCnv,
		char **target, const char *targetLimit,
		const char **source, const char *sourceLimit,
		UChar *pivotStart, UChar **pivotSource,
		UChar **pivotTarget, const UChar *pivotLimit,
		UBool reset, UBool flush,
		UErrorCode *pErrorCode);

void
am_ucnv_dlink(void)
{
	static void* handle = NULL;

	if(handle == NULL)
		handle = dlopen("libicuuc.so", RTLD_LAZY);
	
	assert(handle);

#define LOAD_UCNV_SYMBOL(name, post)\
	if(!am_##name##_ptr)\
		am_##name##_ptr = dlsym(handle, #name post);
#define LOAD_UCNV_SYMBOLS(post)\
	LOAD_UCNV_SYMBOL(ucnv_open, post)\
	LOAD_UCNV_SYMBOL(ucnv_close, post)\
	LOAD_UCNV_SYMBOL(ucnv_convertEx, post)

	LOAD_UCNV_SYMBOLS("")
	LOAD_UCNV_SYMBOLS("_48")
	LOAD_UCNV_SYMBOLS("_51")
	LOAD_UCNV_SYMBOLS("_53")
}

