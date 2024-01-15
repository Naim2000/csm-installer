#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>

int sysmenu_process();

bool hasPriiloader();
uint64_t getSmNUSTitleID();
const uint8_t* getSmTitleKey();
uint32_t getArchiveCid();
size_t getArchiveSize();
bool isArchive(sha1);
uint16_t getSmVersion();
char getSmVersionMajor();
char getSmRegion();
