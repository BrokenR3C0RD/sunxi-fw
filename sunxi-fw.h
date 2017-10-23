#ifndef __SUNXI_FW_H__
#define __SUNXI_FW_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_SPL_SIZE	32768

enum image_type {
	IMAGE_ERROR,
	IMAGE_SHORT,
	IMAGE_UNKNOWN,
	IMAGE_BOOT0,
	IMAGE_SPL1,
	IMAGE_SPL2,
	IMAGE_SPLx,
	IMAGE_UBOOT,
	IMAGE_FIT,
	IMAGE_MBR,
};

/* Like lseek(), but works on pipes as well. Implies SEEK_CUR. */
int pseek(FILE *stream, long offset);

/*
 * Copy the content of <inf> from the current position to <outf>.
 * Copy <length> bytes, unless <length> is -1, in this case copy till EOF.
 */
off_t copy_file(FILE *inf, FILE *outf, off_t length);

/* sunxi-img.c */
enum image_type identify_image(const void *buffer);
int find_firmware_image(FILE *inf, enum image_type img, void *sector,
			FILE *outf);
int extract_image(FILE *inf, FILE *outf, const char *extract);
void output_image_info(FILE *inf, FILE *outf, bool verbose);

/**
 * output_*_info(): output information about image type
 * sector: pointer to buffer containing the first sector (512 bytes)
 * inf: input stream, for reading further data
 * stream: output stream, for dumping information
 * verbose: whether to output more elaborate information
 */

/* sunxi-mbr.c */
void output_mbr_info(void *sector, FILE *stream, bool verbose);

/* sunxi-spl.c */
void output_spl_info(void *sector, FILE *inf, FILE *stream, bool verbose);

#endif