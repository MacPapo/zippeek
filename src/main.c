/*
 * main.c -- Main Zipper
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
