#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ogc/isfs.h>
#include <fat.h>

#include "fs.h"

int progressbar(size_t read, size_t total) {
	printf("\r[");
	for (size_t i = 0; i < total; i += FS_CHUNK)
		putchar((i < read) ? '=' : ' ');

	printf("] %u / %u bytes (%.2f%%) ", read, total, (read / (float)total) * 100);
	if (read == total)
		putchar('\n');

	return 0;
}

int NAND_GetFileSize(const char* filepath, size_t* size) {
	__attribute__((aligned(0x20)))
	fstats file_stats;

	if (size)
		*size = 0;

	int fd = ISFS_Open(filepath, 0);
	if (fd < 0)
		return fd;

	if (size) {
		int ret = ISFS_GetFileStats(fd, &file_stats);
		if (ret < 0)
			return ret;
		*size = file_stats.file_length;
	}
	ISFS_Close(fd);
	return 0;
}

int FAT_GetFileSize(const char* filepath, size_t* size) {
	FILE* fp = fopen(filepath, "rb");
	if (!fp)
		return -errno;

	if (size) {
		fseek(fp, 0, SEEK_END);
		*size = ftell(fp);
	}

	fclose(fp);
	return 0;
}

int NAND_Read(const char* filepath, void* buffer, size_t filesize, RWCallback callback) {
	if (!filesize || !buffer) return -EINVAL;

	int ret = ISFS_Open(filepath, ISFS_OPEN_READ);
	if (ret < 0)
		return ret;

	int fd = ret;
	size_t read = 0;
	while (read < filesize) {
		ret = ISFS_Read(fd, buffer + read, MAXIMUM(FS_CHUNK, filesize - read));
		if (ret <= 0)
			break;

		read += ret;
		if (callback) callback(read, filesize);
	}

	ISFS_Close(fd);

	if (read == filesize)
		return 0;

	if (ret < 0)
		return ret;
	else
		return -EIO;
}

int FAT_Read(const char* filepath, void* buffer, size_t filesize, RWCallback callback) {
	FILE* fp = fopen(filepath, "rb");
	if (!fp)
        return -errno;

	size_t read = 0;
	while (read < filesize) {
		size_t _read = fread(buffer + read, 1, MAXIMUM(FS_CHUNK, filesize - read), fp);
		if (!_read)
			break;

		read += _read;
		if (callback) callback(read, filesize);
	}
	fclose(fp);

	if (read == filesize)
		return 0;

	if (errno) //?
		return -errno;
	else
		return -EIO;
}

int NAND_Write(const char* filepath, const void* buffer, size_t filesize, RWCallback callback) {
	int ret = ISFS_Open(filepath, ISFS_OPEN_WRITE);
	if (ret < 0)
		return ret;

	int fd = ret;
	size_t wrote = 0;
	while (wrote < filesize) {
		ret = ISFS_Write(fd, buffer + wrote, MAXIMUM(FS_CHUNK, filesize - wrote));
		if (ret <= 0)
			break;

		wrote += ret;
		if (callback) callback(wrote, filesize);
	}

	ISFS_Close(fd);
	if (wrote == filesize)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

int FAT_Write(const char* filepath, const void* buffer, size_t filesize, RWCallback callback) {
	FILE* fp = fopen(filepath, "wb");
	if (!fp)
		return -errno;

	size_t wrote = 0;
	while (wrote < filesize) {
		size_t _wrote = fwrite(buffer + wrote, 1, MAXIMUM(FS_CHUNK, filesize - wrote), fp);
		if (!_wrote)
			break;

		wrote += _wrote;
		if (callback) callback(wrote, filesize);
	}
	fclose(fp);

	if (wrote == filesize)
		return 0;
	else if (errno) // tends to be set
		return -errno;
	else
		return -EIO;
}
