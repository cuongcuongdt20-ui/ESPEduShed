# NETWORK PROGRAMMING – FINAL PROJECT
## ESPEduShed: Máy chủ Quản lý Công việc và Lịch Giảng dạy Mini trên ESP32-S3 & TCP Socket

> **Academic Year 2026–2027**  
> **Course:** Network Programming (Lập trình mạng)

---

# 1. Project Information

## 1.1. Project Name

**ESPEduShed** (*Embedded Smart Personal Education Scheduler*) – Máy chủ quản lý công việc và lịch giảng dạy mini sử dụng vi điều khiển ESP32-S3 và giao thức TCP Socket tùy biến.

## 1.2. Topic

**Custom Topic (Giảng viên phê duyệt):** Xây dựng máy chủ quản lý công việc và lịch giảng dạy mini độc lập trên phần cứng nhúng ESP32-S3, giao tiếp qua TCP Socket và UDP Service Discovery với ứng dụng Client đa nền tảng.

## 1.3. Group

Nhóm bài tập 08, nhóm học phần INT1433-20261-11.

| No. | Student ID | Full Name | Main Responsibility | Contribution |
| :-: | :--------: | :-------- | :------------------ | -----------: |
| 1 | B23DCCN112 | Nguyễn Duy Cường | **Trưởng nhóm & Network/Platform**: Thiết kế kiến trúc mạng, TCP Server một task với `select()`, bộ tách/ghép khung (Framing), Session/Auth (Challenge–Response, chống Brute-force/Replay), LittleFS Storage, Electron TCP Client & Auth Core | 25% |
| 2 | B23DCCN486 | Nguyễn Khánh Linh | **Catalog & Service Discovery**: Module trường, cơ sở, bảng tiết học linh hoạt, quy đổi tiết, ma trận thời gian di chuyển, UDP Discovery (port 5001), mDNS Service, UI Catalog & Discovery Client | 25% |
| 3 | B23DCCN584 | Nguyễn Hoàng Nam | **Schedule & Task Management**: Module lịch dạy định kỳ, công việc/hạn chót cá nhân, hệ thống ngoại lệ (dời/huỷ/bù), thuật toán sinh buổi (`sched_expand`), UI Quản lý lịch & công việc | 25% |
| 4 | B23DCCN559 | Nguyễn Quang Minh | **Calendar & Conflict Detection**: Thuật toán quét và phát hiện trùng lịch/xung đột thời gian di chuyển, API `calendar.week`/`range`, FullCalendar UI, Bảng xung đột, Xuất PDF thời khóa biểu | 25% |
| | | | **Total** | **100%** |

## 1.4. Instructor

**ThS. Ngô Văn Quyền**  
Bộ môn Lập trình mạng – Học viện Công nghệ Bưu chính Viễn thông (PTIT)

---

# 2. Project Summary

## 2.1. Problem

Hiện nay, giảng viên và sinh viên thường phải công tác, học tập tại nhiều trường hoặc cơ sở đào tạo khác nhau. Mỗi trường lại áp dụng một khung giờ/tiết học riêng biệt (ví dụ: Tiết 1 bắt đầu từ 06:45, trường khác bắt đầu từ 07:00; thời lượng tiết 45 phút hoặc 50 phút). Việc quản lý lịch dạy, lịch học, lịch coi thi, chấm thi và deadline thường xuyên đối mặt với:
1. **Xung đột thời gian và di chuyển:** Trùng lặp thời gian hoặc khoảng cách giữa 2 cơ sở không đủ để di chuyển kịp giờ.
2. **Sự phức tạp của ngoại lệ:** Lịch dạy định kỳ có thể bị huỷ 1 buổi, chuyển phòng học, dời ngày hoặc bố trí dạy bù.
3. **Phụ thuộc hạ tầng đám mây cồng kềnh:** Các giải pháp phần mềm quản lý lịch phổ biến (Google Calendar, Outlook) yêu cầu kết nối Internet liên tục, máy chủ Cloud đắt đỏ và không hỗ trợ mô hình quy đổi tiết học linh hoạt theo danh mục cơ sở giáo dục Việt Nam.

## 2.2. Motivation

Môn học **Lập trình mạng** tập trung vào việc hiểu sâu sắc cách thức hoạt động của các tầng giao thức (Transport, Application), xử lý truyền nhận dữ liệu qua Socket, đồng bộ trạng thái giữa Client–Server, và giải quyết các bài toán kỹ thuật mạng cốt lõi (TCP Stream Framing, Concurrency, Packet Fragmentation/Concatenation, Network Security, Service Discovery).

Thay vì triển khai server trên máy tính xách tay hoặc Cloud thông thường, nhóm chọn hiện thực hóa một **Standalone Embedded Network Server** hoàn chỉnh chạy trực tiếp trên vi điều khiển **ESP32-S3** (Dual-core 240MHz, 512KB SRAM):
- Biến ESP32 thành một "máy chủ thiết bị mini" bỏ túi, cắm nguồn USB bất kỳ là tạo thành máy chủ lịch dạy độc lập trong mạng LAN/Wi-Fi gia đình hoặc cơ quan.
- Chứng minh khả năng lập trình socket cấp thấp (BSD Socket trên nền lwIP), tối ưu hóa tài nguyên mạng cực hạn trên phần cứng nhúng mà vẫn đảm bảo độ tin cậy, bảo mật và khả năng phục vụ nhiều client đồng thời.

## 2.3. Objectives

Project hướng tới các mục tiêu cụ thể:
1. **Lập trình TCP Server cấp thấp:** Tự xây dựng Server TCP trên ESP32-S3 bằng API lwIP BSD Sockets (`socket`, `bind`, `listen`, `accept`, `select`, `recv`, `send`) theo cơ chế **Non-blocking I/O Multiplexing** (`select()`), không dùng HTTP server hay thư viện mạng cấp cao.
2. **Thiết kế Custom Application Protocol & Framing:** Xây dựng cơ chế đóng/tách khung (Length-prefixed binary framing: 4-byte Big-Endian length + JSON payload) nhằm giải quyết triệt để vấn đề ranh giới thông điệp trên dòng byte stream của TCP.
3. **Đồng bộ thời gian thực (Server Push):** Cho phép máy chủ chủ động đẩy thông báo thay đổi (`push notification`) đến tất cả các Client đang kết nối khi có dữ liệu được tạo mới hoặc cập nhật.
4. **Service Discovery tự động:** Thiết kế giao thức UDP Broadcast (port 5001) và mDNS (`espedushed.local`) để Client tự động tìm thấy máy chủ trong mạng LAN mà người dùng không cần gõ địa chỉ IP thủ công.
5. **Bảo mật ứng dụng tầng mạng:** Hiện thực quy trình xác thực **Challenge–Response** (sử dụng Salt, PBKDF2-HMAC-SHA256, Replay-resistant Nonce), Token phiên ngẫu nhiên trong RAM, cơ chế chống tấn công Brute-force. Hệ thống dành cho **một người dùng** (một tài khoản duy nhất).
6. **Xử lý nghiệp vụ phức tạp trên chip nhúng:** Quy đổi tiết linh hoạt theo từng trường, tự động tính toán thời gian di chuyển giữa các cơ sở, phát hiện xung đột lịch (Conflict Detection), mở rộng chuỗi lịch định kỳ kèm ngoại lệ (`sched_expand`) và lưu trữ an toàn chống mất điện (Atomic file write trên LittleFS).
7. **Ứng dụng Client hiện đại:** Xây dựng ứng dụng Desktop Client bằng Electron + React + TypeScript với kiến trúc cách ly tiến trình (Context Isolation), giao tiếp Main–Renderer an toàn qua IPC, hiển thị thời khóa biểu trực quan qua FullCalendar.

## 2.4. Scope

### In scope
- **Thiết bị máy chủ:** Vi điều khiển ESP32-S3 hoạt động độc lập (kết nối Wi-Fi Station hoặc phát Wi-Fi AP cấu hình).
- **Giao tiếp mạng:**
  - TCP Server port 5000: Quản lý tối đa 8 kết nối đồng thời qua `select()`.
  - UDP Broadcast port 5001 & mDNS: Khám phá dịch vụ trong mạng LAN.
  - Giao thức nhị phân ghép khung 4-byte length prefix + JSON UTF-8 payload.
  - HTTP port 8080: trang web chỉ đọc xem lịch trên điện thoại, bảo vệ bằng link bí mật.
  - Truy cập từ mạng ngoài (4G, Wi-Fi khác) qua port forwarding trên router + tên miền động DuckDNS do ESP32 tự cập nhật.
- **Nghiệp vụ lịch & công việc:**
  - Quản lý trường, cơ sở, cấu hình bảng tiết độc lập cho từng trường, ma trận khoảng cách di chuyển, ngày nghỉ lễ.
  - Quản lý lịch dạy định kỳ (theo tiết hoặc theo giờ thực), công việc (nhiệm vụ, deadline cá nhân), ngoại lệ buổi (huỷ, đổi giờ, dời ngày, dạy bù).
  - Thuật toán kiểm tra xung đột thời gian và di chuyển.
- **Lưu trữ nhúng:** LittleFS flash filesystem với cơ chế ghi nguyên tử (atomic swap qua file tạm) chống hỏng dữ liệu khi mất điện đột ngột; NVS lưu cấu hình Wi-Fi.
- **Ứng dụng Client:** Ứng dụng Desktop cho Laptop (Windows/macOS/Linux) đóng gói bằng Electron.
- **Kiểm thử & Đánh giá:** Bộ test host chạy bằng GCC trên máy tính giả lập logic, kiểm thử phân mảnh gói tin TCP (fragmentation & concatenation), đo lường độ trễ (latency), thông lượng (throughput) và mức tiêu thụ tài nguyên.

### Out of scope
- Ứng dụng di động (Mobile App) – điện thoại chỉ xem lịch qua trang web của ESP32, mọi thao tác sửa làm trên app Desktop.
- Nhiều tài khoản và phân quyền – hệ thống phục vụ một người dùng.
- Mã hóa toàn bộ kênh truyền TLS/SSL – do hạn chế tài nguyên RAM của ESP32 khi chạy đồng thời nhiều kết nối TCP và thuật toán lịch; thay vào đó nhóm bảo vệ mật khẩu bằng Challenge–Response; dữ liệu đi dạng rõ, xem mục 16.
- Đồng bộ hóa qua Cloud công cộng của bên thứ ba.

---

# 3. System Overview

## 3.1. System Description

Hệ thống **ESPEduShed** gồm 3 thành phần chính:
1. **ESPEduShed Embedded Server (Firmware ESP32-S3):** Đóng vai trò là máy chủ trung tâm lưu trữ toàn bộ cơ sở dữ liệu và thực thi logic nghiệp vụ. Máy chủ kết nối vào mạng Wi-Fi cục bộ (hoặc tự phát AP khi chưa có cấu hình mạng), duy trì TCP server port 5000 và UDP discovery port 5001. Khi nhận request từ Client, máy chủ thẩm định token, giải mã lệnh, tương tác với LittleFS và trả về kết quả hoặc đẩy thông điệp cập nhật `push` cho toàn bộ các client khác đang mở.
2. **ESPEduShed Desktop App (Client):** Ứng dụng Electron đa nền tảng chạy trên Laptop. Tiến trình Main của Electron chịu trách nhiệm quản lý Socket kết nối TCP/UDP cấp thấp (qua module `net` và `dgram` của Node.js), thực hiện mã hóa bảo mật mật khẩu (PBKDF2/HMAC), tự động kết nối lại khi mất mạng và trao đổi dữ liệu với giao diện Renderer (React + FullCalendar) thông qua IPC bảo mật.
3. **Trang web xem lịch (điện thoại):** ESP32 mở thêm HTTP cổng 8080 phục vụ một trang chỉ đọc, hiển thị lịch tuần giống app. Trang được bảo vệ bằng link bí mật, thư viện FullCalendar nhúng sẵn trong firmware. Ở ngoài mạng nhà, app và điện thoại vào ESP32 qua port forwarding trên router và tên miền DuckDNS do ESP32 tự cập nhật.

```text
       ┌────────────────────────────────────────────────────────┐
       │                   Local Network (Wi-Fi)                │
       └───────────────────────────┬────────────────────────────┘
                                   │
                 ┌─────────────────┴─────────────────┐
                 │                                   │
      ┌──────────▼───────────┐            ┌──────────▼───────────┐
      │ Client 1 (Laptop)    │            │ Client 2 (Laptop)    │
      │ Electron + React UI  │            │ Electron + React UI  │
      └──────────┬───────────┘            └──────────┬───────────┘
                 │ TCP 5000 / UDP 5001 (Discovery)   │
                 └─────────────────┬─────────────────┘
                                   │
                    ┌──────────────▼─────────────┐
                    │    ESPEduShed Server       │
                    │   (ESP32-S3 Dual-Core)     │
                    │  - FreeRTOS + lwIP Sockets │
                    │  - LittleFS Data Storage   │
                    └────────────────────────────┘
```

## 3.2. Target Users

Hệ thống dành cho **một người dùng** (ví dụ một giảng viên dạy ở nhiều trường), dùng một tài khoản duy nhất:
- Cấu hình Wi-Fi, danh mục trường, cơ sở, bảng tiết, ngày nghỉ, thời gian di chuyển.
- Tạo và theo dõi lịch giảng dạy định kỳ theo tiết/giờ tại nhiều trường, lịch công tác, lịch thi; xử lý dạy bù/đổi lịch; theo dõi xung đột thời gian di chuyển.
- Quản lý công việc cá nhân và hạn chót (Deadlines).
- Xem lịch trên điện thoại ở bất kỳ đâu qua link bí mật.

## 3.3. Main Functions

Trạng thái: **Đã code** = có mã nguồn; phần nào đã chạy trên ESP32 thật, phần nào chưa, xem mục 12.

| Module | Function | Description | Status |
| :--- | :--- | :--- | :---: |
| **Mạng (TV1)** | TCP server đa client | lwIP socket, `select()` trên một task, tối đa 8 kết nối, kết nối thừa nhận `BUSY` | Đã code |
| | Ghép khung | 4 byte độ dài big-endian + JSON, tối đa 8 KB, xử lý khung bị cắt / dính | Đã code, có test |
| | Giữ kết nối | `system.ping` mỗi 15 giây, đóng kết nối im lặng 60 giây hoặc 30 giây chưa đăng nhập, TCP keep-alive | Đã code |
| | Tin đẩy | Server đẩy `{"push":"changed"}` tới các client đã đăng nhập | Đã code |
| **Bảo mật (TV1)** | Đăng nhập challenge–response | PBKDF2-HMAC-SHA256, mật khẩu không đi trên mạng | Đã code, có test phía client |
| | Chống gửi lại và dò mật khẩu | `nonce` một lần (30 giây); khoá IP 60 giây sau 5 lần sai | Đã code, có test |
| | Phiên | Token 16 byte trong RAM, một tài khoản duy nhất | Đã code, có test |
| | Tài khoản | Mật khẩu ngẫu nhiên in ra serial lần đầu, bắt đổi ở lần đăng nhập đầu | Đã code |
| **Nền tảng (TV1)** | Wi-Fi | STA tự nối lại; AP cài đặt `ESPEduShed-XXXX` và lệnh `system.wifi` | Đã code |
| | Lưu trữ | LittleFS, ghi file tạm rồi đổi tên; sao lưu / khôi phục theo mẩu 4 KB | Đã code |
| | Trạng thái server | `system.status`: RAM, số kết nối, số yêu cầu, handler chậm nhất | Đã code |
| | DuckDNS | ESP32 tự báo IP công khai mỗi 5 phút, lệnh `system.ddns` | Đã code |
| **Web xem lịch** | Trang lịch trên điện thoại | HTTP 8080, link bí mật `web.link` / `web.rotate`, giao diện giống trang Lịch của app | Đã code |
| **Tìm server (TV2)** | UDP broadcast, mDNS | Cổng 5001, `espedushed.local`, tối đa 10 trả lời/giây | Đã code |
| **Danh mục (TV2)** | Trường, cơ sở, bảng tiết, quy đổi tiết, di chuyển, ngày nghỉ, môn, lớp | 25 lệnh, lưu `/data/catalog.json` | Đã code, có test logic |
| **Lịch (TV3)** | Lịch dạy định kỳ, công việc / hạn chót, ngoại lệ (huỷ, đổi phòng, đổi giờ, dời, dạy bù), sinh buổi | Kiểm tra trùng ở server, `force` để lưu đè | Đã code, có test logic |
| | Màu riêng | Chọn màu cho lịch dạy, công việc, trường từ bảng 12 màu có sẵn (không nhập mã màu); lịch dạy không chọn thì theo màu trường | Đã code |
| **Lịch tuần (TV4)** | Kiểm tra trùng, FullCalendar, bảng xung đột, xuất PDF | | Đã code, có test logic |


---

# 4. System Architecture

## 4.1. Architecture Diagram

Sơ đồ kiến trúc tổng thể giữa ứng dụng Desktop Client và Firmware ESP32-S3:

```text
+-----------------------------------------------------------------------------------+
|                            CLIENT (Desktop Application)                           |
|  +-----------------------------------------------------------------------------+  |
|  |                 Renderer Process (React 18 + TypeScript + Vite)             |  |
|  |   [FullCalendar View]   [Catalog Editor]   [Schedule Form]   [Settings Page]|  |
|  |                                  │                                          |  |
|  |                            window.api (IPC)                                 |  |
|  +--------------------------------──┼──────────────────────────────────────────+  |
|                                     │ contextBridge                               |
|  +--------------------------------──┼──────────────────────────────────────────+  |
|  |                 Main Process (Node.js Electron Runtime)                     |  |
|  |   [Crypto / PBKDF2]  [SafeStorage]  [UDP Discovery]  [TCP Client Socket]    |  |
|  |                                                             │               |  |
|  +-------------------------------------------------------------┼---------------+  |
+----------------------------------------------------------------┼------------------+
                                                                 │
                                                   TCP Port 5000 │ (Binary Framed JSON)
                                                                 │
+----------------------------------------------------------------┼------------------+
|                            SERVER (ESP32-S3 SoC Firmware)      │                  |
|  +-------------------------------------------------------------▼---------------+  |
|  |                     lwIP BSD Sockets & FreeRTOS Platform                    |  |
|  |         TCP Server Loop (select() Non-blocking I/O Multiplexing)            |  |
|  |          - Frame Parser (4-byte length header + JSON message body)          |  |
|  |          - Session Manager & Dispatcher (Tra bảng lệnh theo prefix)         |  |
|  +--------------------------------------┬────────────────────────────────------+  |
|                                         │                                         |
|      ┌──────────────────┬───────────────┴───────────────┬──────────────────┐      |
|      ▼                  ▼                               ▼                  ▼      |
| [platform]         [catalog]                       [schedule]         [calendar]  |
| - auth.*           - school.*, periods.*           - schedule.*       - calendar.*|
| - system.*         - campus.*, travel.*            - exception.*      - conflict.*|
| - system.ddns      - holiday.*, subject.*          - task.*                       |
|                    - UDP Discovery (5001)          - expand.c                     |
|      │                  │                               │                  │      |
|      └──────────────────┼───────────────────────────────┼──────────────────┘      |
|                         ▼                               ▼                         |
|  +-----------------------------------------------------------------------------+  |
|  |              Storage Abstraction Layer (LittleFS + Atomic Swap)             |  |
|  |     /data/users.json   /data/catalog.json   /data/schedule.json  /conflict_ack.json|
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

Ngoài TCP 5000, component **webview** mở HTTP 8080 (`esp_http_server`, task riêng) cho trang xem lịch trên điện thoại: `GET /c/<khoá>`, `GET /api/<khoá>/range`, `GET /fc.js`. Trang gọi lại `calendar_range_json` của TV4 nên hiển thị đúng dữ liệu như lệnh `calendar.range`.

## 4.2. Components

| Component | Layer | Language / Tech | Responsibility |
| :--- | :--- | :--- | :--- |
| **Electron Main Process** | Client Core | TypeScript / Node.js | Quản lý cửa sổ app, lưu trữ mật mã an toàn (`safeStorage`), mở UDP socket phát hiện server, quản lý kết nối TCP socket bền vững, ghép khung tin, tự động kết nối lại khi mất mạng. |
| **Electron Preload / IPC** | Client Bridge | TypeScript | Cung cấp API an toàn (`window.api`) thông qua `contextBridge` và `contextIsolation: true`, ngăn chặn tấn công XSS từ giao diện renderer. |
| **React Renderer** | Client UI | React 18, Tailwind, shadcn/ui | Giao diện hiển thị, tiếp nhận thao tác người dùng, tương tác với FullCalendar, hiển thị bảng xung đột, vẽ thời khóa biểu, điều hướng routes. |
| **Platform Component** | Server Core | C / ESP-IDF | Khởi tạo TCP Server (socket, bind, listen, accept, select), quản lý bộ đệm ghép khung tin nhị phân, tra cứu bảng lệnh, xác thực Challenge-Response, quản lý phiên làm việc, lưu trữ LittleFS và Wi-Fi/SNTP. |
| **Discovery Component** | Server Core | C / ESP-IDF | Chạy task FreeRTOS độc lập lắng nghe broadcast UDP cổng 5001 (`ESPEDUSHED_DISCOVER`) và dịch vụ mDNS `espedushed.local`. |
| **Catalog Component** | Server Business | C | Quản lý danh mục trường học, cơ sở đào tạo, bảng tiết học, thuật toán quy đổi tiết sang giờ, ma trận thời gian di chuyển giữa các cơ sở, lịch nghỉ lễ. |
| **Schedule Component** | Server Business | C | Quản lý mô hình dữ liệu lịch dạy định kỳ, danh sách công việc/hạn nộp bài, các ngoại lệ từng buổi học (dời ngày, đổi phòng, dạy bù), thuật toán mở rộng chuỗi buổi học (`sched_expand`). |
| **Webview Component** | Server Web | C / esp_http_server | Trang web chỉ đọc xem lịch trên điện thoại (HTTP 8080), khoá link bí mật trong NVS, lệnh `web.link` / `web.rotate`. |
| **Calendar Component** | Server Business | C | Thuật toán quét và kiểm tra xung đột trùng lặp thời gian hoặc vi phạm khoảng thời gian di chuyển (`conflict_check`), tạo cấu trúc dữ liệu hiển thị lịch tuần (`calendar.week`). |

## 4.3. Data Flow

Luồng dữ liệu mẫu khi người dùng thêm một lịch dạy mới trên Client:

```text
User           React UI           Electron Main          TCP Socket (ESP32)      Schedule Module      LittleFS
 │                │                     │                        │                      │                │
 ├─── Thêm lịch ─►│                     │                        │                      │                │
 │                ├── window.api.request('schedule.create', data)│                      │                │
 │                │────────────────────►│                        │                      │                │
 │                │                     ├── Gắn ID & Token       │                      │                │
 │                │                     ├── Đóng gói 4B Header   │                      │                │
 │                │                     ├──── Gửi Frame TCP ────►│                      │                │
 │                │                     │                        ├── Ghép đủ 4B + Data  │                │
 │                │                     │                        ├── Tra bảng lệnh      │                │
 │                │                     │                        ├── Thẩm định Token    │                │
 │                │                     │                        ├──── Gọi Handler ────►│                │
 │                │                     │                        │                      ├── conflict_check()
 │                │                     │                        │                      ├── Ghi file tạm │
 │                │                     │                        │                      ├── Atomic swap ─► Lưu Flash
 │                │                     │                        │◄─── Kết quả OK ──────┤                │
 │                │                     │◄─── TCP Response Frame ┤                      │                │
 │                │                     │    {"id":12, "ok":true}├── Gửi Tin Đẩy (Push) ───────────────► Gửi tới các Client khác
 │                │◄── Trả kết quả ─────┤                        │   {"push":"changed"}                  (Cập nhật thời gian thực)
 │                │                     │                        │                      │                │
 │◄── Render lại ─┤                     │                        │                      │                │
```

---

# 5. Network Communication Design

## 5.1. Communication Model

Hệ thống hoạt động theo mô hình **Client–Server** sử dụng hai giao thức cốt lõi:
1. **UDP Socket (Service Discovery):** Mô hình Connectionless Broadcast (1-to-N) trên cổng 5001 dùng để tự động tìm kiếm địa chỉ IP máy chủ trong mạng LAN.
2. **TCP Socket (Data Communication & Real-time Synchronization):** Kết nối Connection-oriented bền vững (Persistent Connection) trên cổng 5000. Máy chủ sử dụng kỹ thuật **I/O Multiplexing (`select()`)** để giám sát đồng thời socket lắng nghe và tối đa 8 socket client trên cùng một task FreeRTOS mà không cần cấp phát thread riêng cho mỗi kết nối, tiết kiệm tối đa bộ nhớ RAM của vi điều khiển.

## 5.2. Protocol

* **Transport Layer:** TCP (Transmission Control Protocol) – đảm bảo dữ liệu truyền nhận tin cậy, không mất gói và đúng thứ tự.
* **Application Layer:** Custom Binary-Framed JSON Protocol.
* **TCP Stream Problem & Solution:** Do TCP là giao thức hướng luồng byte (byte-stream), không duy trì ranh giới thông điệp (message boundary), các gói tin có thể bị phân mảnh (fragmentation) hoặc dính liền (concatenation) tại tầng mạng. Hệ thống giải quyết bằng quy cách đóng khung:
  - **Header:** 4 bytes số nguyên không dấu (Big-Endian network byte order) quy định chính xác độ dài byte của phần thân (Payload Length).
  - **Body:** Chuỗi JSON mã hóa UTF-8 có kích thước đúng bằng giá trị ghi trong Header (kích thước tối đa $8192$ bytes/khung).
  - Cả Server và Client đều hiện thực một bộ đệm nhận (Accumulation Buffer): Chỉ tiến hành giải mã thông điệp khi đã tích lũy đủ 4 byte Header và nhận đủ $N$ byte Payload tương ứng.

## 5.3. Message Format

### 5.3.1. Cấu trúc khung tin TCP (Binary Framing)

```text
+-----------------------+------------------------------------------------------+
| 4 Bytes (Big-Endian)  |               N Bytes (JSON Payload)                 |
|     Payload Length    |    {"id": ..., "cmd": "...", "token": "...", ...}    |
+-----------------------+------------------------------------------------------+
```

### 5.3.2. Yêu cầu từ Client (Client Request)

Ví dụ minh hoạ; tên trường trong `data` của từng lệnh do thành viên phụ trách chốt trong [`docs/protocol.md`](docs/protocol.md).

```json
{
  "id": 105,
  "cmd": "schedule.create",
  "token": "4f8a12e9b0c2d3e4f5a6b7c8d9e0f1a2",
  "data": {
    "subject_id": 2,
    "school_id": 1,
    "campus_id": 1,
    "day_of_week": 2,
    "period_start": 1,
    "period_count": 3,
    "room": "A2-301",
    "date_from": "2026-09-07",
    "date_to": "2026-11-30"
  }
}
```

### 5.3.3. Phản hồi thành công từ Server (Server Success Response)

Phản hồi mang lại đúng trường `id` của yêu cầu:

```json
{
  "id": 105,
  "ok": true,
  "data": {
    "id": 14,
    "message": "Tạo lịch giảng dạy thành công"
  }
}
```

### 5.3.4. Phản hồi lỗi từ Server (Server Error Response)

```json
{
  "id": 105,
  "ok": false,
  "error": {
    "code": "CONFLICT",
    "message": "Trùng lịch giảng dạy hoặc không đủ thời gian di chuyển",
    "field": "period_start"
  },
  "data": {
    "conflicts": [
      {
        "source_id": 8,
        "title": "Lập trình mạng",
        "date": "2026-09-07",
        "time_start": "07:00",
        "time_end": "09:30",
        "room": "B1-204",
        "conflict_type": "OVERLAP"
      }
    ]
  }
}
```

### 5.3.5. Tin đẩy thời gian thực từ Server (Server Push Notification)

Tin đẩy không có trường `id`, gửi tới mọi client đã xác thực:

```json
{
  "push": "changed",
  "entity": "schedule",
  "id": 14
}
```

### 5.3.6. UDP Service Discovery Format

- **Client Broadcast (UDP 255.255.255.255:5001):**
  ```text
  ESPEDUSHED_DISCOVER
  ```
- **Server Response (UDP Unicast về Client IP/Port):**
  ```json
  {
    "name": "espedushed",
    "ip": "192.168.1.37",
    "port": 5000,
    "fw": "1.0.0"
  }
  ```

## 5.4. Communication Sequence

### 5.4.1. Quy trình Xác thực Challenge–Response (Không gửi mật khẩu thô)

```text
Client (Desktop)                                                 Server (ESP32-S3)
   │                                                                    │
   ├── [1] {"cmd": "auth.challenge", "data": {"user": "admin"}} ───────►│
   │                                                                    ├── Sinh nonce 16 byte (32 ký tự hex)
   │                                                                    ├── Lưu Nonce trong RAM (hạn 30s)
   │◄── [2] {"ok": true, "data": {"nonce": "A1B2...",                  │
   │                              "salt": "9C8D...", "iter": 10000}} ───┤
   │                                                                    │
   ├── Tính Key = PBKDF2(pass, salt, 10000, 32)                         │
   ├── Tính Proof = HMAC-SHA256(Key, Nonce)                             │
   │                                                                    │
   ├── [3] {"cmd": "auth.login", "data": {"user": "admin",              │
   │                                      "nonce": "A1B2...",           │
   │                                      "proof": "E5F6..."}} ────────►│
   │                                                                    ├── Kiểm tra Nonce còn hạn?
   │                                                                    ├── Tính HMAC từ Key lưu trữ
   │                                                                    ├── So sánh Proof == ExpectedProof
   │                                                                    ├── Sinh Token phiên 16-byte ngẫu nhiên
   │◄── [4] {"ok": true, "data": {"token": "7D4E...",                   │
   │                              "user": {"id": 1, "name": "admin"}}} ─┤
```

### 5.4.2. Duy trì kết nối và Xử lý ngắt kết nối (Heartbeat / Keep-Alive)

```text
Client (Desktop)                                                 Server (ESP32-S3)
   │                                                                    │
   ├── [Mỗi 15 giây] {"cmd": "system.ping", "id": 1} ──────────────────►│
   │◄── {"id": 1, "ok": true, "data": {"pong": 1729600000}} ────────────┤
   │                                                                    │
   │ ... (Mất mạng hoặc Client đột ngột ngắt nguồn) ...                 │
   │                                                                    ├── Không nhận dữ liệu > 60 giây
   │                                                                    ├── Đóng Socket, thu hồi bộ đệm
```

## 5.5. Error Handling

Hệ thống định nghĩa bảng mã lỗi chuẩn toàn hệ thống:

| Mã lỗi (Code) | Ý nghĩa kỹ thuật | Hành vi xử lý của Hệ thống |
| :--- | :--- | :--- |
| `BAD_JSON` | Gói tin không đúng định dạng JSON chuẩn hoặc sai cú pháp | Đóng gói mã lỗi gửi lại Client; nếu sai lệch kích thước khung tin sẽ reset kết nối để tránh lệch byte stream. |
| `UNKNOWN_CMD` | Lệnh không tồn tại trong bảng đăng ký lệnh (`ld_register_cmd`) | Trả về mã lỗi kèm tên lệnh sai, ghi nhận log cảnh báo trên Serial Monitor. |
| `BAD_ARG` | Dữ liệu đầu vào thiếu trường bắt buộc hoặc sai kiểu/miền giá trị | Chỉ rõ trường bị lỗi trong thuộc tính `field` của phản hồi lỗi. |
| `UNAUTHORIZED` | Không có Token hoặc Token đã hết hạn / không hợp lệ | Client tự động hủy Token cũ trong `safeStorage` và điều hướng về trang Login. |
| `FORBIDDEN` | Chưa đổi mật khẩu ban đầu | Từ chối mọi lệnh trừ `auth.password`, `auth.logout`, `auth.me`, `system.ping`, `system.status`. |
| `NOT_FOUND` | Không tìm thấy tài nguyên (id sai hoặc đã xoá) | Trả về `NOT_FOUND`; web xem lịch trả HTTP 404 khi khoá link sai. |
| `CONFLICT` | Trùng lịch giảng dạy hoặc vi phạm thời gian di chuyển giữa 2 cơ sở | Trả về danh sách chi tiết các lịch bị xung đột trong `data.conflicts`; cho phép Client gửi lại với `"force": true` để ghi đè có chủ đích. |
| `FULL` | Bộ nhớ lưu trữ Flash LittleFS hoặc RAM bị đầy | Ngăn chặn ghi thêm bản ghi mới; thông báo người dùng dọn dẹp hoặc sao lưu dữ liệu. |
| `BUSY` | Quá số lượng kết nối tối đa (8 clients) hoặc IP bị chặn Brute-force | Từ chối kết nối mới hoặc tạm khóa IP vi phạm trong vòng 60 giây. |
| `INTERNAL` | Lỗi phát sinh từ phần cứng, filesystem hoặc phân bổ bộ nhớ nhúng | Trả thông báo lỗi an toàn, ghi mã lỗi chi tiết vào console log để debug. |

---

# 6. Technology Stack

| Phân loại | Công nghệ / Thư viện | Phiên bản / Chi tiết | Mục đích sử dụng |
| :--- | :--- | :--- | :--- |
| **Phần cứng** | ESP32-S3-WROOM-1 | Dual-Core LX7 240MHz, 512KB SRAM, 8MB Flash | Máy chủ nhúng chạy dịch vụ mạng độc lập |
| **Hệ điều hành nhúng** | FreeRTOS | Đi kèm ESP-IDF v5.1+ | Quản lý đa nhiệm, đồng bộ mutex/semaphore, task scheduling |
| **Ngôn ngữ Server** | C (C99/C11) | GCC toolchain cho Xtensa | Lập trình firmware hiệu năng cao, tối ưu bộ nhớ |
| **Network Stack** | lwIP (Lightweight IP) | BSD Socket API | Mở TCP/UDP Sockets, hàm `select()`, `bind()`, `listen()`, `accept()` |
| **C Parser** | cJSON | Bản tích hợp ESP-IDF | Đóng gói và phân tích cú pháp chuỗi JSON |
| **Lưu trữ nhúng** | LittleFS & NVS | LittleFS v2.x | Lưu trữ file JSON bền bỉ, an toàn điện; lưu Wi-Fi credential |
| **Đồng bộ thời gian** | SNTP | ICT-7 (UTC+7) | Đồng bộ ngày giờ quốc tế qua Internet, phục vụ tính toán lịch |
| **Client Platform** | Electron | 31 | Đóng gói ứng dụng Desktop đa nền tảng |
| **Ngôn ngữ Client** | TypeScript | v5.x | Kiểm soát kiểu tĩnh chặt chẽ cho toàn bộ Client |
| **Giao diện Client** | React, Vite, electron-vite | 18.3 / 5.4 / 2.3 | Xây dựng Single Page Application tương tác thời gian thực |
| **Component UI** | Tailwind CSS 3, component theo kiểu shadcn/ui, lucide-react | Tailwind 3.4 | Giao diện, chế độ sáng / tối |
| **Thư viện Lịch** | FullCalendar React | v6.x (Giấy phép MIT) | Hiển thị lịch tuần, lịch ngày, kéo thả và định dạng sự kiện |
| **Networking Client** | Node.js `net` & `dgram` | Node.js Built-in | Socket TCP/UDP thuần không qua trung gian |
| **Mã hóa Client** | Node.js `crypto` & `safeStorage` | PBKDF2, HMAC, DPAPI | Bảo vệ mật khẩu, chống lưu trữ plaintext, mã hóa token trên Windows |
| **Công cụ kiểm thử** | Python 3 & GCC | Thư viện chuẩn (`socket`) | Viết script test tự động, kiểm thử host C trên máy tính |

---

# 7. Novelty and Contributions

## 7.1. Baseline

Hầu hết các đồ án môn học hoặc giải pháp quản lý lịch hiện nay đều dựa trên các mô hình baseline phổ biến:
- **Baseline 1 (Web/Cloud-based):** Sử dụng máy chủ Web (Node.js/Express, Spring Boot, Django) chạy trên máy tính hoặc Cloud, client giao tiếp qua giao thức HTTP RESTful/WebSocket không có cơ chế tối ưu cho thiết bị nhúng.
- **Baseline đo được:** `esp_http_server` trên cùng bo ESP32-S3, trả cùng dữ liệu lịch, client hỏi vòng để biết thay đổi (mục 14.3).
- **Baseline 2 (IoT Sensor Node truyền thống):** Vi điều khiển nhúng (như ESP8266/ESP32) chỉ đóng vai trò làm Client thụ động hoặc trạm thu thập cảm biến, đẩy dữ liệu lên máy chủ trung gian (Blynk, ThingSpeak, MQTT Broker) để máy chủ xử lý toàn bộ logic.

## 7.2. Proposed Improvement

1. **Máy chủ chạy hoàn toàn trên ESP32-S3:** server ứng dụng, nghiệp vụ lịch và lưu trữ đều nằm trên vi điều khiển. Laptop chỉ là client, không có backend nào trên máy tính làm thay.
2. **Giao thức ứng dụng tự thiết kế trên TCP thuần:** khung tin "4 byte độ dài + JSON" giải quyết bài toán ranh giới thông điệp trên luồng byte TCP; mỗi khung chỉ tốn thêm 4 byte so với nội dung, kết nối giữ lâu dài và server chủ động đẩy tin thay đổi tới các client.
3. **Phục vụ nhiều client bằng `select()` trên một task FreeRTOS:** không tạo task riêng cho mỗi kết nối, nên RAM thêm cho mỗi client chỉ là bộ đệm ghép khung (cấp đúng độ dài khi đang nhận dở một khung, trả lại ngay sau khi xử lý) và hàng đợi gửi.
4. **Bảo mật mức ứng dụng phù hợp tài nguyên ESP32:** đăng nhập challenge–response (mật khẩu không đi trên mạng, chống gửi lại gói), token trong RAM, chặn dò mật khẩu theo IP, thay cho TLS: cấu hình mbedTLS của dự án cần bộ đệm 16 KB vào + 4 KB ra mỗi phiên.
5. **Nghiệp vụ lịch nhiều trường trên vi điều khiển:** bảng tiết riêng cho từng trường, quy đổi tiết sang giờ, lịch lặp kèm ngoại lệ, kiểm tra trùng lịch và thời gian di chuyển giữa các cơ sở, viết bằng C và test được trên máy tính.

## 7.3. Contributions

Cột "Bằng chứng" chỉ ghi những gì đã có trong mã nguồn hoặc đã chạy được; CSV nằm trong `statics/results/`.

| No. | Contribution | Description | Evidence |
| :-: | :--- | :--- | :--- |
| 1 | TCP server đa client trên ESP32-S3 không dùng HTTP | lwIP BSD socket, socket không chặn, `select()` trên một task, tối đa 8 kết nối, kết nối thừa nhận `BUSY`, đóng kết nối treo | [`tcp_server.c`](source/server/components/platform/src/tcp_server.c); kịch bản `loadtest.py maxconn` |
| 2 | Ghép khung trên luồng byte TCP | Cùng một quy tắc ở C (server), TypeScript (app) và Python (công cụ); xử lý khung bị cắt, khung dính, khung quá dài | [`frame.c`](source/server/components/platform/src/frame.c), [`frame.ts`](source/client/electron/frame.ts); test [`test_platform.c`](source/server/test_host/platform/test_platform.c), [`frame.test.ts`](source/client/electron/tests/frame.test.ts); kịch bản `loadtest.py fragment` |
| 3 | Đăng nhập challenge–response, chống replay và dò mật khẩu | PBKDF2-HMAC-SHA256 + nonce một lần gắn với kết nối; khoá IP 60 giây sau 5 lần sai | [`auth.c`](source/server/components/platform/src/auth.c), [`limiter.c`](source/server/components/platform/src/limiter.c), [`auth.ts`](source/client/electron/auth.ts); test [`crypto.test.ts`](source/client/electron/tests/crypto.test.ts); kịch bản `loadtest.py replay` |
| 4 | Lưu trữ an toàn khi mất điện | Ghi file tạm rồi đổi tên nguyên tử trên LittleFS | [`storage.c`](source/server/components/platform/src/storage.c); `reset_during_write.csv` (20/20 lần dữ liệu cũ nguyên; chưa thử rút nguồn thật) |
| 5 | Kiểm tra trùng lịch và thời gian di chuyển | Thuật toán sắp xếp rồi quét trên danh sách buổi | [`conflict.c`](source/server/components/calendar/src/conflict.c); test [`test_calendar.c`](source/server/test_host/calendar/test_calendar.c) |
| 6 | Tìm server trong LAN qua UDP broadcast và mDNS | Client không phải nhập IP | [`udp_discovery.c`](source/server/components/discovery/src/udp_discovery.c), [`discovery.ts`](source/client/electron/discovery.ts) |
| 7 | Xem lịch trên điện thoại và truy cập từ mạng ngoài | HTTP server thứ hai trên ESP32 dùng chung dữ liệu với TCP server; link bí mật; DuckDNS tự cập nhật từ ESP32 | [`webview.c`](source/server/components/webview/src/webview.c), [`ddns.c`](source/server/components/platform/src/ddns.c), [hướng dẫn](docs/huong-dan-truy-cap-tu-xa.md); `compare_http_run*.csv`; đường từ mạng ngoài: `fw_1.0.1/external/` (mục 14.4) |

## 7.4. Baseline vs Proposed

| Khía cạnh | Baseline (server Web/REST trên máy tính hoặc cloud) | Proposed (ESPEduShed) |
| :--- | :--- | :--- |
| Vị trí server | PC, VPS hoặc cloud | ESP32-S3 trong mạng LAN, cấp nguồn qua USB |
| Giao thức | HTTP/REST, mỗi yêu cầu kèm header văn bản | Giao thức tự thiết kế trên TCP, khung "4 byte độ dài + JSON" |
| Kết nối | Thường đóng mở theo yêu cầu; muốn nhận cập nhật phải hỏi vòng hoặc thêm WebSocket | Một kết nối lâu dài, server chủ động đẩy tin `changed` |
| Đa client | Thread pool hoặc event loop của hệ điều hành lớn | `select()` trên một task FreeRTOS |
| Tìm server | Tên miền hoặc nhập địa chỉ | UDP broadcast và mDNS trong LAN |
| Dữ liệu | Lưu ở server bên thứ ba | Lưu cục bộ trên flash của thiết bị |

So sánh định lượng với `esp_http_server` trên cùng bo: mục 14.3.

---

# 8. Project Structure

```text
ESPEduShed/
├── README.md                              Tài liệu dự án (theo mẫu của học phần)
├── docs/
│   └── protocol.md                        Đặc tả giao thức: khung tin, mã lỗi, toàn bộ lệnh
├── statics/
│   ├── architecture.png                   Sơ đồ kiến trúc (cần bổ sung)
│   └── results/                           Kết quả đo thật do source/tools/loadtest.py ghi ra (CSV)
├── report/                                Báo cáo Nhom08_ESPEduShed_Report.docx và bản PDF
└── source/
    ├── server/                            Firmware ESP32-S3 (dự án ESP-IDF, ngôn ngữ C)
    │   ├── CMakeLists.txt, partitions.csv, sdkconfig.defaults
    │   ├── main/app_main.c                Khởi tạo và nối các module
    │   ├── components/
    │   │   ├── platform/                  (TV1) TCP server, giao thức, đăng nhập, Wi-Fi, lưu trữ
    │   │   │   ├── include/platform.h     Hợp đồng cho mọi module: ld_register_cmd, ld_fail, ld_push_changed, store_*, time_*
    │   │   │   ├── include/ld_lock.h      Khoá dùng chung (chạy cả trên máy tính khi test)
    │   │   │   └── src/
    │   │   │       ├── tcp_server.c       socket, bind, listen, select(), accept, hàng đợi gửi, tin đẩy, hết thời gian chờ
    │   │   │       ├── frame.c            Ghép khung 4 byte độ dài + JSON (logic thuần)
    │   │   │       ├── protocol.c         Phân tích {id, cmd, token, data}, bảng lệnh, kiểm tra token
    │   │   │       ├── session.c          Token phiên trong RAM (logic thuần)
    │   │   │       ├── limiter.c          Chặn dò mật khẩu theo IP (logic thuần)
    │   │   │       ├── auth.c             Lệnh auth.*: challenge–response, đổi mật khẩu
    │   │   │       ├── users.c            Tài khoản duy nhất, /data/users.json, mật khẩu đầu
    │   │   │       ├── crypto.c           HMAC-SHA256, PBKDF2 (mbedTLS)
    │   │   │       ├── wifi.c             STA, AP cài đặt, lệnh system.wifi
    │   │   │       ├── time_svc.c         SNTP, múi giờ UTC+7
    │   │   │       ├── storage.c          LittleFS, ghi file nguyên tử
    │   │   │       ├── system_cmd.c       Lệnh system.ping, status, backup, restore
    │   │   │       └── ddns.c             Cập nhật DuckDNS, lệnh system.ddns
    │   │   ├── discovery/                 (TV2) UDP 5001 và mDNS
    │   │   ├── catalog/                   (TV2) Trường, bảng tiết, ngày nghỉ, thư viện ngày giờ dt
    │   │   ├── schedule/                  (TV3) Lịch dạy, công việc, ngoại lệ, sinh buổi
    │   │   ├── calendar/                  (TV4) Kiểm tra trùng, lịch tuần
    │   │   └── webview/                   Web xem lịch trên điện thoại (HTTP 8080): web/calendar.html,
    │   │                                      web/fullcalendar.js.gz (FullCalendar 6.1.15 nhúng sẵn)
    │   └── test_host/                     Test logic C bằng gcc trên máy tính (Makefile, mỗi người một thư mục)
    ├── client/                            App desktop Electron + React + TypeScript
    │   ├── package.json, electron.vite.config.ts, tailwind.config.ts, electron-builder.yml, index.html
    │   ├── electron/                      Main process (Node.js)
    │   │   ├── main.ts                    Cửa sổ, kênh IPC, in PDF                         (TV1)
    │   │   ├── preload.ts                 Cầu nối an toàn cho giao diện                    (TV1)
    │   │   ├── tcp-client.ts              net.Socket, ghép khung, id, tin đẩy, ping, tự nối lại (TV1)
    │   │   ├── frame.ts, crypto.ts        Ghép khung, PBKDF2/HMAC (logic thuần, có test)  (TV1)
    │   │   ├── auth.ts                    Đăng nhập, safeStorage, tự đăng nhập lại         (TV1)
    │   │   ├── discovery.ts               UDP broadcast tìm ESP                            (TV2)
    │   │   └── tests/                     Test chạy bằng node --test
    │   └── src/                           Giao diện React
    │       ├── App.tsx, main.tsx          Gắn router và route của các module              (TV1)
    │       ├── core/                      Khung app, api.ts, phiên, component UI, trang kết nối,
    │       │                              đăng nhập, đổi mật khẩu, truy cập từ xa, trạng thái,
    │       │                              bảng chọn màu components/color-picker.tsx        (TV1)
    │       ├── catalog/                   (TV2)    schedule/  (TV3)    calendar/  (TV4)
    └── tools/                             Công cụ Python (chỉ thư viện chuẩn)              (TV1)
        ├── espproto.py                    Ghép khung và challenge–response dùng chung
        ├── tcp_client.py                  Gửi lệnh từ dòng lệnh
        ├── fake_server.py                 Server giả lập ESP32 để làm app khi chưa có phần cứng
        ├── loadtest.py                    Từng kịch bản đo, ghi CSV vào statics/results
        └── run_experiments.py             Chạy toàn bộ thực nghiệm trên ESP32 bằng một lệnh
```

---

# 9. Requirements

## 9.1. Hardware

- **Server:** bo mạch ESP32-S3 có flash 8 MB (ví dụ ESP32-S3-DevKitC-1 N8 hoặc N8R2), cáp USB.
- **Client:** laptop Windows 10/11 (app cũng build được cho macOS, Linux).
- **Mạng:** router Wi-Fi 2.4 GHz chung cho ESP32 và laptop. Không có router thì laptop nối thẳng vào Wi-Fi AP do ESP32 phát.

## 9.2. Software

| Thành phần | Phiên bản |
| :--- | :--- |
| ESP-IDF (build firmware) | 5.1 trở lên, có toolchain ESP32-S3 |
| Node.js và npm (app) | Node.js 22 LTS trở lên (test dùng `node --test` với TypeScript) |
| Python (công cụ, đo thực nghiệm) | 3.9 trở lên, chỉ thư viện chuẩn |
| gcc và make (test logic C trên máy tính) | Linux, macOS, WSL hoặc MSYS2 trên Windows |
| Driver USB–serial | CP210x hoặc CH340 tuỳ bo mạch |

Thư viện bên ngoài: cJSON (có sẵn trong ESP-IDF), `joltwallet/littlefs`, `espressif/mdns` (tải qua IDF Component Manager), mbedTLS (có sẵn trong ESP-IDF); phía app xem `source/client/package.json` (Electron, React, Vite, Tailwind, FullCalendar, lucide-react).

---

# 10. Installation

## 10.1. Clone / Download

```bash
git clone https://github.com/<tai-khoan>/espedushed.git
cd espedushed
```

## 10.2. Install Dependencies

```bash
cd source/client
npm install
```

Thư viện của firmware tự tải ở lần `idf.py build` đầu tiên.

## 10.3. Storage Setup (phân vùng flash)

Không có cơ sở dữ liệu ngoài. Dữ liệu nằm trong phân vùng LittleFS `data` (3 MB), tự format ở lần chạy đầu
([`partitions.csv`](source/server/partitions.csv)):

```csv
# Name,   Type, SubType, Offset, Size
nvs,      data, nvs,     ,       0x6000
phy_init, data, phy,     ,       0x1000
factory,  app,  factory, ,       2M
data,     data, spiffs,  ,       3M
```

| File trên ESP | Module | Nội dung |
| :--- | :--- | :--- |
| `/data/users.json` | TV1 | Tài khoản duy nhất: salt, key (PBKDF2), cờ phải đổi mật khẩu |
| `/data/catalog.json` | TV2 | Trường, cơ sở, bảng tiết, ngày nghỉ, môn, lớp |
| `/data/schedule.json` | TV3 | Lịch dạy, công việc, ngoại lệ |

`catalog.json` và `schedule.json` lưu dạng **JSON Lines** (`schema: 2`): dòng đầu là header `{"schema":2,"nextId":..}`,
mỗi dòng sau một bản ghi có trường `"k"` (loại bản ghi). Ghi và nạp từng dòng nên chỉ cần vài KB RAM; dựng cả file
thành một cây JSON khi dữ liệu đầy cần hàng trăm KB, vượt RAM của ESP32 (đã đo). File dạng cũ (`schema: 1`, một object)
vẫn đọc được. Mọi lần ghi đi qua task ghi riêng (`store_save_later`), gom thay đổi trong 1 giây và đổi tên nguyên tử.
| `/data/conflict_ack.json` | TV4 | Xung đột người dùng đã xác nhận |
| NVS `wifi`, `auth`, `ddns`, `web` | TV1 | Cấu hình Wi-Fi; mật khẩu ban đầu (xoá khi đổi mật khẩu); DuckDNS; khoá link xem lịch |

Xoá toàn bộ dữ liệu: `idf.py erase-flash` rồi nạp lại firmware.

## 10.4. Configuration

`idf.py menuconfig` → **ESPEduShed - Platform (TV1)**:

| Mục | Mặc định | Ý nghĩa |
| :--- | :--- | :--- |
| `LD_WIFI_SSID`, `LD_WIFI_PASS` | rỗng | Wi-Fi mặc định khi NVS chưa có cấu hình; để trống thì ESP phát AP cài đặt |
| `LD_AP_PASS` | `espedushed` | Mật khẩu Wi-Fi AP cài đặt `ESPEduShed-XXXX` |
| `LD_TCP_PORT` | 5000 | Cổng TCP của server |
| `LD_MAX_CLIENTS` | 8 | Số kết nối tối đa |
| `LD_DDNS_DOMAIN`, `LD_DDNS_TOKEN` | rỗng | DuckDNS mặc định (đổi được trong app, mục Truy cập từ xa) |
| `LD_BOOT_BUTTON_GPIO` | 0 | Nút giữ 3 giây lúc khởi động để vào chế độ AP |

**ESPEduShed - Web xem lich**: `LD_HTTP_PORT` (mặc định 8080), cổng của trang web xem lịch.

---

# 11. Running the Project

## 11.1. Start Server

```bash
cd source/server
idf.py set-target esp32s3
idf.py build
idf.py -p COM5 flash monitor          # thay COM5 bằng cổng của bo mạch
```

Lần chạy đầu, log serial in mật khẩu ban đầu (link xem lịch không in ra log, lấy trong app mục **Truy cập từ xa**):

```text
W (1234) users:   Mat khau ban dau: Ab3dEf9h
I (1300) web: web xem lich o cong 8080, lay link trong app (muc Truy cap tu xa)
```

- Chưa cấu hình Wi-Fi: ESP phát Wi-Fi `ESPEduShed-XXXX` (mật khẩu `espedushed`), server ở `192.168.4.1:5000`. Laptop nối vào Wi-Fi đó rồi gửi cấu hình:

  ```bash
  python source/tools/tcp_client.py --host 192.168.4.1 --cmd system.wifi --data "{\"ssid\":\"TenWifi\",\"pass\":\"matkhau\"}"
  ```

- Đã có Wi-Fi: log in `co IP 192.168.x.y, TCP cong 5000`.

## 11.1b. Truy cập từ mạng ngoài (4G, Wi-Fi khác)

Hướng dẫn từng bước kèm xử lý sự cố: [`docs/huong-dan-truy-cap-tu-xa.md`](docs/huong-dan-truy-cap-tu-xa.md). Tóm tắt:

1. **IP cố định trong LAN:** trên router, đặt DHCP reservation (gán IP tĩnh theo MAC) cho ESP32.
2. **Port forwarding:** mở TCP **5000** (app) và TCP **8080** (web) từ WAN về IP đó.
3. **Kiểm tra CGNAT:** so IP WAN hiện trên trang quản trị router với IP ở whatismyip.com. Khác nhau nghĩa là nhà mạng
   dùng CGNAT, mở cổng không có tác dụng; gọi nhà mạng xin IP public (Viettel, VNPT, FPT thường hỗ trợ).
   ESP32 bắt Wi-Fi phát từ điện thoại (4G) cũng không mở cổng được, khi đó chỉ các máy cùng Wi-Fi đó vào được.
4. **DuckDNS:** tạo tên miền miễn phí ở duckdns.org, rồi trong app vào **Truy cập từ xa** nhập tên và token.
   ESP32 tự cập nhật IP mỗi 5 phút. Router đã có DDNS riêng thì bỏ qua bước này.
5. **App:** ở ngoài mạng nhà, màn hình kết nối nhập `ten.duckdns.org`, cổng 5000.
6. **Điện thoại:** trong app vào **Truy cập từ xa**, bấm **Sao chép link** và gửi cho điện thoại, ví dụ
   `http://ten.duckdns.org:8080/c/<khoá>`. Lộ link thì bấm **Đổi link**.

Lưu ý: kênh truyền không mã hoá. Mật khẩu không đi trên mạng, nhưng nội dung lịch, token và link xem lịch có thể bị
nghe lén trên Wi-Fi công cộng. Dùng mật khẩu mạnh.

## 11.2. Start Client

```bash
cd source/client
npm run dev            # chạy thử
npm run dist           # tạo bản cài đặt Windows trong source/client/release
```

**Người dùng không cần code:** tải `ESPEduShed-Setup-<phiên bản>.exe` ở mục [Releases](https://github.com/cuongcuongdt20-ui/ESPEduShed/releases/latest) và cài. Mỗi lần mở (và 4 giờ một lần khi đang mở), app hỏi GitHub Releases, có bản mới thì tải ngầm và hỏi khởi động lại để cài.

**Phát hành bản mới (TV1):** tăng `version` trong `source/client/package.json` (ví dụ `1.1.1`), commit và push lên `main`, rồi đẩy tag cùng số:

```bash
cd source/client && npm version 1.1.1 --no-git-tag-version && cd ../..
git add source/client/package.json source/client/package-lock.json
git commit -m "Client 1.1.1"
git push origin main
git tag client-v1.1.1
git push origin client-v1.1.1
```

Tag phải đúng `client-v<version trong package.json>`, nếu không workflow dừng ở bước kiểm tra ("Tag ... khong khop package.json"). Đặt sai thì xoá tag (`git tag -d <tag>` và `git push origin --delete <tag>`), sửa version rồi tag lại.

GitHub Actions ([`release-client.yml`](.github/workflows/release-client.yml)) chạy test, build bản cài và đưa lên Releases kèm `latest.yml`. Bản cài chưa ký số nên Windows SmartScreen sẽ cảnh báo lần cài đầu ("More info" → "Run anyway").

Màn hình đầu tiên: nhập IP hoặc tên DDNS của ESP (hoặc bấm tìm trong LAN) → nhập mật khẩu → đổi mật khẩu bắt buộc ở lần đầu.

**Chưa có ESP32:** chạy server giả lập rồi cho app nối vào `127.0.0.1:5000`:

```bash
python source/tools/fake_server.py --dev      # mật khẩu admin123
```

## 11.3. Start Multiple Clients

- Mở app trên nhiều laptop cùng mạng, hoặc chạy `npm run start` nhiều lần trên một máy.
- Hoặc dùng công cụ dòng lệnh để giữ một client chờ tin đẩy:

  ```bash
  python source/tools/tcp_client.py --host 192.168.1.37 --password <mat_khau> --listen 120
  ```

## 11.4. Run Test

```bash
# Logic C trên máy tính: platform (TV1); catalog, catalog_cmd (TV2); schedule, schedule_cmd,
# schedule_store (TV3); calendar, calendar_cmd (TV4)
# (Windows: cài GCC bằng `winget install BrechtSanders.WinLibs.POSIX.UCRT`, rồi trong Git Bash chạy
#  mingw32-make CC=gcc CJSON_DIR=C:/Espressif/v5.5.6/esp-idf/components/json/cJSON
#  Hoặc dùng MSYS2/WSL có gcc và make. restore_check, catalog_cmd, schedule_cmd,
#  schedule_store và calendar_cmd cần cJSON của ESP-IDF: lấy theo IDF_PATH sau khi chạy export
#  của ESP-IDF, hoặc đặt CJSON_DIR. Không thấy cJSON thì make báo bỏ qua 5 bộ đó.)
cd source/server/test_host
make                   # chạy cả 9 bộ test (4 bộ nếu không có cJSON)
# hoặc chạy từng phần: make platform | restore_check | catalog | catalog_cmd | schedule | schedule_cmd |
# schedule_store | calendar | calendar_cmd

# App: ghép khung, PBKDF2/HMAC phía client, gợi ý giờ tiết
cd source/client
npm test
npm run typecheck

# Giao thức với server thật (hoặc fake_server.py)
cd source/tools
python tcp_client.py --host 192.168.1.37 --cmd system.status
python loadtest.py fragment --host 192.168.1.37

# Toàn bộ thực nghiệm trên ESP32 bằng một lệnh, ghi CSV vào statics/results
# (kịch bản reset cần pyserial: chạy bằng Python của ESP-IDF hoặc pip install pyserial)
python run_experiments.py --host 192.168.1.37 --password <mat_khau> --serial COM3 --repeats 3
python loadtest.py http --host 192.168.1.37 --password <mat_khau>    # riêng phép so với HTTP

# Sau khi nạp firmware mới: chỉ chạy lại reset giữa lúc ghi và khôi phục dữ liệu (ESP khởi động lại)
python run_experiments.py --host 192.168.1.37 --password <mat_khau> --serial COM3 --only reset,restore

# Nhiều client cùng ghi: 4 client × 10 công việc, không mất, không trùng, có đủ trong file
python run_experiments.py --host 192.168.1.37 --password <mat_khau> --only concurrent

# Tìm server: gói đúng được trả lời, gói sai bị bỏ qua (không cần đăng nhập)
python loadtest.py discovery --host 192.168.1.37 --count 20
```

---

# 12. Main Features

Cột Test chỉ ghi những gì đã chạy thật. Test trên máy tính: Windows 11, GCC 16.2 (WinLibs), cJSON của ESP-IDF 5.5.6, Node.js 24. Trên bo: ESP32-S3, ngày 30/10/2026, xem mục 13.

| No. | Feature | Description | Test |
| :-: | :--- | :--- | :--- |
| 0 | Build và nạp firmware | Toàn bộ firmware (6 component) | Build đạt với ESP-IDF 5.5.6, không cảnh báo trong code của nhóm; `espedushed.bin` 1 258 960 byte (phân vùng app còn trống 40%); đã nạp và chạy trên bo |
| 1 | Ghép khung TCP | Khung bị cắt từng byte, nhiều khung dính, khung quá 8 KB | 3/3 đạt trên ESP32 (`fragment.csv`); `make platform`, `npm test` đạt |
| 2 | Giới hạn 8 kết nối | Kết nối thứ 9 nhận `BUSY` | Đạt trên ESP32 (`maxconn.csv`) |
| 3 | Đăng nhập challenge–response | Mật khẩu không đi trên mạng | Đăng nhập đạt trên ESP32; PBKDF2/HMAC khớp vector chuẩn (`npm test`) |
| 4 | Chống gửi lại gói đăng nhập | `nonce` dùng một lần | Đạt trên ESP32 (`replay.csv`) |
| 5 | Chặn dò mật khẩu | Sai 5 lần, khoá IP 60 giây | Đạt trên ESP32 (`lockout.csv`); `make platform` đạt 76/76 |
| 6 | Bắt đổi mật khẩu lần đầu | Mọi lệnh khác trả `FORBIDDEN` | Đạt với `fake_server.py`; chưa chạy riêng trên ESP32 |
| 7 | Tin đẩy thời gian thực | Sửa lịch ở client A, client B nhận `changed` | Đạt trên ESP32, trễ trung bình 147,7 ms (`compare_http_run*.csv`) |
| 8 | Tự nối lại khi mất kết nối | App thử lại mỗi 2 giây, tự đăng nhập lại nếu ESP đã khởi động lại | Đạt với `fake_server.py`; chưa đo với app trên ESP32 |
| 9 | Wi-Fi AP cài đặt, sao lưu / khôi phục | `system.wifi`, `system.backup`, `system.restore`; file khôi phục hỏng bị từ chối, giữ nguyên dữ liệu cũ | Sao lưu, khôi phục đạt 7/7 trên ESP32 (`fw_1.0.1/restore_check.csv`); `make restore_check` đạt 27/27; `system.wifi` chưa chạy trên ESP32 |
| 10 | Danh mục trường, bảng tiết (TV2) | Quy đổi tiết, tiết không liên tục, 25 lệnh CRUD, gợi ý tiết 50 phút trên form | `make catalog` đạt 19/19, `make catalog_cmd` đạt 178/178; `school.list`, `campus.list` đạt trên ESP32 |
| 11 | Lịch dạy, công việc, ngoại lệ (TV3) | Sinh buổi, huỷ, dời, đổi phòng, dạy bù, hạn chót; lưu JSON Lines | `make schedule` đạt 44/44, `make schedule_cmd` đạt 134/134, `make schedule_store` đạt 39/39; `occurrence.range`, `task.create`, `task.delete` đạt trên ESP32 |
| 12 | Lịch tuần FullCalendar, kiểm tra trùng (TV4) | Trùng giờ, thiếu thời gian di chuyển, xác nhận xung đột | `make calendar` đạt 29/29, `make calendar_cmd` đạt 97/97; `calendar.week`, `calendar.range` đạt trên ESP32 |
| 13 | Một tài khoản duy nhất | Đăng nhập chỉ bằng mật khẩu; `users.json` bản nhiều tài khoản tự chuyển sang, giữ tài khoản admin | Đăng nhập đạt trên ESP32 sau khi nạp đè firmware mới |
| 14 | Web xem lịch trên điện thoại | `/c/<khoá>`, sai khoá trả 404, `web.rotate` làm link cũ hết hiệu lực | API `/api/<khoá>/range` đạt trên ESP32 (`compare_http_run*.csv`); sai khoá trả 404 trên fw 1.0.1; nhóm đã mở trang trên điện thoại qua 4G (ảnh trong báo cáo); `web.rotate` làm link cũ trả 404 (`fw_1.0.1/protocol_check.csv`) |
| 15 | DuckDNS, truy cập từ mạng ngoài | Cập nhật IP mỗi 5 phút, app nối bằng tên miền | Log serial xác nhận ESP32 cập nhật DuckDNS thành công; qua hotspot điện thoại đạt 50/50 yêu cầu, RTT TB 69,84 ms, p95 159,2 ms (`fw_1.0.1/external/`) |
| 16 | Chọn màu cho lịch dạy, công việc, trường | Bảng 12 màu, hiện trên lịch app và web | `npm run typecheck` đạt; chưa kiểm tra trên giao diện với ESP32 |
| 17 | Mã QR cho link xem lịch | App tự sinh mã bằng `qrcode.react`, không gửi link ra dịch vụ ngoài | Giải mã ảnh QR sinh ra đúng link (jsQR, OpenCV); chưa quét bằng điện thoại thật |
| 18 | Tìm server bằng UDP, mDNS | Trả lời gói `ESPEDUSHED_DISCOVER`, bỏ qua gói sai | UDP gửi thẳng trên fw 1.0.1: 20/20 gói đúng, 3/3 gói sai bị bỏ qua (`fw_1.0.1/discovery.csv`); broadcast 10/10 và 9/10; ESP trả lời mDNS 5/5 (`fw_1.0.1/discovery_lan.csv`); `ping espedushed.local` trên laptop đo chưa phân giải được |
| 19 | Nhiều client cùng ghi | Handler chạy tuần tự, không mất hay trùng bản ghi, tin đẩy tới mọi client | 4 client × 10 công việc đạt 6/6 trên ESP32 (`fw_1.0.1/concurrent_write.csv`) |

---

# 13. Experimental Setup

## 13.1. Environment

| Mục | Giá trị |
| :--- | :--- |
| Server | ESP32-S3, flash 16 MB, PSRAM 8 MB chưa bật (đọc bằng `esptool flash_id`; bảng phân vùng dùng 8 MB), firmware từ commit `b4e0394`, đo ngày 30/10/2026 |
| Client | Laptop Windows 11 Home, Python 3.14 |
| Mạng | ESP32 và laptop cùng nối hotspot Wi-Fi 2,4 GHz phát từ điện thoại; RSSI của ESP từ −47 đến −50 dBm |
| Phần mềm | ESP-IDF 5.5.6, GCC 16.2, Node.js 24 |

## 13.2. Dataset

Không dùng dataset có sẵn. Dữ liệu thử là dữ liệu mẫu của firmware: 2 trường, 2 cơ sở, 2 lịch dạy, không có công việc; tuần 07/09–13/09/2026 có 2 buổi, khoảng 62 ngày có 18 buổi. Số liệu vì vậy chưa nói được gì về lịch dày.

## 13.3. Test Scenarios

Chạy tất cả (chạy bằng Python của ESP-IDF, hoặc `pip install pyserial`; dùng `--only` để chạy từng phần):

```bash
cd source/tools
python run_experiments.py --host <ip> --password <mat_khau> --serial COM3 --repeats 3
```

| Kịch bản | Cách làm | Đo gì | Kết quả ghi vào |
| :--- | :--- | :--- | :--- |
| 1. Ghép khung | Một khung gửi từng byte; 10 khung trong một send; độ dài 8 193 | Đạt / không đạt | `fragment.csv` |
| 2. Nhiều client | 3 lệnh × 1, 2, 4, 8 client × 200 yêu cầu × 3 lần | RTT trung bình, p95, lệnh/giây, số lỗi | `latency_*_run*.csv`, `latency_summary.csv` |
| 3. Giới hạn kết nối | Mở 10 kết nối | Kết nối 9, 10 nhận `BUSY` | `maxconn.csv` |
| 4. Bảo mật đăng nhập | Gửi lại gói `auth.login`; sai mật khẩu 6 lần | Gói cũ bị từ chối; lần 6 `BUSY` | `replay.csv`, `lockout.csv` |
| 5. Lỗi giao thức, biên | JSON hỏng, lệnh lạ, thiếu tham số, token sai, khung 0 byte; khoảng 1, 7, 62, 63 ngày | Đúng mã lỗi; client khác không bị ảnh hưởng | `protocol_errors.csv`, `range_limits.csv` |
| 6. So với HTTP | `calendar.range` (TCP) và `GET /api/<khoá>/range` (HTTP), cùng bo, cùng dữ liệu, 1 và 4 client, 3 lần | Byte mỗi yêu cầu, RTT, RAM mỗi kết nối, độ trễ nhận thay đổi | `compare_http_run*.csv` |
| 7. Reset giữa lúc ghi | Reset ESP qua chân EN 10 lần sau 0,2 giây và 10 lần sau 2 giây kể từ khi ghi | Dữ liệu cũ còn nguyên; thời gian trả lời lại | `reset_during_write.csv` |

## 13.4. Evaluation Metrics

- Thời gian khứ hồi (ms) đo ở client, từ lúc gửi yêu cầu tới lúc nhận đủ phản hồi.
- Số lệnh mỗi giây theo số client đồng thời; tỉ lệ lỗi.
- RAM trên ESP (`heapFree`, `heapMin`) và thời gian handler chậm nhất (`maxHandlerUs`) trong `system.status`.
- Thời gian ESP trả lời lại sau khi reset.

---

# 14. Experimental Results

Mọi số dưới đây đo trên ESP32-S3 thật, không phải `fake_server.py`. Dữ liệu gốc nằm trong `statics/results/`; số đo trước đó trên commit `c9cab28` giữ trong `statics/results/old_fw_c9cab28/`.

## 14.1. Functional Results

| Kịch bản | Mong đợi | Thực tế |
| :--- | :--- | :--- |
| 1. Khung cắt từng byte, 10 khung dính, khung 8 193 byte | Đúng id; đủ 10 phản hồi; server đóng kết nối | Đạt 3/3 |
| 3. Mở 10 kết nối | 1–8 nhận, 9 và 10 `BUSY` | Đạt |
| 4. Gửi lại gói `auth.login` trên kết nối khác | Bị từ chối | Đạt |
| 4. Sai mật khẩu 6 lần, đúng mật khẩu khi đang khoá, rồi sau 61 giây | Lần 1–5 `UNAUTHORIZED`, lần 6 `BUSY`, vẫn `BUSY`, rồi đăng nhập được | Đạt |
| 5. 9 trường hợp lỗi giao thức | Đúng mã lỗi; client khác vẫn ping được | Đạt 9/9 |
| 5. Truy vấn 1, 7, 62, 63 ngày, 2 lệnh | OK cho 1, 7, 62 ngày; 63 ngày `BAD_ARG` | Đạt 8/8 |
| 7. Reset sau 2 giây, 10 lần | Bản ghi còn, dữ liệu cũ nguyên | Đạt 10/10 |
| 7. Reset sau 0,2 giây, 10 lần | Bản ghi mới có thể mất, dữ liệu cũ nguyên | Mất 10/10 bản ghi mới, dữ liệu cũ nguyên 10/10 |

Sau reset, ESP trả lời lại sau trung bình 13,7 giây (3,6–31,4 giây, 20 lần). Reset qua chân EN khác rút nguồn thật vì flash vẫn có điện.

## 14.2. Performance Results

200 yêu cầu mỗi client, 3 lần, trung bình ± độ lệch chuẩn; không có lỗi. Hình: [`fig_rtt_theo_client.png`](statics/results/fig_rtt_theo_client.png).

| Lệnh | Số client | RTT TB (ms) | RTT p95 (ms) | Lệnh/giây |
| :--- | :-: | :-: | :-: | :-: |
| `system.ping` | 1 | 11,98 ± 1,01 | 13,85 ± 0,38 | 81,6 ± 6,3 |
| `system.ping` | 2 | 16,95 ± 3,24 | 17,64 ± 3,39 | 119,5 ± 20,9 |
| `system.ping` | 4 | 20,07 ± 5,80 | 43,09 ± 31,77 | 201,4 ± 48,3 |
| `system.ping` | 8 | 27,74 ± 6,09 | 58,95 ± 26,24 | 231,7 ± 35,8 |
| `calendar.week` | 1 | 16,20 ± 1,45 | 18,04 ± 3,41 | 60,0 ± 5,3 |
| `calendar.week` | 2 | 19,40 ± 4,58 | 28,07 ± 12,91 | 105,5 ± 27,2 |
| `calendar.week` | 4 | 29,89 ± 2,79 | 62,96 ± 25,08 | 126,7 ± 9,9 |
| `calendar.week` | 8 | 94,12 ± 13,88 | 208,78 ± 3,45 | 78,3 ± 11,0 |
| `occurrence.range` | 1 | 28,53 ± 23,59 | 85,25 ± 106,66 | 49,4 ± 28,5 |
| `occurrence.range` | 2 | 22,53 ± 4,64 | 64,32 ± 55,58 | 87,2 ± 21,5 |
| `occurrence.range` | 4 | 53,00 ± 42,46 | 184,37 ± 174,31 | 106,7 ± 69,1 |
| `occurrence.range` | 8 | 107,77 ± 8,67 | 253,43 ± 39,84 | 69,0 ± 5,7 |

`maxHandlerUs` sau lượt tải `calendar.week` 8 client (1 600 yêu cầu) là 8,8 ms; `heapMin` lúc đó 31 KB.

## 14.3. Comparison

So với `esp_http_server` trên cùng bo, cùng dữ liệu lịch tuần, 200 yêu cầu mỗi client, 3 lần, không có lỗi. HTTP giới hạn 4 phiên nên không đo 8 client.

| Chỉ số | TCP tự thiết kế | HTTP giữ kết nối | HTTP mở kết nối mới |
| :--- | :-: | :-: | :-: |
| Byte gửi + nhận mỗi yêu cầu | 125 + 496 = 621 | 138 + 626 = 764 | Không đo |
| RTT TB, 1 client (ms) | 33,95 ± 18,94 | 314,80 ± 26,54 | 418,97 ± 56,00 |
| RTT p95, 1 client (ms) | 107,20 ± 89,85 | 520,57 ± 98,84 | 755,70 ± 154,05 |
| RTT TB, 4 client (ms) | 45,89 ± 15,85 | 145,72 ± 2,73 | 146,94 ± 20,95 |
| RTT p95, 4 client (ms) | 147,82 ± 98,45 | 428,74 ± 75,31 | 448,36 ± 163,95 |
| Yêu cầu/giây, 4 client | 90,9 ± 39,8 | 25,3 ± 3,4 | 26,5 ± 4,3 |
| RAM mỗi kết nối đang chờ (byte) | 403 | 403 | Không đo |
| Độ trễ nhận thay đổi (ms) | 147,7 ± 5,4 (đo) | 30 000 (tính từ hỏi vòng 60 giây) | 30 000 (tính) |

Số byte tính ở tầng ứng dụng, chưa gồm header TCP/IP.

## 14.4. Kiểm thử trên firmware 1.0.1

Đo ngày 31/10/2026 trên cùng bo, dữ liệu gốc trong `statics/results/fw_1.0.1/`. Mạng khác với mục 13.1 nên chỉ dùng để xác định đúng hay sai, không so với số ở mục 14.1–14.3.

| Trường hợp | Mạng | Mong đợi | Thực tế | File |
| :--- | :--- | :--- | :--- | :--- |
| Ghép khung | Wi-Fi nhà; mạng ngoài | 3/3 | Đạt 3/3 ở cả hai | `fragment.csv`, `external/fragment.csv` |
| Mở 10 kết nối | Wi-Fi nhà; mạng ngoài | 1–8 nhận, 9 và 10 `BUSY` | Đạt ở cả hai | `maxconn.csv`, `external/maxconn.csv` |
| Web sai khoá | Wi-Fi nhà; mạng ngoài | 404 | 404 ở cả hai (lệnh `curl`, không có CSV) | |
| Tìm server, UDP gửi thẳng | Wi-Fi nhà | 20/20 gói đúng được trả lời, 3 gói sai bị bỏ qua | Đạt, RTT TB 169,27 ms | `discovery.csv` |
| Tìm server, UDP broadcast | Wi-Fi nhà | ESP trả lời đúng IP và cổng | Mạng con 10/10, RTT TB 104,07 ms; `255.255.255.255` 9/10, RTT TB 134,84 ms | `discovery_lan.csv` |
| mDNS, truy vấn A `espedushed.local` | Wi-Fi nhà | ESP trả lời đúng IP | Đạt 5/5 | `discovery_lan.csv` |
| Truy cập từ mạng ngoài qua DuckDNS | Laptop dùng hotspot điện thoại | 50/50 `system.ping` có phản hồi | Đạt 50/50, RTT TB 69,84 ms, p95 159,2 ms, 13,8 lệnh/giây | `external/latency_system_ping.csv` |
| 4 client cùng ghi, mỗi client 10 công việc | Mạng ngoài | 40/40 bản ghi, không trùng id, có đủ trong file, đủ tin đẩy | Đạt 6/6, xong trong 1,36 giây | `concurrent_write.csv` |
| Reset sau 2 giây, 10 lần | Mạng ngoài | Bản ghi còn, dữ liệu cũ nguyên | Đạt 10/10 | `reset_during_write.csv` |
| Reset sau 0,2 giây, 10 lần | Mạng ngoài | Bản ghi mới có thể mất, dữ liệu cũ nguyên | Mất 10/10 bản ghi mới, dữ liệu cũ nguyên 10/10 | `reset_during_write.csv` |
| Khôi phục dữ liệu | Mạng ngoài | Bản cắt và bản rác trả `BAD_ARG`, file giữ nguyên; bản đúng trùng từng byte sau khởi động lại | Đạt 7/7 | `restore_check.csv` |
| Mọi lệnh trong `docs/protocol.md` (`python protocol_check.py`) | Mạng ngoài | Đúng mã kết quả và dữ liệu; lệnh cần đăng nhập từ chối khi không có token | Đạt 174/174 trường hợp, đủ 55 lệnh; `system.wifi`, `system.ddns` chỉ thử nhánh lỗi vì thành công sẽ đổi Wi-Fi hoặc tắt DuckDNS | `protocol_check.csv` |

Sau reset, ESP trả lời lại sau trung bình 4,0 giây (3,1–7,7 giây, 20 lần). Hai phép thử broadcast, mDNS dùng script gửi gói trực tiếp nên chưa thay thế được việc bấm nút tìm server trên app. Lần trượt 1/10 với `255.255.255.255` chưa rõ nguyên nhân.

---

# 15. Discussion

1. **Đạt mục tiêu:** ghép khung, giới hạn kết nối, chống gửi lại, khoá IP, xử lý lỗi và giữ dữ liệu khi reset đều đạt trên bo thật. Truy cập từ mạng ngoài qua DuckDNS đạt với 1 client (mục 14.4). Chưa kiểm chứng: rút nguồn thật, lịch dày, chạy liên tục nhiều ngày.
2. **Mức dùng thật là 1–2 client**, vì sản phẩm phục vụ một người với vài thiết bị: `system.ping` 12–17 ms, `calendar.week` 16–19 ms. 8 client là phép thử giới hạn. Handler chạy tuần tự nên yêu cầu đến cùng lúc phải xếp hàng; `calendar.week` và `occurrence.range` đã giảm thông lượng từ 4 lên 8 client.
3. **Nút thắt không nằm ở handler:** `maxHandlerUs` chỉ 8,8 ms trong khi p95 của `calendar.week` với 8 client là 208,78 ms, rất ổn định. Điều này gợi ý một bộ định thời của TCP hoặc Wi-Fi, nhưng chưa bắt gói để xác nhận.
4. **So với HTTP:** lợi thế đáng tin cậy là ít hơn 143 byte mỗi yêu cầu (19%) và server chủ động đẩy tin; RAM mỗi kết nối bằng nhau. RTT thấp hơn 9,3 lần với 1 client chỉ áp dụng cho `esp_http_server` cấu hình mặc định: thư viện gửi từng phần header bằng lệnh gửi riêng, có thể là nguyên nhân chính nhưng chưa đo tách, nên không kết luận giao thức HTTP chậm.
5. **Hạn chế của thí nghiệm:** một bo, một hotspot, một laptop; dữ liệu thử rất ít; mỗi cấu hình 3 lần và một số ô có độ lệch lớn hơn trung bình, nên chỉ đọc xu hướng. Lần đo cũ trên commit `c9cab28` cho `calendar.week` 8 client đạt 238,9 lệnh/giây so với 78,3 hiện tại; hai lần khác ngày và khác điều kiện mạng nên chưa kết luận firmware mới chậm đi.

---

# 16. Limitations

1. **Số kết nối:** tối đa 8 client, do số socket của lwIP và RAM của ESP32-S3.
2. **Kích thước phản hồi:** mỗi khung tối đa 8 KB, nên truy vấn lịch giới hạn 62 ngày mỗi lần; sao lưu đi theo từng mẩu 4 KB.
3. **Kênh truyền chưa mã hoá:** mật khẩu không đi trên mạng, nhưng dữ liệu lịch, token và link xem lịch thì có. Khi đổi mật khẩu, `key` đi trên mạng một lần. Khi mở cổng ra Internet, nên tránh Wi-Fi công cộng.
4. **Phiên mất khi ESP khởi động lại:** token lưu trong RAM; app tự đăng nhập lại nếu còn mở, nếu đã tắt thì người dùng đăng nhập lại.
5. **Không có giờ thực khi chạy ở chế độ AP:** không có Internet để đồng bộ SNTP, app phải gửi ngày cần xem.
6. **Các handler chạy tuần tự:** một lệnh xử lý lâu làm các client khác chờ.
7. **Truy cập từ xa cần IP public:** nhà mạng dùng CGNAT (hoặc ESP32 bắt Wi-Fi 4G từ điện thoại) thì không mở cổng được.
8. **Web xem lịch qua HTTP thường:** khoá bí mật nằm trong URL nên ai nghe lén được đường truyền (Wi-Fi công cộng, proxy) đều đọc được link; khi nghi lộ thì bấm **Đổi link**. FullCalendar nhúng trong firmware (`/fc.js`), không tải từ CDN.
9. **Ghi trễ 1 giây:** dừng đột ngột trong cửa sổ này mất thay đổi cuối, dù file không hỏng (mục 14.1).
10. **Không chống yêu cầu trùng:** tạo mới đã xử lý nhưng mất phản hồi, bấm lại sẽ tạo bản ghi thứ hai.
11. **Web xem lịch không nhận tin đẩy:** trang tự tải lại khi chuyển tuần, khi mở lại và mỗi phút; muốn thấy ngay thì bấm **Làm mới**.
12. **Đoán mật khẩu ngoại tuyến:** kẻ nghe lén lúc đăng nhập có `nonce`, `salt`, `iter` và `proof` nên có thể thử đoán nếu mật khẩu yếu. Token DuckDNS đi dạng rõ từ app tới ESP32 khi cấu hình bằng `system.ddns`; từ ESP32 tới DuckDNS thì qua HTTPS.
13. **Khôi phục dữ liệu:** `system.restore` chỉ kiểm tra cấu trúc file (JSON Lines hoặc object JSON có `schema`), không kiểm tra từng bản ghi. File đúng cấu trúc nhưng sai nội dung vẫn thay file thật, ví dụ dữ liệu lịch gửi nhầm vào `users.json`, hoặc file JSON Lines bị cắt đúng ở cuối một dòng.
14. **Phiên bản firmware:** số đo hiệu năng trong mục 14 thuộc commit `b4e0394` (fw 1.0.0). Fw 1.0.1 thêm kiểm tra kết quả `fsync`, `fclose` và kiểm tra file khôi phục, không đổi đường đọc đã đo. Đã nạp lên bo và chạy lại đạt, gồm reset giữa lúc ghi và khôi phục dữ liệu (mục 14.4); chưa đo lại các bảng hiệu năng trên fw 1.0.1.

---

# 17. Future Work

1. Mã hoá kênh truyền bằng TLS (mbedTLS) với bộ đệm nhỏ, hoặc bật PSRAM 8 MB có sẵn trên bo (firmware hiện chưa dùng).
2. Mã hoá phân vùng NVS và dùng Secure Boot.
3. Xuất lịch sang định dạng iCalendar (`.ics`).
4. Thêm màn hình e-paper trên ESP32 hiển thị lịch trong ngày.
5. Truy cập từ xa không cần mở cổng: Tailscale qua một máy luôn bật trong nhà, hoặc ESP32 tự nối ra một máy chủ trung gian (reverse tunnel) để chạy được cả khi bị CGNAT hay dùng 4G.
6. Trang web xem lịch nhận cập nhật tức thì qua WebSocket thay vì tải lại mỗi phút.

---

# 18. Report

```text
report/Nhom08_ESPEduShed_Report.docx      (nộp trên Compilatio)
```

Báo cáo phải kiểm tra trên Compilatio (yêu cầu ≤ 20%). Kết quả Compilatio: chưa kiểm tra.

---

# 19. References

1. R. Fielding, M. Nottingham, J. Reschke, "HTTP/1.1", RFC 9112, IETF, 2022.
2. W. Eddy, Ed., "Transmission Control Protocol (TCP)", RFC 9293, IETF, 2022.
3. K. Moriarty, B. Kaliski, A. Rusch, "PKCS #5: Password-Based Cryptography Specification Version 2.1", RFC 8018, IETF, 2017.
4. H. Krawczyk, M. Bellare, R. Canetti, "HMAC: Keyed-Hashing for Message Authentication", RFC 2104, IETF, 1997.
5. S. Cheshire, M. Krochmal, "Multicast DNS", RFC 6762, IETF, 2013.
6. Espressif Systems, *ESP-IDF Programming Guide* v5.5: lwIP, HTTP Server, ESP HTTP Client, NVS, Wi-Fi, mbedTLS.
7. Electron, *Process Model*, *Context Isolation*, *Inter-Process Communication*.
8. B. Desruisseaux, Ed., "Internet Calendaring and Scheduling Core Object Specification (iCalendar)", RFC 5545, IETF, 2009.
9. A. Dunkels, "Full TCP/IP for 8-bit architectures", MobiSys, 2003.
10. W. R. Stevens, B. Fenner, A. M. Rudoff, *UNIX Network Programming*, Vol. 1, 3rd ed., Addison-Wesley, 2003.
11. FullCalendar, *Documentation* v6; DuckDNS, *Specification*.

---

# 20. Team Contribution

| Student | Main Contribution | Percentage |
| :--- | :--- | ---: |
| Nguyễn Duy Cường | TCP server (`select()`, ghép khung, hàng đợi gửi, tin đẩy), giao thức và bảng lệnh, đăng nhập challenge–response, phiên, chặn dò mật khẩu, tài khoản, Wi-Fi/AP, SNTP, lưu trữ, sao lưu; web xem lịch trên điện thoại, DuckDNS; app: TCP client, IPC, đăng nhập, khung giao diện, trang truy cập từ xa và trạng thái, bảng chọn màu; công cụ `tcp_client.py`, `fake_server.py`, `loadtest.py` | 25% |
| Nguyễn Khánh Linh | Danh mục trường, bảng tiết, quy đổi tiết, ngày nghỉ, môn, lớp; UDP discovery và mDNS; giao diện danh mục | 25% |
| Nguyễn Hoàng Nam | Lịch dạy, công việc, ngoại lệ, sinh buổi; biểu mẫu lịch | 25% |
| Nguyễn Quang Minh | Kiểm tra trùng lịch, lịch tuần; giao diện FullCalendar, bảng xung đột, xuất PDF | 25% |
| **Total** | | **100%** |

---

# 21. Demo

## 21.1. Demo Environment

- ESP32-S3 cấp nguồn USB, nối Wi-Fi phòng lab hoặc hotspot điện thoại.
- Hai laptop cùng mạng chạy app; một máy mở log serial của ESP.
- Một điện thoại mở link xem lịch: cùng mạng với ESP32, hoặc dùng 4G nếu router đã mở cổng (xem [hướng dẫn](docs/huong-dan-truy-cap-tu-xa.md)).

## 21.2. Demo Steps

1. App tìm thấy ESP qua UDP (hoặc nhập IP).
2. Đăng nhập bằng mật khẩu in trên serial, đổi mật khẩu.
3. Tạo 2 trường có bảng tiết khác nhau.
4. Nhập lịch dạy ở cả hai trường và thấy cảnh báo trùng.
5. Huỷ một buổi, thêm buổi bù.
6. Thêm hạn nộp báo cáo (công việc có hạn chót), chọn màu riêng cho công việc và một lịch dạy.
7. Mở link xem lịch trên điện thoại: thấy đúng lịch tuần và màu như trên app.
8. Hai laptop cùng mở: sửa ở máy này, máy kia tự cập nhật nhờ tin đẩy.
9. Rút điện ESP: app chuyển "Mất kết nối", thử nối lại mỗi 2 giây; cắm lại, app tự nối và đăng nhập lại, dữ liệu còn nguyên.
10. Mở log serial, giải thích các dòng kết nối, đóng kết nối, đăng nhập sai.

---

# 22. Submission

## 22.1. Source Code

Thư mục `Nhom08_ESPEduShed_Source/` gồm toàn bộ repo này (trừ `node_modules/`, `build/`): mã nguồn firmware và app, cấu hình (`partitions.csv`, `sdkconfig.defaults`, `package.json`), công cụ và test, `README.md`, `docs/protocol.md`, kết quả đo trong `statics/results/`.

## 22.2. Report

`Nhom08_ESPEduShed_Report.docx` nộp trên Compilatio; bản cuối đặt ở `report/`.

## 22.3. Required Contents

```text
Nhom08_ESPEduShed/
├── Source/      (repo này)
└── Report/
```

---

# 23. Final Verification

Đánh dấu khi đã kiểm tra thật:

```text
[x] Project can run              firmware build và nạp được
[x] Server can start             ESP mở TCP 5000 (log "TCP server lang nghe cong 5000")
[x] Client can connect           công cụ Python và app kết nối được trên fw 1.0.1, cả từ mạng ngoài (mục 14.4, ảnh trong báo cáo)
[x] Network communication works  174/174 trên ESP32, cả 55 lệnh (protocol_check.csv); system.wifi, system.ddns chỉ thử nhánh lỗi
[x] Protocol is documented       docs/protocol.md
[x] Multiple clients tested      8 client trên ESP32 (latency_summary.csv)
[x] Error handling tested        BAD_JSON, UNKNOWN_CMD, BAD_ARG, BUSY, khung sai độ dài
[x] Experiments completed        mục 14 có số liệu thật
[x] Results are reproducible     CSV trong statics/results, lệnh ở mục 13.3
[x] Novelty is clearly stated    mục 7
[x] README is complete
[ ] Report is complete
[x] References are included      mục 19
[ ] Compilatio <= 20%
[ ] Google Drive link is accessible
[ ] All team members understand the project
```

---

# 24. Contact

Mọi thông tin trao đổi hoặc thắc mắc về đề tài **ESPEduShed**, vui lòng liên hệ:
- **Trưởng nhóm:** Nguyễn Duy Cường – Email: CuongND.B23CN112@ptit.edu.vn
- **Giảng viên phụ trách học phần:** ThS. Ngô Văn Quyền – Bộ môn Lập trình mạng.
