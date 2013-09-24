#include <stdio.h>
#include "file_cache.h"

int main(){
    printf("Test file cache\n");
    char *files[4];
    files[0] = "file1";
    files[1] = "file2";
    files[2] = "file3";
    files[3] = "file4";
    files[4] = "file5";

    struct file_cache *cache = file_cache_construct(3);
    file_cache_pin_files(cache, (const char **)files, 3);

    file_cache_mutable_file_data(cache, "file1");
    file_cache_mutable_file_data(cache, "file2");

    file_cache_unpin_files(cache, (const char **)files, 2);
    file_cache_destroy(cache);
    printf("Complete\n");
}

