#include <stdio.h>
#include <sys/param.h>
#include <ogc/isfs.h>
#include <fat.h>

typedef int (*RWCallback)(size_t read, size_t filesize);

#ifndef FS_CHUNK
#define FS_CHUNK 0x80000
#endif

int NAND_GetFileSize(const char* filepath, size_t*);
int FAT_GetFileSize(const char* filepath, size_t*);
int NAND_Read(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int FAT_Read(const char* filepath, void* buffer, size_t filesize, RWCallback cb);
int NAND_Write(const char* filepath, const void* buffer, size_t filesize, RWCallback cb);
int FAT_Write(const char* filepath, const void* buffer, size_t filesize, RWCallback cb);
int progressbar(size_t read, size_t filesize);
bool ReadOpenFile(void* __restrict buffer, ssize_t offset, size_t size, FILE* fp);