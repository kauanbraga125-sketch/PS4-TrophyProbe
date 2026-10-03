#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>

static FILE* g_log = nullptr;
static const char* kTitleId = "BREW00901";

static void log_line(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sceKernelDebugOutText(0, buf);
    if (g_log) { fprintf(g_log, "%s\n", buf); fflush(g_log); }
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
    req.useIconImageUri = 1;
    req.targetId = -1;
    strcpy(req.iconUri, "cxml://psnotification/tex_icon_system");
    sceKernelSendNotificationRequest(0, &req, 3120, 0);
}

static void append_text(char* out, size_t out_size, size_t* used, const char* fmt, ...)
{
    if (!out || !used || *used >= out_size - 1) return;
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *used, out_size - *used, fmt, args);
    va_end(args);
    if (written <= 0) return;
    size_t available = out_size - *used;
    size_t n = (size_t)written;
    *used = (n >= available) ? out_size - 1 : *used + n;
}

static bool stat_path(const char* path, long long* size_out, int* err_out)
{
    struct stat st;
    errno = 0;
    if (stat(path, &st) == 0) {
        if (size_out) *size_out = (long long)st.st_size;
        if (err_out) *err_out = 0;
        return true;
    }
    if (size_out) *size_out = -1;
    if (err_out) *err_out = errno;
    return false;
}

static void probe_path(char* out, size_t out_size, size_t* used, const char* label, const char* path)
{
    long long size = -1;
    int err = 0;
    bool ok = stat_path(path, &size, &err);
    if (ok) {
        append_text(out, out_size, used, "%-15s FOUND (%lld B)\n", label, size);
        log_line("[PATH] FOUND %s -> %s (%lld B)", label, path, size);
    } else {
        append_text(out, out_size, used, "%-15s MISS e=%d\n", label, err);
        log_line("[PATH] MISS %s -> %s errno=%d", label, path, err);
    }
}

static inline void common_dialog_set_magic(uint32_t* magic, const OrbisCommonDialogBaseParam* param)
{
    *magic = (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)param);
}

static inline void common_dialog_base_init(OrbisCommonDialogBaseParam* param)
{
    memset(param, 0, sizeof(*param));
    param->size = (uint32_t)sizeof(*param);
    common_dialog_set_magic(&param->magic, param);
}

static inline void msg_dialog_param_init(OrbisMsgDialogParam* param)
{
    memset(param, 0, sizeof(*param));
    common_dialog_base_init(&param->baseParam);
    param->size = sizeof(*param);
}

static bool ensure_dialog_system()
{
    static bool ready = false;
    if (ready) return true;
    int32_t ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
    log_line("[LOAD_MESSAGE_DIALOG] 0x%08X", (uint32_t)ret);
    if (ret < 0) return false;
    ret = sceCommonDialogInitialize();
    log_line("[COMMON_DIALOG_INIT] 0x%08X", (uint32_t)ret);
    if (ret < 0) return false;
    ready = true;
    return true;
}

static bool show_dialog(const char* message)
{
    if (!ensure_dialog_system()) return false;
    int32_t ret = sceMsgDialogInitialize();
    if (ret < 0) return false;

    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam user_param;
    OrbisMsgDialogResult result;
    msg_dialog_param_init(&param);
    memset(&user_param, 0, sizeof(user_param));
    memset(&result, 0, sizeof(result));
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
    user_param.msg = message;
    user_param.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    param.userMsgParam = &user_param;

    ret = sceMsgDialogOpen(&param);
    if (ret < 0) { sceMsgDialogTerminate(); return false; }
    while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
        sceKernelUsleep(10000);
    sceMsgDialogGetResult(&result);
    sceMsgDialogClose();
    sceMsgDialogTerminate();
    return true;
}

static void report_api(char* out, size_t out_size, size_t* used, const char* step, int32_t ret)
{
    append_text(out, out_size, used, "%-19s 0x%08X\n", step, (uint32_t)ret);
    log_line("[%s] 0x%08X (%d)", step, (uint32_t)ret, ret);
}

static void hold_after_failure()
{
    log_line("Holding after failure; close app from PS4 menu.");
    if (g_log) { fclose(g_log); g_log = nullptr; }
    for (;;) sceKernelUsleep(1000000);
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    g_log = fopen("/data/PS4-TrophyProbe.log", "a");
    log_line("==============================");
    log_line("PS4-TrophyProbe V0.1.7 starting");

    char files[4096];
    memset(files, 0, sizeof(files));
    size_t pf = 0;
    append_text(files, sizeof(files), &pf, "PS4 TrophyProbe v0.1.7 - FILES\nTitle ID: %s\n\n", kTitleId);
    probe_path(files, sizeof(files), &pf, "eboot.bin", "/app0/eboot.bin");
    probe_path(files, sizeof(files), &pf, "param.sfo", "/app0/sce_sys/param.sfo");
    probe_path(files, sizeof(files), &pf, "nptitle.dat", "/app0/sce_sys/nptitle.dat");
    probe_path(files, sizeof(files), &pf, "npbind.dat", "/app0/sce_sys/npbind.dat");
    probe_path(files, sizeof(files), &pf, "trophy00.trp", "/app0/sce_sys/trophy/trophy00.trp");
    probe_path(files, sizeof(files), &pf, "libc.prx", "/app0/sce_module/libc.prx");
    probe_path(files, sizeof(files), &pf, "Fios2.prx", "/app0/sce_module/libSceFios2.prx");
    if (!show_dialog(files)) notify("TrophyProbe: files dialog failed");

    char api[4096];
    memset(api, 0, sizeof(api));
    size_t pa = 0;
    append_text(api, sizeof(api), &pa, "PS4 TrophyProbe v0.1.7 - API\n\n");

    int32_t ret = 0;
    int32_t user_id = -1;
    int32_t trophy_context = -1;
    int32_t trophy_handle = -1;

    ret = sceUserServiceInitialize(nullptr);
    report_api(api, sizeof(api), &pa, "USER_SERVICE_INIT", ret);
    if (ret != 0 && (uint32_t)ret != 0x80960003) goto fatal;

    ret = sceUserServiceGetInitialUser(&user_id);
    report_api(api, sizeof(api), &pa, "GET_INITIAL_USER", ret);
    if (ret != 0) goto fatal;

    ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY);
    report_api(api, sizeof(api), &pa, "LOAD_NP_TROPHY", ret);
    if (ret != 0) goto fatal;

    ret = sceNpTrophyCreateContext(&trophy_context, user_id, 0, 0);
    report_api(api, sizeof(api), &pa, "CREATE_CONTEXT", ret);
    if (ret < 0) goto fatal;

    ret = sceNpTrophyCreateHandle(&trophy_handle);
    report_api(api, sizeof(api), &pa, "CREATE_HANDLE", ret);
    if (ret < 0) goto fatal;

    ret = sceNpTrophyRegisterContext(trophy_context, trophy_handle, 0);
    report_api(api, sizeof(api), &pa, "REGISTER_CONTEXT", ret);
    if (ret < 0) {
        if ((uint32_t)ret == 0x80550016)
            append_text(api, sizeof(api), &pa, "\n0x80550016: NP_TITLE_DAT_NOT_FOUND\n");
        append_text(api, sizeof(api), &pa, "\nSTOP: registration failed.\nClose app after OK.\n");
        show_dialog(api);
        hold_after_failure();
    }

    append_text(api, sizeof(api), &pa, "\nREGISTERED CONTEXT: OK\nNext: read-only trophy probes.\n");
    show_dialog(api);

    {
        char info[8192];
        memset(info, 0, sizeof(info));
        size_t pi = 0;
        append_text(info, sizeof(info), &pi, "v0.1.7 - TROPHY INFO (read-only)\n\n");
        int valid = 0;
        for (int id = 0; id < 16; ++id) {
            OrbisNpTrophyDetails details;
            OrbisNpTrophyData data;
            memset(&details, 0, sizeof(details));
            memset(&data, 0, sizeof(data));
            details.size = sizeof(details);
            data.size = sizeof(data);
            int32_t ir = sceNpTrophyGetTrophyInfo(trophy_context, trophy_handle, id, &details, &data);
            if (ir >= 0) {
                ++valid;
                append_text(info, sizeof(info), &pi, "ID %02d OK  unlocked=%d  %s\n", id, data.IsUnlocked ? 1 : 0, details.TrophyName);
                log_line("[TROPHY_INFO %d] OK name='%s' unlocked=%d", id, details.TrophyName, data.IsUnlocked ? 1 : 0);
            } else {
                append_text(info, sizeof(info), &pi, "ID %02d 0x%08X\n", id, (uint32_t)ir);
                log_line("[TROPHY_INFO %d] 0x%08X", id, (uint32_t)ir);
            }
        }
        append_text(info, sizeof(info), &pi, "\nValid trophy records: %d\n", valid);

        char cache1[256];
        char cache2[256];
        snprintf(cache1, sizeof(cache1), "/user/trophy/conf/%s_00-00/TROPHY.TRP", kTitleId);
        snprintf(cache2, sizeof(cache2), "/user/trophy/conf/%s_00-00/TRPPARAM.INI", kTitleId);
        append_text(info, sizeof(info), &pi, "\nLOCAL CACHE PROBE\n");
        probe_path(info, sizeof(info), &pi, "TROPHY.TRP", cache1);
        probe_path(info, sizeof(info), &pi, "TRPPARAM.INI", cache2);
        show_dialog(info);
    }

    if (trophy_handle >= 0) sceNpTrophyDestroyHandle(trophy_handle);
    if (trophy_context >= 0) sceNpTrophyDestroyContext(trophy_context);
    log_line("PS4-TrophyProbe V0.1.7 finished OK");
    if (g_log) { fclose(g_log); g_log = nullptr; }
    return 0;

fatal:
    append_text(api, sizeof(api), &pa, "\nSTOP: pre-registration stage failed.\n");
    show_dialog(api);
    if (trophy_handle >= 0) sceNpTrophyDestroyHandle(trophy_handle);
    if (trophy_context >= 0) sceNpTrophyDestroyContext(trophy_context);
    if (g_log) { fclose(g_log); g_log = nullptr; }
    return 0;
}
