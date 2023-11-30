#include <ogc/isfs.h>
#include <fat.h>

typedef int (*RWCallback)(size_t read, size_t filesize);

#define MAXIMUM(max, size) ( ( size > max ) ? max : size )
#ifndef FS_CHUNK
#define FS_CHUNK 1048576
#endif

int NAND_GetFileSize(const char* filepath, size_t*);
int FAT_GetFileSize(const char* filepath, size_t*);
int NAND_Read(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int FAT_Read(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int NAND_Write(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int FAT_Write(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int progressbar(size_t read, size_t filesize);
