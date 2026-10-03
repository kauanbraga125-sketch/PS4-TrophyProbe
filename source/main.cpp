#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>

static FILE* g_log = nullptr;
static char g_results[4096];
static size_t g_results_len = 0;

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

static void append_text(const char* fmt, ...)
{
    if (g_results_len >= sizeof(g_results) - 1)
        return;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(g_results + g_results_len,
                            sizeof(g_results) - g_results_len,
                            fmt,
                            args);
    va_end(args);

    if (written <= 0)
        return;

    size_t available = sizeof(g_results) - g_results_len;
    size_t used = (size_t)written;
    if (used >= available)
        g_results_len = sizeof(g_results) - 1;
    else
        g_results_len += used;
}

static void report(const char* step, int32_t ret)
{
    log_line("[%s] ret = 0x%08X (%d)", step, (uint32_t)ret, ret);
    append_text("%-18s 0x%08X\n", step, (uint32_t)ret);
}

static bool probe_file(const char* label, const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        log_line("[FILE] MISSING %s -> %s", label, path);
        append_text("%-15s MISSING\n", label);
        return false;
    }

    long size = -1;
    if (fseek(f, 0, SEEK_END) == 0)
        size = ftell(f);
    fclose(f);

    log_line("[FILE] FOUND %s -> %s (size=%ld)", label, path, size);
    if (size >= 0)
        append_text("%-15s FOUND (%ld B)\n", label, size);
    else
        append_text("%-15s FOUND\n", label);
    return true;
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

static bool show_results_dialog(const char* message)
{
    int32_t ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG);
    log_line("[LOAD_MESSAGE_DIALOG] ret = 0x%08X (%d)", (uint32_t)ret, ret);
    if (ret < 0)
        return false;

    ret = sceCommonDialogInitialize();
    log_line("[COMMON_DIALOG_INIT] ret = 0x%08X (%d)", (uint32_t)ret, ret);
    if (ret < 0)
        return false;

    ret = sceMsgDialogInitialize();
    log_line("[MSG_DIALOG_INIT] ret = 0x%08X (%d)", (uint32_t)ret, ret);
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
    log_line("[MSG_DIALOG_OPEN] ret = 0x%08X (%d)", (uint32_t)ret, ret);
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

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    memset(g_results, 0, sizeof(g_results));

    g_log = fopen("/data/PS4-TrophyProbe.log", "a");
    log_line("==============================");
    log_line("PS4-TrophyProbe V0.1.4 starting");

    append_text("PS4 TrophyProbe v0.1.4\n");
    append_text("Title ID: %s\n\n", kTitleId);

    append_text("FILE PROBE\n");
    probe_file("param.sfo", "/app0/sce_sys/param.sfo");
    bool has_np_title = probe_file("nptitle.dat", "/app0/sce_sys/nptitle.dat");
    bool has_np_bind = probe_file("npbind.dat", "/app0/sce_sys/npbind.dat");
    bool has_trophy_trp = probe_file("trophy00.trp", "/app0/sce_sys/trophy/trophy00.trp");
    bool has_conf_trp = probe_file("conf TROPHY.TRP", "/user/trophy/conf/BREW00901_00-00/TROPHY.TRP");
    bool has_conf_ini = probe_file("conf TRPPARAM", "/user/trophy/conf/BREW00901_00-00/TRPPARAM.INI");

    log_line("File summary: nptitle=%d npbind=%d trophy=%d conf_trp=%d conf_ini=%d",
             has_np_title, has_np_bind, has_trophy_trp, has_conf_trp, has_conf_ini);

    append_text("\nTROPHY API\n");

    int32_t ret = 0;
    int32_t user_id = -1;
    int32_t trophy_context = -1;
    int32_t trophy_handle = -1;
    const char* last_step = "START";

    ret = sceUserServiceInitialize(nullptr);
    report("USER_SERVICE_INIT", ret);
    last_step = "USER_SERVICE_INIT";

    // OpenOrbis sample treats 0x80960003 as already initialized.
    if (ret != 0 && (uint32_t)ret != 0x80960003) {
        append_text("\nSTOP: user service init failed.\n");
        goto cleanup;
    }

    ret = sceUserServiceGetInitialUser(&user_id);
    report("GET_INITIAL_USER", ret);
    last_step = "GET_INITIAL_USER";
    if (ret != 0) {
        append_text("\nSTOP: initial user unavailable.\n");
        goto cleanup;
    }
    log_line("Initial user id = %d", user_id);

    ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY);
    report("LOAD_NP_TROPHY", ret);
    last_step = "LOAD_NP_TROPHY";
    if (ret != 0) {
        append_text("\nSTOP: NP Trophy module failed.\n");
        goto cleanup;
    }

    ret = sceNpTrophyCreateContext(&trophy_context, user_id, 0, 0);
    report("CREATE_CONTEXT", ret);
    last_step = "CREATE_CONTEXT";
    if (ret < 0) {
        append_text("\nSTOP: trophy context failed.\n");
        goto cleanup;
    }
    log_line("Context = %d", trophy_context);

    ret = sceNpTrophyCreateHandle(&trophy_handle);
    report("CREATE_HANDLE", ret);
    last_step = "CREATE_HANDLE";
    if (ret < 0) {
        append_text("\nSTOP: trophy handle failed.\n");
        goto cleanup;
    }
    log_line("Handle = %d", trophy_handle);

    ret = sceNpTrophyRegisterContext(trophy_context, trophy_handle, 0);
    report("REGISTER_CONTEXT", ret);
    last_step = "REGISTER_CONTEXT";
    if (ret < 0) {
        if ((uint32_t)ret == 0x80550016)
            append_text("\n0x80550016: NP_TITLE_DAT_NOT_FOUND\n");
        append_text("STOP: context registration failed.\n");
        goto cleanup;
    }

    append_text("\nBASE CONTEXT: OK\n");
    log_line("V0.1.4 SUCCESS: base trophy context created and registered.");

cleanup:
    append_text("\nLast step: %s\n", last_step);

    if (trophy_handle >= 0) {
        int32_t r = sceNpTrophyDestroyHandle(trophy_handle);
        log_line("[DESTROY_HANDLE] ret = 0x%08X (%d)", (uint32_t)r, r);
    }

    if (trophy_context >= 0) {
        int32_t r = sceNpTrophyDestroyContext(trophy_context);
        log_line("[DESTROY_CONTEXT] ret = 0x%08X (%d)", (uint32_t)r, r);
    }

    log_line("PS4-TrophyProbe V0.1.4 finished");
    log_line("==============================");

    if (g_log) {
        fclose(g_log);
        g_log = nullptr;
    }

    if (!show_results_dialog(g_results)) {
        notify("TrophyProbe: nao foi possivel abrir a tela de resultados");
        for (int i = 0; i < 15; ++i)
            sceKernelUsleep(1000000);
    }

    return 0;
}
