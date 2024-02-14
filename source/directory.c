#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <gccore.h>
#include <fat.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "directory.h"
#include "video.h"
#include "pad.h"
#include "fatMounter.h"


struct entry {
	char name[NAME_MAX + 1];
	uint32_t flags;
};

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
	if (!count || !entries) {
		printf("\t\x1b[30;1m[..]\x1b[39m");
		return;
	}

	for (int i = 0; i < MIN(max, count); i++)
		printf("%s	%s\n",
			   (start + i) == selected? ">>" : "",
			   entries[start + i].name);

}

static struct entry* GetDirectoryEntries(const char* path, struct entry** entries, int* count, FileFilter filter) {
	DIR* pdir = NULL;
	struct dirent* pent = NULL;
	struct entry* _entries = NULL;
	int cnt = 0;

	if (!entries || !count || !path) return NULL;

	*count = 0;
	if (!(pdir = opendir(path)))
		return NULL;

	while ( (pent = readdir(pdir)) != NULL ) {
		if (!(strcmp(pent->d_name, ".") && strcmp(pent->d_name, ".."))) continue;

		bool isdir = (pent->d_type == DT_DIR);

		if (!isdir && filter && !filter(pent->d_name))
			continue;

		int i = cnt++;
		_entries = reallocarray(*entries, cnt, sizeof(struct entry));
		if (!_entries) {
			free(*entries);
			return (*entries = NULL);
		}
		*entries = _entries;

		struct entry* entry = _entries + i;
		strcpy(entry->name, pent->d_name);
		if (isdir) strcat(entry->name, "/");
		entry->flags = (isdir << 0);
	}

	*count = cnt;
	closedir(pdir);
	return *entries;
}

static const char* FileSelectAction(uint32_t flags) {
	if (flags & 0x01)	return "Enter";

	return "Install";
}

static void PrintDirectoryHeader(const char* header, const char* cwd, int count, int start, int max) {
	if (!header) header = "";

	printf("\x1b[1;0H%s\n"
		   "Viewing [%s] - %i-%i of %i total\n",
		header, cwd, start + 1, start + MIN(max, count - start), count);
}

enum { // from console_font_8x16.c
//	line_ud  = 0xba,
	line_lr  = 0xcd, 

//	line_tl  = 0xc9,
//	line_tr  = 0xbb,
//	line_bl  = 0xc8,
//	line_br  = 0xbc,

//	line_lj  = 0xcc,
//	line_rj  = 0xb9,
};

int SelectFileMenu(const char* header, const char* defaultFolder, SingleFileCallback sfCallback, FileFilter filter, void* userp) {
	struct entry* entries = NULL;
	char cwd[PATH_MAX];
	char filename[PATH_MAX];
	int cnt = 0, start = 0, index = 0, max = 0;
	int conX = 0, conY = 0;
	static char line[100];

	CON_GetMetrics(&conX, &conY);
	memset(line, line_lr, conX);
	line[conX] = 0;
	max = conY - 6; // 3 lines for the top and 3 lines for the bottom

	sprintf(cwd, "%s:/", GetActiveDeviceName());

	if (defaultFolder)
		sprintf(strrchr(cwd, '/'), "/%s/", defaultFolder);

	if (!GetDirectoryEntries(cwd, &entries, &cnt, filter))
		GetDirectoryEntries(goBack(cwd), &entries, &cnt, filter);

	if (!entries) {
		perror("GetDirectoryEntries failed");
		return -errno;
	}

	for(;;) {
		struct entry* entry = entries + index;
		
		clear();
		PrintDirectoryHeader(header, cwd, cnt, start, max);
		printf("%s", line);
		PrintEntries(entries, start, cnt, max, index);
		printf("\x1b[%i;0H%s"
			"	A/Right\x1a : %-35s Up\x18/Down\x19 : Select\n"
			"	B/\x1bLeft  : %-35s Home/Start: Exit",
			//    ^^^^ Lol!!
			conY - 2, line,
			FileSelectAction(entry->flags),
			(strchr(cwd, '/') == strrchr(cwd, '/')) ? "Exit" : "Go back"
		);

		for(;;) {
			scanpads();
			uint32_t buttons = buttons_down(0);
			if (!cnt) {
				wait_button(0);
				GetDirectoryEntries(goBack(cwd), &entries, &cnt, filter);
				break;
			}
			
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
				if (entry->flags & 0x01) {
					sprintf(strrchr(cwd, '/'), "/%s", entry->name);
					GetDirectoryEntries(cwd, &entries, &cnt, filter);
					index = start = 0;
					break;
				}
				else {
					strcpy(filename, cwd);
					strcat(filename, entry->name);
					clear();
					printf("%s\n\n", filename);
					sfCallback(filename, userp);
				}
				break;
			}
			else if (buttons & WPAD_BUTTON_B) {
				if (!goBack(cwd)) {
					free(entries);
					clear();
					return 0;
				}

				GetDirectoryEntries(cwd, &entries, &cnt, filter);
				index = start = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_HOME) {
				free(entries);
				clear();
				return 0;
			}
		}
	}
}

/* type 1 */
int QuickActionMenu(int argc, const char* argv[]) {
	int i = 0;

	for (;;) {
		clearln();
		printf("%s %s %s",
			i? "<" : "\x1b[30;1m<\x1b[39m",
			argv[i],
			i + 1 < argc? ">" : "\x1b[30;1m>\x1b[39m"
		);

		for (;;) {
			scanpads();
			uint32_t buttons = buttons_down(0);

			if (buttons & WPAD_BUTTON_RIGHT) {
				if (i + 1 < argc) i++;
				break;
			}
			else if (buttons & WPAD_BUTTON_LEFT) {
				if (i) i--;
				break;
			}

			else if (buttons & WPAD_BUTTON_A) { clearln(); return ++i; }
			else if (buttons & (WPAD_BUTTON_B | WPAD_BUTTON_HOME)) { clearln(); return 0; }
		}
	}
}


/* type 2
int QuickActionMenu(const char* argv[]) {
	int cnt = 0, index = 0, curX = 0, curY = 0;
	while (argv[++cnt])
		;

	CON_GetPosition(&curX, &curY);

	for (;;) {
		printf("\x1b[%i;%iH", curY, curX);
		for (int i = 0; i < cnt; i++)
			printf("%s	%s\x1b[40m\x1b[39m\n", i == index? ">>\x1b[47;1m\x1b[30m" : "  ", argv[i]);

		for (;;) {
			scanpads();
			uint32_t buttons = buttons_down(0);

			if (buttons & WPAD_BUTTON_DOWN) {
				if (++index == cnt) index = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_UP) {
				if (--index < 0) index = cnt - 1;
				break;
			}

			else if (buttons & WPAD_BUTTON_A) return ++index;
			else if (buttons & WPAD_BUTTON_B) return -1;
		}
	}
}
*/
