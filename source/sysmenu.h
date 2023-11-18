#include <stdbool.h>
#include <stddef.h>
#include <gctypes.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>

int sysmenu_process();
bool hasPriiloader();
u64 getSmNUSTitleID();
u8* getSmTitleKey();
u32 getArchiveCid();
size_t getArchiveSize();
const char* getArchivePath();
u8* getArchiveHash();
u16 getSmVersion();
char getSmVersionMajor();
char getSmRegion();
