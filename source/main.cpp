#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#include <orbis/libkernel.h>

#include "util.h"

// Crossout Barrier Probe - read-only diagnostic.
// Target metadata is public identity information only:
//   US title: CUSA06848
//   Trophy set: NPWR12164_00
//
// This program does NOT call any trophy-unlock API and does not write to
// trophy/app metadata locations. It only probes filesystem visibility and
// records the result in /data/CrossoutBarrierProbe.log.

static const char* kLogPath = "/data/CrossoutBarrierProbe.log";

static int probe_path(const char* path, long long* outSize)
{
    struct stat st;
    errno = 0;
    if (stat(path, &st) == 0)
    {
        if (outSize) *outSize = (long long)st.st_size;
        return 1;
    }

    int e = errno;
    if (outSize) *outSize = -1;
    return -e;
}

static void fmt_status(int r, char* out, size_t outSize)
{
    if (!out || outSize == 0) return;

    if (r == 1)
        snprintf(out, outSize, "FOUND");
    else
        snprintf(out, outSize, "MISS/NOT-VISIBLE errno=%d", -r);
}

static void log_line(FILE* log, const char* fmt, ...)
{
    if (!log) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);
    fputc('\n', log);
    fflush(log);
}

static int scan_ascii_token(const char* path, const char* prefix, char* out, size_t outSize)
{
    if (!out || outSize == 0) return -EINVAL;
    out[0] = '\0';

    FILE* f = fopen(path, "rb");
    if (!f) return -errno;

    unsigned char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    const size_t prefixLen = strlen(prefix);
    if (prefixLen == 0 || n < prefixLen) return 0;

    for (size_t i = 0; i + prefixLen <= n; ++i)
    {
        if (memcmp(buf + i, prefix, prefixLen) != 0)
            continue;

        size_t j = i;
        size_t w = 0;
        while (j < n && w + 1 < outSize)
        {
            unsigned char c = buf[j];
            if (c < 0x20 || c > 0x7E)
                break;

            out[w++] = (char)c;
            ++j;

            if (w >= 32)
                break;
        }
        out[w] = '\0';
        return w > 0 ? 1 : 0;
    }

    return 0;
}

static int find_user_trophy_data(char* foundPath, size_t foundPathSize, int* rootErr)
{
    if (foundPath && foundPathSize) foundPath[0] = '\0';
    if (rootErr) *rootErr = 0;

    errno = 0;
    DIR* d = opendir("/user/home");
    if (!d)
    {
        if (rootErr) *rootErr = errno;
        return 0;
    }

    struct dirent* ent = NULL;
    while ((ent = readdir(d)) != NULL)
    {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;

        char path[512];
        snprintf(
            path,
            sizeof(path),
            "/user/home/%s/trophy/data/NPWR12164_00",
            ent->d_name
        );

        long long size = 0;
        if (probe_path(path, &size) == 1)
        {
            if (foundPath && foundPathSize)
            {
                strncpy(foundPath, path, foundPathSize - 1);
                foundPath[foundPathSize - 1] = '\0';
            }
            closedir(d);
            return 1;
        }
    }

    closedir(d);
    return 0;
}

static void notify_wait(const char* fmt, ...)
{
    char msg[900];
    memset(msg, 0, sizeof(msg));

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    Notify("%s", msg);
    sceKernelUsleep(650000);
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    FILE* log = fopen(kLogPath, "wb");

    notify_wait("Crossout Barrier Probe: CUSA06848 / NPWR12164_00");

    const char* paths[] = {
        "/user/app/CUSA06848",
        "/mnt/ext0/user/app/CUSA06848",
        "/user/appmeta/CUSA06848",
        "/system_data/priv/appmeta/CUSA06848",
        "/system_data/priv/appmeta/CUSA06848/nptitle.dat",
        "/system_data/priv/appmeta/CUSA06848/npbind.dat",
        "/user/trophy/conf/NPWR12164_00",
        "/user/trophy/conf/NPWR12164_00/TROPHY.TRP",
        "/user/trophy/conf/NPWR12164_00/TRPPARAM.INI"
    };

    const int pathCount = (int)(sizeof(paths) / sizeof(paths[0]));
    int results[pathCount];
    long long sizes[pathCount];

    log_line(log, "Crossout Barrier Probe");
    log_line(log, "Target title: CUSA06848");
    log_line(log, "Target trophy set: NPWR12164_00");
    log_line(log, "READ-ONLY: no unlock call, no metadata writes");
    log_line(log, "");

    for (int i = 0; i < pathCount; ++i)
    {
        results[i] = probe_path(paths[i], &sizes[i]);
        if (results[i] == 1)
            log_line(log, "[FOUND] %s size=%lld", paths[i], sizes[i]);
        else
            log_line(log, "[MISS/NOT-VISIBLE] %s errno=%d", paths[i], -results[i]);
    }

    char a[96], b[96], c[96], d[96], e[96], f[96], g[96], h[96], j[96];
    fmt_status(results[0], a, sizeof(a));
    fmt_status(results[1], b, sizeof(b));
    fmt_status(results[2], c, sizeof(c));
    fmt_status(results[3], d, sizeof(d));
    fmt_status(results[4], e, sizeof(e));
    fmt_status(results[5], f, sizeof(f));
    fmt_status(results[6], g, sizeof(g));
    fmt_status(results[7], h, sizeof(h));
    fmt_status(results[8], j, sizeof(j));

    notify_wait("INSTALL CHECK\ninternal: %s\nexternal: %s", a, b);
    notify_wait("APPMETA CHECK\n/user/appmeta: %s\n/system_data appmeta: %s", c, d);
    notify_wait("CREDENTIAL FILES\nnptitle.dat: %s\nnpbind.dat: %s", e, f);
    notify_wait("TITLE CONF\nNPWR12164_00: %s\nTROPHY.TRP: %s\nTRPPARAM.INI: %s", g, h, j);

    char token[64];
    int tokenRet = scan_ascii_token(
        "/system_data/priv/appmeta/CUSA06848/npbind.dat",
        "NPWR",
        token,
        sizeof(token)
    );

    if (tokenRet == 1)
    {
        notify_wait("NPBIND READ: detected %s", token);
        log_line(log, "npbind ASCII NP token: %s", token);
    }
    else
    {
        notify_wait("NPBIND READ: token unavailable (ret=%d)", tokenRet);
        log_line(log, "npbind ASCII NP token unavailable ret=%d", tokenRet);
    }

    char titleToken[64];
    int titleRet = scan_ascii_token(
        "/system_data/priv/appmeta/CUSA06848/nptitle.dat",
        "CUSA",
        titleToken,
        sizeof(titleToken)
    );

    if (titleRet == 1)
    {
        notify_wait("NPTITLE READ: detected %s", titleToken);
        log_line(log, "nptitle ASCII title token: %s", titleToken);
    }
    else
    {
        notify_wait("NPTITLE READ: token unavailable (ret=%d)", titleRet);
        log_line(log, "nptitle ASCII title token unavailable ret=%d", titleRet);
    }

    char userDataPath[512];
    int homeErr = 0;
    int userDataFound = find_user_trophy_data(userDataPath, sizeof(userDataPath), &homeErr);

    if (userDataFound)
    {
        notify_wait("USER TROPHY DATA: FOUND\n%s", userDataPath);
        log_line(log, "User trophy data: FOUND %s", userDataPath);
    }
    else
    {
        notify_wait("USER TROPHY DATA: not found/visible\n/user/home errno=%d", homeErr);
        log_line(log, "User trophy data: not found/visible; /user/home errno=%d", homeErr);
    }

    const int installed =
        (results[0] == 1 || results[1] == 1);

    const int appMetaCredentials =
        (results[4] == 1 && results[5] == 1);

    const int titleConf =
        (results[7] == 1 && results[8] == 1);

    log_line(log, "");
    log_line(log, "SUMMARY installed=%d appmeta_credentials=%d title_conf=%d user_trophy_data=%d",
             installed, appMetaCredentials, titleConf, userDataFound);

    if (!installed && !appMetaCredentials && !titleConf)
    {
        notify_wait(
            "SUMMARY: Crossout is not installed and no retail trophy context files are visible.\n"
            "This explains how a catalog can list names while the system lacks a real trophy context."
        );
    }
    else
    {
        notify_wait(
            "SUMMARY: some Crossout context data is present/visible.\n"
            "Send the probe results so we can isolate the next failing layer."
        );
    }

    notify_wait("DONE - log saved at /data/CrossoutBarrierProbe.log");

    if (log) fclose(log);

    for (;;)
        sceKernelUsleep(1000000);

    return 0;
}
