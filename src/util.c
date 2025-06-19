/*
 * util.c -- Utility Functions
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

#include "util.h"

int8_t
has_zip_extension(const char* filename)
{
        const char* ext = strrchr(filename, '.');
        if (!ext || ext == filename)
                return 0;

        return strcmp(ext, ".zip") == 0;
}
