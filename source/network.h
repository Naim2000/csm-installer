#include <stddef.h>

typedef struct {
	void* ptr;
	size_t size;
} blob;

int network_init();
void network_deinit();
int DownloadFile(char* url, blob*);
const char* GetLastDownloadError();
