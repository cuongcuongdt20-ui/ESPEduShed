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

App thêm hai mã phía client: `OFFLINE` (chưa kết nối hoặc mất kết nối) và `TIMEOUT` (10 giây không có phản hồi).

### 1.5 Quyền

Server chỉ có **một tài khoản** (tên `admin`). Lệnh chia hai loại: "công khai" (không cần token) và "đăng nhập" (cần
token hợp lệ). Mọi lịch, công việc, xung đột đều thuộc tài khoản đó.
Khi đang bị bắt đổi mật khẩu chỉ được dùng `auth.password`, `auth.logout`, `auth.me`, `system.ping`,
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
- App luôn gửi `user = "admin"`. Tên khác vẫn nhận `salt` (giả, cố định theo tên) nhưng không đăng nhập được.
- Một IP sai 5 lần trong 60 giây thì bị chặn 60 giây.
- `nonce` sai, hết hạn, sai tên hay sai mật khẩu đều trả cùng một thông báo `UNAUTHORIZED`.

### 2.2 Bảng lệnh `auth.*`

| Lệnh | Quyền | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `auth.challenge` | công khai | `{user}` | `{nonce, salt, iter}` | `BUSY` (IP bị chặn), `BAD_ARG` |
| `auth.login` | công khai | `{user, nonce, proof}` | `{token, expiresAt, user: {id, name}, mustChangePassword}` | `UNAUTHORIZED`, `BUSY` |
| `auth.logout` | đăng nhập | – | `{}` | |
| `auth.me` | đăng nhập | – | `{user: {id, name}, mustChangePassword}` | `UNAUTHORIZED` |
| `auth.password` | đăng nhập | `{salt, iter, key}` | `{}` | |

- `user`: 3..32 ký tự `A-Z a-z 0-9 . _ -`.
- `salt`: 32 ký tự hex (16 byte). `key`: 64 ký tự hex (32 byte). `iter`: 1000..200000, app dùng 10000.
- `expiresAt`: ngày hết hạn của token (7 ngày), `null` khi ESP chưa đồng bộ giờ.
- Lần đầu chạy, ESP tạo tài khoản với mật khẩu ngẫu nhiên in ra cổng serial, `mustChangePassword = true`.
- Sau `auth.password`, mọi token cũ mất hiệu lực và mọi kết nối thôi nhận tin đẩy; app tự đăng nhập lại bằng mật khẩu mới.

### 2.3 Bảng lệnh `system.*`

| Lệnh | Quyền | `data` gửi | `data` nhận |
|---|---|---|---|
| `system.ping` | công khai | – | `{pong}` (ms từ lúc ESP khởi động) |
| `system.status` | công khai | – | `{name, fw, mode, ip, rssi?, uptimeS, timeSynced, clients, maxClients, sessions, heapFree, heapMin, heapLargest, requests, errors, maxHandlerUs, dataTotal, dataUsed, ddns?}`; `ddns: {domain, lastOk?, lastAgoS?}` chỉ có khi đã đăng nhập |
| `system.wifi` | công khai ở chế độ AP, đăng nhập ở chế độ STA | `{ssid, pass}` | `{restarting: true}`, ESP khởi động lại sau 1,5 giây |
| `system.ddns` | đăng nhập | `{domain, token}` (`domain` rỗng thì tắt; nhận cả `ten` lẫn `ten.duckdns.org`) | `{}`; ESP gửi cập nhật DuckDNS ngay, sau đó mỗi 5 phút |
| `system.backup` | đăng nhập | `{}` | `{files: [{name, size}]}` |
| `system.backup` | đăng nhập | `{file, offset}` | `{file, offset, size, data, eof}`; `data` là base64 của tối đa 4096 byte |
| `system.restore` | đăng nhập | `{file, offset, data, last?, reboot?}` | `{written, restarting?}` |

- `system.status` dùng cho trang trạng thái của app và cho đo thực nghiệm (RAM, số yêu cầu, handler chậm nhất).
- Sao lưu / khôi phục đi theo từng mẩu 4 KB vì một khung tối đa 8 KB. `file` chỉ gồm chữ thường, số, `_`, đuôi `.json`.
  Khôi phục ghi vào `<file>.restore`, mẩu cuối (`last: true`) mới kiểm tra cấu trúc rồi đổi tên đè file thật. File
  không phải JSON Lines hoặc object JSON có `schema` thì trả `BAD_ARG` (`field: "data"`), file thật giữ nguyên. Đây chỉ là
  kiểm tra cấu trúc: file JSON Lines bị cắt đúng ở cuối một dòng vẫn được nhận. Khôi phục
  xong file cuối thì gửi `reboot: true` để mọi module nạp lại dữ liệu.

### 2.4 Bảng lệnh `web.*` (link xem lịch trên điện thoại)

| Lệnh | Quyền | `data` gửi | `data` nhận |
|---|---|---|---|
| `web.link` | đăng nhập | – | `{key, port}`; link là `http://<địa chỉ>:<port>/c/<key>` |
| `web.rotate` | đăng nhập | – | `{key, port}` của khoá mới; link cũ trả 404 ngay |

`key` là 32 ký tự hex ngẫu nhiên, lưu NVS. Xem mục 7 về các endpoint HTTP.

### Phân trang lệnh danh sách

`school.list`, `campus.list`, `holiday.list`, `subject.list`, `class.list`, `schedule.list`, `task.list`, `occurrence.range`
nhận thêm `offset` (mặc định 0). Mỗi phản hồi chứa tối đa khoảng 6 KB mục; còn mục thì có thêm `"next": n`, client gửi
lại cùng lệnh với `offset: n` cho tới khi phản hồi không còn `next` (app dùng `requestAll` trong `src/core/api.ts`).
Lý do: khung tin tối đa 8 KB, danh sách dài (ví dụ 16 trường × 20 tiết) sẽ vượt nếu trả một lần.

## 3. Lệnh của TV2 (`school.*`, `campus.*`, `periods.*`, `holiday.*`, `subject.*`, `class.*`, `travel.*`)

> TV2: `school.list` trả về trường và bảng tiết; các màn hình danh mục gọi thêm `campus.*`, `holiday.*`, `subject.*`, `class.*` và `travel.*`.
> Mọi lệnh đều cần đăng nhập.

| Lệnh | Quyền | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `school.list` | đăng nhập | `{}` | `{schools: [{id, name, shortName, color, periods: [{no, start, end}]}]}` | |
| `school.create` / `school.update` | đăng nhập | `{name, shortName, color}` / thêm `id` | `{id}` | `BAD_ARG`, `FULL`; sửa thêm `NOT_FOUND`. Không kiểm tra trùng tên trường |
| `school.delete` | đăng nhập | `{id}` | `{}` | `NOT_FOUND`, `CONFLICT` nếu lịch đang dùng trường |
| `campus.list` | đăng nhập | `{schoolId?}` | `{campuses: [{id, schoolId, name, address}]}` | |
| `campus.create` / `campus.update` / `campus.delete` | đăng nhập | trường/cơ sở tương ứng | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `periods.set` | đăng nhập | `{schoolId, periods: [{no, start, end}]}` | `{}` | `BAD_ARG` nếu giờ sai, tiết trùng hoặc chồng lấn |
| `holiday.list` | đăng nhập | `{schoolId?}` | `{holidays: [{id, schoolId, name, fromDate, toDate}]}` | |
| `holiday.create` / `holiday.update` / `holiday.delete` | đăng nhập | `{schoolId, name, from, to}` (`YYYY-MM-DD`; `schoolId: 0` áp dụng tất cả trường); sửa/xoá thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `subject.list` | đăng nhập | `{}` | `{subjects: [{id, code, name, credits}]}` | |
| `subject.create` / `subject.update` / `subject.delete` | đăng nhập | `{code, name, credits}`; sửa/xoá thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` (trùng mã môn, hoặc xoá môn còn lớp) |
| `class.list` | đăng nhập | `{schoolId?}` | `{classes: [{id, code, subjectId, schoolId, size}]}` | |
| `class.create` / `class.update` / `class.delete` | đăng nhập | `{code, subjectId, schoolId, size}`; sửa/xóa thêm `id` | `{id}` hoặc `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` nếu lịch đang dùng lớp |
| `travel.get` | đăng nhập | `{}` | `{travel: [{campusA, campusB, minutes}], defaultTravel}` | |
| `travel.set` / `travel.delete` | đăng nhập | `{campusA, campusB, minutes}` / `{campusA, campusB}` | `{}` | `BAD_ARG`, `NOT_FOUND`, `FULL` |
| `travel.setDefault` | đăng nhập | `{minutes}` | `{}` | `BAD_ARG` |


## 4. Lệnh của TV3 (`schedule.*`, `exception.*`, `task.*`, `occurrence.*`)

Các lệnh sử dụng TCP request/response theo giao thức chung. Server xác định chủ sở hữu dữ liệu từ `ctx->user_id`; client không được tự gửi `owner_id` để xác định chủ sở hữu bản ghi.

| Lệnh | Quyền | `data` gửi | `data` nhận | Lỗi riêng / trạng thái |
|---|---|---|---|---|
| `occurrence.range` | đăng nhập | `{from, to}` | `{items: [{sourceId, kind, status, date, origDate, start, end, school, campus}]}` | `BAD_ARG` nếu ngày sai, khoảng ngày quá 62 ngày hoặc quá nhiều buổi |
| `schedule.list` | đăng nhập | `{}` | `{schedules: [lịch]}`, mỗi lịch `{id, subject_code, subject_name, class_code, class_id, credits, school_id, campus_id, room, size, day_of_week, time_mode, period_start, period_count, start, end, date_from, date_to, weekly, note, color}` | |
| `schedule.get` | đăng nhập | `{id}` | `{schedule: lịch}` | `NOT_FOUND`  |
| `schedule.create` | đăng nhập | `{subject_code, subject_name, school_id, date_from, date_to, day_of_week, time_mode, period_start, period_count \| start, end, weekly?, class_code?, class_id?, credits?, campus_id?, room?, size?, note?, color?, force?}` | `{id}` | `BAD_ARG`, `CONFLICT` (kèm `conflicts`), `FULL` |
| `schedule.update` | đăng nhập | như `schedule.create`, thêm `id` | `{id, removedExceptions}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` |
| `schedule.delete` | đăng nhập | `{id}` | `{}` | `NOT_FOUND` |
| `exception.set` | đăng nhập | `{series_id, orig_date, type, new_date?, new_start?, new_end?, new_room?, note?, force?}` | `{message}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT`, `FULL` |
| `exception.delete` | đăng nhập | `{series_id, orig_date}` | `{}` | `NOT_FOUND` |
| `task.list` | đăng nhập | `{}` | `{tasks: [{id, type, title, content, day, until, start, end, deadline, campus_id, place, status, note, color}]}` | |
| `task.create` | đăng nhập | `{title, day, start, end \| deadline: true, until?, type?, content?, place?, campus_id?, status?, note?, color?, force?}` | `{id}` | `BAD_ARG`, `CONFLICT`, `FULL` |
| `task.update` / `task.delete` | đăng nhập | như `task.create` thêm `id` / `{id}` | `{id}` / `{}` | `BAD_ARG`, `NOT_FOUND`, `CONFLICT` |

### Quy ước trường

- `color` (lịch dạy, công việc): `"#RRGGBB"` chọn từ bảng màu của app, hoặc `""` = không đặt màu riêng (lịch dạy theo màu
  trường, công việc màu xám). Không gửi thì giữ màu cũ khi sửa. `items[].color` của `calendar.*` là màu riêng nếu có, không thì màu trường.

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

Tất cả lệnh cần đăng nhập và làm việc trên dữ liệu của tài khoản duy nhất (`ctx->user_id`). Mã `reason` của xung đột: `1` = trùng giờ, `2` = không đủ thời gian di chuyển giữa hai cơ sở
(khi đó `start`–`end` là khoảng nghỉ hiện có).

| Lệnh | Quyền | `data` gửi | `data` nhận | Lỗi riêng |
|---|---|---|---|---|
| `calendar.week` | đăng nhập | `{date?}` | `{from, to, items, conflicts, truncated?}` | `BAD_ARG` (ESP chưa có giờ mà không gửi `date`) |
| `calendar.range` | đăng nhập | `{from, to}` | như `calendar.week` | `BAD_ARG` (quá 62 ngày, hoặc quá nhiều buổi) |
| `conflict.list` | đăng nhập | `{from?, to?}` (bỏ trống: tuần hiện tại) | `{from, to, conflicts: [{a, b, date, start, end, reason, acked}], truncated?}`, với `a`, `b` là `{sourceId, kind, title, start, end}` | `BAD_ARG` |
| `conflict.check` | đăng nhập | cách 1 (dời buổi có sẵn): `{sourceId, kind?, origDate, curDate?, date, start, end}` (`curDate` = ngày buổi đang nằm, cần khi buổi đã bị dời hoặc là buổi dạy bù; mặc định `origDate`); cách 2 (buổi mới): `{date, start, end, schoolId?, campusId?, ignoreSourceId?}` | `{conflicts: [{with: {sourceId, kind, title, start, end}, date, start, end, reason}]}` (rỗng nếu không trùng); **không** trả lỗi `CONFLICT` | `BAD_ARG`, `NOT_FOUND` (không có buổi đó của người dùng) |
| `conflict.ack` | đăng nhập | `{a, b, date}` (mã nguồn của hai buổi) | `{}` | `NOT_FOUND` (cặp này không phải xung đột của người dùng), `FULL` (quá 128 xác nhận) |

**`items`:** mỗi phần tử `{sourceId, kind, status, date, origDate, start, end, title, room, classCode, school, color}`.
- `kind`: 1 = lịch dạy, 2 = công việc, 3 = dạy bù. `status`: 0 bình thường, 1 đã đổi, 2 đã huỷ, 3 nghỉ lễ.
- Thêm `conflicts: [i, ...]` (chỉ số trong mảng `conflicts` của phản hồi) khi buổi đang có xung đột **chưa xác nhận**.
  Không có trường này nghĩa là không có xung đột (hoặc đã bấm "Đã biết").

**`conflicts`** (trong `calendar.*`): `[{a, b, date, start, end, reason, acked}]`, với `a`, `b` là `sourceId` của hai buổi.

**Quy tắc xét trùng:**
- Hai buổi cùng ngày trùng giờ khi `a.start < b.end && b.start < a.end`. Hai buổi nối sát nhau (10:00 và 10:00) không trùng.
- Hai buổi **liền nhau** (không có buổi khác chen giữa) khác cơ sở và khoảng nghỉ nhỏ hơn thời gian di chuyển của `travel.*` thì báo thiếu thời gian di chuyển.
- Khi sửa hoặc dời một buổi, chỉ bỏ qua chính buổi đó (cùng lịch, cùng ngày gốc); các buổi khác của cùng lịch vẫn được so. Buổi không
  gắn cơ sở (học trực tuyến) không bị xét di chuyển.
- Bỏ qua buổi đã huỷ, nghỉ lễ, và mục không có khoảng giờ (hạn chót, `start >= end`).

**Giới hạn phản hồi (khung tối đa 8 KB):** mỗi buổi khoảng 200 byte. Server dừng thêm `items` khi đủ khoảng 5,8 KB và
đặt `truncated: true`; `conflicts` tối đa 16 mục. App gọi theo từng tuần (7 ngày) nên bình thường không chạm giới hạn.

**Tin đẩy:** `conflict.ack` đẩy `{"push":"changed","entity":"conflict","id":0}` tới các phiên đã đăng nhập. Lịch thay
đổi do TV3 đẩy `schedule` / `task` / `exception`, app tự tải lại.

## 6. Tìm server qua UDP (TV2)

```text
client → broadcast 255.255.255.255:5001   "ESPEDUSHED_DISCOVER"
ESP    → unicast về địa chỉ người gửi      {"name":"espedushed","ip":"192.168.1.37","port":5000,"fw":"1.0.1"}
```

Tin không đúng nội dung thì bỏ qua. Tối đa 10 lần trả lời mỗi giây.

## 7. Web xem lịch (HTTP, cổng 8080)

Server HTTP riêng (`esp_http_server`, component `webview`), **chỉ đọc**, không dùng token mà dùng khoá bí mật trong
đường dẫn (lấy bằng `web.link`).

| Đường dẫn | Trả về |
|---|---|
| `GET /c/<key>` | Trang lịch (HTML nhúng trong firmware) |
| `GET /fc.js` | FullCalendar 6.1.15 + locale vi, nén gzip, nhúng trong firmware; công khai vì không chứa dữ liệu |
| `GET /api/<key>/range?from=YYYY-MM-DD&to=YYYY-MM-DD` | JSON giống hệt `data` của `calendar.range`; `400 {error}` nếu ngày sai, quá 62 ngày hoặc khoảng có quá nhiều buổi |

- Sai khoá: `404`. Mọi phản hồi có `Cache-Control: no-store` và `Referrer-Policy: no-referrer`.
- Trang tự tải lại khi chuyển tuần, khi mở lại tab và mỗi 60 giây lúc đang mở.
- Truy cập từ mạng ngoài: router mở cổng TCP 5000 (app) và 8080 (web) về ESP32, kèm tên miền DDNS (xem README).
