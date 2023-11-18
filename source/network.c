#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ogc/lwp_watchdog.h>
#include <wiisocket.h>
#include <curl/curl.h>

static int network_up = false;

typedef struct xferinfo_data_s {
	u64 start;
} xferinfo_data;

static size_t WriteToBlob(void* buffer, size_t size, size_t nmemb, void* userp) {
	size_t length = size * nmemb;
	blob* blob = userp;

	// If ptr is NULL, then the call is equivalent to malloc(size), for all values of size.
	unsigned char* _buffer = realloc(blob->ptr, blob->size + length);
	if (!_buffer) {
		printf("WriteToBlob: out of memory (%zu + %zu bytes)\n", blob->size, length);
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

	u64 now = gettime();
	u32 elapsed = diff_sec(data->start, now);

	printf("\r%llu/%llu bytes // %.2f KB/s...", dlnow, dltotal, ((double)dlnow / 0x400) / elapsed);

	return 0;
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
	xferinfo_data data;

	curl = curl_easy_init();
	if (!curl)
		return -1;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToBlob);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, blob);
	data.start = gettime();
	res = curl_easy_perform(curl);
	putchar('\n');
	curl_easy_cleanup(curl);

	return res;
}



