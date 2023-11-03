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


extern __attribute__((weak)) void OSReport([[maybe_unused]] const char* fmt, ...);

struct entry {
	u8 flags;
	char name[NAME_MAX];
};
#define MAX_ENTRIES 25

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
static u32 buttons = 0;

static inline const char* fileext(const char* name) {
	if ((name = strrchr(name, '.'))) name += 1;
	return name;
}

static inline bool strequal(const char* a, const char* b) {
	return (strcmp(a, b) == 0) && (strlen(a) == strlen(b));
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

size_t GetDirectoryEntryCount(DIR* p_dir) {
	size_t count = 0;
	DIR* pdir;
	struct dirent* pent;

	if (!(pdir = p_dir)) pdir = opendir(".");
	if (!pdir) return 0;

	while ((pent = readdir(pdir)) != NULL )
		if (!(strequal(pent->d_name, ".") || strequal(pent->d_name, ".."))) count++;

//	OSReport("%s: entry count: %u", __FUNCTION__, count);
	if(p_dir) rewinddir(pdir);
	else closedir(pdir);
	return count;
}

static inline void PrintEntries(struct entry entries[], size_t count, size_t selected) {
	size_t cnt = (count > MAX_ENTRIES) ? MAX_ENTRIES : count;
	for (size_t j = 0; j < cnt; j++) {
		if ((selected > (MAX_ENTRIES - 2)) && (j < (selected - (MAX_ENTRIES - 2)))) {cnt++; continue;};
		if (j == selected) printf(">>");
		printf("\t%s\n", entries[j].name);
	}
}

static size_t ReadDirectory(DIR* p_dir, struct entry entries[], size_t count) {
	DIR* pdir;
	struct dirent *pent;
	struct stat statbuf;
	size_t i = 0;

	if (!(pdir = p_dir)) pdir = opendir(".");
	if (!pdir) return 0;

	while (i < count) {
		pent = readdir(pdir);
		if (!pent) break;
		if(strequal(pent->d_name, ".") || strequal(pent->d_name, "..")) continue;

		stat(pent->d_name, &statbuf);
		entries[i].flags = 0x80 | (S_ISDIR(statbuf.st_mode) > 0);
		strcpy(entries[i].name, pent->d_name);
		i++;
	}
	if (!p_dir) closedir(pdir);
	return i;
}

struct entry* GetDirectoryEntries(DIR* p_dir, size_t* count) {
	DIR* pdir;
	struct entry* entries = NULL;
	size_t cnt = 0;

	if (!(pdir = p_dir)) pdir = opendir(".");
	if (!pdir) goto error;

	cnt = GetDirectoryEntryCount(pdir);
	if (!cnt) goto error;

	entries = calloc(sizeof(struct entry), cnt);
	if (!entries) {
		errno = ENOMEM;
		goto error;
	}

	*count = ReadDirectory(pdir, entries, cnt);
	if (*count == cnt) goto finish;

error:
	free(entries);
	entries = NULL;
finish:
	if(p_dir) rewinddir(pdir);
	else closedir(pdir);
	return entries;
}

static char* SelectFileMenu() {
	struct entry* entries;
	int index = 0;
	size_t cnt = 0;

	static char filename[PATH_MAX];
	char cwd[PATH_MAX], prev_cwd[PATH_MAX];

	if (!getcwd(prev_cwd, sizeof(prev_cwd))) {
		perror("Failed to get current working directory");
		return NULL;
	}

	entries = GetDirectoryEntries(NULL, &cnt);
	if (!entries) {
		perror("GetDirectoryEntries failed");
		return NULL;
	}

	for(;;) {
		printf("\x1b[2J");
		PrintEntries(entries, cnt, index);

		struct entry* entry = entries + index;
		for(;;) {
			scanpads();
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
					free(entries);
					entries = GetDirectoryEntries(NULL, &cnt);
					index = 0;
					break;
				}
				else {
					if (!getcwd(cwd, sizeof(cwd))) {
						perror("Failed to get current working directory");
						return NULL;
					}
				//	printf("cwd: %s, name: %s\n", cwd, entry->name);
					if (filename[sprintf(filename, "%s", cwd) - 1] != '/') sprintf(filename + strlen(filename), "/");
					sprintf(filename + strlen(filename), "%s", entry->name);
				//	printf("Returning to previous cwd %s\n", prev_cwd);
					chdir(prev_cwd);
				//	printf("filename: %s\n", filename);
					return filename;
				}
			}
			else if (buttons & WPAD_BUTTON_B) {
				if (chdir("..") < 0) {
					if (errno == ENOENT) return NULL;
					else perror("Failed to go to parent dir");
				}
				else {
					free(entries);
					entries = GetDirectoryEntries(NULL, &cnt);
					index = 0;
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

	printf("%s", SelectFileMenu());

error:
	while(!SYS_ResetButtonDown());
	return 0;
}
