// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2018, Andre Przywara
 *
 * sunxi-img: routines that iterate through an image file to identify, find
 *            or extract firmware components
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>			/* for ntohl() */
#include <errno.h>

#include "sunxi-fw.h"

#define EGON_MAGIC1	0x4e4f4765		// "eGON"
#define EGON_MAGIC2	0x3054422e		// ".BT0"
#define SPL_MAGIC	0x004c5053		// "SPL\0"
#define IH_MAGIC	0x27051956
#define FDT_MAGIC	0xd00dfeed

static bool check_image_error(FILE *error, enum image_type type)
{
	switch (type) {
	case IMAGE_ERROR:
		fprintf(error, "error while reading image\n");
		return true;
	case IMAGE_SHORT:
		fprintf(error, "image file too short\n");
		return true;
	case IMAGE_UNKNOWN:
		fprintf(error, "unknown image file\n");
		return true;
	default:
		return false;
	}
}

/* iterates through an image files to find and report about components */
void output_image_info(FILE *inf, FILE *outf, bool verbose)
{
	char sector[512];
	enum image_type type;
	size_t ret;

	do {
		ret = fread(sector, 512, 1, inf);
		if (ret == 0 && feof(inf))
			break;

		type = identify_image(sector);
		switch (type) {
		case IMAGE_BOOT0:
			fprintf(outf, "boot0: Allwinner boot0\n");
			pseek(inf, MAX_SPL_SIZE - 512);
			break;
		case IMAGE_SPL1:
		case IMAGE_SPL2:
		case IMAGE_SPLx:
			fprintf(outf, "spl: U-Boot SPLv%c\n",
				type == IMAGE_SPL1 ? '1' :
				(type == IMAGE_SPL2 ? '2' : 'x'));
			output_spl_info(sector, inf, outf, verbose);
			break;
		case IMAGE_UBOOT:
			fprintf(outf, "u-boot.img: U-Boot legacy image\n");
			return;
		case IMAGE_FIT:
			fprintf(outf, "fit: U-Boot FIT image\n");
			return;
		case IMAGE_MBR:
			fprintf(outf, "mbr: MBR\n");
			output_mbr_info(sector, outf, verbose);
			pseek(inf, 8192 - 512);
			break;
		default:
			check_image_error(stderr, type);
			return;
		}
	} while (1);
}

/* check a given sector-sized buffer for magic numbers to identify components */
enum image_type identify_image(const void *buffer)
{
	const uint32_t *magic = buffer;

	if (ntohl(magic[0]) == FDT_MAGIC)
		return IMAGE_FIT;

	if (ntohl(magic[0]) == IH_MAGIC)
		return IMAGE_UBOOT;

	if (magic[1] == EGON_MAGIC1 && magic[2] == EGON_MAGIC2) {
		if (magic[5] < 0x10000)
			return IMAGE_BOOT0;

		if (magic[5] == (SPL_MAGIC | (1U << 24)))
			return IMAGE_SPL1;

		if (magic[5] == (SPL_MAGIC | (2U << 24)))
			return IMAGE_SPL2;

		if ((magic[5] & 0xffffff) == SPL_MAGIC)
			return IMAGE_SPLx;

		return IMAGE_UNKNOWN;
	}

	if (((unsigned char *)buffer)[510] == 0x55 &&
	    ((unsigned char *)buffer)[511] == 0xaa)
		return IMAGE_MBR;

	return IMAGE_UNKNOWN;
}

/* scans a file to find a specific firmware component type */
int find_firmware_image(FILE *inf, enum image_type img, void *sector,
			FILE *outf)
{
	enum image_type type;
	size_t ret;

	do {
		ret = fread(sector, 512, 1, inf);
		if (ret < 0)
			return ret;
		if (feof(inf))
			return -ENOENT;

		type = identify_image(sector);
		if (type == img)
			return 0;
		if (img == IMAGE_SPLx &&
		    (type == IMAGE_SPL1 || type == IMAGE_SPL2))
			return 0;

		switch (type) {

		case IMAGE_MBR:
			if (outf) {
				fwrite(sector, 512, 1, outf);
				copy_file(inf, outf, 8192 - 512);
			} else
				pseek(inf, 8192 - 512);
			break;
		case IMAGE_BOOT0:
		case IMAGE_SPL1:
		case IMAGE_SPL2:
		case IMAGE_SPLx:
			if (outf) {
				fwrite(sector, 512, 1, outf);
				copy_file(inf, outf, MAX_SPL_SIZE - 512);
			} else
				pseek(inf, MAX_SPL_SIZE - 512);
			break;
		case IMAGE_UBOOT:
		case IMAGE_FIT:
			return -ENOENT;
		default:
			if (check_image_error(stderr, type))
				return -ENOENT;
		}
	} while (1);
}

/* find an image type identified by a string and writes it to @outf */
int extract_image(FILE *inf, FILE *outf, const char *extract)
{
	char sector[512];
	enum image_type type = IMAGE_UNKNOWN;
	int ret;

	if (!strcmp(extract, "mbr"))
		type = IMAGE_MBR;
	if (!strcmp(extract, "boot0"))
		type = IMAGE_BOOT0;
	if (!strcmp(extract, "spl"))
		type = IMAGE_SPLx;
	if (!strncmp(extract, "u-boot", 6))
		type = IMAGE_UBOOT;
	if (!strncmp(extract, "fit", 3))
		type = IMAGE_FIT;

	ret = find_firmware_image(inf, type, sector, NULL);
	if (ret)
		return ret;

	switch (type) {
	case IMAGE_MBR:
		fwrite(sector, 1, 512, outf);
		return 0;
	case IMAGE_BOOT0:
	case IMAGE_SPL1:
	case IMAGE_SPL2:
	case IMAGE_SPLx:
		fwrite(sector, 1, 512, outf);
		copy_file(inf, outf, MAX_SPL_SIZE - 512);
		return 0;
	default:
		if (check_image_error(stderr, type))
			return -type;
	}

	return -ENOENT;
}