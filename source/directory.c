#include "directory.h"

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "video.h"
#include "pad.h"

static struct stat statbuf;

struct entry {
	u8 flags;
	char name[NAME_MAX];
};

static char cwd[PATH_MAX];

bool isDirectory(const char* path) {
	stat(path, &statbuf);
	return S_ISDIR(statbuf.st_mode) > 0;
}

static void PrintEntries(struct entry entries[], size_t count, size_t max, size_t selected) {
	if (!count) {
		printf("\t\x1b[30;1m[Nothing.]\x1b[39m");
		return;
	}

	size_t cnt = (count > max) ? max : count;
	for (size_t j = 0; j < cnt; ) {
		if ((selected > (max - 3)) && (j < (selected - (max - 3)))) continue;
		if (j == selected) printf(">>");
		printf("\t%s\n", entries[j].name);
		j++;
	}
}

static size_t GetDirectoryEntryCount(DIR* pdir, FileFilter filter) {
	size_t count = 0;
	struct dirent* pent;

	if (!pdir)
		return 0;

	while ( (pent = readdir(pdir)) != NULL ) {
		if (strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))
			continue;

		strcat(cwd, pent->d_name);
		bool isdir = isDirectory(cwd);
		*(strrchr(cwd, '/') + 1) = '\x00';

		if (!filter || filter(pent->d_name, isdir))
			count++;
	}

	rewinddir(pdir);
	return count;
}

static size_t ReadDirectory(DIR* pdir, struct entry entries[], size_t count, FileFilter filter) {
	if (!pdir)
		return 0;

	size_t i = 0;
	while (i < count) {
		struct dirent* pent = readdir(pdir);
		if (!pent)
			break;

		if (strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))
			continue;

		strcat(cwd, pent->d_name);
		bool isdir = isDirectory(cwd);
		*(strrchr(cwd, '/') + 1) = '\x00';

		if (filter && !filter(pent->d_name, isdir))
			continue;

		entries[i].flags =
			(isdir << 0);
		strcpy(entries[i].name, pent->d_name);
		i++;
	}

	return i;
}

static struct entry* GetDirectoryEntries(const char* path, struct entry** entries, size_t* count, FileFilter filter) {
	DIR* pdir;
	size_t cnt = 0;

	if (!entries || !count || !path) return NULL;
	if (!(pdir = opendir(path)))
		return NULL;

	cnt = GetDirectoryEntryCount(pdir, filter);
	if (!cnt) {
		*count = 0;
		if (*entries)
			(*entries)[0] = (struct entry){ 0x80, {} };
		else
			errno = ENOENT;

		return NULL;
	}

	// If ptr is NULL, then the call is equivalent to malloc(size), for all values of size.
	struct entry* _entries = reallocarray(*entries, cnt, sizeof(struct entry));
	if (!_entries) {
		free(*entries);
		*entries = NULL;
		errno = ENOMEM;
		return NULL;
	}
	memset(_entries, 0, sizeof(struct entry) * cnt);
	*count = ReadDirectory(pdir, _entries, cnt, filter);
	*entries = _entries;

	closedir(pdir);
	return *entries;
}

char* SelectFileMenu(const char* header, FileFilter filter) {
	struct entry* entries = NULL;
	int index = 0;
	size_t cnt = 0, max = MAX_ENTRIES;
	static char filename[PATH_MAX];

	if (header)
		max -= 2;

	getcwd(cwd, sizeof(cwd));
	OSReport("cwd=%s", cwd);

	GetDirectoryEntries(cwd, &entries, &cnt, filter);

	for(;;) {
		if (!entries) {
			perror("GetDirectoryEntries failed");
			return NULL;
		}
		clear();
		if (header)
			printf("%s\n\n", header);

		printf("Current directory: %s\n\n", cwd);
		PrintEntries(entries, cnt, max, index);
		putchar('\n');

		struct entry* entry = entries + index;
		for(;;) {
			scanpads();
			u32 buttons = buttons_down(0);
			if (buttons & WPAD_BUTTON_DOWN) {
				if (index < (cnt - 1)) index += 1;
				else index = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_UP) {
				if (index > 0) index -= 1;
				else index = cnt - 1;
				break;
			}
			else if (buttons & WPAD_BUTTON_A) {
				if (entry->flags & 0x80) {
					*strrchr(cwd, '/') = '\x00';
					*(strrchr(cwd, '/') + 1) = '\x00';
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = 0;
					break;
				}
				if (entry->flags & 0x01) {
				//	chdir(entry->name);
					sprintf(strrchr(cwd, '/'), "/%s/", entry->name);
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = 0;
					break;
				}
				else {
				//	if (filename[sprintf(filename, "%s", pwd()) - 1] != '/') strcat(filename, "/");
					strcpy(filename, cwd);
					strcat(filename, entry->name);
					return filename;
				}
			}
			else if (buttons & WPAD_BUTTON_B) {
				if (strchr(cwd, '/') == strrchr(cwd, '/')) {
					errno = ECANCELED;
					return NULL;
				}
				else {
					// "sd:/apps/cdbackup/". one strrchr('/') will land me right at the end. but so will another. so i will subtract one.
					// i'm not sure why i typed that.
					*strrchr(cwd, '/') = '\x00';
					*(strrchr(cwd, '/') + 1) = '\x00';
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = 0;
					break;
				}
			}
			else if (buttons & WPAD_BUTTON_HOME) {
				errno = ECANCELED;
				return NULL;
			}
		}
	}
}


