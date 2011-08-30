
/**
 * @file vbi_videotext.h vbi_videotext Header file
 */

#ifndef __VTTELETEXT_H___
#define __VTTELETEXT_H___


typedef enum
{
    VT_OFF,
    VT_BLACK,
    VT_MIXED
} eVTState;


INT32S AM_VT_Init(void);
INT32S AM_VT_Exit(void);

#endif
