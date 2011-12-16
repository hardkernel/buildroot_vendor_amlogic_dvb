#include "includes.h"

#include "VTCommon.h"
#include "VTDecoder.h"
#include "VTTopText.h"
#include "VTTeletext.h"
#include "VTDrawer.h"



INT32S AM_VT_Init(void)
{
    if (AM_SUCCESS != VTInitCommonData())
    {
        M_TELETEXT_DIAG(("\n init common data fail! \n"));
        goto fail;
    }
    if (AM_SUCCESS != VTInitTopTextData())
    {
        M_TELETEXT_DIAG(("\n init TopText data fail! \n"));
        goto fail;
    }

    if (AM_SUCCESS != VTInitDecoderData())
    {
        M_TELETEXT_DIAG(("\n init Decoder data fail! \n"));
        goto fail;
    }
    VTInitDrawerData();

    return AM_SUCCESS;

fail:

    return AM_FAILURE;
}


INT32S AM_VT_Exit(void)
{

    VTFreeDecoderData();
    VTFreeTopTextData();

    return AM_SUCCESS;

}








