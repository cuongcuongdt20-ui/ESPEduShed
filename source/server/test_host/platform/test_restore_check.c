// Test kiểm tra file sao lưu của system.restore (TV1). Chạy: make restore_check
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "restore_check.h"
#include "test.h"

#define TMP "build/restore_check.tmp"

static bool check_bytes(const char *data, size_t n)
{
    FILE *f = fopen(TMP, "wb");
    if (!f) return false;
    fwrite(data, 1, n, f);
    fclose(f);
    return restore_file_valid(TMP);
}

static bool check(const char *text)
{
    return check_bytes(text, strlen(text));
}

TEST(json_lines_cua_schedule_va_catalog)
{
    CHECK(check("{\"schema\":2,\"nextId\":11}\n{\"k\":\"series\",\"id\":1}\n{\"k\":\"task\",\"id\":9}\n"));
}

TEST(json_lines_chi_co_header)
{
    CHECK(check("{\"schema\":2,\"nextId\":1}\n"));
}

TEST(mot_object_cua_users_va_ack)
{
    CHECK(check("{\"schema\":2,\"user\":{\"name\":\"admin\",\"iter\":10000}}"));
    CHECK(check("{\"schema\":1,\"acks\":[]}"));
}

TEST(xuong_dong_windows_va_dong_trong)
{
    CHECK(check("{\"schema\":2}\r\n\r\n{\"k\":\"task\"}\r\n\n"));
}

TEST(object_nhieu_dong)
{
    CHECK(check("{\n  \"schema\": 1,\n  \"users\": []\n}\n"));
}

TEST(object_mot_dong_dai_hon_bo_dem_dong)
{
    // conflict_ack.json đủ 128 xác nhận dài hơn 2048 byte mà vẫn hợp lệ
    size_t cap = 6000;
    char *s = malloc(cap);
    size_t n = (size_t) snprintf(s, cap, "{\"schema\":1,\"acks\":[");
    for (int i = 0; i < 128; i++)
        n += (size_t) snprintf(s + n, cap - n, "%s{\"a\":%d,\"b\":%d}", i ? "," : "", i, i + 1);
    snprintf(s + n, cap - n, "]}");
    CHECK(strlen(s) > 2048);
    CHECK(check(s));
    free(s);
}

TEST(file_rong_hoac_khong_co)
{
    CHECK(!check(""));
    CHECK(!check("\n\n"));
    CHECK(!restore_file_valid("build/khong_co_file_nay.json"));
}

TEST(file_bi_cat_giua_chung)
{
    CHECK(!check("{\"schema\":2,\"user\":{\"name\":\"adm"));
    CHECK(!check("{\"schema\":2}\n{\"k\":\"series\",\"id\":1}\n{\"k\":\"ta"));
}

TEST(dong_hong_o_giua_khong_duoc_coi_la_object_nhieu_dong)
{
    CHECK(!check("{\"schema\":2}\nday khong phai json\n{\"k\":\"task\"}\n"));
}

TEST(dong_sau_khong_phai_object)
{
    CHECK(!check("{\"schema\":2}\n[1,2,3]\n"));
    CHECK(!check("{\"schema\":2}\n123\n"));
}

TEST(thieu_hoac_sai_truong_schema)
{
    CHECK(!check("{\"user\":{}}"));
    CHECK(!check("{\"schema\":\"2\"}"));
    CHECK(!check("{\"k\":\"series\"}\n{\"schema\":2}\n"));
}

TEST(khong_phai_object)
{
    CHECK(!check("[{\"schema\":2}]"));
    CHECK(!check("xin chao"));
}

TEST(rac_sau_object)
{
    CHECK(!check("{\"schema\":2} rac"));
    CHECK(!check("{\"schema\":2}{\"schema\":2}"));
}

TEST(file_nhi_phan)
{
    static const char bin[] = {'{', '"', 's', 'c', 'h', 'e', 'm', 'a', '"', ':', '2', '}', 0, 'x', 'y'};
    CHECK(!check_bytes(bin, sizeof(bin)));
    static const char junk[] = {(char) 0xFF, (char) 0xD8, 0, 1, 2, 3};
    CHECK(!check_bytes(junk, sizeof(junk)));
}

TEST(dong_dai_hon_gioi_han_sau_header)
{
    size_t n = 3000;
    char *s = malloc(n + 32);
    int h = sprintf(s, "{\"schema\":2}\n{\"k\":\"");
    memset(s + h, 'a', n - (size_t) h);
    strcpy(s + n, "\"}\n");
    CHECK(!check(s));
    free(s);
}

TEST(object_mot_dong_qua_16_kb)
{
    size_t n = 17 * 1024;
    char *s = malloc(n + 8);
    int h = sprintf(s, "{\"schema\":1,\"x\":\"");
    memset(s + h, 'a', n - (size_t) h);
    strcpy(s + n, "\"}");
    CHECK(!check(s));
    free(s);
}

int main(void)
{
    RUN(json_lines_cua_schedule_va_catalog);
    RUN(json_lines_chi_co_header);
    RUN(mot_object_cua_users_va_ack);
    RUN(xuong_dong_windows_va_dong_trong);
    RUN(object_nhieu_dong);
    RUN(object_mot_dong_dai_hon_bo_dem_dong);
    RUN(file_rong_hoac_khong_co);
    RUN(file_bi_cat_giua_chung);
    RUN(dong_hong_o_giua_khong_duoc_coi_la_object_nhieu_dong);
    RUN(dong_sau_khong_phai_object);
    RUN(thieu_hoac_sai_truong_schema);
    RUN(khong_phai_object);
    RUN(rac_sau_object);
    RUN(file_nhi_phan);
    RUN(dong_dai_hon_gioi_han_sau_header);
    RUN(object_mot_dong_qua_16_kb);
    remove(TMP);
    return REPORT();
}
