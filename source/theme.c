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

#include "sysmenu.h"
#include "fs.h"
#include "network.h"

static const char
	wiithemer_sig[] = "wiithemer",
	binary_path_search[] = "C:\\Revolution\\ipl\\System",
	binary_path_fmt[] = "%c_%c\\ipl\\bin\\RVL\\Final_%c%*s\\main.elf";

static int FindString(void* buf, size_t size, const char* str) {
	int len = strlen(str);
	for (int i = 0; i < (size - len); i++)
		if (memcmp(buf + i, str, len) == 0)
			return i;

	return -1;
}

int SignedTheme(unsigned char* buffer, size_t length) {
	sha1 hash = {};
	mbedtls_sha1_ret(buffer, length, hash);

	if (memcmp(hash, getArchiveHash(), sizeof(sha1)) == 0)
		return 2;
	else if (FindString(buffer, length, wiithemer_sig) >= 0)
		return 1;


	return 0;
}

version_t GetThemeVersion(unsigned char* buffer, size_t length) {
	char rgn = 0, major = 0, minor = 0;

	int off = FindString(buffer, length, binary_path_search);
	if (off < 0) return (version_t){ '?', '?', '?' };

	buffer += off + strlen(binary_path_search);
	sscanf((char*)buffer, binary_path_fmt, &major, &minor, &rgn);
	return (version_t) { major, minor, rgn };
}

int InstallTheme(unsigned char* buffer, size_t size) {
	if (*(u32*)buffer != 0x55AA382D) {
		puts("Not a theme!");
		return -EINVAL;
	}

	switch (SignedTheme(buffer, size)) {
		case 2:
			puts("\x1b[32mThis is the default theme for your Wii menu.\x1b[39m");
			break;
		case 1:
			puts("\x1b[36mThis theme was signed by wii-themer.\x1b[39m");
			break;

		default:
			puts("This theme isn't signed...");
			break;
	}
	putchar('\n');

	version_t themeversion = GetThemeVersion(buffer, size);
	if (themeversion.region != getSmRegion()) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\nTheme region : %c\nSystem region: %c\n", themeversion.region, getSmRegion());
		return -EINVAL;
	}
	else if (themeversion.major != getSmVersionMajor()) {
		printf("\x1b[41;30mIncompatible theme!\x1b[40;39m\nTheme major version : %c\nSystem major version: %c\n", themeversion.major, getSmVersionMajor());
		return -EINVAL;
	}

	printf("%s\n", getArchivePath());
	int ret = FS_Write(getArchivePath(), buffer, size, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		return ret;
	}

	return 0;
}

int InstallOriginalTheme() {
	char url[0x80] = "http://nus.cdn.shop.wii.com/ccs/download/";
	blob download = {};
	mbedtls_aes_context title = {};
	aeskey iv = { 0, 1 };
	char filepath[16];

	puts("Installing original theme.");

	printf("Initializing network... ");
	int ret = network_init();
	if (ret < 0) {
		printf("failed! (%d)\n", ret);
		return ret;
	}
	puts("ok.");

	puts("Downloading...");
	sprintf(strrchr(url, '/'), "/%016llx/%08x", getSmNUSTitleID(), getArchiveCid()); // TODO: vwii // done ?
	ret = DownloadFile(url, &download);
	if (ret != 0) {
		printf("Download failed! (%d)\n", ret);
		return ret;
	}

	puts("Decrypting...");
	mbedtls_aes_setkey_dec(&title, getSmTitleKey(), sizeof(aeskey) * 8);
	mbedtls_aes_crypt_cbc(&title, MBEDTLS_AES_DECRYPT, download.size, iv, download.ptr, download.ptr);

	puts("Saving...");
	sprintf(filepath, "%08x.app", getArchiveCid());
	ret = FAT_Write(filepath, download.ptr, getArchiveSize(), progressbar);
	if (ret < 0)
		printf("Failed to save! (%d)\n", ret);

	puts("Installing...");
	ret = InstallTheme(download.ptr, getArchiveSize());
	free(download.ptr);
	return ret;
}
