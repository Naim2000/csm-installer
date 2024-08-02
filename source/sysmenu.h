#include <stdbool.h>
#include <stddef.h>
#include <gctypes.h>
#include <ogc/es.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>

struct sysmenu {
    uint16_t version;
    char platform;
    char region;
    char versionMajor;
    bool hasPriiloader;
    bool isvWii;
    tmd_content archive;
    aeskey titlekey;
};

extern struct sysmenu sysmenu[];

int sysmenu_process();
