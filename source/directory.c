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

static bool isDirectory(const char* path) {
	struct stat statbuf;
	stat(path, &statbuf);
	return S_ISDIR(statbuf.st_mode) > 0;
}

static char* goBack(char* path) {
	if(strchr(path, '/') == strrchr(path, '/'))
		return NULL;

	*strrchr(path, '/') = '\x00';
	*(strrchr(path, '/') + 1) = '\x00';
	return path;
}

bool hasFileExtension(const char* name, const char* ext) {
	if (!(name = strrchr(name, '.')))
		return false;

	return strcasecmp(name + 1, ext) == 0;
}

static void PrintEntries(struct entry entries[], int start, int count, int max, int selected) {
	if (!count) {
		printf("\t\x1b[30;1m[Nothing.]\x1b[39m");
		return;
	}

	for (int i = 0; i < MIN(max, count); i++)
		printf("%s	%s\n",
			   (start + i) == selected? ">>" : "",
			   entries[start + i].name);

}

static int GetDirectoryEntryCount(DIR* pdir, FileFilter filter) {
	int count = 0;
	struct dirent* pent;

	if (!pdir)
		return 0;

	while ( (pent = readdir(pdir)) != NULL ) {
		if (!strcmp(pent->d_name, ".") || !strcmp(pent->d_name, ".."))
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

		if (!strcmp(pent->d_name, ".") || !strcmp(pent->d_name, ".."))
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

static const char* FileSelectAction(u8 flags) {
	if (flags & 0x01)	return "Enter";
	if (flags & 0x80)	return "Go back";

	return "Install";
}

char* SelectFileMenu(const char* header, const char* defaultFolder, FileFilter filter) {
	struct entry* entries = NULL;
	int cnt = 0, start = 0, index = 0, max = 0;
	int conX = 0, conY = 0;
	static char filename[PATH_MAX];
	static char line[0x80];

	CON_GetMetrics(&conX, &conY);
	memset(line, 0xc4, conX); // from d2x cIOS installer
	line[conX] = 0;
	max = conY - 6; // 3 lines for the top and 3 lines for the bottom

	getcwd(cwd, sizeof(cwd));

	if (defaultFolder)
		sprintf(strrchr(cwd, '/'), "/%s/", defaultFolder);

	if (!GetDirectoryEntries(cwd, &entries, &cnt, filter))
		GetDirectoryEntries(goBack(cwd), &entries, &cnt, filter);


	for(;;) {
		if (!entries) {
			if (errno != ECANCELED)
				perror("GetDirectoryEntries failed");

			return NULL;
		}
		clear();

		struct entry* entry = entries + index;

		printf("\n%s\nCurrent directory: [%s] - %i-%i of %i total\n%s",
			header ? header : "", cwd, start + 1, start + MIN(max, cnt - start), cnt, line);

		PrintEntries(entries, start, cnt, max, index);
		printf("\x1b[%i;0H%s"
		//	"Controls:\n"
			"	A/Right\x10 : %-34s Up\x1e/Down\x1f : Select\n"
			"	B/\x11Left  : %-34s Home/Start: Exit",
			conY - 2, line,
			FileSelectAction(entry->flags),
			(strchr(cwd, '/') == strrchr(cwd, '/')) ? "Exit" : "Go back"
		);

		for(;;) {
			scanpads();
			u32 buttons = buttons_down(0);
			if (buttons & WPAD_BUTTON_DOWN) {
				if (index >= (cnt - 1)) {
					start = index = 0;
					break;
				}

				if ((++index - start) >= max)
					start++;

				break;
			}
			else if (buttons & WPAD_BUTTON_UP) {
				if (index <= 0) {
					index = cnt - 1;
					if (index >= max)
						start = 1 + index - max;

					break;
				}

				if (--index < start)
					start--;

				break;
			}
			else if (buttons & (WPAD_BUTTON_A | WPAD_BUTTON_RIGHT)) {
				if (entry->flags & 0x80) {
					goBack(cwd);
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = start = 0;
					break;
				}
				else if (entry->flags & 0x01) {
					sprintf(strrchr(cwd, '/'), "/%s", entry->name);
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = start = 0;
					break;
				}
				else {
					strcpy(filename, cwd);
					strcat(filename, entry->name);
					free(entries);
					return filename;
				}
				break;
			}
			else if (buttons & (WPAD_BUTTON_B | WPAD_BUTTON_LEFT)) {
				if (!goBack(cwd)) {
					errno = ECANCELED;
					free(entries);
					return NULL;
				}

				GetDirectoryEntries(cwd, &entries, &cnt, filter);
				index = start = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_HOME) {
				errno = ECANCELED;
				free(entries);
				return NULL;
			}
		}
	}
}
