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
"""
import argparse
import csv
import json
import os
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


def main():
    ap = argparse.ArgumentParser(description="Đo thực nghiệm giao thức TCP ESPEduShed")
    ap.add_argument("scenario", choices=["latency", "fragment", "maxconn", "replay"])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--user")
    ap.add_argument("--password", default="")
    ap.add_argument("--clients", default="1,2,4,6,8")
    ap.add_argument("--requests", type=int, default=100)
    ap.add_argument("--cmd", default="system.ping")
    ap.add_argument("--data")
    ap.add_argument("--count", type=int, default=10)
    args = ap.parse_args()
    {"latency": run_latency, "fragment": run_fragment, "maxconn": run_maxconn, "replay": run_replay}[args.scenario](args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
