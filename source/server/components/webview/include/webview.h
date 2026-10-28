// webview.h: trang web chỉ đọc xem lịch trên điện thoại.
//   GET /c/<key>                          trang lịch (HTML nhúng trong firmware)
//   GET /api/<key>/range?from=..&to=..    JSON giống phản hồi calendar.range
// key là 32 ký tự hex ngẫu nhiên lưu NVS; ai có link là xem được lịch, đổi bằng lệnh TCP web.rotate.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void webview_init(void);    // đọc hoặc sinh khoá, đăng ký lệnh web.link và web.rotate; gọi trước platform_start()
void webview_start(void);   // mở HTTP server cổng CONFIG_LD_HTTP_PORT

#ifdef __cplusplus
}
#endif
