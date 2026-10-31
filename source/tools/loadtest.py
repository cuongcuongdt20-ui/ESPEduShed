"""Đo thực nghiệm phần mạng (TV1). Chỉ dùng thư viện chuẩn của Python.

Mỗi kịch bản ghi kết quả thật vào statics/results/<tên>.csv để đưa vào README và báo cáo.
KHÔNG tự sửa số liệu: chạy lại kịch bản nếu cần số mới.

Kịch bản:
  latency   N client đồng thời, mỗi client gửi M lệnh tuần tự, đo thời gian khứ hồi từng lệnh
            python loadtest.py latency --host 192.168.1.37 --clients 1,2,4,6,8 --requests 200 --cmd system.ping
            python loadtest.py latency --host ... --user admin --password ... --cmd calendar.week --data '{"date":"2026-09-02"}'
  fragment  gửi một khung bị cắt thành từng mẩu nhỏ, và nhiều khung dính trong một lần send
            python loadtest.py fragment --host 192.168.1.37
  maxconn   mở nhiều hơn số kết nối tối đa, kiểm tra kết nối thừa nhận BUSY
            python loadtest.py maxconn --host 192.168.1.37 --count 10
  replay    gửi lại gói auth.login cũ, phải bị từ chối
            python loadtest.py replay --host ... --user admin --password ...
  http      so giao thức TCP tự thiết kế với HTTP trên CÙNG bo, CÙNG dữ liệu: calendar.range (TCP 5000) và
            GET /api/<khoá>/range (esp_http_server cổng 8080). Đo byte mỗi yêu cầu, thời gian khứ hồi theo số client,
            RAM mỗi kết nối (heapFree), độ trễ nhận thay đổi (tin đẩy TCP; web hỏi vòng 60 giây)
            python loadtest.py http --host 192.168.1.37 --password ... --clients 1,4 --requests 200
                                    --from 2026-09-07 --to 2026-09-13
  discovery gửi gói tìm server tới cổng UDP 5001 của ESP: gói đúng được trả lời, gói sai bị bỏ qua
            python loadtest.py discovery --host 192.168.1.37 --count 20
"""
import argparse
import csv
import json
import os
import socket
import statistics
import struct
import sys
import threading
import time

from espproto import Client, derive_key, encode_frame, make_proof

# Mặc định ghi vào statics/results của repo; đặt LD_RESULTS_DIR khi chạy thử với fake_server để không lẫn số liệu thật
RESULTS = os.environ.get("LD_RESULTS_DIR") or os.path.join(os.path.dirname(__file__), "..", "..", "statics", "results")


def percentile(xs, p):
    xs = sorted(xs)
    if not xs:
        return float("nan")
    k = (len(xs) - 1) * p / 100
    f = int(k)
    return xs[f] if f == len(xs) - 1 else xs[f] + (xs[f + 1] - xs[f]) * (k - f)


def write_csv(name, header, rows):
    os.makedirs(RESULTS, exist_ok=True)
    path = os.path.abspath(os.path.join(RESULTS, name))
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)
    print(f"-> {path}")


def run_latency(args):
    data = json.loads(args.data) if args.data else None
    rows = []
    for n in [int(x) for x in args.clients.split(",")]:
        lat, errors, lock = [], [0], threading.Lock()
        ready = threading.Barrier(n)

        def worker():
            try:
                c = Client(args.host, args.port)
                if args.user:
                    c.login(args.user, args.password)
            except OSError:
                with lock:
                    errors[0] += args.requests
                ready.wait()
                return
            ready.wait()                                 # mọi client bắt đầu gửi cùng lúc
            local = []
            for _ in range(args.requests):
                t0 = time.perf_counter()
                try:
                    res = c.request(args.cmd, data)
                    if not res.get("ok"):
                        with lock:
                            errors[0] += 1
                    local.append((time.perf_counter() - t0) * 1000)
                except (OSError, TimeoutError, ConnectionError):
                    with lock:
                        errors[0] += 1
            c.close()
            with lock:
                lat.extend(local)

        threads = [threading.Thread(target=worker) for _ in range(n)]
        t_start = time.perf_counter()
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        elapsed = time.perf_counter() - t_start
        total = n * args.requests
        row = [args.cmd, n, args.requests, len(lat), errors[0],
               round(min(lat), 2) if lat else "", round(statistics.mean(lat), 2) if lat else "",
               round(percentile(lat, 50), 2), round(percentile(lat, 95), 2), round(max(lat), 2) if lat else "",
               round(len(lat) / elapsed, 1)]
        rows.append(row)
        print(f"{n} client: {len(lat)}/{total} thanh cong, loi {errors[0]}, trung binh {row[6]} ms, "
              f"p95 {row[8]} ms, {row[10]} lenh/giay")
    write_csv(f"latency_{args.cmd.replace('.', '_')}.csv",
              ["cmd", "clients", "requests_per_client", "ok", "errors", "min_ms", "avg_ms", "p50_ms", "p95_ms",
               "max_ms", "throughput_rps"], rows)


def run_fragment(args):
    rows = []
    # 1) Một khung cắt thành mẩu 1 byte (cả 4 byte độ dài cũng bị cắt), nghỉ 5 ms giữa các mẩu
    c = Client(args.host, args.port)
    frame = encode_frame({"id": 1, "cmd": "system.ping"})
    for i in range(len(frame)):
        c.send_raw(frame[i:i + 1])
        time.sleep(0.005)
    r = c.recv_frame()
    ok1 = r.get("id") == 1 and r.get("ok")
    rows.append(["cat_tung_byte", len(frame), 1, int(ok1)])
    print(f"khung cat tung byte ({len(frame)} lan send): {'DAT' if ok1 else 'KHONG DAT'}")
    # 2) Mười khung dính trong một lần send
    blob = b"".join(encode_frame({"id": 100 + i, "cmd": "system.ping"}) for i in range(10))
    c.send_raw(blob)
    ids = sorted(c.recv_frame().get("id") for _ in range(10))
    ok2 = ids == list(range(100, 110))
    rows.append(["10_khung_dinh", len(blob), 10, int(ok2)])
    print(f"10 khung dinh trong 1 lan send: {'DAT' if ok2 else 'KHONG DAT'}")
    # 3) Khung độ dài quá giới hạn: server phải đóng kết nối
    c.send_raw(struct.pack(">I", 8193))
    try:
        c.recv_frame(timeout=3)
        ok3 = False
    except (ConnectionError, OSError):
        ok3 = True
    rows.append(["khung_qua_8kb", 4, 0, int(ok3)])
    print(f"khung qua 8 KB bi dong ket noi: {'DAT' if ok3 else 'KHONG DAT'}")
    c.close()
    write_csv("fragment.csv", ["scenario", "bytes", "frames", "passed"], rows)


def run_maxconn(args):
    conns, busy, rows = [], 0, []
    for i in range(args.count):
        c = Client(args.host, args.port)
        conns.append(c)
        try:
            # Kết nối thừa: server gửi BUSY (id 0) ngay khi accept rồi đóng. Chờ ngắn trước khi gửi lệnh,
            # nếu gửi trước thì gói của client có thể làm server trả RST và mất khung BUSY.
            state = c.recv_frame(timeout=0.5)["error"]["code"]
        except (TimeoutError, OSError) as e:
            if isinstance(e, ConnectionError):
                state = "closed"
            else:
                try:
                    r = c.request("system.ping", timeout=2)
                    state = "ok" if r.get("ok") else r["error"]["code"]
                except (ConnectionError, OSError, TimeoutError):
                    state = "closed"
        busy += state == "BUSY"
        rows.append([i + 1, state])
        print(f"ket noi {i + 1}: {state}")
    for c in conns:
        c.close()
    write_csv("maxconn.csv", ["connection", "result"], rows)


def run_discovery(args):
    """Gửi UDP thẳng tới ESP (cổng 5001): gói đúng phải được trả lời đúng IP và cổng TCP, gói sai bị bỏ qua.
    Gửi cách nhau 0,2 giây để không chạm giới hạn 10 trả lời/giây. Chưa thử broadcast và mDNS."""
    rows = []
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.0)

    def ask(payload):
        s.sendto(payload, (args.host, 5001))
        t0 = time.perf_counter()
        try:
            data, addr = s.recvfrom(512)
            return json.loads(data), addr[0], (time.perf_counter() - t0) * 1000
        except (TimeoutError, socket.timeout):
            return None, "", None

    got, lat = 0, []
    for _ in range(args.count):
        reply, ip, ms = ask(b"ESPEDUSHED_DISCOVER")
        if reply and reply.get("ip") == args.host and ip == args.host and reply.get("port") == args.port:
            got += 1
            lat.append(ms)
        time.sleep(0.2)
    rows.append(["goi_dung", args.count, got, int(got == args.count), round(statistics.mean(lat), 2) if lat else ""])
    for name, payload in (("sai_noi_dung", b"ESPEDUSHED_DISCOVEX"), ("thua_byte", b"ESPEDUSHED_DISCOVER!"), ("thieu_byte", b"ESPEDUSHED")):
        reply, _, _ = ask(payload)
        rows.append([name, 0, int(reply is not None), int(reply is None), ""])
        time.sleep(0.2)
    for r in rows:
        print(f"{r[0]:14s} mong doi {r[1]} tra loi, thuc te {r[2]}, RTT TB {r[4]} ms")
    s.close()
    write_csv("discovery.csv", ["case", "expected_replies", "actual_replies", "passed", "rtt_avg_ms"], rows)


def run_replay(args):
    c = Client(args.host, args.port)
    ch = c.request("auth.challenge", {"user": args.user})["data"]
    key = derive_key(args.password, ch["salt"], ch["iter"])
    login = {"user": args.user, "nonce": ch["nonce"], "proof": make_proof(key, ch["nonce"])}
    first = c.request("auth.login", login)
    c.close()
    c2 = Client(args.host, args.port)                       # kẻ tấn công gửi lại đúng gói đã bắt được
    second = c2.request("auth.login", login)
    c2.close()
    ok = first.get("ok") and not second.get("ok")
    print(f"lan 1: {first.get('ok')}, gui lai: {second.get('ok')} -> {'DAT' if ok else 'KHONG DAT'}")
    write_csv("replay.csv", ["first_login_ok", "replay_login_ok", "passed"],
              [[int(bool(first.get('ok'))), int(bool(second.get('ok'))), int(bool(ok))]])


class HttpConn:
    """HTTP/1.1 tối giản trên socket thô để đếm đúng số byte đi trên dây (header + thân)."""

    def __init__(self, host, port, keep_alive=True, timeout=5.0):
        self.host, self.port, self.keep_alive, self.timeout = host, port, keep_alive, timeout
        self.sock = None
        self.buf = b""

    def _connect(self):
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.buf = b""

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.sock = None

    def get(self, path):
        """Trả (status, body, byte gửi, byte nhận). Không giữ kết nối thì mở mới cho mỗi yêu cầu."""
        if self.sock is None:
            self._connect()
        req = (f"GET {path} HTTP/1.1\r\nHost: {self.host}:{self.port}\r\n"
               f"Connection: {'keep-alive' if self.keep_alive else 'close'}\r\n\r\n").encode()
        self.sock.sendall(req)
        while b"\r\n\r\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("server dong ket noi")
            self.buf += chunk
        head, rest = self.buf.split(b"\r\n\r\n", 1)
        lines = head.decode("latin-1").split("\r\n")
        status = int(lines[0].split()[1])
        hdr = {k.strip().lower(): v.strip() for k, v in (l.split(":", 1) for l in lines[1:] if ":" in l)}
        if "content-length" in hdr:
            n = int(hdr["content-length"])
            while len(rest) < n:
                chunk = self.sock.recv(4096)
                if not chunk:
                    raise ConnectionError("server dong ket noi giua chung")
                rest += chunk
            body, self.buf = rest[:n], rest[n:]
            raw = n
        else:                                            # chunked (httpd_resp_send_chunk)
            body, raw = b"", 0
            while True:
                while b"\r\n" not in rest:
                    rest += self.sock.recv(4096)
                size_line, rest = rest.split(b"\r\n", 1)
                size = int(size_line.split(b";")[0], 16)
                while len(rest) < size + 2:
                    rest += self.sock.recv(4096)
                body += rest[:size]
                raw += len(size_line) + 2 + size + 2
                rest = rest[size + 2:]
                if size == 0:
                    break
            self.buf = rest
        received = len(head) + 4 + raw
        if not self.keep_alive or hdr.get("connection", "").lower() == "close":
            self.close()
        return status, body, len(req), received


def tcp_frame_sizes(msg_obj, reply_obj):
    return len(encode_frame(msg_obj)), 4 + len(json.dumps(reply_obj, ensure_ascii=False, separators=(",", ":")).encode())


def heap_free(args):
    c = Client(args.host, args.port)
    try:
        return c.request("system.status")["data"]["heapFree"]
    finally:
        c.close()


def run_http(args):
    rng = {"from": args.date_from, "to": args.date_to}
    c = Client(args.host, args.port)
    if not c.login(args.user, args.password).get("ok"):
        print("dang nhap that bai")
        return 1
    link = c.request("web.link")["data"]
    path = f"/api/{link['key']}/range?from={args.date_from}&to={args.date_to}"
    hport = link["port"]
    rows = []

    # 1) Byte mỗi yêu cầu: cùng dữ liệu, cùng khoảng ngày
    msg = {"id": 1, "cmd": "calendar.range", "token": c.token, "data": rng}
    reply = c.request("calendar.range", rng)
    t_sent, t_recv = tcp_frame_sizes(msg, reply)
    h = HttpConn(args.host, hport)
    st, body, h_sent, h_recv = h.get(path)
    h.close()
    if st != 200:
        print(f"HTTP tra {st}: {body[:120]!r}")
        return 1
    items_tcp, items_http = len(reply["data"]["items"]), len(json.loads(body)["items"])
    rows += [["bytes_request", "tcp", 1, t_sent, "byte"], ["bytes_response", "tcp", 1, t_recv, "byte"],
             ["bytes_request", "http", 1, h_sent, "byte"], ["bytes_response", "http", 1, h_recv, "byte"],
             ["items", "tcp", 1, items_tcp, "buoi"], ["items", "http", 1, items_http, "buoi"]]
    print(f"byte/yeu cau  TCP: gui {t_sent}, nhan {t_recv} ({items_tcp} buoi) | "
          f"HTTP: gui {h_sent}, nhan {h_recv} ({items_http} buoi)")

    # 2) Thời gian khứ hồi theo số client: TCP giữ kết nối; HTTP giữ kết nối và HTTP mở kết nối mới mỗi lần
    def measure(kind, n):
        lat, errors, lock = [], [0], threading.Lock()
        ready = threading.Barrier(n)

        def worker():
            try:
                if kind == "tcp":
                    cl = Client(args.host, args.port)
                    cl.token = c.token
                else:
                    cl = HttpConn(args.host, hport, keep_alive=(kind == "http_keepalive"))
            except OSError:
                with lock:
                    errors[0] += args.requests
                ready.wait()
                return
            ready.wait()
            local = []
            for _ in range(args.requests):
                t0 = time.perf_counter()
                try:
                    if kind == "tcp":
                        ok = cl.request("calendar.range", rng).get("ok")
                    else:
                        ok = cl.get(path)[0] == 200
                    if ok:
                        local.append((time.perf_counter() - t0) * 1000)
                    else:
                        with lock:
                            errors[0] += 1
                except (OSError, TimeoutError, ConnectionError, ValueError):
                    with lock:
                        errors[0] += 1
                    if kind != "tcp":
                        cl.close()
            cl.close()
            with lock:
                lat.extend(local)

        threads = [threading.Thread(target=worker) for _ in range(n)]
        t_start = time.perf_counter()
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        elapsed = time.perf_counter() - t_start
        return lat, errors[0], elapsed

    for n in [int(x) for x in args.clients.split(",")]:
        for kind in ("tcp", "http_keepalive", "http_new_conn"):
            lat, err, elapsed = measure(kind, n)
            if not lat:
                print(f"{kind} {n} client: khong co yeu cau nao thanh cong ({err} loi)")
                rows.append(["rtt_avg_ms", kind, n, "", "ms"])
                continue
            avg, p95 = statistics.mean(lat), percentile(lat, 95)
            rows += [["rtt_avg_ms", kind, n, round(avg, 2), "ms"], ["rtt_p95_ms", kind, n, round(p95, 2), "ms"],
                     ["rtt_max_ms", kind, n, round(max(lat), 2), "ms"],
                     ["throughput", kind, n, round(len(lat) / elapsed, 1), "yeu cau/giay"], ["errors", kind, n, err, "lan"]]
            print(f"{kind:15s} {n} client: TB {avg:.2f} ms, p95 {p95:.2f} ms, {len(lat) / elapsed:.1f} yc/giay, loi {err}")

    # 3) RAM mỗi kết nối: heapFree trước và sau khi mở K kết nối đang chờ (đã dùng một lần)
    k = args.idle
    base = heap_free(args)
    held = []
    for _ in range(k):
        cl = Client(args.host, args.port)
        cl.token = c.token
        cl.request("system.ping")
        held.append(cl)
    time.sleep(0.5)
    after_tcp = heap_free(args)
    for cl in held:
        cl.close()
    time.sleep(1.0)
    base2 = heap_free(args)
    held = []
    for _ in range(k):
        hc = HttpConn(args.host, hport)
        hc.get(path)
        held.append(hc)
    time.sleep(0.5)
    after_http = heap_free(args)
    for hc in held:
        hc.close()
    ram_tcp, ram_http = (base - after_tcp) / k, (base2 - after_http) / k
    rows += [["ram_per_conn", "tcp", k, round(ram_tcp), "byte"], ["ram_per_conn", "http_keepalive", k, round(ram_http), "byte"]]
    print(f"RAM moi ket noi ({k} ket noi dang cho): TCP {ram_tcp:.0f} byte, HTTP {ram_http:.0f} byte")

    write_csv("compare_http.csv", ["metric", "transport", "clients", "value", "unit"], rows)   # giữ số đã đo nếu bước sau lỗi

    # 4) Độ trễ nhận thay đổi: B tạo rồi xoá một công việc, đo lúc A nhận tin đẩy "task"
    token = c.token
    c.close()                                            # c đã im quá lâu trong lúc đo HTTP, server có thể đã đóng (60 giây)
    c = Client(args.host, args.port)
    c.token = token
    a = Client(args.host, args.port)
    a.token = c.token
    a.request("system.ping")                             # phiên đã đăng nhập mới nhận tin đẩy
    delays = []
    for i in range(args.push_rounds):
        t0 = time.perf_counter()
        r = c.request("task.create", {"title": f"loadtest push {i}", "day": args.date_from, "start": "23:00",
                                      "deadline": True})
        if not r.get("ok"):
            print(f"task.create loi: {r.get('error')}")
            break
        try:
            while True:
                f = a.recv_frame(timeout=3)
                if f.get("push") and f.get("entity") == "task":
                    delays.append((time.perf_counter() - t0) * 1000)
                    break
        except (TimeoutError, OSError):
            pass
        c.request("task.delete", {"id": r["data"]["id"]})
        try:                                             # bỏ tin đẩy của lệnh xoá
            while True:
                f = a.recv_frame(timeout=0.5)
        except (TimeoutError, OSError):
            pass
    a.close()
    c.close()
    if delays:
        rows += [["push_delay_avg_ms", "tcp", len(delays), round(statistics.mean(delays), 2), "ms"],
                 ["push_delay_p95_ms", "tcp", len(delays), round(percentile(delays, 95), 2), "ms"],
                 ["poll_delay_avg_ms", "http_poll_60s", 0, 30000, "ms (ly thuyet: nua chu ky hoi vong)"]]
        print(f"tin day TCP: TB {statistics.mean(delays):.2f} ms, p95 {percentile(delays, 95):.2f} ms "
              f"({len(delays)} lan); web hoi vong 60 giay: TB 30000 ms")
    write_csv("compare_http.csv", ["metric", "transport", "clients", "value", "unit"], rows)
    return 0


def main():
    ap = argparse.ArgumentParser(description="Đo thực nghiệm giao thức TCP ESPEduShed")
    ap.add_argument("scenario", choices=["latency", "fragment", "maxconn", "replay", "http", "discovery"])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--user")
    ap.add_argument("--password", default="")
    ap.add_argument("--clients", default="1,2,4,6,8")
    ap.add_argument("--requests", type=int, default=100)
    ap.add_argument("--cmd", default="system.ping")
    ap.add_argument("--data")
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--from", dest="date_from", default="2026-09-07", help="http: ngày đầu của khoảng lịch")
    ap.add_argument("--to", dest="date_to", default="2026-09-13", help="http: ngày cuối của khoảng lịch")
    ap.add_argument("--idle", type=int, default=3, help="http: số kết nối mở để đo RAM mỗi kết nối")
    ap.add_argument("--push-rounds", type=int, default=20, help="http: số lần đo độ trễ tin đẩy")
    args = ap.parse_args()
    if args.scenario in ("replay", "http") and not args.user:
        args.user = "admin"
    fn = {"latency": run_latency, "fragment": run_fragment, "maxconn": run_maxconn, "replay": run_replay,
          "http": run_http, "discovery": run_discovery}[args.scenario]
    return fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())
