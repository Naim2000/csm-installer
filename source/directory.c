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

char* pwd() {
	static char cwd[PATH_MAX];
	return getcwd(cwd, sizeof(cwd));
}

static bool isDirectory(const char* path) {
	stat(path, &statbuf);
	return S_ISDIR(statbuf.st_mode) > 0;
}

static void PrintEntries(struct entry entries[], size_t count, size_t max, size_t selected) {
	if (!count) {
		printf("\t\x1b[30;1m[Nothing.]\x1b[39m");
		return;
	}

	size_t cnt = (count > max) ? max : count;
	for (size_t j = 0; j < cnt;) {
		if ((selected > (max - 3)) && (j < (selected - (max - 3)))) continue;
		if (j == selected) printf(">>");
		printf("\t%s\n", entries[j].name);
		j++;
	}
}

static size_t GetDirectoryEntryCount(DIR* p_dir, FileFilter filter) {
	size_t count = 0;
	DIR* pdir;
	struct dirent* pent;

	if (!(
		(pdir = p_dir) ||
		(pdir = opendir("."))
	)) return 0;

	while ((pent = readdir(pdir)) != NULL ) {
		if (
			(!(strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))) &&
			(!filter || filter(pent->d_name, isDirectory(pent->d_name)))
		) count++;
	}

	if(p_dir) rewinddir(pdir);
	else closedir(pdir);
	return count;
}

static size_t ReadDirectory(DIR* p_dir, struct entry entries[], size_t count, FileFilter filter) {
	DIR* pdir;
	struct dirent *pent;
	size_t i = 0;

	if (!(
		(pdir = p_dir) ||
		(pdir = opendir("."))
	)) return 0;

	while (i < count) {
		pent = readdir(pdir);
		if (!pent) break;
		if(strequal(pent->d_name, ".") || strequal(pent->d_name, "..") ||
			(filter && !filter(pent->d_name, isDirectory(pent->d_name)))) continue;

		entries[i].flags =
			(isDirectory(pent->d_name) << 0);

		strcpy(entries[i].name, pent->d_name);
		i++;
	}
	if (!p_dir) closedir(pdir);
	return i;
}

static struct entry* GetDirectoryEntries(struct entry** entries, DIR* p_dir, size_t* count, FileFilter filter) {
	if (!entries || !count) return NULL;
	DIR* pdir;
	size_t cnt = 0;

	if (!(
		(pdir = p_dir) ||
		(pdir = opendir("."))
	)) return NULL;

	cnt = GetDirectoryEntryCount(pdir, filter);
	if (!cnt) {
		*count = 0;
		if (*entries)
			(*entries)[0] = (struct entry){ 0x01, ".." };
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
	*entries = _entries;

	*count = ReadDirectory(pdir, *entries, cnt, filter);

	if(p_dir) rewinddir(pdir);
	else closedir(pdir);
	return *entries;
}

char* SelectFileMenu(const char* header, FileFilter filter) {
	struct entry* entries = NULL;
	int index = 0;
	size_t cnt = 0, max = MAX_ENTRIES;
	static char filename[PATH_MAX];
	char prev_cwd[PATH_MAX];

	if (header) max -= 2;

	if (!getcwd(prev_cwd, sizeof(prev_cwd)))
		perror("Failed to get current working directory?");

	GetDirectoryEntries(&entries, NULL, &cnt, filter);

	for(;;) {
		if (!entries) {
			perror("GetDirectoryEntries failed");
			return NULL;
		}
		clear();
		if (header) printf("%s\n\n", header);
		printf("Current directory: %s\n\n", pwd());
		PrintEntries(entries, cnt, max, index);
		putchar('\n');

		struct entry* entry = entries + index;
		for(;;) {
			scanpads();
			u32 buttons = buttons_down();
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
				if (entry->flags & 0x01) {
					chdir(entry->name);
					GetDirectoryEntries(&entries, NULL, &cnt, filter);
					index = 0;
					break;
				}
				else {
					if (filename[sprintf(filename, "%s", pwd()) - 1] != '/') strcat(filename, "/");
					strcat(filename, entry->name);
					chdir(prev_cwd);
					return filename;
				}
			}
			else if (buttons & WPAD_BUTTON_B) {
				if (chdir("..") < 0) {
					if (errno == ENOENT) {
						errno = ECANCELED;
						return NULL;
					}
					else
						perror("Failed to go to parent directory");
				}
				else {
					GetDirectoryEntries(&entries, NULL, &cnt, filter);
					index = 0;
					break;
				}
			}
		}
	}
}


