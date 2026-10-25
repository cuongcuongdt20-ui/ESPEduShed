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

> TV2: `school.list` trả về trường và bảng tiết; các màn hình danh mục gọi thêm `campus.*`, `holiday.*`, `subject.*`, `class.*` và `travel.*`.
> Mọi lệnh đọc yêu cầu vai trò student trở lên; lệnh thay đổi yêu cầu teacher trở lên (role admin cũng được phép).

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `school.list` | student | `{}` | `{schools: [{id, name, shortName, color, periods: [{no, start, end}]}]}` | |
| `school.create` / `school.update` | teacher | `{name, shortName, color}` / thêm `id` | `{id}` | `BAD_ARG`, `FULL`, `CONFLICT` |
| `school.delete` | teacher | `{id}` | `{}` | `NOT_FOUND`, `CONFLICT` nếu lịch đang dùng trường |
| `campus.list` | student | `{schoolId?}` | `{campuses: [{id, schoolId, name, address}]}` | |
| `campus.create` / `campus.update` / `campus.delete` | teacher | trường/cơ sở tương ứng | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `periods.set` | teacher | `{schoolId, periods: [{no, start, end}]}` | `{}` | `BAD_ARG` nếu giờ sai, tiết trùng hoặc chồng lấn |
| `holiday.list` | student | `{schoolId?}` | `{holidays: [{id, schoolId, name, fromDate, toDate}]}` | |
| `holiday.create` / `holiday.update` / `holiday.delete` | teacher | `{schoolId, name, from, to}` (`YYYY-MM-DD`; `schoolId: 0` áp dụng tất cả trường); sửa/xoá thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `subject.list` | student | `{}` | `{subjects: [{id, code, name, credits}]}` | |
| `subject.create` / `subject.update` / `subject.delete` | teacher | `{code, name, credits}`; sửa/xoá thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` (trùng mã môn, hoặc xoá môn còn lớp) |
| `class.list` | student | `{schoolId?}` | `{classes: [{id, code, subjectId, schoolId, size}]}` | |
| `class.create` / `class.update` / `class.delete` | teacher | `{code, subjectId, schoolId, size}`; sửa/xóa thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` nếu lịch đang dùng lớp |
| `travel.get` | student | `{}` | `{travel: [{campusA, campusB, minutes}], defaultTravel}` | |
| `travel.set` / `travel.delete` | teacher | `{campusA, campusB, minutes}` / `{campusA, campusB}` | `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `travel.setDefault` | teacher | `{minutes}` | `{}` | `BAD_ARG` |


## 4. Lệnh của TV3 (`schedule.*`, `exception.*`, `task.*`, `occurrence.*`)

Các lệnh sử dụng TCP request/response theo giao thức chung. Server xác định chủ sở hữu dữ liệu từ `ctx->user_id`; client không được tự gửi `owner_id` để xác định chủ sở hữu bản ghi.

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng / trạng thái |
|---|---|---|---|---|
| `occurrence.range` | student | `{from, to}` | `{items: [{sourceId, kind, status, date, origDate, start, end, school, campus}]}` | `BAD_ARG` nếu ngày sai, khoảng ngày quá 62 ngày hoặc quá nhiều buổi |
| `schedule.list` | student | `{}` | `{schedules: [lịch]}`, mỗi lịch `{id, subject_code, subject_name, class_code, class_id, credits, school_id, campus_id, room, size, day_of_week, time_mode, period_start, period_count, start, end, date_from, date_to, weekly, note}` | |
| `schedule.get` | student | `{id}` | `{schedule: lịch}` | `NOT_FOUND` (không có, hoặc của người khác) |
| `schedule.create` | teacher | `{subject_code, subject_name, school_id, date_from, date_to, day_of_week, time_mode, period_start, period_count \| start, end, weekly?, class_code?, class_id?, credits?, campus_id?, room?, size?, note?, force?}` | `{id}` | `BAD_ARG`, `CONFLICT` (kèm `conflicts`), `FULL` |
| `schedule.update` | teacher | như `schedule.create`, thêm `id` | `{id, removedExceptions}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` |
| `schedule.delete` | teacher | `{id}` | `{}` | `NOT_FOUND` |
| `exception.set` | teacher | `{series_id, orig_date, type, new_date?, new_start?, new_end?, new_room?, note?, force?}` | `{message}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT`, `FULL` |
| `exception.delete` | teacher | `{series_id, orig_date}` | `{}` | `NOT_FOUND` |
| `task.list` | student | `{}` | `{tasks: [{id, type, title, content, day, until, start, end, deadline, campus_id, place, status, note}]}` | |
| `task.create` | student | `{title, day, start, end \| deadline: true, until?, type?, content?, place?, campus_id?, status?, note?, force?}` | `{id}` | `BAD_ARG`, `CONFLICT`, `FULL` |
| `task.update` / `task.delete` | student | như `task.create` thêm `id` / `{id}` | `{id}` / `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` |

### Quy ước trường

- Ngày dùng `YYYY-MM-DD`; giờ dùng `HH:MM`.
- `day_of_week`: 1 = thứ Hai … 7 = Chủ nhật. `weekly: false` là lịch một lần vào `date_from` (bỏ qua `date_to`, `day_of_week`). Lịch lặp kéo dài tối đa 366 ngày.
- `time_mode`: `0` theo tiết (`period_start`, `period_count`, giờ thực quy đổi bằng bảng tiết của trường mỗi lần sinh buổi; khoảng tiết không được vắt qua tiết bị khuyết), `1` theo giờ (`start`, `end`).
- **Kiểm tra trùng ở server:** `schedule.create/update`, `exception.set` (đổi giờ, dời, dạy bù) và `task.create/update` (công việc có khoảng giờ) gọi `conflict_check` của TV4. Trùng giờ hoặc không đủ thời gian di chuyển thì trả `CONFLICT` kèm `data.conflicts: [{date, start, end, reason, with: {sourceId, kind, title, start, end}}]`; gửi lại với `force: true` để vẫn lưu.
- `exception.set` dùng `series_id` và `orig_date` để xác định buổi gốc; mỗi buổi gốc có một ngoại lệ, đặt lại thì thay. `type`: `1` huỷ, `2` đổi phòng, `3` đổi giờ, `4` dời buổi, `5` dạy bù (buổi gốc thành **đã huỷ**, thêm buổi bù `kind: 3` ở `new_date`).
- Đổi phòng cần `new_room`; đổi giờ cần `new_start`, `new_end`; dời và dạy bù cần thêm `new_date`.
- `schedule.update` xoá các ngoại lệ có `orig_date` không còn là ngày có buổi theo quy tắc mới, trả số lượng trong `removedExceptions`.
- Công việc: `until` bỏ trống hoặc `null` nếu không lặp. `deadline: true` là hạn chót tại `start` (server lưu `end = start`), hiện trên lịch nhưng không tính trùng.
- Không gửi `owner_id`; server tự gán chủ sở hữu từ phiên đăng nhập. Bản ghi của người khác trả `NOT_FOUND`.


## 5. Lệnh của TV4 (`calendar.*`, `conflict.*`)

Tất cả lệnh dành cho mọi vai trò (`student` trở lên) và chỉ làm việc trên dữ liệu của chính người đăng nhập
(`ctx->user_id`). Mã `reason` của xung đột: `1` = trùng giờ, `2` = không đủ thời gian di chuyển giữa hai cơ sở
(khi đó `start`–`end` là khoảng nghỉ hiện có).

| Lệnh | Vai trò | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `calendar.week` | student | `{date?}` | `{from, to, items, conflicts, truncated?}` | `BAD_ARG` (ESP chưa có giờ mà không gửi `date`) |
| `calendar.range` | student | `{from, to}` | như `calendar.week` | `BAD_ARG` (quá 62 ngày, hoặc quá nhiều buổi) |
| `conflict.list` | student | `{from?, to?}` (bỏ trống: tuần hiện tại) | `{from, to, conflicts: [{a, b, date, start, end, reason, acked}], truncated?}`, với `a`, `b` là `{sourceId, kind, title, start, end}` | `BAD_ARG` |
| `conflict.check` | student | cách 1 (dời buổi có sẵn): `{sourceId, origDate, date, start, end}`; cách 2 (buổi mới): `{date, start, end, schoolId?, campusId?, ignoreSourceId?}` | `{conflicts: [{with: {sourceId, kind, title, start, end}, date, start, end, reason}]}` (rỗng nếu không trùng); **không** trả lỗi `CONFLICT` | `BAD_ARG`, `NOT_FOUND` (không có buổi đó của người dùng) |
| `conflict.ack` | student | `{a, b, date}` (mã nguồn của hai buổi) | `{}` | `NOT_FOUND` (cặp này không phải xung đột của người dùng), `FULL` (quá 128 xác nhận) |

**`items`:** mỗi phần tử `{sourceId, kind, status, date, origDate, start, end, title, room, classCode, school, color}`.
- `kind`: 1 = lịch dạy, 2 = công việc, 3 = dạy bù. `status`: 0 bình thường, 1 đã đổi, 2 đã huỷ, 3 nghỉ lễ.
- Thêm `conflicts: [i, ...]` (chỉ số trong mảng `conflicts` của phản hồi) khi buổi đang có xung đột **chưa xác nhận**.
  Không có trường này nghĩa là không có xung đột (hoặc đã bấm "Đã biết").

**`conflicts`** (trong `calendar.*`): `[{a, b, date, start, end, reason, acked}]`, với `a`, `b` là `sourceId` của hai buổi.

**Quy tắc xét trùng:**
- Hai buổi cùng ngày trùng giờ khi `a.start < b.end && b.start < a.end`. Hai buổi nối sát nhau (10:00 và 10:00) không trùng.
- Khác cơ sở và khoảng nghỉ nhỏ hơn thời gian di chuyển của `travel.*` thì báo thiếu thời gian di chuyển. Buổi không
  gắn cơ sở (học trực tuyến) không bị xét di chuyển.
- Bỏ qua buổi đã huỷ, nghỉ lễ, và mục không có khoảng giờ (hạn chót, `start >= end`).

**Giới hạn phản hồi (khung tối đa 8 KB):** mỗi buổi khoảng 200 byte. Server dừng thêm `items` khi đủ khoảng 5,8 KB và
đặt `truncated: true`; `conflicts` tối đa 16 mục. App gọi theo từng tuần (7 ngày) nên bình thường không chạm giới hạn.

**Tin đẩy:** `conflict.ack` đẩy `{"push":"changed","entity":"conflict","id":0}` tới các phiên của chính người đó. Lịch thay
đổi do TV3 đẩy `schedule` / `task` / `exception`, app tự tải lại.

## 6. Tìm server qua UDP (TV2)

```text
client → broadcast 255.255.255.255:5001   "ESPEDUSHED_DISCOVER"
ESP    → unicast về địa chỉ người gửi      {"name":"espedushed","ip":"192.168.1.37","port":5000,"fw":"1.0.0"}
```

Tin không đúng nội dung thì bỏ qua. Tối đa 10 lần trả lời mỗi giây.
