#include <string.h>
#include <stdbool.h>
#include <gctypes.h>
#include <dirent.h>

#define MAX_ENTRIES 20

typedef bool (*FileFilter)(const char* name);

int SelectFileMenu(const char* header, const char* defaultFolder, FileFilter filter, char filepath[PATH_MAX]);
bool hasFileExtension(const char* path, const char* extension);
