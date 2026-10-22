// UDP discovery (TV2). BẢN GIẢ do TV1 tạo để firmware biên dịch được; TV2 viết phần thật.
//
// TODO(TV2):
//  - task riêng: socket(AF_INET, SOCK_DGRAM), bind cổng 5001 (INADDR_ANY), recvfrom
//  - nội dung đúng "ESPEDUSHED_DISCOVER" thì sendto về đúng địa chỉ người gửi:
//      {"name":"espedushed","ip":"<IP hiện tại>","port":5000,"fw":"1.0.0"}
//  - bỏ qua gói khác nội dung, giới hạn tối đa 10 lần trả lời mỗi giây
#include "esp_log.h"
#include "discovery.h"

void mdns_svc_start(void);

void discovery_start(void)
{
    ESP_LOGW("discovery", "UDP discovery chua lam (TV2): client nhap IP thu cong");
    mdns_svc_start();
}
