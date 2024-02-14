#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>

#include "wad.h"
#include "fs.h"
#include "crypto.h"
#include "malloc.h"

#define roundup16(len) (((len) + 0x0F) & ~0x0F)
#define roundup64(off) (((off) + 0x3F) & ~0x3F)

#define DO_PRAGMA_(x) _Pragma(#x)
#define DO_PRAGMA(x) DO_PRAGMA_(x)

#define gcc_warning_push(w) \
DO_PRAGMA(GCC diagnostic push); \
DO_PRAGMA(GCC diagnostic ignored #w); \

#define gcc_warning_pop DO_PRAGMA(GCC diagnostic pop)

__aligned(0x40) // gotta use that hardware accellerated SHA engine eventually
static unsigned char buffer[0x100000];

const char* wad_strerror(int ret) {
	switch (ret) {
		case -EPERM:	return "No perms lol";
		case -EIO:		return "Short read/write (probably read.)";
		case -ENOMEM:	return "Out of memory (!?)";

		case -106:	return "(FS) No such file or directory.";
		case -1005: return "Invalid public key type in certificate.";
		case -1009: return "(ES) Short read.";
		case -1010: return "(ES) Short write. Consider freeing up space in Data Management.";
		case -1012: return "Invalid signature type.";
		case -1016: return "Maximum amount of handles exceeded (3). Who is reopening /dev/es anyways?";
		case -1017: return "(ES) Invalid arguments. ES might not like what you're trying to do.";
		case -1020: return "This ticket is for a specific Wii, and it's not this one.";
		case -1022: return "Hash mismatch.\nIf uninstalling first does not fix this, this WAD is corrupt.";
		case -1024: return "ES is out of memory (!?)";
		case -1026: return "Incorrect access rights (according to the TMD.)";
		case -1027: return "Issuer not found in the certificate chain.";
		case -1028: return "Ticket not installed.";
		case -1029: return "Invalid ticket. This ticket probably uses the vWii common key.\nUninstalling the title first might fix this.";
		case -1031: return "Installed boot2 version is too old (or you are trying to downgrade it.)";
		case -1032: return "Fatal error in early ES initialization (!?)";
		case -1033: return "A ticket limit was exceeded. Play time is over, sorry.";
		case -1035: return "A newer version of this title is already present.\nConsider uninstalling it first.";
		case -1036: return "Required IOS for this title is not present.";
		case -1037: return "Installed number of contents doesn't match TMD.";
	//	case -1039: return "(DI) \"TMD not supplied for disc/nand game\"";

		case -2011: return "(IOSC) Signature check failed. Lol!! No trucha bug!!";
		case -2016: return "(IOSC) Unaligned data.";

		case ES_EINVAL: return "(libogc) Invalid arguments. This WAD is broken (?)";
		case ES_EALIGN: return "(libogc) Unaligned pointer.";
		case ES_ENOTINIT: return "(libogc) ES is not initialized (it is initialized on start..?)";

		default: return "Unknown error, maybe this is an errno ?";
	}
}

static bool sanityCheckHeader(struct wadHeader header) {
	return (
		header.size == 0x20
	&&	header.wadType == ('I' << 8 | 's')
	&&	header.wadVersion == 0x00
	);
}

static const char* sha1str(sha1 hash) {
	static char out[40+1];

	uint32_t* ptr = (uint32_t*)hash;
	sprintf(out, "%08x%08x%08x%08x%08x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4]);
	return out;
}

wad_t* wadInit(const char* filepath) {
	struct wadHeader header = {};
	size_t certsOffset, crlOffset, tikOffset, tmdOffset, contentsStart;
	signed_blob* s_tmd = NULL;
	signed_blob* s_tik = NULL;
	int signatureLevel = 0;
	wad_t* wad = NULL;

	FILE* fp = fopen(filepath, "rb");
	if (!fp) {
		perror("Failed to open WAD file");
		return NULL;
	}

	if (!ReadOpenFile(&header, 0, sizeof(header), fp)) {
		perror("Failed to read WAD header");
		return NULL;
	}

	if (!sanityCheckHeader(header)) {
		puts("Bad WAD header.");
		return NULL;
	}

	certsOffset = roundup64(sizeof(struct wadHeader));

	if (header.crlSize) {
		crlOffset = roundup64(certsOffset + header.certsSize);
		tikOffset = roundup64(crlOffset + header.crlSize);
	}
	else {
		crlOffset = 0;
		tikOffset = roundup64(certsOffset + header.certsSize);
	}

	tmdOffset = roundup64(tikOffset + header.tikSize);
	contentsStart = roundup64(tmdOffset + header.tmdSize);

	puts("\x1b[30;1mProcessing metadata... \x1b[39m");

	s_tmd = memalign32(header.tmdSize);
	if (!s_tmd) {
		puts("Memory allocation for TMD failed.");
		goto fail;
	}

	s_tik = memalign32(header.tikSize);
	if (!s_tik) {
		puts("Memory allocation for ticket failed.");
		goto fail;
	}

	if (!ReadOpenFile(s_tmd, tmdOffset, header.tmdSize, fp)) {
		perror("Failed to read TMD");
		goto fail;
	}

	if (!ReadOpenFile(s_tik, tikOffset, header.tikSize, fp)) {
		perror("Failed to read ticket");
		return NULL;
	}

	if (s_tmd[1]) signatureLevel |= 0x1;
	if (s_tik[1]) signatureLevel |= 0x2;

	tmd* p_tmd = SIGNATURE_PAYLOAD(s_tmd);
	tik* p_tik = SIGNATURE_PAYLOAD(s_tik);

	if (p_tmd->vwii_title)
		puts("\t\x1b[30;1mTMD reports that this is a vWii title \07w\07\x1b[39m");

	if (p_tik->reserved[0xb] == 0x02)
		puts("\t\x1b[30;1mTicket uses the vWii common key!\n\t\x1b[43m\x1b[30mInstalling this requires fakesigning!\x1b[40;39m");
	
	aeskey tkey = {};
	GetTitleKey(p_tik, tkey);

	/* a fun little trick with offsetof */
	wad = calloc(1, offsetof(wad_t, contents[p_tmd->num_contents]));
	if (!wad) {
		puts("Memory allocation for wad structure failed.");
		fclose(fp);
		goto fail;
	}

	wad->header = header;
	wad->fp = fp;

	wad->certsOffset = certsOffset;
	wad->crlOffset = crlOffset;
	wad->tikOffset = tikOffset;
	wad->tmdOffset = tmdOffset;
	wad->contentsStart = contentsStart;
	wad->contentsCount = p_tmd->num_contents;

	for (int i = 0; i < p_tmd->num_contents; i++) {
		struct wadContent* wContent = wad->contents + i;
		tmd_content* content = p_tmd->contents + i;

		wContent->content = *content;
		wContent->offset = (i == 0) ?
			contentsStart :
			wContent[-1].offset + roundup64(wContent[-1].content.size);

		printf("\r\x1b[30;1mVerifying content hash #%i... \x1b[39m", i);
		fseek(fp, wContent->offset, SEEK_SET);

		mbedtls_sha1_context sha = {};
		aesiv iv = {};
		sha1 hash = {};
		size_t csize = content->size;

		iv.index = content->index;
		mbedtls_sha1_starts_ret(&sha);
		
		while (csize) {
			size_t read = MIN(sizeof(buffer), csize);

			if (!ReadOpenFile(buffer, -1, roundup16(read), fp))
				goto fail;

			AES_Decrypt(tkey, iv.full, buffer, buffer, roundup16(read));
			mbedtls_sha1_update_ret(&sha, buffer, read);

			csize -= read;
		}

		mbedtls_sha1_finish_ret(&sha, hash);
		if (memcmp(content->hash, hash, sizeof(sha1)) != 0) {
			printf("failed!\n");
			printf("Content hash : %.40s\n", sha1str(content->hash));
			printf("Computed hash: %.40s\n", sha1str(hash));
			goto fail;
		}
	}

	wad->titleID = p_tmd->title_id;
	wad->titleVer = p_tmd->title_version;
	wad->titleIOS = p_tmd->sys_version & 0xFFFFFFFF;
	memcpy(wad->titleKey, tkey, sizeof(aeskey));

	free(s_tmd);
	free(s_tik);
	return wad;

fail:
	free(s_tmd);
	free(s_tik);
	wadFree(wad);
	return NULL;
}

gcc_warning_push(-Wstringop-overflow)
static inline void void_signature(signed_blob* blob) { memset(SIGNATURE_SIG(blob), 0x00, SIGNATURE_SIZE(blob) - 0x4); }
gcc_warning_pop

static bool Fakesign(signed_blob* s_tmd, signed_blob* s_tik) {
	sha1 hash = {};

	if (s_tmd) {
		void_signature(s_tmd);
		tmd* p_tmd = SIGNATURE_PAYLOAD(s_tmd);
		uint16_t v = 0;
		do {
			p_tmd->fill3 = v++;
			mbedtls_sha1_ret((unsigned char*)p_tmd, TMD_SIZE(p_tmd), hash);
			if (hash[0] == 0x00)
				break;
		} while (v);

		if (!v)
			return false;
	}

	if (s_tik) {
		void_signature(s_tik);
		tik* p_tik = SIGNATURE_PAYLOAD(s_tik);
		uint16_t v = 0;
		do {
			p_tik->padding = v++;
			mbedtls_sha1_ret((unsigned char*)p_tik, sizeof(tik), hash);
			if (hash[0] == 0x00)
				break;
		} while (v);

		if (!v)
			return false;
	}

	return true;
}

int wadInstall(wad_t* wad) {
	int ret;

	signed_blob *s_certs, *s_tik, *s_tmd, *s_crl = NULL;

	s_certs = memalign32(wad->header.certsSize);
	s_tik = memalign32(wad->header.tikSize);
	s_tmd = memalign32(wad->header.tmdSize);
	if (!s_certs || !s_tik || !s_tmd) {
		ret = -ENOMEM;
		goto finish;
	}

	ReadOpenFile(s_certs,	wad->certsOffset,	wad->header.certsSize,	wad->fp);
	ReadOpenFile(s_tik,		wad->tikOffset,		wad->header.tikSize,	wad->fp);
	ReadOpenFile(s_tmd,		wad->tmdOffset,		wad->header.tmdSize,	wad->fp);

	if (wad->header.crlSize) {
		s_crl = memalign32(wad->header.crlSize);
		if (!s_crl) {
			ret = -ENOMEM;
			goto finish;
		}

		ReadOpenFile(s_crl, wad->crlOffset, wad->header.crlSize, wad->fp);
	}

//	tmd* p_tmd = SIGNATURE_PAYLOAD(s_tmd);
	tik* p_tik = SIGNATURE_PAYLOAD(s_tik);

/*
	if (p_tmd->vwii_title) {
		puts("TMD reports that this is a vWii title, let's change that.");
		p_tmd->vwii_title = 0x00;
		if (!Fakesign(s_tmd, 0)) {
			puts("Failed to fakesign TMD!");
			ret = -EPERM;
			goto finish;
		}
	}
*/

	if (p_tik->reserved[0xb] == 0x02) {
		puts("This ticket uses the vWii common key, let's change that.");
		ChangeCommonKey(p_tik, 0);
		if (!Fakesign(0, s_tik)) {
			puts("Failed to fakesign ticket!");
			ret = -EPERM;
			goto finish;
		}
	}

	size_t s_tmd_size = sizeof(sig_rsa2048) + sizeof(tmd) + (sizeof(tmd_content) * wad->contentsCount);
	size_t s_tik_size = sizeof(sig_rsa2048) + sizeof(tik);

	if (wad->header.tikSize != s_tik_size)
		printf("\x1b[1;33mWARNING: Ticket size stated in header is incorrect! (%u != %u)\x1b[39m\n", wad->header.tikSize, s_tik_size);

	if (wad->header.tmdSize != s_tmd_size)
		printf("\x1b[1;33mWARNING: TMD size stated in header is incorrect! (%u != %u)\x1b[39m\n", wad->header.tmdSize, s_tmd_size);

	printf("Installing ticket... ");
	ret = ES_AddTicket(s_tik, s_tik_size, s_certs, wad->header.certsSize, s_crl, wad->header.crlSize);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		goto finish;
	}
	puts("OK!");

	printf("Starting title installation... ");
	ret = ES_AddTitleStart(s_tmd, s_tmd_size, s_certs, wad->header.certsSize, s_crl, wad->header.crlSize);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		goto finish;
	}
	puts("OK!");

	free(s_certs);
	free(s_crl);
	free(s_tik);
	free(s_tmd);
	s_certs = s_crl = s_tik = s_tmd = NULL;

	for (int i = 0; i < wad->contentsCount; i++) {
		struct wadContent* wContent = wad->contents + i;
		tmd_content* content = &wContent->content;
		size_t e_csize = roundup16(content->size);

		printf("\r	Installing content #%i... ", i);
		int cfd = ret = ES_AddContentStart(wad->titleID, content->cid);
		if (ret < 0)
			break;

		fseek(wad->fp, wContent->offset, SEEK_SET);
		while (e_csize) {
			size_t _read = MIN(sizeof(buffer), e_csize);
			if (!ReadOpenFile(buffer, -1, _read, wad->fp)) {
				ret = -errno;
				break;
			}

			ret = ES_AddContentData(cfd, buffer, _read);
			if (ret < 0)
				break;

			e_csize -= _read;
		}

		if (ret >= 0)
			ret = ES_AddContentFinish(cfd);

		if (ret < 0) {
			printf("failed! (%i)\n", ret);
			goto finish;
		}
	}

	if (!ret) {
		printf("\nFinishing title installation... ");
		ret = ES_AddTitleFinish();
	}

	if (ret < 0)
		printf("failed! (%i)\n", ret);
	else
		puts("OK!");
	

finish:
	free(s_certs);
	free(s_crl);
	free(s_tik);
	free(s_tmd);
	return ret;
}

int wadUninstall(wad_t* wad, int level) {
	int ret;


	printf("Removing title contents... ");
	ret = ES_DeleteTitleContent(wad->titleID);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		return ret;
	}

	puts("OK!");

	if (!level--) return ret;

	printf("Removing title... ");
	ret = ES_DeleteTitle(wad->titleID);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		return ret;
	}

	puts("OK!");

	if (!level--) return ret;

	printf("Removing ticket(s)... ");
	uint32_t viewCnt = 0;
	ret = ES_GetNumTicketViews(wad->titleID, &viewCnt);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		return ret;
	}

	if (!viewCnt) {
		puts("OK! (No tickets.)");
		return ret;
	}

	tikview* views = memalign32(sizeof(tikview) * viewCnt);
	if (!views) {
		puts("Memory allocation failed!");
		return -ENOMEM;
	}

	ret = ES_GetTicketViews(wad->titleID, views, viewCnt);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		free(views);
		return ret;
	}

	__aligned(0x20) static tikview tikview_buf = {};
	for (tikview* view = views; view < views + viewCnt; view++) {
		tikview_buf = *view;

		if (tikview_buf.devicetype && !level) {
			puts("This ticket is owned by this Wii, skipping..!");
			continue;
		}

		ret = ES_DeleteTicket(&tikview_buf);
		if (ret < 0) break;
	}

	if (ret < 0)
		printf("failed! (%i)\n", ret);
	else
		puts("OK!");

	return ret;
}


static bool fsnip(FILE* in, size_t offset, size_t size, const char* path) {
	FILE* out = fopen(path, "wb");
	if (!out) return false;

	size_t left = size;
	while (left) {
		size_t len = MIN(sizeof(buffer), left);

		if (!ReadOpenFile(buffer, offset, len, in) || !fwrite(buffer, len, 1, out))
			break;

		left -= len;
	}

	fclose(out);
	return !left;
}

static const char* ContentType(uint16_t type) {
	switch (type) {
		case 0x0001: return "Normal (0x0001)";
		case 0x8001: return "Shared (0x8001)";
		case 0x4001: return "DLC (0x4001)";
	}

	return "Unknown";
}

int wadExtract(wad_t* wad, const char* dir) {
	char path[PATH_MAX];
	char filename[0x40];

	// i would probably use openat() or some crap but how do i know if that even works
	strcpy(path, dir);
	puts(path);

	if (mkdir(path, 0644) < 0 && errno != EEXIST) {
		perror("mkdir");
		return -errno;
	}

	if (chdir(path) < 0) {
		perror("chdir");
		return -errno;
	}

	strcpy(filename, "cert.sys");
	puts("Extracting certificates...");
	if (!fsnip(wad->fp, wad->certsOffset, wad->header.certsSize, filename)) {
		perror(filename);
		return -errno;
	}

	strcpy(filename, "title.tik");
	puts("Extracting ticket...");
	if (!fsnip(wad->fp, wad->tikOffset, wad->header.tikSize, filename)) {
		perror(filename);
		return -errno;
	}

	strcpy(filename, "title.tmd");
	puts("Extracting TMD...");
	if (!fsnip(wad->fp, wad->tmdOffset, wad->header.tmdSize, filename)) {
		perror(filename);
		return -errno;
	}

	puts("Extracting contents...");
	for (int i = 0; i < wad->contentsCount; i++) {
		struct wadContent* wContent = wad->contents + i;
		tmd_content* content = &wContent->content;

		sprintf(filename, "%08x.app", content->index);
		printf("\tExtracting content #%i...\n", content->index);
		FILE* out = fopen(filename, "wb");
		if (!out) {
			perror(filename);
			return -errno;
		}

		fseek(wad->fp, wContent->offset, SEEK_SET);

		aesiv iv = {};
		iv.index = content->index;
		size_t csize = content->size;

		while (csize) {
			size_t read = MIN(sizeof(buffer), csize);

			if (!ReadOpenFile(buffer, -1, roundup16(read), wad->fp))
				break;

			AES_Decrypt(wad->titleKey, iv.full, buffer, buffer, roundup16(read));

			if (!fwrite(buffer, read, 1, out)) {
				perror(filename);
				break;
			}

			csize -= read;
		}

		fclose(out);

		if (csize) 
			return -errno;
	}

	puts("\nWriting details...");
	sprintf(filename, "%s.txt", strrchr(path, '/') + 1);
	FILE* txt = fopen(filename, "w");
	if (!txt) {
		perror(filename);
		return -errno;
	}

	strcpy(filename, "sha1sums.txt");
	FILE* sha1sums = fopen(filename, "w");
	if (!sha1sums) {
		perror(filename);
		fclose(txt);
		return -errno;
	}

	fprintf(txt, "Title ID: %016llx\n", wad->titleID);
	fprintf(txt, "Revision: %hu (0x%04hx/%hhu.%hhu)\n", wad->titleVer, wad->titleVer, wad->titleVer >> 8, wad->titleVer & 0xFF);
	fprintf(txt, "IOS ver : IOS%u\n\n", wad->titleIOS);

	fprintf(txt,
		"Certs size : %u bytes (0x%x)\n"
		"CRL size   : %u bytes (0x%x)\n"
		"Ticket size: %u bytes (0x%x)\n"
		"TMD size   : %u bytes (0x%x)\n\n", 
		wad->header.certsSize,  wad->header.certsSize,
		wad->header.crlSize, wad->header.crlSize,
		wad->header.tikSize, wad->header.tikSize,
		wad->header.tmdSize, wad->header.tmdSize);

	fprintf(txt, "Contents count: %hu\n\n", wad->contentsCount);

	for (int i = 0; i < wad->contentsCount; i++) {
		struct wadContent* wContent = wad->contents + i;
		tmd_content* content = &wContent->content;

		fprintf(txt,
			"Content #%i:\n"
			"	ID  : %08x\n"
			"	Size: %.2fMB (0x%llx)\n"
			"	Type: %s\n"
			"	Hash: %s\n\n",
			content->index, content->cid, content->size / 1048576.f, content->size,
			ContentType(content->type), sha1str(content->hash)
		);

		fprintf(sha1sums, "%.40s *%08x.app\n", sha1str(content->hash), content->index);
	}

	fflush(txt); // feels more natural
	fclose(txt);
	fflush(sha1sums);
	fclose(sha1sums);

	return 0;
}

void wadFree(wad_t* wad) {
	if (!wad) return;

	fclose(wad->fp);
	free(wad);
}
