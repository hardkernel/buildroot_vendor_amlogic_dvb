#ifndef ATVSCANPLAY_H
#define ATVSCANPLAY_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <am_types.h>
#include <android/log.h>
#include <cutils/log.h>
#include <cutils/log.h>
#include <tvin/tvin_api.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern  AM_ErrorCode_t TvinSigDetect_CreateThread();

extern  AM_ErrorCode_t TvinSigDetect_Stop();

#ifdef __cplusplus
}
#endif

#endif
