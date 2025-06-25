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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
        case ZIP_ERR_BAD_BUFFER: return "Bad file descriptor";
        case ZIP_ERR_BUFFER_TOO_SMALL: return "ZIP file is too small or invalid";
        case ZIP_ERR_BUFFER_TRUNCATED: return "File ended prematurely or incomplete read";

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

static uint8_t
find_eocd(const uint8_t* zip_buffer, size_t zip_size, off_t* eocd_pos_out)
{
        if (zip_buffer == NULL) {
                return ZIP_ERR_BAD_BUFFER;
        }

        if (!eocd_pos_out) {
                return ZIP_ERR_INVALID_ARG;
        }


        if (zip_size < EOCD_FIXED_SIZE) {
                return ZIP_ERR_BUFFER_TOO_SMALL;
        }

        off_t search_start_offset;
        if (zip_size > (off_t)EOCD_FIXED_SIZE + EOCD_MAX_COMMENT_LEN) {
                search_start_offset = zip_size - EOCD_FIXED_SIZE - EOCD_MAX_COMMENT_LEN;
        } else {
                search_start_offset = 0;
        }

        for (off_t i = zip_size - EOCD_FIXED_SIZE; i >= search_start_offset; --i) {

                if ((size_t)(i + 3) >= zip_size)
                        continue;

                uint32_t sig =  (uint32_t)zip_buffer[i]             /* 0x00000050 */
                             | ((uint32_t)zip_buffer[i + 1] << 8)   /* 0x00004b00 */
                             | ((uint32_t)zip_buffer[i + 2] << 16)  /* 0x00050000 */
                             | ((uint32_t)zip_buffer[i + 3] << 24); /* 0x06000000 */


                if (sig == EOCD_SIGNATURE) {
                        *eocd_pos_out = i;
                        return ZIP_OK;
                }
        }

        return ZIP_ERR_EOCD_NOT_FOUND;
}

static uint8_t
read_eocd(const uint8_t* zip_buffer, const size_t zip_size, const off_t eocd_pos, struct EOCD* eocd_out)
{
        if (zip_buffer == NULL)
                return ZIP_ERR_BAD_BUFFER;

        if (eocd_out == NULL)
                return ZIP_ERR_INVALID_ARG;

        if (eocd_pos < 0 || (size_t)eocd_pos + EOCD_FIXED_SIZE > zip_size) {
                return ZIP_ERR_BUFFER_TRUNCATED;
        }

        const uint8_t* eocd_ptr = zip_buffer + eocd_pos;
        eocd_out->signature =  (uint32_t)eocd_ptr[0]
                            | ((uint32_t)eocd_ptr[1] << 8)
                            | ((uint32_t)eocd_ptr[2] << 16)
                            | ((uint32_t)eocd_ptr[3] << 24);
        if (eocd_out->signature != EOCD_SIGNATURE)
                return ZIP_ERR_EOCD_SIGNATURE_BAD;

        eocd_out->this_disk =  (uint16_t)eocd_ptr[4]
                            | ((uint16_t)eocd_ptr[5] << 8);

        eocd_out->central_dir_disk =  (uint16_t)eocd_ptr[6]
                                   | ((uint16_t)eocd_ptr[7] << 8);

        eocd_out->total_entries_this_disk =  (uint16_t)eocd_ptr[8]
                                          | ((uint16_t)eocd_ptr[9] << 8);

        eocd_out->total_entries =  (uint16_t)eocd_ptr[10]
                                | ((uint16_t)eocd_ptr[11] << 8);

        eocd_out->central_dir_size =  (uint32_t)eocd_ptr[12]
                                   | ((uint32_t)eocd_ptr[13] << 8)
                                   | ((uint32_t)eocd_ptr[14] << 16)
                                   | ((uint32_t)eocd_ptr[15] << 24);

        eocd_out->central_dir_offset =  (uint32_t)eocd_ptr[16]
                                     | ((uint32_t)eocd_ptr[17] << 8)
                                     | ((uint32_t)eocd_ptr[18] << 16)
                                     | ((uint32_t)eocd_ptr[19] << 24);

        eocd_out->comment_length =  (uint16_t)eocd_ptr[20]
                                 | ((uint16_t)eocd_ptr[21] << 8);

        return ZIP_OK;
}

static uint32_t
fast_cdfh_va_params_sum(const uint8_t* zip_buffer, const size_t zip_size, const off_t cdfh_pos)
{
        if (zip_buffer == NULL)
                return ZIP_ERR_BAD_BUFFER;

        if (cdfh_pos < 0 || (size_t)cdfh_pos + CDFH_FIXED_SIZE > zip_size) {
                fprintf(stderr, "Error: CDFH position %ld is out of bounds or buffer is too small (size %zu) for fixed part in fast_cdfh_va_file_name.\n",
                        (long)cdfh_pos, zip_size);
                return ZIP_ERR_BAD_BUFFER;
        }

        const uint8_t* cdfh_ptr = zip_buffer + cdfh_pos;
        const uint16_t file_name_len =  cdfh_ptr[28]
                                     | (cdfh_ptr[29] << 8);

        const uint16_t extra_field_len =  cdfh_ptr[30]
                                       | (cdfh_ptr[31] << 8);

        const uint16_t file_comment_len =  cdfh_ptr[32]
                                        | (cdfh_ptr[33] << 8);

        return file_name_len + extra_field_len + file_comment_len;
}

static uint16_t
fast_cdfh_va_file_name(const uint8_t* zip_buffer, const size_t zip_size, const off_t cdfh_pos)
{
        if (zip_buffer == NULL)
                return ZIP_ERR_BAD_BUFFER;

        if (cdfh_pos < 0 || (size_t)cdfh_pos + CDFH_FIXED_SIZE > zip_size) {
                fprintf(stderr, "Error: CDFH position %ld is out of bounds or buffer is too small (size %zu) for fixed part in fast_cdfh_va_file_name.\n",
                        (long)cdfh_pos, zip_size);
                return ZIP_ERR_BAD_BUFFER;
        }

        const uint8_t* cdfh_ptr = zip_buffer + cdfh_pos;
        const uint16_t file_name_len =  cdfh_ptr[28]
                                     | (cdfh_ptr[29] << 8);

        return file_name_len;
}

static uint8_t
read_cdfh(const uint8_t* zip_buffer, const size_t zip_size, const off_t cdfh_pos, struct CDFH* cdfh_out)
{
        if (zip_buffer == NULL)
                return ZIP_ERR_BAD_BUFFER;

        if (cdfh_out == NULL)
                return ZIP_ERR_INVALID_ARG;

        if (cdfh_pos < 0 || (size_t)cdfh_pos + CDFH_FIXED_SIZE > zip_size) {
                return ZIP_ERR_BUFFER_TRUNCATED;
        }

        const uint8_t* cdfh_ptr = zip_buffer + cdfh_pos;

        cdfh_out->file_name    = NULL;
        cdfh_out->signature =  (uint32_t)cdfh_ptr[0]
                            | ((uint32_t)cdfh_ptr[1] << 8)
                            | ((uint32_t)cdfh_ptr[2] << 16)
                            | ((uint32_t)cdfh_ptr[3] << 24);
        if (cdfh_out->signature != CDFH_SIGNATURE)
                return ZIP_ERR_CD_ENTRY_SIGNATURE_BAD;

        cdfh_out->version_made_by =  cdfh_ptr[4]
                                  | (cdfh_ptr[5] << 8);

        cdfh_out->version_needed =  cdfh_ptr[6]
                                 | (cdfh_ptr[7] << 8);

        cdfh_out->bit_flag =  cdfh_ptr[8]
                           | (cdfh_ptr[9] << 8);

        cdfh_out->comp_method =  cdfh_ptr[10]
                              | (cdfh_ptr[11] << 8);

        cdfh_out->last_mod_file_time =  cdfh_ptr[12]
                                     | (cdfh_ptr[13] << 8);

        cdfh_out->last_mod_file_date =  cdfh_ptr[14]
                                     | (cdfh_ptr[15] << 8);

        cdfh_out->crc32 =  (uint32_t)cdfh_ptr[16]
                        | ((uint32_t)cdfh_ptr[17] << 8)
                        | ((uint32_t)cdfh_ptr[18] << 16)
                        | ((uint32_t)cdfh_ptr[19] << 24);

        cdfh_out->comp_size =  (uint32_t)cdfh_ptr[20]
                            | ((uint32_t)cdfh_ptr[21] << 8)
                            | ((uint32_t)cdfh_ptr[22] << 16)
                            | ((uint32_t)cdfh_ptr[23] << 24);

        cdfh_out->uncomp_size =  (uint32_t)cdfh_ptr[24]
                              | ((uint32_t)cdfh_ptr[25] << 8)
                              | ((uint32_t)cdfh_ptr[26] << 16)
                              | ((uint32_t)cdfh_ptr[27] << 24);

        cdfh_out->file_name_len =  cdfh_ptr[28]
                                | (cdfh_ptr[29] << 8);

        cdfh_out->extra_field_len =  cdfh_ptr[30]
                                  | (cdfh_ptr[31] << 8);

        cdfh_out->file_comment_len =  cdfh_ptr[32]
                                   | (cdfh_ptr[33] << 8);

        cdfh_out->disk_num_start =  cdfh_ptr[34]
                                 | (cdfh_ptr[35] << 8);

        cdfh_out->internal_file_attr =  cdfh_ptr[36]
                                     | (cdfh_ptr[37] << 8);

        cdfh_out->external_file_attr =  (uint32_t)cdfh_ptr[38]
                                     | ((uint32_t)cdfh_ptr[39] << 8)
                                     | ((uint32_t)cdfh_ptr[40] << 16)
                                     | ((uint32_t)cdfh_ptr[41] << 24);

        cdfh_out->local_header_offset =  (uint32_t)cdfh_ptr[42]
                                      | ((uint32_t)cdfh_ptr[43] << 8)
                                      | ((uint32_t)cdfh_ptr[44] << 16)
                                      | ((uint32_t)cdfh_ptr[45] << 24);

        if (cdfh_out->file_name_len > 0) {
                const off_t file_name_offset = cdfh_pos + CDFH_FIXED_SIZE;
                if ((size_t)file_name_offset + cdfh_out->file_name_len > zip_size)
                        return ZIP_ERR_BUFFER_TRUNCATED;

                cdfh_out->file_name = malloc(cdfh_out->file_name_len + 1);
                if (!cdfh_out->file_name)
                        return ZIP_ERR_MEM_ALLOC;

                memcpy(cdfh_out->file_name, zip_buffer + file_name_offset, cdfh_out->file_name_len);
                cdfh_out->file_name[cdfh_out->file_name_len] = '\0';
        }

        return ZIP_OK;
}

struct ZipEntry*
zip_read_directory(const int_fast32_t fp, uint32_t* entry_count)
{
        if (fp < 0 || entry_count == NULL) {
                fprintf(stderr, "Error: Invalid file descriptor provided.\n");
                return NULL;
        }

        struct stat st;
        if (fstat(fp, &st) == -1) {
                perror("fstat");

                return NULL;
        }

        off_t file_size = st.st_size;
        if (file_size == 0) {
                *entry_count = 0;

                return NULL;
        }

        uint8_t* mapped_zip_file = (uint8_t*)mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fp, 0);
        if (mapped_zip_file == MAP_FAILED) {
                perror("mmap");

                return NULL;
        }
        close(fp);

        off_t eocd_pos;
        uint8_t err = find_eocd(mapped_zip_file, file_size, &eocd_pos);
        if (err != ZIP_OK) {
                fprintf(stderr, "Error finding EOCD: %s\n", get_zip_error_message(err));
                if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_BUFFER_TRUNCATED)
                        fprintf(stderr, "  System error details: %s\n", strerror(errno));

                return NULL;
        }

         struct EOCD eocd;
         err = read_eocd(mapped_zip_file, file_size, eocd_pos, &eocd);
         if (err != ZIP_OK) {
                 fprintf(stderr, "Error reading EOCD at offset %ld: %s\n", (long)eocd_pos, get_zip_error_message(err));
                 if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_BUFFER_TRUNCATED)
                         fprintf(stderr, "  System error details: %s\n", strerror(errno));

                 return NULL;
         }

         *entry_count = eocd.total_entries;
         if (*entry_count == 0) {
                 munmap(mapped_zip_file, file_size);
                 return NULL;
         }

         size_t total_va_file_name_size = 0;
         size_t total_zip_entries_size = (size_t)(*entry_count) * sizeof(struct ZipEntry);

         off_t current_cdfh_offset = eocd.central_dir_offset;
         for (uint32_t i = 0; i < *entry_count; ++i) {
                 if (current_cdfh_offset < 0 || (size_t)current_cdfh_offset + CDFH_FIXED_SIZE > (size_t)file_size) {
                         fprintf(stderr, "Error: Central Directory entry %u offset (%ld) is out of bounds or too small.\n", i, (long)current_cdfh_offset);
                         munmap(mapped_zip_file, file_size);
                         return NULL;
                 }

                 uint32_t va_params_len = fast_cdfh_va_params_sum(mapped_zip_file, file_size, current_cdfh_offset);
                 total_va_file_name_size += fast_cdfh_va_file_name(mapped_zip_file, file_size, current_cdfh_offset) + 1; /* Plus 1 for NULL terminator */

                 current_cdfh_offset += va_params_len + CDFH_FIXED_SIZE;
                 if (current_cdfh_offset > eocd.central_dir_offset + eocd.central_dir_size) {
                         fprintf(stderr, "Error: Central Directory parsing error, offset exceeds bounds.\n");
                         munmap(mapped_zip_file, file_size);
                         return NULL;
                 }
         }

         struct ZipEntry* entries = malloc(total_zip_entries_size + total_va_file_name_size);
         if (!entries) {
                 perror("malloc for zip entries and data");
                 munmap(mapped_zip_file, file_size);
                 return NULL;
         }

         current_cdfh_offset = eocd.central_dir_offset;
         uint8_t* va_params_data_ptr = (uint8_t*)entries + total_zip_entries_size;
         for (uint32_t i = 0; i < *entry_count; ++i) {
                 struct CDFH cdfh;
                 cdfh.file_name = NULL;

                 err = read_cdfh(mapped_zip_file, file_size, current_cdfh_offset, &cdfh);
                 if (err != ZIP_OK) {
                         fprintf(stderr, "Error reading Central Directory entry %u: %s\n", i, get_zip_error_message(err));
                         if (err >= ZIP_ERR_IO_READ && err <= ZIP_ERR_BUFFER_TRUNCATED) {
                                 fprintf(stderr, "  System error details: %s\n", strerror(errno));
                         }

                         free(cdfh.file_name);
                         cdfh.file_name = NULL;

                         munmap(mapped_zip_file, file_size);
                         return NULL;
                 }

                 entries[i].compressed_size          = cdfh.comp_size;
                 entries[i].uncompressed_size        = cdfh.uncomp_size;
                 entries[i].compression_method       = cdfh.comp_method;
                 entries[i].local_header_offset      = cdfh.local_header_offset;
                 entries[i].crc32                    = cdfh.crc32;
                 entries[i].general_purpose_bit_flag = cdfh.bit_flag;

                 if (cdfh.file_name_len > 0) {
                         entries[i].file_name = (char*)va_params_data_ptr;
                         memcpy(entries[i].file_name, cdfh.file_name, cdfh.file_name_len);
                         entries[i].file_name[cdfh.file_name_len] = '\0';
                         va_params_data_ptr += cdfh.file_name_len + 1;
                 }

                 free(cdfh.file_name);
                 cdfh.file_name = NULL;

                 current_cdfh_offset += CDFH_FIXED_SIZE + cdfh.file_name_len + cdfh.extra_field_len + cdfh.file_comment_len;
         }

         munmap(mapped_zip_file, file_size);
         return entries;
}
