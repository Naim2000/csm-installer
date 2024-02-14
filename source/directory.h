#include <string.h>
#include <stdbool.h>

typedef bool (*FileFilter)(const char* name);
/* Soon (?) */
typedef int (*SingleFileCallback)(const char* filepath, void* userp); // obligatory void* argument for every callback
typedef int (*MultiFileCallback)(int cnt, const char* const list[], void* userp);

int SelectFileMenu(const char* header, const char* defaultFolder, SingleFileCallback, FileFilter, void* userp);
bool hasFileExtension(const char* path, const char* extension);
int QuickActionMenu(int argc, const char* argv[]);
