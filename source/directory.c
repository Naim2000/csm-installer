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

extern __attribute__((weak)) void OSReport([[maybe_unused]] const char* fmt, ...)
	__attribute((format (printf, 1, 2) ));

struct entry {
	u8 flags;
	char name[NAME_MAX];
};
#define MAX_ENTRIES 20

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static u32 buttons = 0;

__attribute__((pure)) static inline const char* fileext(const char* name) {
	if ((name = strrchr(name, '.'))) name += 1;
	return name;
}



static inline void init_video(int row, int col) {
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("\x1b[%d;%dH", row, col);
}

static inline void scanpads() {
	WPAD_ScanPads();
	buttons = WPAD_ButtonsDown(0);
}

static inline void PrintEntries(struct entry entries[], int count, int selected) {
	for (int j = 0; j < count; j++) {
		if (j == selected) printf(">>");
		printf("\t%s\n", entries[j].name);
	}
}

static int ReadCurrentDirectory(struct entry entries[]) {
	DIR *pdir;
	struct dirent *pent;
	struct stat statbuf;
	int i = 0;

	pdir = opendir(".");
	if (!pdir)
		return -errno;

	while (i < MAX_ENTRIES) {
		pent = readdir(pdir);
		if (!pent) break;
		if(pent->d_name[0] == '.' && strlen(pent->d_name) == 1) continue;

		stat(pent->d_name, &statbuf);
		entries[i].flags = 0x80 | (S_ISDIR(statbuf.st_mode) > 0);
		strcpy(entries[i].name, pent->d_name);
		i++;
	}
	closedir(pdir);
	return i;
}

static char* SelectFileMenu() {
	struct entry entries[MAX_ENTRIES] = {};
	int index = 0;

	static char filename[PATH_MAX];
	char cwd[PATH_MAX], prev_cwd[PATH_MAX];

	if (!getcwd(prev_cwd, sizeof(prev_cwd))) {
		perror("Failed to get current working directory");
		return NULL;
	}

	int i = ReadCurrentDirectory(entries);
	if (i < 0) {
		perror("ReadCurrentDirectory failed");
		return NULL;
	}

	for(;;) {
		printf("\x1b[2J");
		PrintEntries(entries, i, index);

		struct entry* entry = entries + index;
		for(;;) {
			scanpads();
			if (buttons & WPAD_BUTTON_DOWN) {
				if (index < (i - 1)) index += 1;
				else index = 0;
				break;
			}
			else if (buttons & WPAD_BUTTON_UP) {
				if (index > 0) index -= 1;
				else index = i - 1;
				break;
			}
			else if (buttons & WPAD_BUTTON_A) {
				if (entry->flags & 0x01) {
					chdir(entry->name);
					i = ReadCurrentDirectory(entries);
					index = 0;
					break;
				}
				else {
					if (!getcwd(cwd, sizeof(cwd))) {
						perror("Failed to get current working directory");
						return NULL;
					}
					printf("cwd: %s, name: %s\n", cwd, entry->name);
					if (filename[sprintf(filename, "%s", cwd) - 1] != '/') sprintf(filename + strlen(filename), "/");
					sprintf(filename + strlen(filename), "%s", entry->name);
					printf("filename: %s\n", filename);
					return filename;
				}
			}
			else if (buttons & WPAD_BUTTON_B) {
				if (chdir("..") < 0) perror("Failed to go to parent dir");
				else {
					i = ReadCurrentDirectory(entries);
					break;
				}
			}
		}
	}
}

int main(int argc, char* argv[]) {
	init_video(2, 0);
	WPAD_Init();
	if (!fatInitDefault()) {
		printf("fatInitDefault failure: terminating\n");
		goto error;
	}

	SelectFileMenu();

error:
	while(!SYS_ResetButtonDown());
	return 0;
}
