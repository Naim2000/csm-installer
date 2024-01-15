#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>

#include "wad.h"
#include "crypto.h"
#include "malloc.h"

#define roundup16(len) (((len) + 0x0F) & ~0x0F)
#define roundup64(off) (((off) + 0x3F) & ~0x3F)

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

		case ES_EINVAL: return "(libogc) Invalid arguments. Only practical reason for this is an\nincorrect TMD/ticket size in the WAD header. This WAD is broken.";
		case ES_EALIGN: return "(libogc) Unaligned pointer.";
		case ES_ENOTINIT: return "(libogc) ES is not initialized (it is initialized on start..?)";

		default: return NULL;
	}
}

static bool sanityCheckHeader(struct wadHeader header) {
	return (
		header.size == 0x20
	&&	header.wadType == ('I' << 8 | 's')
	&&	header.wadVersion == 0x00
	);
}

wad_t* wadInit(const char* filepath) {
	struct wadHeader header = {};
	size_t certsOffset, crlOffset, tikOffset, tmdOffset, contentsStart;
	signed_blob* s_tmd = NULL;
	signed_blob* s_tik = NULL;
	wad_t* wad = NULL;

	FILE* fp = fopen(filepath, "rb");
	if (!fp) {
		perror("Failed to open WAD file");
		return NULL;
	}

	if (!fread(&header, sizeof(header), 1, fp)) {
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

	s_tmd = memalign32(header.tmdSize);
	if (!s_tmd) {
		puts("Memory allocation for TMD failed.");
		goto fail;
	}

	fseek(fp, tmdOffset, SEEK_SET);
	if (!fread(s_tmd, header.tmdSize, 1, fp)) {
		perror("Failed to read TMD");
		goto fail;
	}

	tmd* p_tmd = SIGNATURE_PAYLOAD(s_tmd);

	if (p_tmd->vwii_title)
		puts("TMD reports that this is a vWii title");

	s_tik = memalign32(header.tikSize);
	if (!s_tmd) {
		puts("Memory allocation for ticket failed.");
		goto fail;
	}

	fseek(fp, tikOffset, SEEK_SET);
	if (!fread(s_tik, header.tikSize, 1, fp)) {
		perror("Failed to read ticket");
		goto fail;
	}

	tik* p_tik = SIGNATURE_PAYLOAD(s_tik);

	aeskey tkey = {};
	GetTitleKey(p_tik, tkey);

	size_t wad_size = sizeof(wad_t) + (p_tmd->num_contents * sizeof(struct wadContent));
	wad = malloc(wad_size);
	if (!wad) {
		puts("Memory allocation for wad structure failed.");
		goto fail;
	}
	memset(wad, 0, wad_size);

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

		fseek(fp, wContent->offset, SEEK_SET);

		__attribute__((aligned(0x20)))
		static unsigned char buffer[0x4000];
		mbedtls_aes_context  aes = {};
		mbedtls_sha1_context sha = {};
		aesiv iv = {};
		sha1 hash = {};
		size_t csize = content->size;

		iv.index = content->index;
		mbedtls_aes_setkey_dec(&aes, tkey, 128);
		mbedtls_sha1_starts_ret(&sha);
		
		while (csize) {
			size_t read = MIN(sizeof(buffer), csize - read);

			if (!fread(buffer, roundup16(read), 1, fp))
				goto fail;

			mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, roundup16(read), iv.full, buffer, buffer);
			mbedtls_sha1_update_ret(&sha, buffer, read);

			csize -= read;
		}

		mbedtls_sha1_finish_ret(&sha, hash);
		if (memcmp(content->hash, hash, sizeof(sha1)) != 0) {
			printf("Hash mismatch for content #%i! (cid=%u)\n", i, content->cid);
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

static inline void void_signature(signed_blob* blob) { memset(SIGNATURE_SIG(blob), 0x00, SIGNATURE_SIZE(blob) - 0x4); }

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

	signed_blob* s_certs = NULL;
	signed_blob* s_crl = NULL;
	signed_blob* s_tik = NULL;
	signed_blob* s_tmd = NULL;

	s_certs = memalign32(wad->header.certsSize);
	s_tik = memalign32(wad->header.tikSize);
	s_tmd = memalign32(wad->header.tmdSize);
	if (!s_certs || !s_tik || !s_tmd) {
		ret = -ENOMEM;
		goto finish;
	}

	fseek(wad->fp, wad->certsOffset, SEEK_SET);
	fread(s_certs, 1, wad->header.certsSize, wad->fp);

	fseek(wad->fp, wad->tikOffset, SEEK_SET);
	fread(s_tik, 1, wad->header.tikSize, wad->fp);

	fseek(wad->fp, wad->tmdOffset, SEEK_SET);
	fread(s_tmd, 1, wad->header.tmdSize, wad->fp);

	if (wad->header.crlSize) {
		s_crl = memalign32(wad->header.crlSize);
		if (!s_crl) {
			ret = -ENOMEM;
			goto finish;
		}

		fseek(wad->fp, wad->crlOffset, SEEK_SET);
		fread(s_certs, 1, wad->header.crlSize, wad->fp);
	}

	tmd* p_tmd = SIGNATURE_PAYLOAD(s_tmd);
	tik* p_tik = SIGNATURE_PAYLOAD(s_tik);

	if (p_tmd->vwii_title) {
		puts("TMD reports that this is a vWii title, let's change that.");
		p_tmd->vwii_title = 0x00;
		if (!Fakesign(s_tmd, 0)) {
			puts("Failed to fakesign TMD!");
			ret = -EPERM;
			goto finish;
		}
	}

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

	printf(">   Installing ticket... ");
	ret = ES_AddTicket(s_tik, s_tik_size, s_certs, wad->header.certsSize, s_crl, wad->header.crlSize);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		goto finish;
	}
	puts("OK!");

	if (wad->header.tmdSize != s_tmd_size)
		printf("\x1b[1;33mWARNING: TMD size stated in header is incorrect! (%u != %u)\x1b[39m\n", wad->header.tmdSize, s_tmd_size);

	printf(">   Starting title installation... ");
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
		__attribute__((__aligned__(0x20)))
		static unsigned char buffer[0x4000];

		struct wadContent* wContent = wad->contents + i;
		tmd_content* content = &wContent->content;
		size_t e_csize = roundup16(content->size);

		printf(">>    Installing content #%02i... ", i);
		int cfd = ret = ES_AddContentStart(wad->titleID, content->cid);
		if (ret < 0)
			break;

		fseek(wad->fp, wContent->offset, SEEK_SET);
		while (e_csize) {
			size_t _read = MIN(sizeof(buffer), e_csize);
			if (fread(buffer, 1, _read, wad->fp) < _read) {
				ret = -EIO;
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

		putchar('\r');
	}

	if (!ret) {
		printf(">   Finishing title installation... ");
		ret = ES_AddTitleFinish();
	}

	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		ES_AddTitleCancel();
	}
	else {
		puts("OK!");
	}

finish:
	free(s_certs);
	free(s_crl);
	free(s_tik);
	free(s_tmd);
	return ret;
}

void wadFree(wad_t* wad) {
	if (!wad) return;

	fclose(wad->fp);
	free(wad);
}
