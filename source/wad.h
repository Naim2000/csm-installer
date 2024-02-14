#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <ogc/es.h>

struct wadHeader {
	uint32_t size;
	uint16_t wadType;
	uint16_t wadVersion;
	uint32_t certsSize;
	uint32_t crlSize;
	uint32_t tikSize;
	uint32_t tmdSize;
	uint32_t contentDataSize;
	uint32_t footerSize;

	char padding[0x20];
};
static_assert(sizeof(struct wadHeader) == 0x40, "Bad header struct!");

struct wadContent {
	tmd_content content;
	size_t offset;
};

typedef struct {
	struct wadHeader header;
	FILE* fp;

	size_t certsOffset;
	size_t crlOffset;
	size_t tikOffset;
	size_t tmdOffset;
	size_t contentsStart;
	uint16_t contentsCount;

	int64_t titleID;
	uint16_t titleVer;
	int32_t titleIOS;
	aeskey titleKey;

	struct wadContent contents[];
} wad_t;

enum {
	REMOVE_CONTENTS,
	REMOVE_TMD,
	REMOVE_TIK,
	REMOVE_TIK_FORCE,
};

__result_use_check
wad_t* wadInit(const char* __restrict filepath);
void wadFree(wad_t*);

int wadInstall(wad_t* __restrict);
int wadUninstall(wad_t* __restrict, int level);
int wadExtract(wad_t* __restrict, const char* dir);


const char* wad_strerror(int);
