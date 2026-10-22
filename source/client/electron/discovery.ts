// Tìm ESP32 trong mạng LAN bằng UDP broadcast (TV2). BẢN GIẢ do TV1 tạo để app chạy được;
// trong lúc chờ, màn hình kết nối cho nhập IP bằng tay.
//
// TODO(TV2): dgram.createSocket('udp4'), setBroadcast(true), gửi "ESPEDUSHED_DISCOVER" tới 255.255.255.255:5001
//            (và địa chỉ broadcast của từng card mạng trong os.networkInterfaces()), gom phản hồi
//            {name, ip, port, fw} trong timeoutMs, bỏ trùng theo ip.
import type { ServerInfo } from './shared'

export async function discover(timeoutMs = 1500): Promise<ServerInfo[]> {
  void timeoutMs
  return []
}
