#ifndef FS_H
#define FS_H

// Возвращает количество найденных файлов .TXT (8.3), максимум max_files.
// files[i] заполняется именем (до 12 символов + '\0').
int fs_list_txt_files(char files[][13], int max_files);

// Загружает текстовый файл в буфер (макс. max_len-1), добавляет '\0'.
// Возвращает длину (байт), либо -1 при ошибке.
int fs_load_text(const char* path, char* out, int max_len);

// Сохраняет текстовый файл (len байт из data).
// Возвращает 0 при успехе, иначе -1.
int fs_save_text(const char* path, const char* data, int len);

#endif

