/*
 * zip.c -- Zip Reader
 * Copyright (C) 2025 Jacopo Costantini
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zip.h"
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* End of Central Directory */
struct EOCD {
        uint32_t signature;     /* EOCD_SIGNATURE */
        uint16_t this_disk;
        uint16_t central_dir_disk;
        uint16_t total_entries_this_disk;
        uint16_t total_entries;
        uint32_t central_dir_size;
        uint32_t central_dir_offset;
        uint16_t comment_length;
        char* comment;
};

/* Local File Header */
struct LFH {
        uint32_t signature;     /* LFH_SIGNATURE */
        uint16_t version;
        uint16_t bit_flag;
        uint16_t comp_method;
        uint16_t last_mod_file_time;
        uint16_t last_mod_file_date;
        uint32_t crc32;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint16_t file_name_len;
        uint16_t extra_field_len;
        char* file_name;
        char* field;
};

/* Central Directory File Header */
struct CDFH {
        uint32_t signature;
        uint16_t version_made_by;
        uint16_t version_needed;
        uint16_t bit_flag;
        uint16_t comp_method;
        uint16_t last_mod_file_time;
        uint16_t last_mod_file_date;
        uint32_t crc32;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint16_t file_name_len;
        uint16_t extra_field_len;
        uint16_t file_comment_len;
        uint16_t disk_num_start;
        uint16_t internal_file_attr;
        uint32_t external_file_attr;
        uint32_t local_header_offset;
        char* file_name;
        uint8_t* extra_field;
        char* file_comment;
};

const char*
get_zip_error_message(int code) {
    switch (code) {
        case ZIP_OK: return "No error";
        case ZIP_ERR_IO_READ: return "Failed to read data from the file";
        case ZIP_ERR_IO_WRITE: return "Failed to write data to the file";
        case ZIP_ERR_IO_SEEK: return "Failed to change file offset";
        case ZIP_ERR_MEM_ALLOC: return "Memory allocation failed";
        case ZIP_ERR_INVALID_ARG: return "Invalid argument provided";
        case ZIP_ERR_BAD_FILE_DESC: return "Bad file descriptor";
        case ZIP_ERR_FILE_TOO_SMALL: return "ZIP file is too small or invalid";
        case ZIP_ERR_FILE_TRUNCATED: return "File ended prematurely or incomplete read";

        case ZIP_ERR_EOCD_NOT_FOUND: return "End of Central Directory record not found";
        case ZIP_ERR_EOCD_SIGNATURE_BAD: return "Incorrect EOCD signature";
        case ZIP_ERR_EOCD_CORRUPT_FIELDS: return "EOCD fields are corrupted or inconsistent";

        case ZIP_ERR_CENTRAL_DIR_LOC: return "Failed to seek to Central Directory";
        case ZIP_ERR_CENTRAL_DIR_READ: return "Failed to read Central Directory data";
        case ZIP_ERR_CENTRAL_DIR_CORRUPT: return "Central Directory data is corrupted";
        case ZIP_ERR_CD_ENTRY_SIGNATURE_BAD: return "Central Directory entry has incorrect signature";
        case ZIP_ERR_CD_ENTRY_CORRUPT: return "Central Directory entry fields are corrupted";

        case ZIP_ERR_LFH_LOC: return "Failed to seek to Local File Header";
        case ZIP_ERR_LFH_READ: return "Failed to read Local File Header";
        case ZIP_ERR_LFH_SIGNATURE_BAD: return "Local File Header has incorrect signature";
        case ZIP_ERR_LFH_CORRUPT: return "Local File Header fields are corrupted";

        case ZIP_ERR_COMPRESSION_UNSUPPORTED: return "Compression method not supported";
        case ZIP_ERR_DECOMPRESSION_FAILED: return "Decompression failed";

        case ZIP_ERR_GENERIC: return "An unclassified generic error occurred";
        default: return "Unknown ZIP error code";
    }
}

static void
free_cdfh_fields(struct CDFH* cdfh) {
        if (!cdfh)
                return;


        if (cdfh->file_name) {
                free(cdfh->file_name);
                cdfh->file_name = NULL;
        }

        if (cdfh->extra_field) {
                free(cdfh->extra_field);
                cdfh->extra_field = NULL;
        }

        if (cdfh->file_comment) {
                free(cdfh->file_comment);
                cdfh->file_comment = NULL;
        }
}

void
print_buffer(const uint8_t* buf, const size_t size)
{
        for (size_t i = 0; i < size; ++i)
                printf("[%02x]\n", buf[i]);
}

void
print_eocd_comment(const struct EOCD* eocd)
{
        if (eocd->comment) {
                printf("ZIP COMMENT: %s\n\n", eocd->comment);
        }
}

uint8_t
find_eocd(const int_fast32_t fd, off_t* eocd_pos_out)
{
        if (fd < 0) {
                return ZIP_ERR_BAD_FILE_DESC;
        }

        if (!eocd_pos_out) {
                return ZIP_ERR_INVALID_ARG;
        }

        off_t file_pos = lseek(fd, 0, SEEK_END);
        if (file_pos == -1) {
                return ZIP_ERR_IO_SEEK;
        }

        if (file_pos < EOCD_FIXED_SIZE) {
                return ZIP_ERR_FILE_TOO_SMALL;
        }

        off_t search_start = file_pos - EOCD_FIXED_SIZE - EOCD_MAX_COMMENT_LEN;
        if (search_start < 0)
                search_start = 0;

        size_t read_size = file_pos - search_start;
        if (read_size > EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE)
                read_size = EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE;

        uint8_t buf[EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE + 1];

        if (lseek(fd, search_start, SEEK_SET) == -1)
                return ZIP_ERR_IO_SEEK;

        ssize_t bytes_read = read(fd, buf, read_size);
        if (bytes_read < 0)
                return ZIP_ERR_IO_READ;

        if ((size_t)bytes_read < EOCD_FIXED_SIZE)
                return ZIP_ERR_FILE_TRUNCATED;

        for (off_t i = bytes_read - EOCD_FIXED_SIZE; i >= 0; --i) {
                uint32_t sig =  (uint32_t)buf[i]             /* 0x00000050 */
                             | ((uint32_t)buf[i + 1] << 8)   /* 0x00004b00 */
                             | ((uint32_t)buf[i + 2] << 16)  /* 0x00050000 */
                             | ((uint32_t)buf[i + 3] << 24); /* 0x06000000 */


                if (sig == EOCD_SIGNATURE) {
                        *eocd_pos_out = search_start + i;
                        return ZIP_OK;
                }
        }

        return ZIP_ERR_EOCD_NOT_FOUND;
}

uint8_t
read_eocd(const int_fast32_t fp, off_t eocd_pos, struct EOCD* eocd_out)
{
        if (fp < 0)
                return ZIP_ERR_BAD_FILE_DESC;

        if (!eocd_out)
                return ZIP_ERR_INVALID_ARG;

        eocd_out->comment = NULL;
        uint8_t buf[EOCD_FIXED_SIZE];
        if (lseek(fp, eocd_pos, SEEK_SET) == -1)
                return ZIP_ERR_IO_SEEK;

        ssize_t bytes_read = read(fp, buf, EOCD_FIXED_SIZE);
        if (bytes_read != EOCD_FIXED_SIZE)
                return ZIP_ERR_FILE_TRUNCATED;

        eocd_out->signature =
                   (uint32_t)buf[0]
                | ((uint32_t)buf[1] << 8)
                | ((uint32_t)buf[2] << 16)
                | ((uint32_t)buf[3] << 24);
        if (eocd_out->signature != EOCD_SIGNATURE)
                return ZIP_ERR_EOCD_SIGNATURE_BAD;

        eocd_out->this_disk = buf[4] | (buf[5] << 8);
        eocd_out->central_dir_disk = buf[6] | (buf[7] << 8);
        eocd_out->total_entries_this_disk = buf[8] | (buf[9] << 8);
        eocd_out->total_entries = buf[10] | (buf[11] << 8);

        eocd_out->central_dir_size =
                   buf[12]
                | (buf[13] << 8)
                | (buf[14] << 16)
                | (buf[15] << 24);

        eocd_out->central_dir_offset =
                   buf[16]
                | (buf[17] << 8)
                | (buf[18] << 16)
                | (buf[19] << 24);

        eocd_out->comment_length = buf[20] | (buf[21] << 8);
        if (eocd_out->comment_length > 0) {
                eocd_out->comment = malloc(eocd_out->comment_length + 1);
                if (!eocd_out->comment) {
                        return ZIP_ERR_MEM_ALLOC;
                }

                bytes_read = read(fp, eocd_out->comment, eocd_out->comment_length);
                if (bytes_read != eocd_out->comment_length) {
                        free(eocd_out->comment);
                        eocd_out->comment = NULL;
                        return ZIP_ERR_FILE_TRUNCATED;
                }

                eocd_out->comment[eocd_out->comment_length] = '\0';
        } else {
                eocd_out->comment = NULL;
        }

        return ZIP_OK;
}

uint8_t
read_cdfh(const int_fast32_t fp, struct CDFH* cdfh_out)
{
        if (fp < 0)
                return ZIP_ERR_BAD_FILE_DESC;

        if (!cdfh_out)
                return ZIP_ERR_INVALID_ARG;

        cdfh_out->file_name = NULL;
        cdfh_out->extra_field = NULL;
        cdfh_out->file_comment = NULL;

        uint8_t buf[CDFH_FIXED_SIZE];

        ssize_t bytes_read = read(fp, buf, CDFH_FIXED_SIZE);
        if (bytes_read != CDFH_FIXED_SIZE)
                return ZIP_ERR_FILE_TRUNCATED;

        cdfh_out->signature =
                   (uint32_t)buf[0]
                | ((uint32_t)buf[1] << 8)
                | ((uint32_t)buf[2] << 16)
                | ((uint32_t)buf[3] << 24);

        if (cdfh_out->signature != CDFH_SIGNATURE)
                return ZIP_ERR_EOCD_SIGNATURE_BAD;

        cdfh_out->version_made_by = buf[4] | (buf[5] << 8);
        cdfh_out->version_needed  = buf[6] | (buf[7] << 8);

        cdfh_out->bit_flag = buf[8] | (buf[9] << 8);
        cdfh_out->comp_method = buf[10] | (buf[11] << 8);

        cdfh_out->last_mod_file_time = buf[12] | (buf[13] << 8);
        cdfh_out->last_mod_file_date = buf[14] | (buf[15] << 8);

        cdfh_out->crc32 =
                   (uint32_t)buf[16]
                | ((uint32_t)buf[17] << 8)
                | ((uint32_t)buf[18] << 16)
                | ((uint32_t)buf[19] << 24);

        cdfh_out->comp_size =
                   (uint32_t)buf[20]
                | ((uint32_t)buf[21] << 8)
                | ((uint32_t)buf[22] << 16)
                | ((uint32_t)buf[23] << 24);

        cdfh_out->uncomp_size =
                   (uint32_t)buf[24]
                | ((uint32_t)buf[25] << 8)
                | ((uint32_t)buf[26] << 16)
                | ((uint32_t)buf[27] << 24);

        cdfh_out->file_name_len = buf[28] | (buf[29] << 8);
        cdfh_out->extra_field_len = buf[30] | (buf[31] << 8);
        cdfh_out->file_comment_len = buf[32] | (buf[33] << 8);
        cdfh_out->disk_num_start = buf[34] | (buf[35] << 8);
        cdfh_out->internal_file_attr = buf[36] | (buf[37] << 8);

        cdfh_out->external_file_attr =
                   (uint32_t)buf[38]
                | ((uint32_t)buf[39] << 8)
                | ((uint32_t)buf[40] << 16)
                | ((uint32_t)buf[41] << 24);

        cdfh_out->local_header_offset =
                   (uint32_t)buf[42]
                | ((uint32_t)buf[43] << 8)
                | ((uint32_t)buf[44] << 16)
                | ((uint32_t)buf[45] << 24);

        if (cdfh_out->file_name_len > 0) {
                cdfh_out->file_name = malloc(cdfh_out->file_name_len + 1);
                if (!cdfh_out->file_name) {
                        free_cdfh_fields(cdfh_out);
                        return ZIP_ERR_MEM_ALLOC;
                }

                bytes_read = read(fp, cdfh_out->file_name, cdfh_out->file_name_len);
                if (bytes_read != cdfh_out->file_name_len) {
                        free_cdfh_fields(cdfh_out);

                        return ZIP_ERR_FILE_TRUNCATED;
                }
                cdfh_out->file_name[cdfh_out->file_name_len] = '\0';
        }

        if (cdfh_out->extra_field_len > 0) {
                cdfh_out->extra_field = malloc(cdfh_out->extra_field_len);
                if (!cdfh_out->extra_field) {
                        free_cdfh_fields(cdfh_out);

                        return ZIP_ERR_MEM_ALLOC;
                }

                bytes_read = read(fp, cdfh_out->extra_field, cdfh_out->extra_field_len);
                if (bytes_read != cdfh_out->extra_field_len) {
                        free_cdfh_fields(cdfh_out);

                        return ZIP_ERR_FILE_TRUNCATED;
                }
        }

        if (cdfh_out->file_comment_len > 0) {
                cdfh_out->file_comment = malloc(cdfh_out->file_comment_len + 1);
                if (!cdfh_out->file_comment) {
                        free_cdfh_fields(cdfh_out);
                        return ZIP_ERR_MEM_ALLOC;
                }

                bytes_read = read(fp, cdfh_out->file_comment, cdfh_out->file_comment_len);
                if (bytes_read != cdfh_out->file_comment_len) {
                        free_cdfh_fields(cdfh_out);

                        return ZIP_ERR_FILE_TRUNCATED;
                }
                cdfh_out->file_comment[cdfh_out->file_comment_len] = '\0';
        }

        /*
         * print_buffer(buf, CDFH_FIXED_SIZE); /\* PRINT BUFFER DEBUG *\/
         */

        return ZIP_OK;
}

uint8_t
zip_read_directory(const int_fast32_t fp, struct ZipEntry*** entries, uint32_t* entry_count)
{
        if (fp < 0) {
                fprintf(stderr, "Error: Invalid file descriptor provided.\n");
                return ZIP_ERR_BAD_FILE_DESC;
        }

        off_t eocd_pos;
        uint8_t err = find_eocd(fp, &eocd_pos);
        if (err != ZIP_OK) {
                fprintf(stderr, "Error finding EOCD: %s\n", get_zip_error_message(err));
                if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_FILE_TRUNCATED)
                        fprintf(stderr, "  System error details: %s\n", strerror(errno));

                return (uint8_t)err;
        }

        struct EOCD eocd;
        eocd.comment = NULL;

        err = read_eocd(fp, eocd_pos, &eocd);
        if (err != ZIP_OK) {
                fprintf(stderr, "Error reading EOCD at offset %ld: %s\n", (long)eocd_pos, get_zip_error_message(err));
                if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_FILE_TRUNCATED)
                        fprintf(stderr, "  System error details: %s\n", strerror(errno));

                if (eocd.comment) {
                        free(eocd.comment);
                        eocd.comment = NULL;
                }

                return (uint8_t)err;
        }

        print_eocd_comment(&eocd); /* PRINT COMMENT IF EXISTS */

        *entry_count = eocd.total_entries;
        *entries = calloc((*entry_count), sizeof(struct ZipEntry*));
        if (!(*entries)) {
                fprintf(stderr, "Error: Memory allocation failed for ZIP entries array.\n");
                return (uint8_t)ZIP_ERR_MEM_ALLOC;
        }

        if (lseek(fp, eocd.central_dir_offset, SEEK_SET) == -1) {
                fprintf(stderr, "Error seeking to Central Directory at offset %u: %s\n", eocd.central_dir_offset, strerror(errno));

                return (uint8_t)ZIP_ERR_CENTRAL_DIR_LOC;
        }

        size_t k = 0;
        for (uint32_t i = 0; i < eocd.total_entries; ++i) {
                struct CDFH cdfh;

                cdfh.file_name = NULL;
                cdfh.extra_field = NULL;
                cdfh.file_comment = NULL;

                err = read_cdfh(fp, &cdfh);
                if (err != ZIP_OK) {
                        fprintf(stderr, "Error reading Central Directory entry %u: %s\n", i, get_zip_error_message(err));
                        if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_FILE_TRUNCATED) {
                                fprintf(stderr, "  System error details: %s\n", strerror(errno));
                        }

                        free_cdfh_fields(&cdfh);
                        return (uint8_t)err;
                }

                (*entries)[k] = malloc(sizeof(struct ZipEntry));
                if (!(*entries)[k]) {
                        fprintf(stderr, "Error: Memory allocation failed for ZipEntry %ld.\n", k);
                        zip_free_entries(entries, *entry_count);
                        free_cdfh_fields(&cdfh);

                        return (uint8_t)ZIP_ERR_MEM_ALLOC;
                }

                (*entries)[k]->file_name = NULL;
                (*entries)[k]->file_name = malloc(cdfh.file_name_len * sizeof(char) + 1);
                if (!(*entries)[k]->file_name) {
                    fprintf(stderr, "Error: Memory allocation failed for file name of entry %ld.\n", k);
                    zip_free_entries(entries, *entry_count);
                    free_cdfh_fields(&cdfh);

                    return (uint8_t)ZIP_ERR_MEM_ALLOC;
                }

                strcpy((*entries)[k]->file_name, cdfh.file_name);
                (*entries)[k]->compressed_size = cdfh.comp_size;
                (*entries)[k]->uncompressed_size = cdfh.uncomp_size;
                (*entries)[k]->compression_method = cdfh.comp_method;
                (*entries)[k]->local_header_offset = cdfh.local_header_offset;
                (*entries)[k]->crc32 = cdfh.crc32;
                (*entries)[k]->general_purpose_bit_flag = cdfh.bit_flag;

                free_cdfh_fields(&cdfh);
                ++k;
        }

        free(eocd.comment);
        return ZIP_OK;
}

void
zip_free_entries(struct ZipEntry*** entries, const uint32_t entry_count)
{
        for (size_t i = 0; i < entry_count; ++i) {
                if ((*entries)[i] != NULL) {
                        free((*entries)[i]->file_name);
                        (*entries)[i]->file_name = NULL;

                        free((*entries)[i]);
                }
        }

        free(*entries);
        entries = NULL;
}
