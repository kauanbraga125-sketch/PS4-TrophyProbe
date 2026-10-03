#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>

static FILE* g_log = nullptr;

static void log_line(const char* fmt, ...)
{
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    sceKernelDebugOutText(0, buf);

    if (g_log) {
        fprintf(g_log, "%s\n", buf);
        fflush(g_log);
    }
}

static void notify(const char* fmt, ...)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));

    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message), fmt, args);
    va_end(args);

    req.type = OrbisNotificationRequestType::NotificationRequest;
    req.unk3 = 0;
    req.useIconImageUri = 1;
    req.targetId = -1;
    strcpy(req.iconUri, "cxml://psnotification/tex_icon_system");

    sceKernelSendNotificationRequest(0, &req, 3120, 0);
}

static void report(const char* step, int32_t ret)
{
    log_line("[%s] ret = 0x%08X (%d)", step, (uint32_t)ret, ret);
    notify("TrophyProbe: %s -> 0x%08X", step, (uint32_t)ret);
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    g_log = fopen("/data/PS4-TrophyProbe.log", "a");
    log_line("==============================");
    log_line("PS4-TrophyProbe V0.1 starting");

    int32_t ret = 0;
    int32_t user_id = -1;
    int32_t trophy_context = -1;
    int32_t trophy_handle = -1;

    ret = sceUserServiceInitialize(nullptr);
    report("USER_SERVICE_INIT", ret);

    // OpenOrbis sample treats this specific return as 'already initialized'.
    if (ret != 0 && (uint32_t)ret != 0x80960003) {
        log_line("STOP: user service initialization failed.");
        goto cleanup;
    }

    ret = sceUserServiceGetInitialUser(&user_id);
    report("GET_INITIAL_USER", ret);
    if (ret != 0) {
        log_line("STOP: could not obtain initial user.");
        goto cleanup;
    }
    log_line("Initial user id = %d", user_id);

    ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY);
    report("LOAD_NP_TROPHY", ret);
    if (ret != 0) {
        log_line("STOP: NP Trophy system module did not load.");
        goto cleanup;
    }

    ret = sceNpTrophyCreateContext(&trophy_context, user_id, 0, 0);
    report("CREATE_CONTEXT", ret);
    if (ret < 0) {
        log_line("STOP: trophy context creation failed.");
        goto cleanup;
    }
    log_line("Context = %d", trophy_context);

    ret = sceNpTrophyCreateHandle(&trophy_handle);
    report("CREATE_HANDLE", ret);
    if (ret < 0) {
        log_line("STOP: trophy handle creation failed.");
        goto cleanup;
    }
    log_line("Handle = %d", trophy_handle);

    ret = sceNpTrophyRegisterContext(trophy_context, trophy_handle, 0);
    report("REGISTER_CONTEXT", ret);
    if (ret < 0) {
        log_line("STOP: context registration failed.");
        goto cleanup;
    }

    log_line("V0.1 SUCCESS: base trophy context was created and registered.");
    notify("TrophyProbe V0.1: base context OK");

cleanup:
    if (trophy_handle >= 0) {
        int32_t r = sceNpTrophyDestroyHandle(trophy_handle);
        log_line("[DESTROY_HANDLE] ret = 0x%08X (%d)", (uint32_t)r, r);
    }

    if (trophy_context >= 0) {
        int32_t r = sceNpTrophyDestroyContext(trophy_context);
        log_line("[DESTROY_CONTEXT] ret = 0x%08X (%d)", (uint32_t)r, r);
    }

    log_line("PS4-TrophyProbe V0.1 finished");
    log_line("==============================");

    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }

    for (;;) {
        sceKernelUsleep(1000000);
    }

    return 0;
}
