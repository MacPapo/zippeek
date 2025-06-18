#ifndef ZIP_H
#define ZIP_H

#include <stdint.h>
#include <stdio.h>

/* CODES */
#define ZIP_OK                 0
#define ZIP_ERR_EOCD_NOT_FOUND 1
#define ZIP_ERR_INVALID_SIG    2
#define ZIP_ERR_MEM_ALLOC      3

/* SIGNATURES */
#define CENTRAL_FILE_HEADER_SIGNATURE 0x02014b50
#define EOCD_SIGNATURE                0x06054b50

#define EOCD_MAX_COMMENT_LEN 0xFFFF
#define EOCD_FIXED_SIZE      22

struct ZipEntry {
        char* filename;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t compression_method;
        uint32_t local_header_offset;
};

uint8_t zip_read_directory(const int_fast32_t fp, struct ZipEntry* entries[], uint32_t* entry_count);
void zip_free_entries(struct ZipEntry* entries, uint32_t entry_count);

#endif
