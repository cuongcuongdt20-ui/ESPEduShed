// discovery.h: hợp đồng của TV2 với app_main (TV1).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Mở dịch vụ tìm server: UDP cổng 5001 trả lời "ESPEDUSHED_DISCOVER", và mDNS espedushed.local (_espedushed._tcp).
// Không chặn: tạo task riêng rồi trả về ngay.
void discovery_start(void);

#ifdef __cplusplus
}
#endif
