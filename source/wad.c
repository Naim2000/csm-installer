#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <mbedtls/aes.h>

#include "wad.h"

#define roundup16(len) (((len) + 0x0F) & ~0x0F)
#define roundup64(off) (((off) + 0x3F) & ~0x3F)

extern void* memalign(size_t, size_t);

static const aeskey wii_ckey  = { 0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7 };

const char* wad_strerror(int ret) {
	switch (ret) {
		case -EIO: return "Short read/write (probably read.)";
		case -ENOMEM: return "Out of memory (!?)";

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
		tikOffset = roundup64((crlOffset + header.crlSize));
	}
	else {
		crlOffset = 0;
		tikOffset = roundup64(certsOffset + header.certsSize);
	}

	tmdOffset = roundup64(tikOffset + header.tikSize);
	contentsStart = roundup64(tmdOffset + header.tmdSize);

	s_tmd = memalign(0x20, header.tmdSize);
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

	s_tik = memalign(0x20, header.tikSize);
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

	if (p_tik->reserved[0xb] != 0x00)
		printf("WARNING: common key index 0x%02x is not 0 (!?)\n", p_tik->reserved[0xb]);

	mbedtls_aes_context aes = {};
	aeskey tkey = {};
	int64_t iv[2] = { p_tik->titleid, 0 };

	mbedtls_aes_setkey_dec(&aes, wii_ckey, sizeof(aeskey) * 8);
	mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, sizeof(aeskey), (unsigned char*)iv, p_tik->cipher_title_key, tkey);

	wad_t* wad = memalign(0x20, sizeof(wad_t) + (p_tmd->num_contents * sizeof(struct wadContent)));
	if (!wad) {
		puts("Memory allocation for wad structure failed.");
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

		wContent->content = p_tmd->contents[i];
		wContent->offset = (i == 0) ?
			contentsStart :
			wContent[-1].offset + roundup64(wContent[-1].content.size);
		// wContent->offset = (i? wContent[i - 1].offset : contentsStart) + roundup64(i? wContent[i - 1].content.size : 0);
	}

	wad->titleID = p_tmd->title_id;
	wad->titleVer = p_tmd->title_version;
	wad->titleIOS = (uint32_t)(p_tmd->sys_version & 0xFFFFFFFF);
	memcpy(wad->titleKey, tkey, sizeof(aeskey));

	free(s_tmd);
	free(s_tik);
	return wad;

fail:
	free(s_tmd);
	free(s_tik);
	return NULL;
}

int wadInstall(wad_t* wad) {
	int ret;

	signed_blob* s_certs = NULL;
	signed_blob* s_crl = NULL;
	signed_blob* s_tik = NULL;
	signed_blob* s_tmd = NULL;

	s_certs = memalign(0x20, wad->header.certsSize);
	s_tik = memalign(0x20, wad->header.tikSize);
	s_tmd = memalign(0x20, wad->header.tmdSize);
	if (!s_certs || !s_tik || !s_tmd) {
		free(s_certs);
		free(s_tik);
		free(s_tmd);
		return -ENOMEM;
	}

	fseek(wad->fp, wad->certsOffset, SEEK_SET);
	fread(s_certs, 1, wad->header.certsSize, wad->fp);

	fseek(wad->fp, wad->tikOffset, SEEK_SET);
	fread(s_tik, 1, wad->header.tikSize, wad->fp);

	fseek(wad->fp, wad->tmdOffset, SEEK_SET);
	fread(s_tmd, 1, wad->header.tmdSize, wad->fp);

	if (wad->header.crlSize) {
		s_crl = memalign(0x20, wad->header.crlSize);
		if (!s_crl)
			return -ENOMEM;

		fseek(wad->fp, wad->crlOffset, SEEK_SET);
		fread(s_certs, 1, wad->header.crlSize, wad->fp);
	}

	size_t s_tmd_size = sizeof(sig_rsa2048) + sizeof(tmd) + (sizeof(tmd_content) * wad->contentsCount);
	size_t s_tik_size = sizeof(sig_rsa2048) + sizeof(tik);

	if (wad->header.tikSize != s_tik_size)
		printf("WARNING: Ticket size stated in header is incorrect! (%i != %i)\n", wad->header.tikSize, s_tik_size);

	printf(">   Installing ticket... ");
	ret = ES_AddTicket(s_tik, s_tik_size, s_certs, wad->header.certsSize, s_crl, wad->header.crlSize);
	if (ret < 0) {
		printf("failed! (%i)\n", ret);
		goto finish;
	}
	puts("OK!");

	if (wad->header.tmdSize != s_tmd_size)
		printf("WARNING: TMD size stated in header is incorrect! (%i != %i)\n", wad->header.tmdSize, s_tmd_size);

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

		puts("OK!");
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
