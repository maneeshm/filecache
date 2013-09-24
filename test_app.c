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

    struct file_cache *cache = filecacheconstruct(3);
    filecachepinfiles(cache, (const char **)files, 3);

    filecachemutablefiledata(cache, "file1");
    filecachemutablefiledata(cache, "file2");

    filecacheunpinfiles(cache, (const char **)files, 2);
    filecachedestroy(cache);
    printf("Complete\n");
}

