

/**
 * @file vtmosaicgraphics.h vtmosaicgraphics Header file
 */

#ifndef __VTMOSAICGRAPHICS_H___
#define __VTMOSAICGRAPHICS_H___

#include "VTCommon.h"

void DrawG1Mosaic(LPRECT lpRect, INT8U uChar, INT8U uColor, BOOLEAN bSeparated);

INT32S Mosaic_RegisterCallback(INT32S (*fill_rectangle)(INT32S left, INT32S top, INT32U width, INT32U height, INT32U color),
                               INT32U (*convert_color)(INT8U alpah, INT8U red,  INT8U green,  INT8U blue));


INT32S Mosaic_init(void);
INT32S Mosaic_deinit(void);



#endif
