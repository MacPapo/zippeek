#include "util.h"
#include <string.h>

int has_zip_extension(const char* filename)
{
        const char* ext = strrchr(filename, '.');
        if (!ext || ext == filename)
                return 0;

        return strcmp(ext, ".zip") == 0;
}
