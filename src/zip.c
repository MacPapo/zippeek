#include "zip.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

struct EOCD {
        uint32_t signature;
        uint16_t this_disk;
        uint16_t central_dir_disk;
        uint16_t total_entries_this_disk;
        uint16_t total_entries;
        uint32_t central_dir_size;
        uint32_t central_dir_offset;
        uint16_t comment_length;
        char* comment;
};

off_t
find_eocd(const int_fast32_t fd)
{
        off_t file_pos = lseek(fd, 0, SEEK_END);
        if (file_pos == -1) {
                perror("LSEEK (SEEK_END)");
                exit(EXIT_FAILURE);
        }

        off_t search_start = file_pos - EOCD_FIXED_SIZE - EOCD_MAX_COMMENT_LEN;
        if (search_start < 0)
                search_start = 0;

        size_t read_size = file_pos - search_start;
        if (read_size > EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE)
                read_size = EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE;

        unsigned char buff[EOCD_MAX_COMMENT_LEN + EOCD_FIXED_SIZE + 1];

        if (lseek(fd, search_start, SEEK_SET) == -1) {
                perror("LSEEK (SEEK_SET)");
                exit(EXIT_FAILURE);
        }

        ssize_t bytes_read = read(fd, buff, read_size);
        if (bytes_read < 0) {
                perror("READ");
                exit(EXIT_FAILURE);
        }

        for (off_t i = bytes_read - EOCD_FIXED_SIZE; i >= 0; --i) {
                uint32_t sig =  (uint32_t)buff[i]             /* 0x00000050 */
                             | ((uint32_t)buff[i + 1] << 8)   /* 0x00004b00 */
                             | ((uint32_t)buff[i + 2] << 16)  /* 0x00050000 */
                             | ((uint32_t)buff[i + 3] << 24); /* 0x06000000 */


                if (sig == EOCD_SIGNATURE)
                        return search_start + i;
        }

        return -1;
}

int8_t
read_eocd(const int_fast32_t fp, off_t eocd_pos, struct EOCD* eocd_out)
{
        unsigned char buff[EOCD_FIXED_SIZE];

        if (lseek(fp, eocd_pos, SEEK_SET) == -1) {
                perror("lseek to EOCD");
                return -1;
        }

        if (read(fp, buff, EOCD_FIXED_SIZE) != EOCD_FIXED_SIZE) {
                perror("read EOCD");
                return -1;
        }

        eocd_out->signature =
                   (uint32_t)buff[0]
                | ((uint32_t)buff[1] << 8)
                | ((uint32_t)buff[2] << 16)
                | ((uint32_t)buff[3] << 24);

        eocd_out->this_disk = buff[4] | (buff[5] << 8);
        eocd_out->central_dir_disk = buff[6] | (buff[7] << 8);
        eocd_out->total_entries_this_disk = buff[8] | (buff[9] << 8);
        eocd_out->total_entries = buff[10] | (buff[11] << 8);

        eocd_out->central_dir_size =
                   buff[12]
                | (buff[13] << 8)
                | (buff[14] << 16)
                | (buff[15] << 24);

        eocd_out->central_dir_offset =
                   buff[16]
                | (buff[17] << 8)
                | (buff[18] << 16)
                | (buff[19] << 24);

        eocd_out->comment_length = buff[20] | (buff[21] << 8);

        if (eocd_out->comment_length > 0) {
                eocd_out->comment = malloc(eocd_out->comment_length + 1);
                if (!eocd_out->comment) {
                        perror("malloc comment");
                        return -1;
                }

                if (read(fp, eocd_out->comment, eocd_out->comment_length) != eocd_out->comment_length) {
                        perror("read comment");
                        free(eocd_out->comment);
                        return -1;
                }

                eocd_out->comment[eocd_out->comment_length] = '\0';
        } else {
                eocd_out->comment = NULL;
        }

        return 0;
}

uint8_t
zip_read_directory(const int_fast32_t fp, struct ZipEntry* entries[], uint32_t* entry_count)
{
        off_t eocd_pos = find_eocd(fp);
        if (eocd_pos == -1) {
                fprintf(stderr, "NO EOCD FOUND: Error %d\n", ZIP_ERR_EOCD_NOT_FOUND);
                return ZIP_ERR_EOCD_NOT_FOUND;
        }

        struct EOCD eocd;
        if (read_eocd(fp, eocd_pos, &eocd) != 0) {
                fprintf(stderr, "Failed to read EOCD\n");
                return ZIP_ERR_EOCD_INVALID;
        }

        if (eocd.comment) {
                free(eocd.comment);
                eocd.comment = NULL;
        }

        return 0;
}
