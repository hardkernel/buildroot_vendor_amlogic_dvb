/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package. *
* Description:
*/

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "am_debug.h"
#include "am_sys_write.h"

/**\brief read sysfs value
 * \param[in] path file name
 * \param[out] value: read file info
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_SystemControl_Read_Sysfs(const char *path, char *value)
{
    int ret = 0;
    int fd = open(path, O_RDONLY);

    if (path == NULL || value == NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_Read_Sysfs path or value is null");
        return AM_FAILURE;
    }
    if (fd <= 0)
    {
        ret = AM_FAILURE;
        goto end;
    }
    if (read(fd, value, 1024) <= 0)
    {
        ret = AM_FAILURE;
        goto end;
    }
    else
        return AM_SUCCESS;

end:
    if (fd > 0)
        close (fd);
    return ret;
}
/**\brief read num sysfs value
 * \param[in] path file name
 * \param[in] size: need read file length
 * \param[out] value: read file info
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_SystemControl_ReadNum_Sysfs(const char *path, char *value, int size)
{
    int ret;
    int fd;
    char buffer[32] = {0};
    if (path == NULL || value == NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_ReadNum_Sysfs path or value is null");
        return AM_FAILURE;
    }

    if (value != NULL && access(path, 0) != -1)
    {
        fd = open(path, O_RDONLY);
        if (fd <= 0)
            return AM_FAILURE;
        if (read(fd, value, size) <= 0)
        {
            close (fd);
            return AM_FAILURE;
        }
        value[size] = '\0';
        close (fd);
        return AM_SUCCESS;
    }
    return AM_FAILURE;
}
/**\brief write sysfs value
 * \param[in] path: file name
 * \param[in] value: write info to file
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_SystemControl_Write_Sysfs(const char *path, char *value)
{
    int fd;
    if (path == NULL || value == NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_Write_Sysfs path or value is null");
        return AM_FAILURE;
    }
    fd = open(path, O_WRONLY);
    if (fd <= 0)
        return AM_FAILURE;
    write(fd, value, strlen(value));
    close(fd);
    return AM_SUCCESS;
    return AM_FAILURE;
}
