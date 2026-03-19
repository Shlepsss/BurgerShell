#include "fs.h"

#include <dos.h>
#include <stdio.h>
#include <string.h>

int fs_list_txt_files(char files[][13], int max_files) {
    struct find_t f;
    int count = 0;

    if (max_files <= 0) return 0;

    if (_dos_findfirst("*.TXT", _A_NORMAL, &f) != 0) {
        if (_dos_findfirst("*.txt", _A_NORMAL, &f) != 0) return 0;
    }

    for (;;) {
        if (count >= max_files) break;
        strncpy(files[count], f.name, 12);
        files[count][12] = '\0';
        count++;
        if (_dos_findnext(&f) != 0) break;
    }

    return count;
}

int fs_load_text(const char* path, char* out, int max_len) {
    FILE* fp;
    int got;

    if (!path || !out || max_len <= 1) return -1;

    fp = fopen(path, "rb");
    if (!fp) return -1;

    got = (int)fread(out, 1, max_len - 1, fp);
    out[got] = '\0';
    fclose(fp);
    return got;
}

int fs_save_text(const char* path, const char* data, int len) {
    FILE* fp;
    int wrote;

    if (!path || !data || len < 0) return -1;

    fp = fopen(path, "wb");
    if (!fp) return -1;

    wrote = (int)fwrite(data, 1, len, fp);
    fclose(fp);
    return (wrote == len) ? 0 : -1;
}

