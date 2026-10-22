# Đặc tả giao thức ESPEduShed

Tài liệu dùng chung của cả nhóm. **Mỗi thành viên chỉ sửa mục lệnh của mình.** Phần 1 (khung tin, định dạng chung)
và phần 2 (lệnh `auth.*`, `system.*`) do TV1 viết và đã cài đặt trong firmware.

## 1. Quy ước chung

### 1.1 Kết nối

| Mục | Giá trị |
|---|---|
| Giao thức vận chuyển | TCP, một kết nối giữ lâu dài, hai chiều |
| Cổng | 5000 (`CONFIG_LD_TCP_PORT`) |
| Số kết nối tối đa | 8 (`CONFIG_LD_MAX_CLIENTS`). Kết nối thứ 9 nhận khung `BUSY` có `id` 0 rồi bị đóng |
| Đóng kết nối | Client im lặng quá 60 giây, hoặc kết nối 30 giây chưa đăng nhập, hoặc khung sai độ dài |
| Giữ kết nối | Client gửi `system.ping` mỗi 15 giây; server bật thêm TCP keep-alive (30 giây, 5 giây × 3 lần) |
| Tìm server | UDP broadcast cổng 5001 (mục 6), mDNS `espedushed.local` |

### 1.2 Khung tin

```text
+--------------------------+------------------------------------------+
| 4 byte độ dài, big-endian | N byte JSON UTF-8 (1 ≤ N ≤ 8192)        |
+--------------------------+------------------------------------------+
```

- Độ dài không tính chính 4 byte đầu.
- `N = 0` hoặc `N > 8192`: luồng byte đã lệch, server đóng kết nối ngay.
- Hai bên tự ghép khung, vì TCP không giữ ranh giới tin: một khung có thể đến qua nhiều lần `recv`, và nhiều khung có
  thể đến trong một lần `recv`. Cài đặt tham khảo: `source/server/components/platform/src/frame.c` (C),
  `source/client/electron/frame.ts` (TypeScript), `source/tools/espproto.py` (Python).

### 1.3 Yêu cầu, phản hồi, tin đẩy

```json
// yêu cầu (client → server)
{"id": 12, "cmd": "school.list", "token": "9f2c…", "data": {}}

// phản hồi thành công: mang lại đúng id
{"id": 12, "ok": true, "data": {"schools": []}}

// phản hồi lỗi
{"id": 12, "ok": false, "error": {"code": "BAD_ARG", "message": "Thieu ten truong", "field": "name"}}

// lỗi kèm dữ liệu (ví dụ danh sách trùng lịch)
{"id": 12, "ok": false, "error": {"code": "CONFLICT", "message": "...", "field": null}, "data": {"conflicts": []}}

// tin đẩy (server → mọi client đã đăng nhập), không có id
{"push": "changed", "entity": "schedule", "id": 5}
```

- `id`: số nguyên do client chọn, client dùng để ghép phản hồi với yêu cầu. Server không kiểm tra trùng.
- `token`: bắt buộc với mọi lệnh có vai trò tối thiểu khác "công khai".
- `data`: object, có thể bỏ trống.
- Thông báo lỗi (`message`) của firmware là tiếng Việt không dấu, để gọn và tránh lỗi mã hoá trên log serial.

### 1.4 Mã lỗi

| code | Khi nào |
|---|---|
| `BAD_JSON` | Nội dung khung không phải JSON object |
| `UNKNOWN_CMD` | Không có lệnh này (`message` là tên lệnh) |
| `BAD_ARG` | Thiếu hoặc sai tham số; `field` là tên tham số |
| `UNAUTHORIZED` | Không có token, token hết hạn, hoặc đăng nhập sai |
| `FORBIDDEN` | Không đủ quyền, hoặc tài khoản phải đổi mật khẩu trước |
| `NOT_FOUND` | Không có bản ghi, hoặc bản ghi thuộc người khác |
| `CONFLICT` | Trùng lịch (kèm `data.conflicts`), trùng tên, hoặc đang được dùng nên không xoá được |
| `FULL` | Hết chỗ lưu (RAM hoặc flash) |
| `BUSY` | Đủ kết nối, hoặc IP đang bị chặn vì sai mật khẩu nhiều lần |
| `INTERNAL` | Lỗi server, hoặc lệnh chưa làm |

App thêm hai mã phía client: `OFFLINE` (chưa kết nối hoặc mất kết nối) và `TIMEOUT` (5 giây không có phản hồi).

### 1.5 Vai trò

`student` (1) < `teacher` (2) < `admin` (3). Mỗi lệnh có vai trò tối thiểu; "công khai" nghĩa là không cần token.
Tài khoản đang bị bắt đổi mật khẩu chỉ được dùng `auth.password`, `auth.logout`, `auth.me`, `system.ping`,
`system.status`.

### 1.6 Kiểu dữ liệu

Ngày `"YYYY-MM-DD"`, giờ `"HH:MM"`, thứ 1 = thứ Hai … 7 = Chủ nhật, múi giờ UTC+7.

## 2. Lệnh của TV1

### 2.1 Đăng nhập challenge–response

```text
client                                                   server
  | auth.challenge {user}                                   |
  |------------------------------------------------------->|  sinh nonce 16 byte, gắn với kết nối này, hạn 30 giây
  |               {nonce, salt, iter}                       |
  |<-------------------------------------------------------|
  | key   = PBKDF2-HMAC-SHA256(mật khẩu, salt, iter, 32)    |
  | proof = HMAC-SHA256(key, nonce)                         |
  | auth.login {user, nonce, proof}                         |
  |------------------------------------------------------->|  so proof với HMAC(key đã lưu, nonce); xoá nonce
  |   {token, expiresAt, user, mustChangePassword}          |
  |<-------------------------------------------------------|
```

- Mật khẩu không đi trên mạng. Server chỉ lưu `salt`, `iter`, `key`.
- `nonce` dùng một lần, gắn với kết nối đã xin nó: gửi lại gói `auth.login` cũ (kể cả trên kết nối khác) đều bị từ chối.
- Tài khoản không tồn tại vẫn nhận `salt` (giả, cố định theo tên) để không dò được tên nào có thật.
- Một IP sai 5 lần trong 60 giây thì bị chặn 60 giây.
- `nonce` sai, hết hạn, sai tên hay sai mật khẩu đều trả cùng một thông báo `UNAUTHORIZED`.

### 2.2 Bảng lệnh `auth.*`

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `auth.challenge` | công khai | `{user}` | `{nonce, salt, iter}` | `BUSY` (IP bị chặn), `BAD_ARG` |
| `auth.login` | công khai | `{user, nonce, proof}` | `{token, expiresAt, user: {id, name, role}, mustChangePassword}` | `UNAUTHORIZED`, `BUSY` |
| `auth.logout` | student | – | `{}` | |
| `auth.me` | student | – | `{user: {id, name, role}, mustChangePassword}` | `UNAUTHORIZED` |
| `auth.password` | student | `{salt, iter, key, user?}` | `{}` | `FORBIDDEN` (đổi của người khác mà không phải admin), `NOT_FOUND` |
| `auth.register` | admin | `{user, role, salt, iter, key}` | `{id}` | `CONFLICT` (trùng tên), `FULL` (quá 32 tài khoản) |
| `auth.users` | admin | – | `{users: [{id, name, role, mustChangePassword}]}` | |
| `auth.remove` | admin | `{user}` | `{}` | `NOT_FOUND`, `BAD_ARG` (tự xoá mình) |

- `user`: 3..32 ký tự `A-Z a-z 0-9 . _ -`. `role`: `admin`, `teacher`, `student`.
- `salt`: 32 ký tự hex (16 byte). `key`: 64 ký tự hex (32 byte). `iter`: 1000..200000, app dùng 10000.
- `expiresAt`: ngày hết hạn của token (7 ngày), `null` khi ESP chưa đồng bộ giờ.
- `auth.password` không có `user`: đổi mật khẩu của mình. Có `user` (admin): đặt lại mật khẩu người khác, người đó phải
  đổi lại ở lần đăng nhập tới. Sau khi đổi, mọi token cũ của người đó mất hiệu lực.
- `auth.register`: tài khoản mới luôn có `mustChangePassword = true`.
- Tin đẩy: `auth.register`, `auth.remove` đẩy `{"push":"changed","entity":"user","id":<id>}`.

### 2.3 Bảng lệnh `system.*`

| Lệnh | Vai trò | `data` gửi | `data` nhận |
|---|---|---|---|
| `system.ping` | công khai | – | `{pong}` (ms từ lúc ESP khởi động) |
| `system.status` | công khai | – | `{name, fw, mode, ip, rssi?, uptimeS, timeSynced, clients, maxClients, sessions, heapFree, heapMin, heapLargest, requests, errors, maxHandlerUs, dataTotal, dataUsed}` |
| `system.wifi` | công khai ở chế độ AP, admin ở chế độ STA | `{ssid, pass}` | `{restarting: true}`, ESP khởi động lại sau 1,5 giây |
| `system.backup` | admin | `{}` | `{files: [{name, size}]}` |
| `system.backup` | admin | `{file, offset}` | `{file, offset, size, data, eof}`; `data` là base64 của tối đa 4096 byte |
| `system.restore` | admin | `{file, offset, data, last?, reboot?}` | `{written, restarting?}` |

- `system.status` dùng cho trang trạng thái của app và cho đo thực nghiệm (RAM, số yêu cầu, handler chậm nhất).
- Sao lưu / khôi phục đi theo từng mẩu 4 KB vì một khung tối đa 8 KB. `file` chỉ gồm chữ thường, số, `_`, đuôi `.json`.
  Khôi phục ghi vào `<file>.restore`, mẩu cuối (`last: true`) mới đổi tên đè file thật. Khôi phục xong file cuối thì
  gửi `reboot: true` để mọi module nạp lại dữ liệu.

## 3. Lệnh của TV2 (`school.*`, `campus.*`, `periods.*`, `holiday.*`, `subject.*`, `class.*`, `travel.*`)

> TV2 điền mục này. Hiện firmware có `school.list` (chạy được) và bản giả của `school.create`, `periods.set`,
> `holiday.list`, `travel.get` (trả `INTERNAL` "Chua lam").

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `school.list` | student | – | `{schools: [{id, name, shortName, color, periods: [{no, start, end}]}]}` | |

## 4. Lệnh của TV3 (`schedule.*`, `exception.*`, `task.*`, `occurrence.*`)

> TV3 điền mục này. Hiện firmware có `occurrence.range` (chạy được) và bản giả của `schedule.list`,
> `schedule.create`, `exception.set`, `task.list`, `task.create`.

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `occurrence.range` | student | `{from, to}` | `{items: [{sourceId, kind, status, date, origDate, start, end, school, campus}]}` | `BAD_ARG` (quá 62 ngày) |

## 5. Lệnh của TV4 (`calendar.*`, `conflict.*`)

> TV4 điền mục này. Hiện firmware có `calendar.week`, `calendar.range` (chạy được, chưa gắn xung đột) và bản giả của
> `conflict.list`, `conflict.check`, `conflict.ack`.

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `calendar.week` | student | `{date?}` | `{from, to, items: [{sourceId, kind, status, date, origDate, start, end, title, room, classCode, school, color}], conflicts: []}` | `BAD_ARG` (ESP chưa có giờ mà không gửi `date`) |
| `calendar.range` | student | `{from, to}` | như `calendar.week` | `BAD_ARG` (quá 62 ngày) |

## 6. Tìm server qua UDP (TV2)

```text
client → broadcast 255.255.255.255:5001   "ESPEDUSHED_DISCOVER"
ESP    → unicast về địa chỉ người gửi      {"name":"espedushed","ip":"192.168.1.37","port":5000,"fw":"1.0.0"}
```

Tin không đúng nội dung thì bỏ qua. Tối đa 10 lần trả lời mỗi giây.
