
#include <ISystemControlService.h>

#include <binder/Binder.h>
#include <binder/IServiceManager.h>
#include <utils/Atomic.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <utils/threads.h>
#include <unistd.h>

#include "am_debug.h"

#include "am_sys_write.h"

using namespace android;

class DeathNotifier: public IBinder::DeathRecipient
{
    public:
        DeathNotifier()
        {
        } 
        void binderDied(const wp < IBinder > &who)
        {
            AM_DEBUG(1, "system_write died !");
        }
};

static sp < ISystemControlService > amSystemControlService;
static sp < DeathNotifier > amDeathNotifier;
static Mutex amgLock;

/**\brief 获取system control 服务
 * \param[in] none
 * \return
 *   - ISystemControlService 成功
 *   - 其他值 null
 */
const sp < ISystemControlService > &getSystemControlService()
{
    Mutex::Autolock _l(amgLock);
    if (amSystemControlService.get() == 0)
    {
        sp < IServiceManager > sm = defaultServiceManager();
        sp < IBinder > binder;
        do
        {
            binder = sm->getService(String16("system_control"));
            if (binder != 0)
                break;
            AM_DEBUG(1,"SystemControl not published, waiting...");
            usleep(500000); // 0.5 s
        }
        while (true);
        if (amDeathNotifier == NULL)
        {
            amDeathNotifier = new DeathNotifier();
        }
        binder->linkToDeath(amDeathNotifier);
        amSystemControlService =
            interface_cast < ISystemControlService > (binder);
    }
    //ALOGE_IF(amSystemControlService == 0, "no System Control Service!?");
    return amSystemControlService;
}

/**\brief read sysfs value
 * \param[in] path file name
 * \param[out] value: read file info
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码
 */
AM_ErrorCode_t AM_SystemControl_Read_Sysfs(const char *path, char *value)
{
    if(path==NULL||value==NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_Read_Sysfs path or value is null");
        return AM_FAILURE;
    }
    //AM_DEBUG(1,"AM_SystemControl_Read_Sysfs:%s",path);
    const sp < ISystemControlService > &scs = getSystemControlService();
    if (scs != 0)
    {
        String16 v;
        if (scs->readSysfs(String16(path), v))
        {
            strcpy(value, String8(v).string());
            return AM_SUCCESS;
        }
    }
    //AM_DEBUG(1,"[false]AM_SystemControl_Read_Sysfs%s,",path);
    return AM_FAILURE;
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
    if(path==NULL||value==NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_ReadNum_Sysfs path or value is null");
        return AM_FAILURE;
    }
    //AM_DEBUG(1,"AM_SystemControl_ReadNum_Sysfs:%s",path);
    const sp < ISystemControlService > &scs = getSystemControlService();
    if (scs != 0 && value != NULL && access(path, 0) != -1)
    {
        String16 v;
        if (scs->readSysfs(String16(path), v))
        {
            if (v.size() != 0)
            {
                //AM_DEBUG(1,"readSysfs ok:%s,%s,%d", path, String8(v).string(), String8(v).size());
                memset(value, 0, size);
                if (size <= String8(v).size() + 1)
                {
                    memcpy(value, String8(v).string(),
                           size - 1);
                    value[strlen(value)] = '\0';
                }
                else
                {
                    strcpy(value, String8(v).string());
                }
                return AM_SUCCESS;
            }
        }
    }
    //AM_DEBUG(1,"[false]AM_SystemControl_ReadNum_Sysfs%s,",path);
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
    if(path==NULL||value==NULL)
    {
        AM_DEBUG(1,"[false]AM_SystemControl_Write_Sysfs path or value is null");
        return AM_FAILURE;
    }
    //AM_DEBUG(1,"AM_SystemControl_Write_Sysfs:%s",path);
    const sp < ISystemControlService > &scs = getSystemControlService();
    if (scs != 0)
    {
        String16 v(value);
        if (scs->writeSysfs(String16(path), v))
        {
            //AM_DEBUG(1,"writeSysfs ok");
            return AM_SUCCESS;
        }
    }
    //AM_DEBUG(1,"[false]AM_SystemControl_Write_Sysfs%s,",path);
    return AM_FAILURE;
}
