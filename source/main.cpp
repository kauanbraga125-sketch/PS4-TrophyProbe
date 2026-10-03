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

static void append_text(char* out, size_t out_size, size_t* used, const char* fmt, ...)
{
    if (!out || !used || *used >= out_size - 1)
        return;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *used, out_size - *used, fmt, args);
    va_end(args);

    if (written <= 0)
        return;

    size_t available = out_size - *used;
    size_t n = (size_t)written;
    if (n >= available)
        *used = out_size - 1;
    else
        *used += n;
}

static const char* yn(bool value)
{
    return value ? "YES" : "NO";
}

static bool stat_path(const char* path, long long* size_out, int* err_out)
{
    struct stat st;
    errno = 0;
    if (stat(path, &st) == 0) {
        if (size_out)
            *size_out = (long long)st.st_size;
        if (err_out)
            *err_out = 0;
        return true;
    }

    if (size_out)
        *size_out = -1;
    if (err_out)
        *err_out = errno;
    return false;
}

static void probe_path(char* out, size_t out_size, size_t* used,
                       const char* label, const char* path)
{
    long long size = -1;
    int err = 0;
    bool ok = stat_path(path, &size, &err);

    if (ok) {
        append_text(out, out_size, used, "%-14s FOUND (%lld B)\n", label, size);
        log_line("[PATH] FOUND %s -> %s (%lld B)", label, path, size);
    } else {
        append_text(out, out_size, used, "%-14s MISS e=%d\n", label, err);
        log_line("[PATH] MISSING %s -> %s errno=%d", label, path, err);
    }
}

static bool probe_dir(const char* path, int* err_out)
{
    errno = 0;
    DIR* d = opendir(path);
    if (!d) {
        if (err_out)
            *err_out = errno;
        return false;
    }
    closedir(d);
    if (err_out)
        *err_out = 0;
    return true;
}

static void list_dir(char* out, size_t out_size, size_t* used,
                     const char* label, const char* path, int max_entries)
{
    errno = 0;
    DIR* d = opendir(path);
    if (!d) {
        append_text(out, out_size, used, "%s: CANNOT OPEN (e=%d)\n", label, errno);
        log_line("[DIR] cannot open %s (%s), errno=%d", label, path, errno);
        return;
    }

    append_text(out, out_size, used, "%s:\n", label);
    log_line("[DIR] listing %s -> %s", label, path);

    int count = 0;
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr && count < max_entries) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        append_text(out, out_size, used, "  %s\n", ent->d_name);
        log_line("[DIR]   %s", ent->d_name);
        ++count;
    }

    if (count == 0)
        append_text(out, out_size, used, "  <empty>\n");
    else if (ent != nullptr)
        append_text(out, out_size, used, "  ...\n");

    closedir(d);
}

static inline void common_dialog_set_magic(uint32_t* magic,
                                           const OrbisCommonDialogBaseParam* param)
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
    if (ready)
        return true;

    int32_t ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
    log_line("[LOAD_MESSAGE_DIALOG] ret=0x%08X", (uint32_t)ret);
    if (ret < 0)
        return false;

    ret = sceCommonDialogInitialize();
    log_line("[COMMON_DIALOG_INIT] ret=0x%08X", (uint32_t)ret);
    if (ret < 0)
        return false;

    ready = true;
    return true;
}

static bool show_dialog(const char* message)
{
    if (!ensure_dialog_system())
        return false;

    int32_t ret = sceMsgDialogInitialize();
    log_line("[MSG_DIALOG_INIT] ret=0x%08X", (uint32_t)ret);
    if (ret < 0)
        return false;

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
    log_line("[MSG_DIALOG_OPEN] ret=0x%08X", (uint32_t)ret);
    if (ret < 0) {
        sceMsgDialogTerminate();
        return false;
    }

    while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
        sceKernelUsleep(10000);

    sceMsgDialogGetResult(&result);
    sceMsgDialogClose();
    sceMsgDialogTerminate();
    return true;
}

static void report_api(char* out, size_t out_size, size_t* used,
                       const char* step, int32_t ret)
{
    append_text(out, out_size, used, "%-18s 0x%08X\n", step, (uint32_t)ret);
    log_line("[%s] ret=0x%08X (%d)", step, (uint32_t)ret, ret);
}

static void hold_after_register_failure()
{
    log_line("Holding process after REGISTER_CONTEXT failure; skipping trophy cleanup to avoid post-test crash.");
    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }

    for (;;) {
        sceKernelUsleep(1000000);
    }
}

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    g_log = fopen("/data/PS4-TrophyProbe.log", "a");

    log_line("==============================");
    log_line("PS4-TrophyProbe V0.1.6 starting");

    char page1[4096];
    char page2[4096];
    memset(page1, 0, sizeof(page1));
    memset(page2, 0, sizeof(page2));
    size_t p1 = 0;
    size_t p2 = 0;

    append_text(page1, sizeof(page1), &p1, "PS4 TrophyProbe v0.1.6\n");
    append_text(page1, sizeof(page1), &p1, "Title ID: %s\n\n", kTitleId);

    char cwd[512];
    memset(cwd, 0, sizeof(cwd));
    if (getcwd(cwd, sizeof(cwd))) {
        append_text(page1, sizeof(page1), &p1, "CWD: %s\n", cwd);
        log_line("CWD=%s", cwd);
    } else {
        append_text(page1, sizeof(page1), &p1, "CWD: unavailable (e=%d)\n", errno);
        log_line("getcwd failed errno=%d", errno);
    }

    const char* sandbox = sceKernelGetFsSandboxRandomWord();
    append_text(page1, sizeof(page1), &p1, "Sandbox: %s\n\n",
                (sandbox && sandbox[0]) ? sandbox : "<none>");
    log_line("Sandbox word=%s", sandbox ? sandbox : "<null>");

    int e_app0 = 0, e_sys = 0, e_module = 0;
    bool d_app0 = probe_dir("/app0", &e_app0);
    bool d_sys = probe_dir("/app0/sce_sys", &e_sys);
    bool d_module = probe_dir("/app0/sce_module", &e_module);

    append_text(page1, sizeof(page1), &p1, "MOUNTS\n");
    append_text(page1, sizeof(page1), &p1, "/app0              %s (e=%d)\n", yn(d_app0), e_app0);
    append_text(page1, sizeof(page1), &p1, "/app0/sce_sys      %s (e=%d)\n", yn(d_sys), e_sys);
    append_text(page1, sizeof(page1), &p1, "/app0/sce_module   %s (e=%d)\n\n", yn(d_module), e_module);

    append_text(page1, sizeof(page1), &p1, "CONTROL FILES\n");
    probe_path(page1, sizeof(page1), &p1, "eboot.bin", "/app0/eboot.bin");
    probe_path(page1, sizeof(page1), &p1, "libc.prx", "/app0/sce_module/libc.prx");
    probe_path(page1, sizeof(page1), &p1, "Fios2.prx", "/app0/sce_module/libSceFios2.prx");

    append_text(page1, sizeof(page1), &p1, "\nSCE_SYS TESTS\n");
    probe_path(page1, sizeof(page1), &p1, "param.sfo", "/app0/sce_sys/param.sfo");
    probe_path(page1, sizeof(page1), &p1, "icon0.png", "/app0/sce_sys/icon0.png");
    probe_path(page1, sizeof(page1), &p1, "right.sprx", "/app0/sce_sys/about/right.sprx");
    probe_path(page1, sizeof(page1), &p1, "nptitle.dat", "/app0/sce_sys/nptitle.dat");
    probe_path(page1, sizeof(page1), &p1, "npbind.dat", "/app0/sce_sys/npbind.dat");

    append_text(page2, sizeof(page2), &p2, "PS4 TrophyProbe v0.1.6 - DIRS\n\n");
    list_dir(page2, sizeof(page2), &p2, "/app0", "/app0", 12);
    append_text(page2, sizeof(page2), &p2, "\n");
    list_dir(page2, sizeof(page2), &p2, "/app0/sce_sys", "/app0/sce_sys", 12);

    if (!show_dialog(page1))
        notify("TrophyProbe: mount diagnostics dialog failed");
    if (!show_dialog(page2))
        notify("TrophyProbe: directory diagnostics dialog failed");

    char api[4096];
    memset(api, 0, sizeof(api));
    size_t pa = 0;
    append_text(api, sizeof(api), &pa, "PS4 TrophyProbe v0.1.6 - TROPHY API\n\n");

    int32_t ret = 0;
    int32_t user_id = -1;
    int32_t trophy_context = -1;
    int32_t trophy_handle = -1;
    const char* last_step = "START";

    ret = sceUserServiceInitialize(nullptr);
    report_api(api, sizeof(api), &pa, "USER_SERVICE_INIT", ret);
    last_step = "USER_SERVICE_INIT";
    if (ret != 0 && (uint32_t)ret != 0x80960003) {
        append_text(api, sizeof(api), &pa, "\nSTOP: user service init failed.\n");
        goto cleanup;
    }

    ret = sceUserServiceGetInitialUser(&user_id);
    report_api(api, sizeof(api), &pa, "GET_INITIAL_USER", ret);
    last_step = "GET_INITIAL_USER";
    if (ret != 0) {
        append_text(api, sizeof(api), &pa, "\nSTOP: initial user unavailable.\n");
        goto cleanup;
    }

    ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY);
    report_api(api, sizeof(api), &pa, "LOAD_NP_TROPHY", ret);
    last_step = "LOAD_NP_TROPHY";
    if (ret != 0) {
        append_text(api, sizeof(api), &pa, "\nSTOP: NP Trophy module failed.\n");
        goto cleanup;
    }

    ret = sceNpTrophyCreateContext(&trophy_context, user_id, 0, 0);
    report_api(api, sizeof(api), &pa, "CREATE_CONTEXT", ret);
    last_step = "CREATE_CONTEXT";
    if (ret < 0) {
        append_text(api, sizeof(api), &pa, "\nSTOP: trophy context failed.\n");
        goto cleanup;
    }

    ret = sceNpTrophyCreateHandle(&trophy_handle);
    report_api(api, sizeof(api), &pa, "CREATE_HANDLE", ret);
    last_step = "CREATE_HANDLE";
    if (ret < 0) {
        append_text(api, sizeof(api), &pa, "\nSTOP: trophy handle failed.\n");
        goto cleanup;
    }

    ret = sceNpTrophyRegisterContext(trophy_context, trophy_handle, 0);
    report_api(api, sizeof(api), &pa, "REGISTER_CONTEXT", ret);
    last_step = "REGISTER_CONTEXT";
    if (ret < 0) {
        if ((uint32_t)ret == 0x80550016)
            append_text(api, sizeof(api), &pa, "\n0x80550016: NP_TITLE_DAT_NOT_FOUND\n");
        append_text(api, sizeof(api), &pa,
                    "STOP: context registration failed.\n"
                    "Cleanup skipped intentionally.\n"
                    "Close the app from the PS4 menu after OK.\n");
        append_text(api, sizeof(api), &pa, "\nLast step: %s\n", last_step);

        if (!show_dialog(api))
            notify("TrophyProbe: Trophy API dialog failed");

        hold_after_register_failure();
    }

    append_text(api, sizeof(api), &pa, "\nBASE CONTEXT: OK\n");

cleanup:
    append_text(api, sizeof(api), &pa, "\nLast step: %s\n", last_step);

    if (trophy_handle >= 0)
        sceNpTrophyDestroyHandle(trophy_handle);
    if (trophy_context >= 0)
        sceNpTrophyDestroyContext(trophy_context);

    if (!show_dialog(api))
        notify("TrophyProbe: Trophy API dialog failed");

    log_line("PS4-TrophyProbe V0.1.6 finished");
    log_line("==============================");

    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }

    return 0;
}
