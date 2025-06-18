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
                if (buff[i]     == 0x50 &&
                    buff[i + 1] == 0x4b &&
                    buff[i + 2] == 0x05 &&
                    buff[i + 3] == 0x06) {
                        return search_start + i;
                }
        }

        return -1;
}

uint8_t
zip_read_directory(const int_fast32_t fp, struct ZipEntry* entries[], uint32_t* entry_count)
{
        off_t eocd_pos = find_eocd(fp);
        if (eocd_pos == -1) {
                fprintf(stderr, "NO EOCD FOUND: Error %d\n", ZIP_ERR_EOCD_NOT_FOUND);
                return ZIP_ERR_EOCD_NOT_FOUND;
        }

        return 0;
}
