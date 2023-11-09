#include <stdbool.h>
#include <gctypes.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>

static char sysmenu_filepath[ISFS_MAXPATH] = "/title/00000001/00000002/content/";

int sysmenu_process();
bool hasPriiloader();
u32 getArchiveCid();
