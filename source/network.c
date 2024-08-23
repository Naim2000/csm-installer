#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/ipc.h>
#include <wiisocket.h>
#include <curl/curl.h>

static int network_up = false;
static char ebuffer[CURL_ERROR_SIZE] = {};

typedef struct xferinfo_data_s {
	u64 start;
} xferinfo_data;

static size_t WriteToBlob(void* buffer, size_t size, size_t nmemb, void* userp) {
	size_t length = size * nmemb;
	blob* blob = userp;

	// If ptr is NULL, then the call is equivalent to malloc(size), for all values of size.
	unsigned char* _buffer = realloc(blob->ptr, blob->size + length);
	if (!_buffer) {
		printf("WriteToBlob: out of memory (%u + %u bytes)\n", blob->size, length);
		free(blob->ptr);
		blob->ptr = NULL;
		blob->size = 0;
		return 0;
	}

	blob->ptr = _buffer;
	memcpy(blob->ptr + blob->size, buffer, length);
	blob->size += length;

	return length;
}

static int xferinfo_cb(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	xferinfo_data* data = userp;
	if (!dltotal) {
		printf("...\r");
		return 0;
	}

	if (!data->start)
		data->start = gettime();

	uint64_t now = gettime();
	uint32_t elapsed = diff_msec(data->start, now);

	float f_dlnow = (dlnow / 0x400.p0);
	float f_dltotal = (dltotal / 0x400.p0);

	printf("\r%.2f/%.2fKB // %.2f KB/s...    ",
		   f_dlnow, f_dltotal, f_dlnow / ((float)elapsed / 1000));

	return 0;
}

int network_getlasterror(void) {
	int ret;

	int fd = ret = IOS_Open("/dev/net/ncd/manage", IPC_OPEN_READ);
	if (ret < 0) {
		printf("IOS_Open failed (%i)\n", ret);
		return ret;
	}

	int stbuf[8] __aligned(0x20) = {};
	ioctlv vecs[1] __aligned(0x20) = {
		{
			.data = stbuf,
			.len  = sizeof(stbuf),
		}
	};

	ret = IOS_Ioctlv(fd, 0x7, 0, 1, vecs);
	if (ret < 0) {
		printf("NCDGetLinkStatus returned %i\n", ret);
		// return ret;
	}
	IOS_Close(fd);

	puts("NCDGetLinkStatus:");
	printf("%i:%i:%i:%i:%i:%i:%i:%i\n", stbuf[0], stbuf[1], stbuf[2], stbuf[3], stbuf[4], stbuf[5], stbuf[6], stbuf[7]);

	return ret;
}

int network_init() {
	if ((network_up = wiisocket_get_status()) > 0)
		return 0;

	int ret = wiisocket_init();
	if (ret >= 0) {
		network_up = true;
		curl_global_init(0);
	}

	return ret;
}

void network_deinit() {
	if (network_up) {
		wiisocket_deinit();
		curl_global_cleanup();
		network_up = false;
	}
}

int DownloadFile(char* url, blob* blob) {
	CURL* curl;
	CURLcode res;
	xferinfo_data data = {};

	curl = curl_easy_init();
	if (!curl)
		return -1;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, ebuffer);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToBlob);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, blob);
	ebuffer[0] = '\x00';
	res = curl_easy_perform(curl);
	putchar('\n');
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		if (!ebuffer[0])
			strcpy(ebuffer, curl_easy_strerror(res));
	}

	return res;
}

const char* GetLastDownloadError() {
	return ebuffer;
}


