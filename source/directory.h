#include <string.h>
#include <stdbool.h>
#include <gctypes.h>

#define MAX_ENTRIES 20

typedef bool (*FileFilter)(const char* name);

char* pwd();
char* SelectFileMenu(const char* header, const char* defaultFolder, FileFilter filter);
bool hasFileExtension(const char* path, const char* extension);
