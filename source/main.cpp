#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>

#include "util.h"

// OpenOrbis trophy registration control.
// Uses only the public BREW00094 sample data.
// Before RegisterContext it installs the sample Title Conf expected by
// the OpenOrbis README:
//   /user/trophy/conf/BREW00094_00-00/TROPHY.TRP
//   /user/trophy/conf/BREW00094_00-00/TRPPARAM.INI
// No trophy unlock API is called.

static const char* kConfDir = "/user/trophy/conf/BREW00094_00-00";
static const char* kConfTrp = "/user/trophy/conf/BREW00094_00-00/TROPHY.TRP";
static const char* kConfIni = "/user/trophy/conf/BREW00094_00-00/TRPPARAM.INI";
static const char* kPkgTrp  = "/app0/sce_sys/trophy/trophy00.trp";

static const char kTrpParam[] =
    "TROPSYSVER=1.0\n"
    "TROPTITLEID=BREW00094_00-00\n"
    "TROPAPPVER=1.0\n";

int32_t UserID = 0;
int32_t NPContext = 0;
int32_t NPHandle = 0;

static int copy_file(const char* src, const char* dst)
{
    FILE* in = fopen(src, "rb");
    if (!in)
        return -errno;

    FILE* out = fopen(dst, "wb");
    if (!out)
    {
        int e = errno;
        fclose(in);
        return -e;
    }

    char buf[16384];
    size_t total = 0;
    for (;;)
    {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n > 0)
        {
            if (fwrite(buf, 1, n, out) != n)
            {
                int e = errno ? errno : EIO;
                fclose(out);
                fclose(in);
                return -e;
            }
            total += n;
        }

        if (n < sizeof(buf))
        {
            if (ferror(in))
            {
                int e = errno ? errno : EIO;
                fclose(out);
                fclose(in);
                return -e;
            }
            break;
        }
    }

    fflush(out);
    fclose(out);
    fclose(in);
    return (int)total;
}

static int write_ini(void)
{
    FILE* f = fopen(kConfIni, "wb");
    if (!f)
        return -errno;

    const size_t len = sizeof(kTrpParam) - 1;
    size_t n = fwrite(kTrpParam, 1, len, f);
    fflush(f);
    fclose(f);

    if (n != len)
        return -(errno ? errno : EIO);

    return (int)n;
}

static int ensure_conf_dir(void)
{
    errno = 0;
    if (mkdir(kConfDir, 0777) == 0)
        return 0;
    if (errno == EEXIST)
        return 0;
    return -errno;
}

static int file_size(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return -errno;
    return (int)st.st_size;
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);

    Notify("OpenOrbis Trophy Conf Control: starting");

    int prep = ensure_conf_dir();
    if (prep < 0)
    {
        Notify("CONF DIR failed errno=%d", -prep);
        goto end;
    }

    prep = copy_file(kPkgTrp, kConfTrp);
    if (prep < 0)
    {
        Notify("TROPHY.TRP copy failed errno=%d", -prep);
        goto end;
    }

    int trpSize = file_size(kConfTrp);
    if (trpSize <= 0)
    {
        Notify("TROPHY.TRP verify failed %d", trpSize);
        goto end;
    }

    prep = write_ini();
    if (prep < 0)
    {
        Notify("TRPPARAM.INI write failed errno=%d", -prep);
        goto end;
    }

    int iniSize = file_size(kConfIni);
    if (iniSize <= 0)
    {
        Notify("TRPPARAM.INI verify failed %d", iniSize);
        goto end;
    }

    Notify("TITLE CONF READY - TRP=%d B INI=%d B", trpSize, iniSize);

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

    ret = sceNpTrophyRegisterContext(NPContext, NPHandle, 0);
    if (ret < 0)
    {
        Notify("REGISTER_CONTEXT failed 0x%08X", (uint32_t)ret);
        goto end;
    }

    Notify("REGISTER_CONTEXT 0x00000000 - TITLE CONF PASS");

end:
    for (;;)
        sceKernelUsleep(1000000);

    return 0;
}
