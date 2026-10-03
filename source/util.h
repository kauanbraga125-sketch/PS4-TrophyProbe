#pragma once

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <orbis/libkernel.h>

inline static void Notify(const char* FMT, ...)
{
    OrbisNotificationRequest Buffer;
    memset(&Buffer, 0, sizeof(Buffer));

    va_list args;
    va_start(args, FMT);
    vsnprintf(Buffer.message, sizeof(Buffer.message), FMT, args);
    va_end(args);

    Buffer.type = OrbisNotificationRequestType::NotificationRequest;
    Buffer.unk3 = 0;
    Buffer.useIconImageUri = 1;
    Buffer.targetId = -1;
    strcpy(Buffer.iconUri, "cxml://psnotification/tex_icon_champions_league");

    sceKernelSendNotificationRequest(0, &Buffer, 3120, 0);
}
