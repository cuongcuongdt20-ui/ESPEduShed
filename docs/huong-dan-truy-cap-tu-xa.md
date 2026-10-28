# Hướng dẫn thiết lập truy cập ESPEduShed từ mọi mạng

Làm theo hướng dẫn này để:

- mở app ESPEduShed trên laptop ở bất kỳ đâu (quán cà phê, trường học, 4G) mà vẫn nối được tới ESP32 ở nhà;
- xem lịch trên điện thoại bằng 4G.

Cách làm: router nhà chuyển tiếp hai cổng về ESP32 (**port forwarding**), còn **DuckDNS** cho một tên miền cố định
luôn trỏ về IP công khai của nhà bạn, kể cả khi nhà mạng đổi IP.

```text
 Điện thoại (4G) ──► ten.duckdns.org:8080 ──►┐
                                             │  Router nhà  ──► ESP32 (192.168.1.50)
 Laptop (Wi-Fi khác) ► ten.duckdns.org:5000 ─┘  mở cổng 5000, 8080     TCP 5000: app
                                                                       HTTP 8080: web xem lịch
```

Thời gian: khoảng 20–30 phút. Chỉ cần làm một lần.

---

## 0. Chuẩn bị

| Cần có | Ghi chú |
|---|---|
| ESP32 đã nạp firmware và nối Wi-Fi nhà | Log serial in dòng `co IP 192.168.x.y, TCP cong 5000` |
| App ESPEduShed đã đăng nhập được trong mạng nhà | |
| Quyền vào trang quản trị router | Địa chỉ, tài khoản và mật khẩu thường in trên nhãn dán dưới đáy router |
| Một điện thoại dùng 4G | Để thử từ mạng ngoài |

> **Không dùng được** khi ESP32 bắt Wi-Fi phát từ điện thoại (hotspot 4G). Nhà mạng di động không cho mở cổng, nên
> chỉ các máy cùng bắt Wi-Fi đó mới vào được ESP32.

---

## 1. Kiểm tra nhà mạng có cho mở cổng không (CGNAT)

Đây là bước quan trọng nhất. Nếu nhà mạng dùng CGNAT (nhiều nhà dùng chung một IP công khai), mở cổng sẽ không có
tác dụng, các bước sau đều vô ích.

1. Đăng nhập trang quản trị router (thường là `http://192.168.1.1` hoặc `http://192.168.0.1`).
2. Tìm mục **Trạng thái / Status / WAN** và ghi lại **địa chỉ IP WAN**.
3. Trên máy trong mạng nhà, mở trang `https://whatismyip.com` (hoặc tìm "what is my ip") và xem **IP công khai**.
4. So sánh:

| Kết quả | Ý nghĩa | Làm gì |
|---|---|---|
| Hai IP **giống nhau** | Có IP public | Làm tiếp bước 2 |
| IP WAN bắt đầu bằng `100.64`–`100.127`, `10.`, `172.16`–`172.31` hoặc `192.168.` | Bị CGNAT, hoặc router nhà nằm sau một router khác | Gọi tổng đài nhà mạng xin **"IP public động"** cho gói Internet; thường miễn phí hoặc phí thấp. Nếu có hai router nối nhau, cần mở cổng trên cả hai (hoặc chuyển modem nhà mạng sang chế độ bridge) |

---

## 2. Gán IP cố định cho ESP32 (DHCP reservation)

Port forwarding trỏ tới một IP trong mạng nhà. Nếu ESP32 khởi động lại và nhận IP khác thì cổng sẽ trỏ sai chỗ.

1. Trong trang quản trị router, tìm mục tên **DHCP**, **LAN**, **DHCP Static / Address Reservation** hoặc
   **Gán IP tĩnh**.
2. Trong danh sách thiết bị, tìm ESP32. Tên thường là `espressif` hoặc `esp32s3`, cũng có thể nhận ra bằng IP trong log
   serial.
3. Chọn **Thêm / Reserve**, giữ địa chỉ MAC của ESP32 và đặt một IP cố định, ví dụ `192.168.1.50`.
4. Lưu, rồi rút điện ESP32 cắm lại. Kiểm tra log serial thấy đúng IP vừa đặt.

---

## 3. Mở cổng (port forwarding)

1. Tìm mục **NAT**, **Port Forwarding**, **Virtual Server**, **Chuyển tiếp cổng** hoặc **Ứng dụng / Application**
   (tên khác nhau tuỳ hãng router).
2. Thêm **hai** quy tắc:

| Tên (tuỳ ý) | Giao thức | Cổng ngoài (WAN / External) | IP nội bộ (LAN / Internal) | Cổng trong (Internal) |
|---|---|---|---|---|
| `espedushed-app` | **TCP** | `5000` | `192.168.1.50` | `5000` |
| `espedushed-web` | **TCP** | `8080` | `192.168.1.50` | `8080` |

3. Lưu và bật (Enable) cả hai quy tắc. Một số router cần khởi động lại.

Ghi chú:

- Không cần mở cổng UDP 5001. Cổng này chỉ dùng để tìm ESP32 trong mạng nhà.
- Nếu cổng 5000 hoặc 8080 đã bị thiết bị khác dùng, chọn cổng ngoài khác (ví dụ `15000` → `5000`). Khi kết nối từ xa
  thì nhập cổng ngoài đó.
- Không bật **DMZ** cho ESP32. DMZ mở mọi cổng, không cần thiết và kém an toàn hơn.

---

## 4. Tạo tên miền DuckDNS (miễn phí)

IP công khai nhà bạn có thể đổi sau mỗi lần router khởi động lại. DuckDNS cho một tên cố định, và ESP32 tự báo IP mới
cho DuckDNS mỗi 5 phút.

1. Vào `https://www.duckdns.org` và đăng nhập bằng Google, GitHub hoặc tài khoản khác.
2. Ô **sub domain**: nhập tên muốn dùng, ví dụ `lichcuatoi`, rồi bấm **add domain**. Tên đầy đủ là
   `lichcuatoi.duckdns.org`.
3. Chép chuỗi **token** ở đầu trang (dạng `a1b2c3d4-....`). **Không chia sẻ token** cho người khác.
4. Mở app ESPEduShed (đang ở mạng nhà), vào menu **Truy cập từ xa**.
5. Ở thẻ **Tên miền động DuckDNS**, nhập:
   - **Tên miền:** `lichcuatoi` (có hoặc không có `.duckdns.org` đều được, hoa thường đều được);
   - **Token:** chuỗi vừa chép.
6. Bấm **Lưu**. Sau vài giây, dòng trạng thái hiện *"Đang dùng lichcuatoi.duckdns.org, cập nhật được 0 phút trước."*

Nếu dòng trạng thái báo *"lần cập nhật gần nhất bị lỗi"*: kiểm tra lại token, và kiểm tra ESP32 có Internet không
(log serial có dòng `cap nhat DuckDNS that bai`).

> Router đã có sẵn DDNS (No-IP, DynDNS, tên miền của hãng)? Có thể dùng DDNS của router và bỏ qua bước này.

---

## 5. Lấy link xem lịch cho điện thoại

1. Trong app, menu **Truy cập từ xa**, thẻ **Xem lịch trên điện thoại**.
2. Ô **Địa chỉ dùng trong link**: để trống thì app tự dùng tên DuckDNS vừa cài.
3. Bấm **Sao chép link**. Link có dạng:

   ```text
   http://lichcuatoi.duckdns.org:8080/c/3f9a0c...(32 ký tự)
   ```

4. Gửi link sang điện thoại, ví dụ qua Zalo hoặc email cho chính mình, rồi mở bằng trình duyệt.
5. Trên điện thoại, chọn **Thêm vào màn hình chính** (Chrome: menu ⋮; Safari: nút Chia sẻ) để lần sau mở nhanh như một
   ứng dụng.

Trang web chỉ để xem, không sửa được lịch. Trang tự tải lại khi chuyển tuần, khi mở lại và mỗi phút; muốn tải ngay
thì bấm **Làm mới**.

---

## 6. Kết nối app từ mạng ngoài

1. Mở app khi laptop đang ở mạng khác, hoặc phát 4G từ điện thoại cho laptop để thử.
2. Màn hình **Kết nối tới ESPEduShed**:
   - **Địa chỉ IP hoặc tên miền:** `lichcuatoi.duckdns.org`
   - **Cổng:** `5000` (hoặc cổng ngoài bạn đã chọn ở bước 3)
3. Bấm **Kết nối** và nhập mật khẩu.

Nút **Tìm server trong mạng LAN (UDP)** chỉ tìm được khi ở cùng mạng với ESP32. Ở ngoài thì nhập tên miền.

---

## 7. Kiểm tra cuối

Tắt Wi-Fi trên điện thoại, chỉ dùng 4G:

| Thử | Kết quả đúng |
|---|---|
| Mở link xem lịch | Thấy lịch tuần như trong app |
| Sửa một chữ trong khoá ở cuối link rồi mở | Trang báo *Không tìm thấy* (404) |
| Laptop dùng 4G, app nối `lichcuatoi.duckdns.org:5000` | Đăng nhập được, sửa lịch được |
| Sửa lịch trên app, rồi bấm **Làm mới** trên điện thoại | Điện thoại hiện thay đổi |

---

## 8. Xử lý sự cố

| Hiện tượng | Nguyên nhân hay gặp | Cách xử lý |
|---|---|---|
| Trong nhà vào được, ngoài 4G không vào được | Bị CGNAT; chưa mở cổng; IP nội bộ trong quy tắc sai | Làm lại bước 1, 2, 3. Kiểm tra IP trong quy tắc trùng IP của ESP32 trong log serial |
| Mở `ten.duckdns.org` ngay trong mạng nhà không được, ngoài 4G thì được | Router không hỗ trợ "NAT loopback" | Bình thường. Trong nhà dùng IP nội bộ (để trống ô địa chỉ, hoặc bấm tìm trong LAN) |
| Lúc được lúc không, nhất là sau khi mất điện | IP công khai đổi, DuckDNS chưa kịp cập nhật | Đợi tối đa 5 phút. Xem trạng thái DuckDNS trong **Truy cập từ xa** |
| App báo *Server không trả lời ... sau 10 giây* | Mạng di động chậm hoặc ESP32 mất Wi-Fi | Thử lại. Kiểm tra ESP32 còn sáng đèn và còn nối Wi-Fi |
| Điện thoại báo *Link không còn hiệu lực* | Đã bấm **Đổi link** | Lấy link mới trong app |
| Điện thoại báo *Không tải được thư viện lịch* | Mạng chập chờn khi tải trang (khoảng 80 KB) | Tải lại trang |
| App báo *Server da du ket noi* | Đã có 8 app cùng nối | Đóng bớt app trên các máy khác |

---

## 9. An toàn

- Kênh truyền **không mã hoá** (không có HTTPS/TLS). Mật khẩu **không** đi trên mạng nhờ cơ chế challenge–response.
  Nhưng nội dung lịch, token phiên và link xem lịch có thể bị đọc nếu ai đó nghe lén cùng mạng (Wi-Fi công cộng).
- Ai có link xem lịch là xem được lịch. Chỉ gửi link cho chính mình. Nghi bị lộ thì vào **Truy cập từ xa** và bấm
  **Đổi link**: link cũ hết hiệu lực ngay.
- Đặt mật khẩu đăng nhập mạnh (từ 8 ký tự, có chữ và số). Server tự khoá một địa chỉ IP 60 giây sau 5 lần sai mật
  khẩu.
- Chỉ mở đúng hai cổng 5000 và 8080. Không bật DMZ hay UPnP cho ESP32.
- Không dùng nữa thì xoá hai quy tắc port forwarding trên router, và tắt DuckDNS trong app (xoá tên miền rồi bấm
  **Tắt**).
