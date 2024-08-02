#include "theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <gctypes.h>
#include <ogc/es.h>
#include <mbedtls/sha1.h>
#include <mbedtls/aes.h>

#include "malloc.h"
#include "sysmenu.h"
#include "fs.h"
#include "fatMounter.h"
#include "network.h"

#include "43db/u8.h"
#include "43db/ardb.h"

static const char
	wiithemer_sig[] = "wiithemer",
	binary_path_search[] = "C:\\Revolution\\ipl\\System",
	binary_path_search_2[] = "D:\\Compat_irdrepo\\ipl\\Compat",
	binary_path_search_3[] = "c:\\home\\neceCheck\\WiiMenu\\ipl\\bin\\RVL\\Final_", // What
	binary_path_fmt12[] = "%c_%c\\ipl\\bin\\RVL\\Final_%c";

extern void* memmem(const void* haystack, size_t haystack_len, const void* needle, size_t needle_len);

SignatureLevel SignedTheme(const void* buffer, size_t length) {
	sha1 hash = {};
	mbedtls_sha1_ret(buffer, length, hash);

	if (!memcmp(hash, sysmenu->archive.hash, sizeof(sha1)))
		return default_theme;
	else if (memmem(buffer, length, wiithemer_sig, strlen(wiithemer_sig)))
		return wiithemer_signed;

	else
		return unknown;
}

version_t GetThemeVersion(const void* buffer, size_t length) {
	ThemeBase base;
	char rgn = 0, major = 0, minor = 0;
	char* ptr;


	if ((ptr = memmem(buffer, length, binary_path_search, strlen(binary_path_search)))) {
		base = Wii;
		ptr += strlen(binary_path_search);
	}

	else if ((ptr = memmem(buffer, length, binary_path_search_2, strlen(binary_path_search_2)))) {
		base = vWii;
		ptr += strlen(binary_path_search_2);
	}

	else if ((ptr = memmem(buffer, length, binary_path_search_3, strlen(binary_path_search_3)))) { // Wii mini bruh
		ptr += strlen(binary_path_search_3);
		rgn = *ptr;
		return (version_t) { Mini, '4', '3', rgn };
	}

	if (!ptr)
		return (version_t) { '?', '?', '?', '?' };

	sscanf(ptr, binary_path_fmt12, &major, &minor, &rgn);
	return (version_t) { base, major, minor, rgn };
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
	printf("%s\n", filepath);
	int ret = NAND_Write(filepath, buffer, fsize, progressbar);
	if (ret < 0)
		printf("error! (%d)\n", ret);

	return ret;
}

static int PatchTheme43DB(U8Context* ctx) {
	U8Node* wwdb = NULL;
	uint32_t wwdb_index;
	AspectRatioDatabase* ardb = NULL;

	puts("Patching WiiWare 4:3 database...");

	if (!(wwdb = u8GetFileNodeByPath(ctx, "/titlelist/wwdb.bin", &wwdb_index)))
	{
		puts("Failed to find /titlelist/wwdb.bin in archive?");
		return 0;
	}

	ardb = (AspectRatioDatabase*)(ctx->u8_buf + wwdb->data_offset);
	uint32_t filtered = filter_ardb(ardb, ardb_filter_wc24);

	if (filtered) {
		// Find the real location of the node
		U8Node* nodes = (U8Node*)(ctx->u8_buf + ctx->u8_header.root_node_offset);
		nodes[wwdb_index].size -= (filtered * sizeof(uint32_t));
	}

	return filtered;
}

int InstallTheme(void* buffer, size_t size, int dbpatching) {
	U8Context ctx = {};

	if (memcmp(buffer, "PK", 2) == 0) {
		puts("\x1b[31;1mPlease do not rename .mym files.\x1b[39m\n"
			"Follow https://wii.hacks.guide/themes to properly convert it."
		);
		return -EINVAL;
	}
	else if (!u8ContextInit(buffer, &ctx)) {
		puts("u8ContextInit() failed, is this really a theme?");
		return -EINVAL;
	}

	version_t themeversion = GetThemeVersion(buffer, size);
	if (themeversion.region != sysmenu->region || themeversion.major != sysmenu->versionMajor || themeversion.base != sysmenu->platform) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\n"
			   "Theme version : %c %c.X%c\n"
			   "System version: %c %c.X%c\n", themeversion.base, themeversion.major,   themeversion.region,
											  sysmenu->platform, sysmenu->versionMajor, sysmenu->region);
		return -EINVAL;
	}

	switch (SignedTheme(buffer, size)) {
		case default_theme:
			puts("\x1b[32;1mThis is the default theme for your Wii menu.\x1b[39m");
			break;
		case wiithemer_signed:
			puts("\x1b[36;1mThis theme was signed by wii-themer.\x1b[39m");
			break;

		default:
			puts("\x1b[30;1mThis theme isn't signed...\x1b[39m");
			if (!sysmenu->hasPriiloader) {
				puts("Consider installing Priiloader before installing unsigned themes.");
				return -EPERM;
			}
			break;
	}

	if (themeversion.base == vWii && dbpatching)
		PatchTheme43DB(&ctx);

	u8ContextFree(&ctx);

	return WriteThemeFile(buffer, size);
}

int DownloadOriginalTheme() {
	int ret;
	char url[192] = "http://nus.cdn.shop.wii.com/ccs/download/";
	char filepath[ISFS_MAXPATH];
	size_t fsize = sysmenu->archive.size;
	blob download = {};
	mbedtls_aes_context title = {};
	aeskey iv = { 0x00, 0x01 };

	puts("Downloading original theme.");

	void* buffer = memalign32(fsize);
	if (!buffer) {
		printf("No memory...??? (failed to allocate %u bytes)\n", fsize);
		return -ENOMEM;
	}

	sprintf(filepath, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);
	ret = NAND_Read(filepath, buffer, fsize, NULL);

	sprintf(filepath, "%s:/themes/%08x-v%hu.app", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->version);
	if (ret >= 0 && (SignedTheme(buffer, fsize) == default_theme)) goto save;

	ret = FAT_Read(filepath, buffer, fsize, NULL);
	if (ret >= 0 && (SignedTheme(buffer, fsize) == default_theme)) {
		printf("Already saved. Look for '%s'\n", filepath);
		goto finish;
	}

	puts("Initializing network... ");
	ret = network_init();
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
	mbedtls_aes_setkey_dec(&title, sysmenu->titlekey, 128);
	mbedtls_aes_crypt_cbc(&title, MBEDTLS_AES_DECRYPT, download.size, iv, download.ptr, buffer);
	free(download.ptr);

	if (SignedTheme(buffer, fsize) != default_theme) {
		puts("Decryption failed?? (hash mismatch)");
		goto finish;
	}

save:
	puts("Saving...");
	ret = FAT_Write(filepath, buffer, fsize, progressbar);
	if (ret < 0)
		perror("Failed to save original theme");
	else
		printf("Saved original theme to '%s'.\n", filepath);

finish:
	free(buffer);
//	network_deinit();
	return ret;
}

int SaveCurrentTheme(void) {
	int ret;
	char filepath[256];
	size_t fsize;
	sha1 hash = {};

	snprintf(filepath, ISFS_MAXPATH, "/title/00000001/00000002/content/%08x.app", sysmenu->archive.cid);

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
		printf("error! (%i)\n", ret);
		goto finish;
	}

	mbedtls_sha1_ret(buffer, fsize, hash);
	if (!memcmp(hash, sysmenu->archive.hash, sizeof(sha1))) {
		sprintf(filepath, "%s:/themes/%08x-v%hu.app", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->version);
	}
	else {
		uint32_t* hashptr = (uint32_t*)hash;

		sprintf(filepath, "%s:/themes/%08x-v%hu_%08x%08x.csm", GetActiveDeviceName(), sysmenu->archive.cid, sysmenu->version, hashptr[0], hashptr[1]);
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

	if (!u8ContextInit(buffer, &ctx)) {
		puts("u8ContentInit() failed? What?");
		goto finish;
	}

	if (PatchTheme43DB(&ctx))
		ret = WriteThemeFile(buffer, fsize);
	else
		puts("PatchTheme43DB() returned 0, did you already patch it?");

finish:
	u8ContextFree(&ctx);
	free(buffer);
	return ret;

}
