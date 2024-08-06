#include <stdbool.h>
#include <stddef.h>
#include <gctypes.h>
#include <ogc/es.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>

struct sysmenu {
    tmd tmd;
    tik ticket;
    tmd_content archive;

    char platform;
    char region;
    char versionMajor;

    bool hasPriiloader;
    bool isvWii;
};

extern struct sysmenu sysmenu[];

int sysmenu_process();
