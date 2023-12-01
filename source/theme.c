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

void* memalign(size_t, size_t);

static const char
	wiithemer_sig[] = "wiithemer",
	binary_path_search[] = "C:\\Revolution\\ipl\\System",
	binary_path_search_2[] = "D:\\Compat_irdrepo\\ipl\\Compat",
	binary_path_fmt[] = "%c_%c\\ipl\\bin\\RVL\\Final_%c";

static int FindString(void* buf, size_t size, const char* str) {
	int len = strlen(str);
	for (int i = 0; i < (size - len); i++)
		if (memcmp(buf + i, str, len) == 0)
			return i;

	return -1;
}

SignatureLevel SignedTheme(unsigned char* buffer, size_t length) {
	sha1 hash = {};
	mbedtls_sha1_ret(buffer, length, hash);

	if (memcmp(hash, getArchiveHash(), sizeof(sha1)) == 0)
		return default_theme;
	else if (FindString(buffer, length, wiithemer_sig) >= 0)
		return wiithemer_signed;


	return unknown;
}

version_t GetThemeVersion(unsigned char* buffer, size_t length) {
	char rgn = 0, major = 0, minor = 0;

	int len = strlen(binary_path_search);
	int off = FindString(buffer, length, binary_path_search);

	if (off < 0) {
		len = strlen(binary_path_search_2);
		off = FindString(buffer, length, binary_path_search_2);
	}
	if (off < 0)
		return (version_t){ '?', '?', '?' };

	buffer += off + len;
	sscanf((char*)buffer, binary_path_fmt, &major, &minor, &rgn);
	return (version_t) { major, minor, rgn };
}

int InstallTheme(unsigned char* buffer, size_t size) {
	if (*(u16*)buffer == ('P' << 8 | 'K')) {
		puts("\x1b[31;1mPlease do not rename .mym files.\x1b[39m\n"
			"Follow https://wii.hacks.guide/themes to properly convert it."
		);
		return -EINVAL;
	}
	else if (*(u32*)buffer != 0x55AA382D) {
		puts("\x1b[31;1mNot a theme!\x1b[39m");
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
	int ret = NAND_Write(getArchivePath(), buffer, size, progressbar);
	if (ret < 0) {
		printf("error. (%d)\n", ret);
		return ret;
	}

	return 0;
}

int InstallOriginalTheme() {
	int ret;
	char url[0x80] = "http://nus.cdn.shop.wii.com/ccs/download/";
	blob download = {};
	mbedtls_aes_context title = {};
	aeskey iv = { 0x00, 0x01 };
	char filepath[16];

	puts("Installing original theme.");

	void* buffer = memalign(0x20, getArchiveSize());
	if (!buffer) {
		printf("No memory...??? (failed to allocate %zu bytes)\n", getArchiveSize());
		return -ENOMEM;
	}


	ret = NAND_Read(getArchivePath(), buffer, getArchiveSize(), NULL);
	if (ret >= 0) {
		if (SignedTheme(buffer, getArchiveSize()) == 2) {
			puts("You still have the original theme.");
			return 0;
		}
	}

	sprintf(filepath, "%08x.app", getArchiveCid());
	ret = FAT_Read(filepath, buffer, getArchiveSize(), NULL);
	if (ret >= 0)
		if (SignedTheme(buffer, getArchiveSize()) == 2)
			goto install;

	printf("Initializing network... ");
	ret = network_init();
	if (ret < 0) {
		printf("failed! (%d)\n", ret);
		return ret;
	}
	puts("ok.");

	puts("Downloading...");
	sprintf(strrchr(url, '/'), "/%016llx/%08x", getSmNUSTitleID(), getArchiveCid());
	ret = DownloadFile(url, &download);
	if (ret != 0) {
		printf(
			"Download failed! (%d)\n"
			"Error details:\n"
			"	%s\n", ret, GetLastDownloadError());
		return ret;
	}

	puts("Decrypting...");
	mbedtls_aes_setkey_dec(&title, getSmTitleKey(), sizeof(aeskey) * 8);
	mbedtls_aes_crypt_cbc(&title, MBEDTLS_AES_DECRYPT, download.size, iv, download.ptr, download.ptr);
	memcpy(buffer, download.ptr, getArchiveSize());
	free(download.ptr);

	puts("Saving...");
	ret = FAT_Write(filepath, buffer, getArchiveSize(), progressbar);
	if (ret < 0)
		perror("Failed to save");

install:
	puts("Installing...");
	ret = InstallTheme(buffer, getArchiveSize());
	free(buffer);
	return ret;
}
