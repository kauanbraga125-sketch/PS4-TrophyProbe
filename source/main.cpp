#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>

#include "util.h"

// OpenOrbis trophy sample control build.
// Registration path intentionally matches the public sample.
// Trophy unlocking is intentionally NOT called: this build stops after
// sceNpTrophyRegisterContext so we can isolate registration behavior.

int32_t UserID = 0;
int32_t NPContext = 0;
int32_t NPHandle = 0;

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    Notify("OpenOrbis Trophy Control: starting");

    int ret = sceUserServiceInitialize(NULL);
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

    // Public OpenOrbis sample notes that a failure here can forcefully log out the user.
    ret = sceNpTrophyRegisterContext(NPContext, NPHandle, 0);
    if (ret < 0)
    {
        Notify("REGISTER_CONTEXT failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    Notify("REGISTER_CONTEXT 0x00000000 - CONTROL PASS");

end:
    for (;;)
        sceKernelUsleep(1000000);

    return 0;
}
