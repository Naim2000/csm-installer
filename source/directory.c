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


struct entry {
	u8 flags;
	char name[NAME_MAX];
};

static char cwd[PATH_MAX];

bool isDirectory(const char* path) {
	struct stat statbuf;
	stat(path, &statbuf);
	return S_ISDIR(statbuf.st_mode) > 0;
}

static void PrintEntries(struct entry entries[], int count, int max, int selected) {
	if (!count) {
		printf("\t\x1b[30;1m[Nothing.]\x1b[39m");
		return;
	}

	int i = 0;
	if (selected > (max - 4)) i += (selected - (max - 4));
	for (int j = 0; i < count && j < max; j++) {
		if (i == selected) printf(">>");
		printf("\t%s\n", entries[i++].name);
	}
}

static int GetDirectoryEntryCount(DIR* pdir, FileFilter filter) {
	int count = 0;
	struct dirent* pent;

	if (!pdir)
		return 0;

	while ( (pent = readdir(pdir)) != NULL ) {
		if (strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))
			continue;

		strcat(cwd, pent->d_name);
		bool isdir = isDirectory(cwd);
		*(strrchr(cwd, '/') + 1) = '\x00';

		if (isdir || !filter || filter(pent->d_name))
			count++;
	}

	rewinddir(pdir);
	return count;
}

static int ReadDirectory(DIR* pdir, struct entry entries[], int count, FileFilter filter) {
	if (!pdir)
		return 0;

	int i = 0;
	while (i < count) {
		struct dirent* pent = readdir(pdir);
		if (!pent)
			break;

		if (strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))
			continue;

		strcat(cwd, pent->d_name);
		bool isdir = isDirectory(cwd);
		*(strrchr(cwd, '/') + 1) = '\x00';

		if (!isdir && filter && !filter(pent->d_name))
			continue;

		entries[i].flags =
			(isdir << 0);

		strcpy(entries[i].name, pent->d_name);
		if (isdir)
			strcat(entries[i].name, "/");

		i++;
	}

	return i;
}

static struct entry* GetDirectoryEntries(const char* path, struct entry** entries, int* count, FileFilter filter) {
	DIR* pdir;
	int cnt = 0;

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

char* SelectFileMenu(const char* header, const char* defaultFolder, FileFilter filter) {
	struct entry* entries = NULL;
	int index = 0;
	int cnt = 0, max = MAX_ENTRIES;
	static char filename[PATH_MAX];

	if (header)
		max--;

	getcwd(cwd, sizeof(cwd));

	if (defaultFolder) {
		sprintf(strrchr(cwd, '/'), "/%s/", defaultFolder);
		if (!GetDirectoryEntries(cwd, &entries, &cnt, filter)) {
			*strrchr(cwd, '/') = '\x00';
			*(strrchr(cwd, '/') + 1) = '\x00';
			GetDirectoryEntries(cwd, &entries, &cnt, filter);
		}
	}
	else {
		GetDirectoryEntries(cwd, &entries, &cnt, filter);
	}

	for(;;) {
		if (!entries) {
			perror("GetDirectoryEntries failed");
			return NULL;
		}

		clear();
		if (header)
			printf("%s\n", header);

		struct entry* entry = entries + index;
		printf("Current directory: [%s]\n\n", cwd);
		PrintEntries(entries, cnt, max, index);
		printf("\x1b[23;0H"
		//	"Controls:\n"
			"	A/RIGHT: %-7s		UP/DOWN   : Select\n"
			"	B/LEFT : %-7s		HOME/START: Exit\n",
			entry->flags & 0x01 ? "Enter" :
				(entry->flags & 0x80 ? "Go Back" : "\x1b[32;1mInstall\x1b[39m"), // mighty stupid
			(strchr(cwd, '/') == strrchr(cwd, '/')) ? "Exit" : "Go back"
		);

		for(;;) {
			scanpads();
			u32 buttons = buttons_down();
			if (buttons & WPAD_BUTTON_DOWN) {
				if (index < (cnt - 1))
					index += 1;
				else
					index = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_UP) {
				if (index > 0)
					index -= 1;
				else
					index = cnt - 1;
				break;
			}
			else if (buttons & (WPAD_BUTTON_A | WPAD_BUTTON_RIGHT)) {
				if (entry->flags & 0x80) {
					*strrchr(cwd, '/') = '\x00';
					*(strrchr(cwd, '/') + 1) = '\x00';
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = 0;
					break;
				}
				if (entry->flags & 0x01) {
				//	chdir(entry->name);
					sprintf(strrchr(cwd, '/'), "/%s", entry->name);
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = 0;
					break;
				}
				else {
				//	if (filename[sprintf(filename, "%s", pwd()) - 1] != '/') strcat(filename, "/");
					strcpy(filename, cwd);
					strcat(filename, entry->name);
					free(entries);
					return filename;
				}
				break;
			}
			else if (buttons & (WPAD_BUTTON_B | WPAD_BUTTON_LEFT)) {
				if (strchr(cwd, '/') == strrchr(cwd, '/')) { // sd:/ <-- first and last /
					errno = ECANCELED;
					free(entries);
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
				free(entries);
				return NULL;
			}
		}
	}
}


