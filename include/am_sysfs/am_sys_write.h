#ifndef AM_SYS_WRITE_H
#define AM_SYS_WRITE_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <am_types.h>
AM_ErrorCode_t AM_SystemControl_Read_Sysfs(const char *path, char *value);
AM_ErrorCode_t AM_SystemControl_ReadNum_Sysfs(const char *path, char *value,int size);
AM_ErrorCode_t AM_SystemControl_Write_Sysfs(const char *path, char *value);

#ifdef  __cplusplus
}
#endif
#endif
