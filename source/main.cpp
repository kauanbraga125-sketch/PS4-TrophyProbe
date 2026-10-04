#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>

#include "util.h"

// Read-only OpenOrbis trophy probe.
// Requires the public BREW00094 Title Conf already present in:
// /user/trophy/conf/BREW00094_00-00/
// This build never calls sceNpTrophyUnlockTrophy.

int32_t UserID = 0;
int32_t NPContext = 0;
int32_t NPHandle = 0;

static void notify_and_wait(const char* fmt, ...)
{
    char msg[900];
    memset(msg, 0, sizeof(msg));

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Notify("%s", msg);
    sceKernelUsleep(350000);
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    notify_and_wait("OpenOrbis Trophy Read Probe: starting");

    int ret = 0;

    ret = sceUserServiceInitialize(NULL);
    if (ret != 0 && (uint32_t)ret != 0x80960003)
    {
        Notify("USER_SERVICE_INIT failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    if ((ret = sceUserServiceGetInitialUser(&UserID)) != 0)
    {
        Notify("GET_INITIAL_USER failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    if ((ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY)) != 0)
    {
        Notify("LOAD_NP_TROPHY failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    if ((ret = sceNpTrophyCreateContext(&NPContext, UserID, 0, 0)) < 0)
    {
        Notify("CREATE_CONTEXT failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    if ((ret = sceNpTrophyCreateHandle(&NPHandle)) < 0)
    {
        Notify("CREATE_HANDLE failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    ret = sceNpTrophyRegisterContext(NPContext, NPHandle, 0);
    if (ret < 0)
    {
        Notify("REGISTER_CONTEXT failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    notify_and_wait("REGISTER_CONTEXT 0x00000000 - PASS");
    notify_and_wait("READ TEST: querying trophy IDs 0-15");

    int okCount = 0;
    uint32_t firstError = 0;

    for (int id = 0; id < 16; ++id)
    {
        OrbisNpTrophyDetails details;
        OrbisNpTrophyData data;

        memset(&details, 0, sizeof(details));
        memset(&data, 0, sizeof(data));

        details.size = sizeof(details);
        data.size = sizeof(data);

        ret = sceNpTrophyGetTrophyInfo(
            NPContext,
            NPHandle,
            id,
            &details,
            &data
        );

        if (ret == 0)
        {
            ++okCount;

            char safeName[81];
            memset(safeName, 0, sizeof(safeName));
            strncpy(safeName, details.TrophyName, sizeof(safeName) - 1);

            notify_and_wait(
                "TROPHY %02d OK | unlocked=%d | %s",
                id,
                data.IsUnlocked ? 1 : 0,
                safeName[0] ? safeName : "(no name)"
            );
        }
        else if (firstError == 0)
        {
            firstError = (uint32_t)ret;
        }
    }

    if (okCount > 0)
    {
        Notify(
            "READ PASS: %d trophies readable | first non-OK=0x%08X",
            okCount,
            firstError
        );
    }
    else
    {
        Notify(
            "READ FAILED: no trophy info readable | first=0x%08X",
            firstError
        );
    }

end:
    for (;;)
        sceKernelUsleep(1000000);

    return 0;
}
