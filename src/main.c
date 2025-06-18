#include "zip.h"
#include "util.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int
main(int argc, char* argv[])
{
        if (argc != 2) {
                fprintf(stderr, "Use: %s file.zip\n", argv[0]);
                exit(EXIT_FAILURE);
        }

        const char* filename = argv[1];
        if (!has_zip_extension(filename)) {
                fprintf(stderr, "File must be a ZIP file\n");
                exit(EXIT_FAILURE);
        }

        int_fast32_t fd = open(filename, O_RDONLY);
        if (fd == -1) {
                perror("OPEN");
                exit(EXIT_FAILURE);
        }

        uint32_t entry_count = 0;
        struct ZipEntry* entries = NULL;
        if (zip_read_directory(fd, &entries, &entry_count) != 0) {
                close(fd);
                exit(EXIT_FAILURE);
        }

        puts("EOP!");

        close(fd);
        return EXIT_SUCCESS;
}
