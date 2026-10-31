// Kiểm tra file sao lưu (TV1). Xem restore_check.h. Không phụ thuộc ESP-IDF, test được trên máy tính.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "restore_check.h"

#define LINE_CAP  2048          // dòng dài nhất mà module đọc JSON Lines chấp nhận (schedule_store.c)
#define WHOLE_MAX (16 * 1024)   // cJSON dựng cả cây trong RAM: không phân tích file một dòng lớn hơn mức này

// Yêu cầu hết chuỗi sau object, để file có rác phía sau không lọt qua
static bool is_object(const char *text, bool need_schema)
{
    cJSON *o = cJSON_ParseWithOpts(text, NULL, 1);
    bool ok = cJSON_IsObject(o)
              && (!need_schema || cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(o, "schema")));
    cJSON_Delete(o);
    return ok;
}

// Dạng một object JSON: users.json, conflict_ack.json, file catalog/schedule của bản cũ
static bool whole_file_valid(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0) return false;
    long size = ftell(f);
    if (size <= 0 || size > WHOLE_MAX || fseek(f, 0, SEEK_SET) != 0) return false;
    char *text = malloc((size_t) size + 1);
    if (!text) return false;
    bool ok = fread(text, 1, (size_t) size, f) == (size_t) size;
    text[size] = 0;
    ok = ok && strlen(text) == (size_t) size && is_object(text, true);    // có byte 0 ở giữa là file nhị phân
    free(text);
    return ok;
}

// Dạng JSON Lines. Trả số dòng đã đọc đúng; -1 nếu dòng đầu chưa đúng dạng này (có thể là object nhiều dòng hoặc rất dài),
// -2 nếu đã qua dòng đầu mà gặp dòng hỏng.
static int check_lines(FILE *f, char *line)
{
    int count = 0;
    long total = 0;
    while (fgets(line, LINE_CAP, f)) {
        size_t n = strlen(line);
        total += (long) n;
        bool complete = n > 0 && (line[n - 1] == '\n' || feof(f));
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (complete && n == 0) continue;                     // dòng trống
        if (!complete || !is_object(line, count == 0)) return count == 0 ? -1 : -2;
        count++;
    }
    if (ferror(f)) return -2;
    if (count == 0) return -1;
    return ftell(f) == total ? count : -2;                    // fgets không báo byte 0 trong dòng: file nhị phân
}

bool restore_file_valid(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char *line = malloc(LINE_CAP);
    bool ok = false;
    if (line) {
        int r = check_lines(f, line);
        free(line);
        ok = r > 0 || (r == -1 && whole_file_valid(f));
    }
    fclose(f);
    return ok;
}
