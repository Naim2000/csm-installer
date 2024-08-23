#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <gctypes.h>
#include <ogc/es.h>
#include <mbedtls/sha1.h>
#include <mbedtls/aes.h>

#include "theme.h"
#include "malloc.h"
#include "sysmenu.h"
#include "crypto.h"
#include "fs.h"
#include "fatMounter.h"
#include "network.h"
#include "u8.h"

#include "43db/ardb.h"

static const char
	binary_path_search[] = "\\System",
	binary_path_search_2[] = "\\Compat_irdrepo\\ipl\\Compat",
	binary_path_search_3[] = "\\home\\neceCheck\\WiiMenu\\ipl\\bin\\RVL\\Final_", // What
	binary_path_fmt12[] = "%c_%c\\ipl\\bin\\RVL\\Final_%c";

static const char* getiplSettingPath(char region) {
	switch (region) {
		case 'U': return "/html/US2/iplsetting.ash";
		case 'E': return "/html/EU2/iplsetting.ash";
		case 'J': return "/html/JP2/iplsetting.ash";
		case 'K': return "/html/KR2/iplsetting.ash";
	}

	return "/html/??\?/iplsetting.ash";
}

// < Bruh moment. >
static const char* getmainSelPath(char region) {
	switch (region) {
		case 'U': return "/FINAL/US2/main.sel";
		case 'E': return "/FINAL/EU2/main.sel";
		case 'J': return "/FINAL/JP2/main.sel";
		case 'K': return "/FINAL/KR2/main.sel";
	}

	return "/FINAL/??\?/main.sel";
}

// This whole signature thing is kinda weird!!!
static ThemeSignature FindSignature(void* sel_header) {
	if (!strcasecmp((const char*)sel_header + 0x100, "Wii_Themer"))
		return WiiThemer_v2;

	else if (!strcmp((const char*)sel_header + 0xF0, "ModMii_________\xa9______XFlak"))
		return ModMii;

	else if (strstr((const char*)sel_header + 0xF0, "wiithemer")) // Forgot how this one looks
		return WiiThemer_v1;

	return unknown;
}

static void ExtractElfPath(const char* path, version_t* out) {
	const char* ptr;

	if ((ptr = strstr(path, binary_path_search))) {
		out->base = Wii;
		ptr += strlen(binary_path_search);
	}

	else if ((ptr = strstr(path, binary_path_search_2))) {
		out->base = vWii;
		ptr += strlen(binary_path_search_2);
	}

	else if ((ptr = strstr(path, binary_path_search_3))) { // Wii mini bruh
		ptr += strlen(binary_path_search_3);
		out->base   = Mini;
		out->major  = '4';
		out->minor  = '3';
		out->region = *ptr;
		return;
	}

	if (!ptr) {
		out->base = out->major = out->minor = out->region = '?';
		return;
	}

	sscanf(ptr, binary_path_fmt12, &out->major, &out->minor, &out->region);
}

static uint32_t filter_ardb(AspectRatioDatabase* ardb, bool (*filter)(uint32_t))
{
	if (!ardb || !filter) return 0;

	uint32_t temp[ardb->entry_count];
	uint32_t* out_ptr = temp;
	int filtered = 0;


	for (uint32_t* entry = ardb->entries; entry < ardb->entries + ardb->entry_count; entry++)
		if (filter(*entry))
			*out_ptr++ = *entry;
		else
			filtered++;

	while (out_ptr < temp + ardb->entry_count)
		// *out_ptr++ = 0x5A5A5A00; // ZZZ\x00
		*out_ptr++ = 0;

	memcpy(ardb->entries, temp, sizeof(uint32_t) * ardb->entry_count);
	ardb->entry_count -= filtered;
	return filtered;
}

static bool ardb_filter_wc24(uint32_t entry) {
	entry >>= 8;
	return (entry != 0x48414A && entry != 0x484150); // HAJx, HAPx
}

static int WriteThemeFile(void* buffer, size_t fsize) {
	char filepath[ISFS_MAXPATH] = {};
	sprintf(filepath, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);
	puts(filepath);
	int ret = NAND_Write(filepath, buffer, fsize, progressbar);
	if (ret < 0)
		printf("error! (%d)\n", ret);

	return ret;
}

static int PatchTheme43DB(U8Context* ctx) {
	int ret;
	U8File wwdb = {};
	AspectRatioDatabase* ardb = NULL;

	puts("Patching WiiWare 4:3 database...");

	ret = U8OpenFile(ctx, "/titlelist/wwdb.bin", &wwdb);
	if (ret < 0)
	{
		printf("Failed to open /titlelist/wwdb.bin in archive? (%i)\n", ret);
		return 0;
	}

	ardb = (AspectRatioDatabase*)(ctx->ptr + wwdb.offset);
	uint32_t filtered = filter_ardb(ardb, ardb_filter_wc24);

	if (filtered)
		ctx->nodes[wwdb.index].size -= (filtered * sizeof(uint32_t));

	return filtered;
}

int InstallTheme(void* buffer, size_t size, int dbpatching) {
	U8Context ctx = {};
	int ret;

	if (U8Init(buffer, &ctx) != 0) {
		puts("U8Init() failed, is this really a theme?");
		return -EINVAL;
	}

	static const char* const testPaths[] = { "/www.arc", "/layout/common/health.ash", "/sound/IplSound.brsar", NULL };

	for (int i = 0; testPaths[i]; i++) {
		ret = U8OpenFile(&ctx, testPaths[i], NULL);
		if (ret < 0) {
			printf("Failed to open '%s' (%i)\n", testPaths[i], ret);
			puts  ("Is this even a theme? Or just a U8 archive?");
			return -EINVAL;
		}
	}

	ret = U8OpenFile(&ctx, getiplSettingPath(sysmenu->region), NULL);
	if (ret < 0) {
		printf("Failed to open %s (%i)\n", getiplSettingPath(sysmenu->region), ret);
		puts("Is this theme really for this region?");
		return -EINVAL;
	}

	U8File main_sel;
	ret = U8OpenFile(&ctx, getmainSelPath(sysmenu->region), &main_sel);
	if (ret < 0) {
		printf("Failed to open %s (%i)\n", getiplSettingPath(sysmenu->region), ret);
		return -EINVAL;
	}

	uint32_t* sel_header = (uint32_t*)main_sel.ptr;
	const char* elf_path = (const char*)main_sel.ptr + sel_header[4]; // path_offset
	// uint32_t path_len = sel_header[5];

	version_t themeversion;
	ExtractElfPath(elf_path, &themeversion);
	if (themeversion.region != sysmenu->region || themeversion.major != sysmenu->versionMajor || themeversion.base != sysmenu->platform) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\n"
			   "Theme version : %c %c.%c%c\n"
			   "System version: %c %c.X%c\n", themeversion.base, themeversion.major,   themeversion.minor, themeversion.region,
											  sysmenu->platform, sysmenu->versionMajor,                    sysmenu->region);
		return -EINVAL;
	}

	if (CheckHash(buffer, size, sysmenu->archive.hash)) {
		puts("\x1b[32;1mThis is the default theme for your Wii menu.\x1b[39m");
	}
	else {
		switch (FindSignature(sel_header)) {
			case WiiThemer_v1:
			case WiiThemer_v2:
				puts("\x1b[36;1mThis theme was signed by wii-themer.\x1b[39m");
				break;

			case ModMii:
				puts("\x1b[36;1mThis theme was signed by ModMii.\x1b[39m");
				break;

			default:
				puts("\x1b[30;1mThis theme isn't signed...\x1b[39m");
				if (!sysmenu->hasPriiloader) {
					puts("Consider installing Priiloader before installing unsigned themes.");
					return -EPERM;
				}
				break;
		}
	}

	if (themeversion.base == vWii && dbpatching)
		PatchTheme43DB(&ctx);

	// U8Examine(&ctx);
	return WriteThemeFile(buffer, size);
}

int DownloadOriginalTheme(bool silent) {
	int ret;
	char filepath[ISFS_MAXPATH];
	void* buffer;
	size_t fsize = sysmenu->archive.size;
	char url[192] = "http://nus.cdn.shop.wii.com/ccs/download/";
	blob download = {};

	if (!silent) puts("Downloading original theme.");

	buffer = memalign32(fsize);
	if (!buffer) {
		printf("No memory...??? (failed to allocate %u bytes)\n", fsize);
		return -ENOMEM;
	}

	sprintf(filepath, "%s:/themes/%08x-v%hu.app", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->tmd.title_version);
	ret = FAT_Read(filepath, buffer, fsize, NULL);
	if (ret >= 0 && CheckHash(buffer, fsize, sysmenu->archive.hash)) {
		if (!silent) printf("Already saved. Look for '%s'\n", filepath);
		goto finish;
	}

	sprintf(filepath, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);
	ret = NAND_Read(filepath, buffer, fsize, NULL);

	if (ret >= 0 && CheckHash(buffer, fsize, sysmenu->archive.hash))
		goto save;

	if (silent)
		goto finish;

	puts("Initializing network... ");
	ret = network_init();
	// network_getlasterror();
	if (ret < 0) {
		printf("Failed to intiailize network! (%d)\n", ret);
		goto finish;
	}

	puts("Downloading...");
	uint64_t titleID = sysmenu->isvWii ? 0x0000000700000002LL : 0x0000000100000002LL;
	sprintf(strrchr(url, '/'), "/%016llx/%08x", titleID, sysmenu->archive.cid);
	ret = DownloadFile(url, &download);
	if (ret != 0) {
		printf(
			"Download failed! (%d)\n"
			"Error details:\n"
			"	%s\n", ret, GetLastDownloadError());

		goto finish;
	}

	puts("Decrypting...");
	DecryptTitleContent(&sysmenu->ticket, 1, download.ptr, download.size, buffer, NULL);
	free(download.ptr);

	if (!CheckHash(buffer, fsize, sysmenu->archive.hash)) {
		puts("Decryption failed?? (hash mismatch)");
		goto finish;
	}

save:
	sprintf(filepath, "%s:/themes/%08x-v%hu.app", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->tmd.title_version);

	puts(silent ? "Saving original theme backup..." : "Saving...");

	ret = FAT_Write(filepath, buffer, fsize, progressbar);
	if (ret < 0)
		perror("Failed to save original theme");
	else
		printf("Saved original theme to '%s'.\n", filepath);

	if (silent) usleep(5000000);
finish:
	free(buffer);
//	network_deinit();
	return ret;
}

int SaveCurrentTheme(void) {
	int ret;
	char filepath[ISFS_MAXPATH];
	size_t fsize;

	sprintf(filepath, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);

	ret = NAND_GetFileSize(filepath, &fsize);
	if (ret < 0) {
		printf("NAND_GetFileSize failed? (%i)\n", ret);
		return ret;
	}

	void* buffer = memalign32(fsize);
	if (!buffer) {
		printf("No memory...??? (failed to allocate %u bytes)\n", fsize);
		return -ENOMEM;
	}

	puts(filepath);
	ret = NAND_Read(filepath, buffer, fsize, progressbar);
	if (ret < 0) {
		printf("NAND_Read failed? (%i)\n", ret);
		goto finish;
	}

	char* ptr = filepath + sprintf(filepath, "%s:/themes/%08x-v%hu", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->tmd.title_version);

	if (CheckHash(buffer, fsize, sysmenu->archive.hash)) {
		strcpy(ptr, ".app");
		if (!FAT_GetFileSize(filepath, NULL)) {
			puts(filepath);
			puts("File already exists.");
			goto finish;
		}
	}
	else {
		for (unsigned i = 0; i < 9999; i++) {
			sprintf(ptr, "_%04u.csm", i);
			if (FAT_GetFileSize(filepath, NULL) < 0)
				break;
		}
	}

	printf("Saving to %s\n", filepath);
	ret = FAT_Write(filepath, buffer, fsize, progressbar);
	if (ret < 0)
		perror("Failed to save");

finish:
	free(buffer);
	return ret;
}

int PatchThemeInPlace(void) {
	int ret;
	char filepath[ISFS_MAXPATH];
	size_t fsize;
	U8Context ctx = {};

	if (sysmenu->platform != vWii) {
		puts("This option is for vWii (Wii U) only.");
		return 0;
	}

	sprintf(filepath, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);

	puts("Loading theme...");

	ret = NAND_GetFileSize(filepath, &fsize);
	if (ret < 0) {
		printf("NAND_GetFileSize failed? (%i)\n", ret);
		return ret;
	}

	void* buffer = memalign32(fsize);
	if (!buffer) {
		printf("No memory...??? (failed to allocate %u bytes)\n", fsize);
		return -ENOMEM;
	}

	ret = NAND_Read(filepath, buffer, fsize, progressbar);
	if (ret < 0) {
		printf("error! (%i)\n", ret);
		goto finish;
	}

	ret = U8Init(buffer, &ctx);
	if (ret != 0) {
		printf("U8Init() failed? What? (%i)\n", ret);
		goto finish;
	}

	if (!PatchTheme43DB(&ctx)) {
		puts("PatchTheme43DB() returned 0, did you already patch it?");
		goto finish;
	}

	puts("Saving changes...");

	// ret = WriteThemeFile(buffer, fsize); // Waste of time
	/* This feels a bit sketchy tho! */
	int fd = ret = ISFS_Open(filepath, ISFS_OPEN_RW); // Don't truncate pls
	if (ret < 0) { // Not supposed to happen
		printf("ISFS_Open failed? (%i)\n", ret);
		goto finish;
	}

	do {
		// I'm a little afraid of alignment issues right now.
		ret = ISFS_Write(fd, buffer, ctx.header.data_offset);
		if (ret != ctx.header.data_offset) {
			printf("ISFS_Write failed? (%i)\n", ret);
			break;
		}

		U8File wwdb;
		U8OpenFile(&ctx, "/titlelist/wwdb.bin", &wwdb);

		// No guarantees!
		off_t  write_ofs = __builtin_align_down(wwdb.offset, 0x20);
		void  *write_ptr = ctx.ptr + write_ofs;
		size_t write_len = wwdb.size + (wwdb.offset & 0x1F);

		ret = ISFS_Seek(fd, write_ofs, SEEK_SET);
		if (ret < 0) {
			printf("ISFS_Seek failed? (%i)\n", ret);
			break;
		}

		ret = ISFS_Write(fd, write_ptr, write_len);
		if (ret != write_len) {
			printf("ISFS_Write failed? (%i)\n", ret);
			break;
		}

		ret = 0;
	} while (0);
	ISFS_Close(fd);

	if (!ret)
		puts("OK!");


/*
	sprintf(filepath, "%s:/themes/%08x-v%hu", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->tmd.title_version);

	if (memcmp(hash, sysmenu->archive.hash, sizeof(sha1)) != 0)
		sprintf(strchr(filepath, 0), "_%016llx", *(uint64_t*)hash);

	strcat(filepath, "-43patched.csm");

	printf("'%s'\n", filepath);
	if (!FAT_GetFileSize(filepath, NULL)) {
		puts("File already exists.");
	} else {
		ret = FAT_Write(filepath, buffer, fsize, progressbar);
		if (ret < 0)
			perror("Failed to save");
	}
*/
finish:
	free(buffer);
	return ret;

}
