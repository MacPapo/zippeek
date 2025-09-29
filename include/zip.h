#ifndef ZIP_H
#define ZIP_H

#include <stdint.h>
#include <stdio.h>

/* SIGNATURES */
#define EOCD_SIGNATURE 0x06054b50

#define EOCD_MAX_COMMENT_LEN 0xFFFF
#define EOCD_FIXED_SIZE 22

#define CDFH_SIGNATURE 0x02014b50
#define CDFH_FIXED_SIZE 46

#define LFD_SIGNATURE 0x06054b50

typedef struct ZipArchive ZipArchive;

/* Local File Header */
typedef struct {
	uint32_t signature; /* local file header signature 4 bytes (0x04034b50) */
	uint16_t version_needed; /* version needed to extract 2 bytes */
	uint16_t bit_flag; /* general purpose bit flag 2 bytes */
	uint16_t comp_method; /* compression method 2 bytes */
	uint16_t last_mod_file_time; /* last mod file time 2 bytes */
	uint16_t last_mod_file_date; /* last mod file date 2 bytes */
	uint32_t crc32; /* crc-32 4 bytes */
	uint32_t comp_size; /* compressed size 4 bytes */
	uint32_t uncomp_size; /* uncompressed size 4 bytes */
	uint16_t file_name_len; /* file name length 2 bytes */
	uint16_t extra_field_len; /* extra field length 2 bytes */
	char *file_name; /* file name (variable size) */
	char *extra_field; /* extra field (variable size) */
} __attribute__((packed)) LFH;

/* Central Directory File Header */
typedef struct {
	uint32_t signature; /* central file header signature 4 bytes (0x02014b50) */
	uint16_t version; /* version made by 2 bytes */
	uint16_t version_needed; /* version needed to extract 2 bytes */
	uint16_t bit_flag; /* general purpose bit flag 2 bytes */
	uint16_t comp_method; /* compression method 2 bytes */
	uint16_t last_mod_file_time; /* last mod file time 2 bytes */
	uint16_t last_mod_file_date; /* last mod file date 2 bytes */
	uint32_t crc32; /* crc-32 4 bytes */
	uint32_t comp_size; /* compressed size 4 bytes */
	uint32_t uncomp_size; /* uncompressed size 4 bytes */
	uint16_t file_name_len; /* file name length 2 bytes */
	uint16_t extra_field_len; /* extra field length 2 bytes */
	uint16_t file_comment_len; /* file comment length 2 bytes */
	uint16_t disk_num_start; /* disk number start 2 bytes */
	uint16_t internal_file_attr; /* internal file attributes 2 bytes */
	uint32_t external_file_attr; /* external file attributes 4 bytes */
	uint32_t local_header_offset; /* relative offset of local header 4 bytes */
	char *file_name; /* file name (variable size) */
	/* NOT IMPLEMENTED extra field (variable size) */
	/* NOT IMPLEMENTED file comment (variable size) */
} __attribute__((packed)) CDFH;

/* Data Descriptor */
typedef struct {
	uint32_t crc32; /* crc-32 4 bytes */
	uint32_t comp_size; /* compressed size 4 bytes */
	uint32_t uncomp_size; /* uncompressed size 4 bytes */
} __attribute__((packed)) DD;

/* Zip64 end of central directory record */
typedef struct {
	uint32_t signature; /* zip64 end of central dir signature 4 bytes  (0x06064b50) */
	uint64_t zip64_eocd_size; /* size of zip64 end of central directory record 8 bytes */
	uint16_t version; /* version made by 2 bytes */
	uint16_t version_needed; /* version needed to extract 2 bytes */
	uint32_t this_disk; /* number of this disk 4 bytes */
	uint32_t central_dir_disk; /* number of the disk with the start of the central directory 4 bytes */
	uint64_t total_entries_this_disk; /* total number of entries in the central directory on this disk 8 bytes */
	uint64_t total_entries; /* total number of entries in the central directory 8 bytes */
	uint64_t central_dir_size; /* size of the central directory 8 bytes */
	uint64_t central_dir_offset; /* offset of start of central directory with respect to the starting disk number 8 bytes */
	/* NOT IMPLEMENTED zip64 extensible data sector (variable size) */
} __attribute__((packed)) ZIP64EOCD;

/* Zip64 end of central directory locator */
typedef struct {
	uint32_t signature; /* zip64 end of central dir locator signature 4 bytes  (0x07064b50) */
	uint32_t zip64_eocd; /* number of the disk with the start of the zip64 end of central directory 4 bytes */
	uint64_t zip64_eocd_offset; /* relative offset of the zip64 end of central directory record 8 bytes */
	uint32_t total_entries; /* total number of disks 4 bytes */
} __attribute__((packed)) ZIP64EOCDLOCATOR;

/* End of Central Directory */
typedef struct {
	uint32_t signature; /* end of central dir signature 4 bytes  (0x06054b50) */
	uint16_t this_disk; /* number of this disk 2 bytes */
	uint16_t central_dir_disk; /* number of the disk with the start of the central directory 2 bytes */
	uint16_t total_entries_this_disk; /* total number of entries in the central directory on this disk 2 bytes */
	uint16_t total_entries; /* total number of entries in the central directory 2 bytes */
	uint32_t central_dir_size; /* size of the central directory 4 bytes */
	uint32_t central_dir_offset; /* offset of start of central directory with respect to the starting disk number 4 bytes */
	uint16_t comment_length; /* .ZIP file comment length 2 bytes */
	/* NOT IMPLEMENTED .ZIP file comment (variable size) */
} __attribute__((packed)) EOCD;

ZipArchive *zip_open_archive(const char *filename);
void zip_close_archive(ZipArchive *archive);
void zip_inspect_archive(ZipArchive *archive);
int8_t find_eocd(FILE *fp, EOCD *eocd);

static inline uint16_t read_u16(const unsigned char *buffer, size_t offset)
{
	return (uint16_t)buffer[offset] | ((uint16_t)buffer[offset + 1] << 8);
}

static inline uint32_t read_u32(const unsigned char *buffer, size_t offset)
{
	return (uint32_t)buffer[offset] | ((uint32_t)buffer[offset + 1] << 8) |
	       ((uint32_t)buffer[offset + 2] << 16) |
	       ((uint32_t)buffer[offset + 3] << 24);
}

static inline uint64_t read_u64(const unsigned char *buffer, size_t offset)
{
	return (uint64_t)buffer[offset] | ((uint64_t)buffer[offset + 1] << 8) |
	       ((uint64_t)buffer[offset + 2] << 16) |
	       ((uint64_t)buffer[offset + 3] << 24) |
	       ((uint64_t)buffer[offset + 4] << 32) |
	       ((uint64_t)buffer[offset + 5] << 40) |
	       ((uint64_t)buffer[offset + 6] << 48) |
	       ((uint64_t)buffer[offset + 7] << 56);
}

#endif
