"""Chạy toàn bộ kịch bản đo tự động trên ESP32 thật và ghi CSV vào statics/results (TV1). Chỉ dùng thư viện chuẩn.

  python run_experiments.py --host 192.168.1.37 --password <mat_khau> --serial COM3 [--repeats 3]

Thứ tự: thời gian khứ hồi (3 lệnh × số client × số lần lặp), ghép khung, giới hạn kết nối, gửi lại gói đăng nhập,
so với HTTP, lỗi giao thức, biên khoảng ngày, reset giữa lúc ghi (cần --serial, dùng pyserial: chạy bằng Python của
ESP-IDF hoặc pip install pyserial), khôi phục dữ liệu (ESP khởi động lại một lần), cuối cùng là sai mật khẩu (khoá IP
của máy chạy trong 60 giây).
Không sửa số liệu: chạy lại nếu cần số mới.
"""
import argparse
import base64
import csv
import json
import os
import shutil
import socket
import statistics
import struct
import sys
import threading
import time

import loadtest
from espproto import Client, encode_frame


def ns(base, **kw):
    a = argparse.Namespace(**vars(base))
    for k, v in kw.items():
        setattr(a, k, v)
    return a


def results_path(name):
    return os.path.abspath(os.path.join(loadtest.RESULTS, name))


def write(name, header, rows):
    loadtest.write_csv(name, header, rows)


def run_latency_series(args):
    cmds = [("system.ping", None, None),
            ("calendar.week", json.dumps({"date": args.date_from}), args.user),
            ("occurrence.range", json.dumps({"from": args.date_from, "to": args.date_to}), args.user)]
    summary = []
    for cmd, data, user in cmds:
        runs = []
        for r in range(1, args.repeats + 1):
            print(f"\n== latency {cmd} lan {r}/{args.repeats}")
            loadtest.run_latency(ns(args, cmd=cmd, data=data, user=user, clients=args.clients, requests=args.requests))
            src = results_path(f"latency_{cmd.replace('.', '_')}.csv")
            dst = results_path(f"latency_{cmd.replace('.', '_')}_run{r}.csv")
            shutil.move(src, dst)
            with open(dst, encoding="utf-8") as f:
                runs.extend(csv.DictReader(f))
            time.sleep(2)
        for n in sorted({int(x["clients"]) for x in runs}):
            sel = [x for x in runs if int(x["clients"]) == n]
            avg = [float(x["avg_ms"]) for x in sel]
            p95 = [float(x["p95_ms"]) for x in sel]
            rps = [float(x["throughput_rps"]) for x in sel]
            sd = lambda v: round(statistics.stdev(v), 2) if len(v) > 1 else 0.0
            summary.append([cmd, n, len(sel), round(statistics.mean(avg), 2), sd(avg), round(statistics.mean(p95), 2), sd(p95),
                            round(statistics.mean(rps), 1), sd(rps), sum(int(x["errors"]) for x in sel)])
    write("latency_summary.csv", ["cmd", "clients", "runs", "avg_ms_mean", "avg_ms_sd", "p95_ms_mean", "p95_ms_sd",
                                   "rps_mean", "rps_sd", "errors_total"], summary)


def raw_reply(c, payload: bytes):
    c.send_raw(struct.pack(">I", len(payload)) + payload)
    return c.recv_frame(timeout=3)


def run_errors(args):
    rows = []
    good = Client(args.host, args.port)                   # client khác phải không bị ảnh hưởng
    good.login(args.user, args.password)

    def case(name, expect, fn):
        try:
            got = fn()
        except (ConnectionError, OSError) as e:
            got = "closed" if not isinstance(e, TimeoutError) else "timeout"
        ok_other = good.request("system.ping").get("ok")
        passed = got == expect and ok_other
        rows.append([name, expect, got, int(bool(ok_other)), int(bool(passed))])
        print(f"{name:28s} mong doi {expect:12s} thuc te {got:12s} client khac {'OK' if ok_other else 'LOI'}")

    def code_of(f):
        return "OK" if f.get("ok") else f["error"]["code"]

    c = Client(args.host, args.port)
    case("json_hong", "BAD_JSON", lambda: code_of(raw_reply(c, b"{khong phai json")))
    case("lenh_khong_co", "UNKNOWN_CMD", lambda: code_of(c.request("khong.co")))
    case("thieu_truong_cmd", "BAD_ARG", lambda: code_of(raw_reply(c, b'{"id":5}')))
    case("chua_dang_nhap", "UNAUTHORIZED", lambda: code_of(c.request("calendar.week", {"date": args.date_from})))
    c.token = "0" * 32
    case("token_sai", "UNAUTHORIZED", lambda: code_of(c.request("task.list")))
    c.token = good.token
    case("thieu_tham_so", "BAD_ARG", lambda: code_of(c.request("calendar.range", {"from": args.date_from})))
    case("ngay_sai", "BAD_ARG", lambda: code_of(c.request("calendar.range", {"from": "2026-02-30", "to": "2026-03-01"})))
    case("van_dung_sau_loi", "OK", lambda: code_of(c.request("system.ping")))
    c.close()
    c = Client(args.host, args.port)
    c.send_raw(struct.pack(">I", 0))
    case("do_dai_khung_0", "closed", lambda: code_of(c.recv_frame(timeout=3)))
    c.close()
    good.close()
    write("protocol_errors.csv", ["case", "expected", "actual", "other_client_ok", "passed"], rows)


def run_range(args):
    rows = []
    c = Client(args.host, args.port)
    c.login(args.user, args.password)
    import datetime as dt
    d0 = dt.date.fromisoformat(args.date_from)
    for days, expect in ((1, "OK"), (7, "OK"), (62, "OK"), (63, "BAD_ARG")):
        to = (d0 + dt.timedelta(days=days - 1)).isoformat()
        for cmd in ("calendar.range", "occurrence.range"):
            r = c.request(cmd, {"from": args.date_from, "to": to})
            got = "OK" if r.get("ok") else r["error"]["code"]
            items = len(r["data"].get("items", [])) if r.get("ok") else ""
            trunc = int(bool(r.get("ok") and r["data"].get("truncated")))
            rows.append([cmd, days, expect, got, items, trunc, int(got == expect)])
            print(f"{cmd:17s} {days:2d} ngay: mong doi {expect:8s} thuc te {got:8s} {items} buoi"
                  + (" (truncated)" if trunc else ""))
    c.close()
    write("range_limits.csv", ["cmd", "days", "expected", "actual", "items", "truncated", "passed"], rows)


def wait_back(args, t_reset, limit=60):
    """Chờ ESP khởi động lại, vào lại Wi-Fi và trả lời system.ping. Trả số ms từ lúc reset, None nếu quá limit giây."""
    while time.perf_counter() - t_reset < limit:
        try:
            c = Client(args.host, args.port, timeout=1.0)
            ok = c.request("system.ping", timeout=1.0).get("ok")
            c.close()
            if ok:
                return (time.perf_counter() - t_reset) * 1000
        except (OSError, TimeoutError, ConnectionError, ValueError):
            pass
        time.sleep(0.2)
    return None


def run_reset(args):
    """Reset ESP qua chân EN (RTS của mạch nạp USB) ngay sau khi ghi, giống mất điện đột ngột:
    - reset sau 0,2 giây: thay đổi chưa tới hạn ghi (gộp ghi sau 1 giây), được phép mất, nhưng file không được hỏng;
    - reset sau 2 giây: thay đổi đã ghi xong, phải còn.
    Sau mỗi lần, kiểm tra server nạp lại đúng dữ liệu cũ (số công việc đếm được khớp kỳ vọng)."""
    if not args.serial:
        raise RuntimeError("can --serial COMx de reset ESP")
    import serial                                         # pyserial, chỉ cần cho kịch bản này
    port = serial.Serial()                                # đặt DTR, RTS trước khi mở: RTS bật sẽ giữ ESP ở trạng thái reset
    port.port, port.baudrate = args.serial, 115200
    port.dtr = port.rts = False
    port.open()

    def reset():
        port.rts = True
        time.sleep(0.1)
        port.rts = False
        return time.perf_counter()

    def session():
        c = Client(args.host, args.port)
        if not c.login(args.user, args.password).get("ok"):
            raise RuntimeError("dang nhap that bai sau reset")
        return c

    def task_ids(c):
        out, off = [], 0
        while True:
            d = c.request("task.list", {"offset": off})["data"]
            out += [t["id"] for t in d["tasks"] if t["title"].startswith("reset test")]
            if "next" not in d:
                return out
            off = d["next"]

    rows, created = [], []
    if wait_back(args, time.perf_counter()) is None:      # mở cổng COM có thể làm ESP khởi động lại
        raise RuntimeError("ESP khong tra loi sau khi mo cong COM")
    c = session()
    for t in task_ids(c):                                 # dọn lần chạy trước
        c.request("task.delete", {"id": t})
    time.sleep(1.5)
    for i in range(args.reset_rounds):
        for delay in (0.2, 2.0):
            r = c.request("task.create", {"title": f"reset test {i} {delay}", "day": args.date_from, "start": "23:30",
                                          "deadline": True})
            new_id = r["data"]["id"]
            expect_before = set(created)
            time.sleep(delay)
            c.close()
            back = wait_back(args, reset())
            if back is None:
                rows.append([i, delay, "", "", "", 0])
                print(f"lan {i} reset sau {delay}s: ESP khong tra loi sau 60 giay")
                c = session()
                continue
            c = session()
            ids = set(task_ids(c))
            kept = new_id in ids
            intact = expect_before <= ids                 # mọi thay đổi đã ghi trước đó còn nguyên
            if kept:
                created.append(new_id)
            ok = intact and (kept or delay < 1.0)
            rows.append([i, delay, round(back), int(kept), int(intact), int(ok)])
            print(f"lan {i} reset sau {delay}s: len lai sau {back / 1000:.1f}s, ban ghi moi {'con' if kept else 'mat'}, "
                  f"du lieu cu {'nguyen' if intact else 'HONG'}")
    for t in task_ids(c):
        c.request("task.delete", {"id": t})
    c.close()
    port.close()
    write("reset_during_write.csv", ["round", "reset_after_s", "back_ms", "new_record_kept", "old_data_intact", "passed"], rows)


def run_lockout(args):
    rows = []
    for i in range(1, 7):
        c = Client(args.host, args.port)
        r = c.login(args.user, args.password + "_sai")
        got = "OK" if r.get("ok") else r["error"]["code"]
        expect = "BUSY" if i == 6 else "UNAUTHORIZED"
        rows.append([i, expect, got, int(got == expect)])
        print(f"lan sai {i}: mong doi {expect:12s} thuc te {got}")
        c.close()
    c = Client(args.host, args.port)
    r = c.login(args.user, args.password)
    got = "OK" if r.get("ok") else r["error"]["code"]
    rows.append(["dung_mat_khau_khi_bi_khoa", "BUSY", got, int(got == "BUSY")])
    print(f"dang nhap dung khi dang bi khoa: {got}")
    c.close()
    if args.wait_unlock:
        print("cho 61 giay de het khoa...")
        time.sleep(61)
        c = Client(args.host, args.port)
        r = c.login(args.user, args.password)
        got = "OK" if r.get("ok") else r["error"]["code"]
        rows.append(["dung_mat_khau_sau_61_giay", "OK", got, int(got == "OK")])
        print(f"dang nhap dung sau 61 giay: {got}")
        c.close()
    write("lockout.csv", ["attempt", "expected", "actual", "passed"], rows)


def read_backup(c, name):
    data, offset = b"", 0
    while True:
        d = c.request("system.backup", {"file": name, "offset": offset})["data"]
        chunk = base64.b64decode(d["data"])
        data += chunk
        offset += len(chunk)
        if d["eof"]:
            return data


def send_restore(c, name, content, reboot=False):
    """Gửi cả file theo từng mẩu 4 KB như app sẽ làm. Trả phản hồi của mẩu lỗi đầu tiên hoặc của mẩu cuối."""
    offset = 0
    while True:
        chunk = content[offset:offset + 4096]
        last = offset + len(chunk) >= len(content)
        r = c.request("system.restore", {"file": name, "offset": offset, "data": base64.b64encode(chunk).decode(),
                                         "last": last, "reboot": reboot and last}, timeout=15)
        if last or not r.get("ok"):
            return r
        offset += len(chunk)


def run_restore(args):
    """Khôi phục trên bo thật, ngay sau khi tự sao lưu nên dữ liệu không đổi. Cuối kịch bản ESP khởi động lại:
    - bản hỏng (bị cắt, rác) phải bị từ chối, file thật giữ nguyên và server vẫn ghi được;
    - bản đúng phải được nhận, sau khi khởi động lại dữ liệu trùng khớp từng byte."""
    name = "schedule.json"
    rows = []

    def add(case, expect, got):
        rows.append([case, expect, got, int(got == expect)])
        print(f"{case:32s} mong doi {expect:8s} thuc te {got}")

    def code(r):
        return "OK" if r.get("ok") else r["error"]["code"]

    c = Client(args.host, args.port)
    if not c.login(args.user, args.password).get("ok"):
        raise RuntimeError("dang nhap that bai")
    original = read_backup(c, name)
    add("sao_luu_duoc", "OK", "OK" if original else "RONG")
    add("ban_bi_cat", "BAD_ARG", code(send_restore(c, name, original[:-2])))     # cắt giữa dòng cuối
    add("ban_rac", "BAD_ARG", code(send_restore(c, name, b"khong phai json\n")))
    add("file_that_giu_nguyen", "OK", "OK" if read_backup(c, name) == original else "KHAC")

    # Server chưa bị chặn ghi: tạo một công việc, chờ qua cửa sổ ghi trễ 1 giây, file phải đổi; xoá rồi file được ghi lại
    t = c.request("task.create", {"title": "restore test", "day": args.date_from, "start": "23:30", "deadline": True})
    time.sleep(2)
    add("van_ghi_duoc_sau_khi_tu_choi", "OK", "OK" if t.get("ok") and read_backup(c, name) != original else "KHONG")
    c.request("task.delete", {"id": t["data"]["id"]})
    time.sleep(2)
    original = read_backup(c, name)            # nextId đã tăng: đây là bản chuẩn để so sau khi khôi phục

    r = send_restore(c, name, original, reboot=True)
    add("ban_dung", "OK", code(r))
    c.close()
    time.sleep(3)                              # ESP khởi động lại sau 1,5 giây: đừng ping nhầm lúc nó chưa tắt
    back = wait_back(args, time.perf_counter())
    if back is None:
        add("len_lai_sau_khoi_phuc", "OK", "KHONG")
    else:
        c = Client(args.host, args.port)
        if not c.login(args.user, args.password).get("ok"):
            raise RuntimeError("dang nhap that bai sau khi khoi phuc")
        add("du_lieu_trung_sau_khoi_dong_lai", "OK", "OK" if read_backup(c, name) == original else "KHAC")
        c.close()
    write("restore_check.csv", ["case", "expected", "actual", "passed"], rows)


def run_concurrent(args):
    """Nhiều client cùng tạo công việc một lúc: không được mất hay trùng bản ghi, sau khi gộp ghi 1 giây file phải có đủ.
    Đếm thêm tin đẩy mỗi client nhận được; hàng đợi tin đẩy 32 phần tử, đầy thì bỏ, nên có thể thiếu khi tải dồn."""
    n_clients, per_client = 4, 10
    title = "concurrent test"
    results, lock, barrier = [], threading.Lock(), threading.Barrier(n_clients)

    def session():
        c = Client(args.host, args.port)
        if not c.login(args.user, args.password).get("ok"):
            raise RuntimeError("dang nhap that bai")
        return c

    def task_ids(c):
        out, off = [], 0
        while True:
            d = c.request("task.list", {"offset": off})["data"]
            out += [t["id"] for t in d["tasks"] if t["title"].startswith(title)]
            if "next" not in d:
                return out
            off = d["next"]

    def worker(c, k):
        ids, errors, t0 = [], 0, time.perf_counter()
        barrier.wait()
        for i in range(per_client):
            r = c.request("task.create", {"title": f"{title} {k} {i}", "day": args.date_from, "start": "23:30", "deadline": True},
                          timeout=15)
            if r.get("ok"):
                ids.append(r["data"]["id"])
            else:
                errors += 1
        with lock:
            results.append((ids, errors, time.perf_counter() - t0))

    clients = [session() for _ in range(n_clients)]
    for t in task_ids(clients[0]):                        # dọn lần chạy trước
        clients[0].request("task.delete", {"id": t})
    time.sleep(1.5)
    for c in clients:                                     # bỏ tin đẩy cũ trước khi đếm
        c.pending.clear()
    threads = [threading.Thread(target=worker, args=(c, k)) for k, c in enumerate(clients)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    expected = n_clients * per_client
    created = [i for ids, _, _ in results for i in ids]
    errors = sum(e for _, e, _ in results)
    time.sleep(2)                                         # qua cửa sổ gộp ghi 1 giây

    pushes = []
    for c in clients:
        got = 0
        c.sock.settimeout(0.3)
        while True:
            try:
                f = c.recv_frame(timeout=0.3)
            except (TimeoutError, OSError):
                break
            got += "push" in f
        pushes.append(got)

    listed = task_ids(clients[0])
    file_data = read_backup(clients[0], "schedule.json").decode("utf-8", "replace")
    in_file = sum(f'"id":{i},' in file_data or f'"id":{i}}}' in file_data for i in created)
    rows = [["so_lenh_loi", 0, errors, int(errors == 0)],
            ["ban_ghi_tao_duoc", expected, len(created), int(len(created) == expected)],
            ["id_khong_trung", expected, len(set(created)), int(len(set(created)) == expected)],
            ["co_trong_task_list", expected, sum(i in listed for i in created), int(set(created) <= set(listed))],
            ["co_trong_file_sau_2_giay", expected, in_file, int(in_file == expected)],
            ["tin_day_it_nhat_moi_client", expected, min(pushes), int(min(pushes) == expected)]]
    for r in rows:
        print(f"{r[0]:28s} mong doi {r[1]:3d} thuc te {r[2]:3d} {'DAT' if r[3] else 'KHONG DAT'}")
    print(f"thoi gian tao: {max(x[2] for x in results):.2f} giay; tin day moi client: {pushes}")
    for t in task_ids(clients[0]):
        clients[0].request("task.delete", {"id": t})
    for c in clients:
        c.close()
    write("concurrent_write.csv", ["case", "expected", "actual", "passed"], rows)


def main():
    ap = argparse.ArgumentParser(description="Chạy toàn bộ kịch bản đo tự động trên ESP32")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--user", default="admin")
    ap.add_argument("--password", required=True)
    ap.add_argument("--clients", default="1,2,4,8")
    ap.add_argument("--requests", type=int, default=200)
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--from", dest="date_from", default="2026-09-07")
    ap.add_argument("--to", dest="date_to", default="2026-09-13")
    ap.add_argument("--serial", help="cổng COM của ESP để reset (kịch bản reset)")
    ap.add_argument("--reset-rounds", type=int, default=10)
    ap.add_argument("--only", help="chỉ chạy các phần này, cách nhau dấu phẩy: latency,fragment,maxconn,replay,http,"
                                   "errors,range,concurrent,discovery,reset,restore,lockout")
    ap.add_argument("--no-wait-unlock", dest="wait_unlock", action="store_false")
    args = ap.parse_args()
    args.data, args.cmd, args.count, args.idle, args.push_rounds = None, "system.ping", 10, 3, 20
    steps = [("latency", run_latency_series), ("fragment", loadtest.run_fragment), ("maxconn", loadtest.run_maxconn),
             ("replay", loadtest.run_replay), ("http", lambda a: loadtest.run_http(ns(a, clients="1,4"))),
             ("errors", run_errors), ("range", run_range), ("concurrent", run_concurrent),
             ("discovery", loadtest.run_discovery), ("reset", run_reset), ("restore", run_restore), ("lockout", run_lockout)]
    only = set(args.only.split(",")) if args.only else None
    status = {}
    with socket.create_connection((args.host, args.port), timeout=5) as s:     # kiểm tra trước: có thấy ESP không
        pass
    for name, fn in steps:
        if only and name not in only:
            continue
        print(f"\n######## {name}")
        t0 = time.time()
        try:
            fn(args)
            status[name] = f"xong ({time.time() - t0:.0f} giay)"
        except Exception as e:                            # một kịch bản hỏng không chặn các kịch bản sau
            status[name] = f"LOI: {e!r}"
        time.sleep(2)
    print("\n######## tong ket")
    for k, v in status.items():
        print(f"{k:10s} {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
