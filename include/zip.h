#ifndef ZIP_H
#define ZIP_H

#include <stdint.h>
#include <stdio.h>

/* --- ZIP Library Error Codes --- */

/* Success code */
#define ZIP_OK 0

/*
 * System/Resource Errors (reflects underlying system call failures)
 */
#define ZIP_ERR_IO_READ        1 /* Failed to read data from the file. */
#define ZIP_ERR_IO_WRITE       2 /* Failed to write data to the file. */
#define ZIP_ERR_IO_SEEK        3 /* Failed to change file offset (lseek error). */
#define ZIP_ERR_MEM_ALLOC      4 /* Memory allocation failed (out of memory). */
#define ZIP_ERR_INVALID_ARG    5 /* Invalid argument provided (e.g., NULL pointer). */
#define ZIP_ERR_BAD_FILE_DESC  6 /* The provided file descriptor is invalid. */
#define ZIP_ERR_FILE_TOO_SMALL 7 /* ZIP file too small for basic structures. */
#define ZIP_ERR_FILE_TRUNCATED 8 /* An expected amount of data could not be read. */


/*
 * ZIP Format Specific Errors (related to parsing ZIP structures)
 */
/* EOCD (End of Central Directory) related errors */
#define ZIP_ERR_EOCD_NOT_FOUND           10 /* EOCD record (signature 0x06054b50) not found. */
#define ZIP_ERR_EOCD_SIGNATURE_BAD       11 /* EOCD found, but its signature is incorrect. */
#define ZIP_ERR_EOCD_CORRUPT_FIELDS      12 /* EOCD fields inconsistent or invalid. */

/* Central Directory related errors */
#define ZIP_ERR_CENTRAL_DIR_LOC        20 /* Failed to seek to Central Directory start. */
#define ZIP_ERR_CENTRAL_DIR_READ       21 /* Failed to read Central Directory data. */
#define ZIP_ERR_CENTRAL_DIR_CORRUPT    22 /* Central Directory contents malformed. */
#define ZIP_ERR_CD_ENTRY_SIGNATURE_BAD 23 /* CD file header entry has incorrect signature. */
#define ZIP_ERR_CD_ENTRY_CORRUPT       24 /* CD file header entry fields are corrupted. */

/* Local File Header related errors */
#define ZIP_ERR_LFH_LOC           30 /* Failed to seek to a Local File Header. */
#define ZIP_ERR_LFH_READ          31 /* Failed to read a Local File Header. */
#define ZIP_ERR_LFH_SIGNATURE_BAD 32 /* LFH has an incorrect signature. */
#define ZIP_ERR_LFH_CORRUPT       33 /* LFH fields are corrupted. */

/* Decompression/Compression related errors (for future expansion) */
#define ZIP_ERR_COMPRESSION_UNSUPPORTED 50 /* Compression method not supported. */
#define ZIP_ERR_DECOMPRESSION_FAILED    51 /* Decompression process failed. */

/* Generic/Fallback error */
#define ZIP_ERR_GENERIC        99 /* A general, unclassified error occurred. */

/* SIGNATURES */
#define EOCD_SIGNATURE 0x06054b50

#define EOCD_MAX_COMMENT_LEN 0xFFFF
#define EOCD_FIXED_SIZE      22

#define CDFH_SIGNATURE  0x02014b50
#define CDFH_FIXED_SIZE 46

#define LFD_SIGNATURE  0x06054b50

struct ZipEntry {
        char* file_name;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t compression_method;
        uint32_t local_header_offset;
};

uint8_t zip_read_directory(const int_fast32_t fp, struct ZipEntry*** entries, uint32_t* entry_count);
void zip_free_entries(struct ZipEntry*** entries, uint32_t entry_count);

#endif
