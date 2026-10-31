"""Chạy mọi lệnh trong docs/protocol.md trên ESP32 thật và ghi protocol_check.csv (TV1). Chỉ dùng thư viện chuẩn.

  $env:ESP_PW = "<mat_khau>"
  python protocol_check.py --host 192.168.1.37 --snapshot <thu_muc_ngoai_repo>

Dữ liệu thử đặt ở năm 2030, tên bắt đầu bằng ZZ. Trước khi chạy, script sao lưu catalog.json, schedule.json,
conflict_ack.json vào --snapshot; cuối cùng khôi phục lại, khởi động lại ESP và so từng byte.

Lệnh có tác dụng thật:
  auth.password  đặt lại CÙNG mật khẩu với salt mới, mọi phiên đang mở phải đăng nhập lại
  web.rotate     đổi khoá thật: link xem lịch và mã QR cũ hết hiệu lực
  system.wifi, system.ddns  chỉ thử nhánh lỗi, vì thành công sẽ đổi Wi-Fi hoặc tắt DuckDNS và mất đường tới ESP
Không ghi token, key hay mật khẩu ra CSV.
"""
import argparse
import csv
import http.client
import os
import sys
import time

import loadtest
from espproto import Client, new_secret
from run_experiments import read_backup, send_restore, wait_back

DATA_FILES = ["catalog.json", "schedule.json", "conflict_ack.json"]
EMPTY_ACK = b'{"schema":1,"acks":[]}'
Y = "2030"


class Check:
    def __init__(self, args):
        self.args = args
        self.rows = []
        self.seen = set()
        self.c = None

    def connect(self):
        if self.c:
            self.c.close()
        self.c = Client(self.args.host, self.args.port, timeout=10)
        r = self.c.login(self.args.user, self.args.password)
        if not r.get("ok"):
            raise RuntimeError("dang nhap that bai: " + r["error"]["code"])
        return r["data"]

    def record(self, cmd, case, expect, got, detail=""):
        ok = got == expect
        self.rows.append([cmd, case, expect, got, int(ok), detail])
        self.seen.add(cmd)
        print(f"{'DAT ' if ok else 'SAI '} {cmd:18s} {case:34s} mong doi {expect:12s} thuc te {got} {detail}")
        return ok

    def t(self, cmd, data=None, expect="OK", case="", client=None, check=None):
        """Gửi lệnh, so mã kết quả. check(data) trả chuỗi rỗng nếu đúng, ngược lại là mô tả sai."""
        c = client or self.c
        r = c.request(cmd, data if data is not None else {}, timeout=15)
        got = "OK" if r.get("ok") else r["error"]["code"]
        out = r.get("data") or {}
        detail = ""
        if got == expect and check:
            detail = check(out) or ""
            if detail:
                got = "SAI_DU_LIEU"
        if got != expect and not r.get("ok"):
            detail = (r["error"].get("message") or "")[:70]
        self.record(cmd, case or expect.lower(), expect, got, detail)
        return out


def http_status(args, path):
    c = http.client.HTTPConnection(args.host, 8080, timeout=15)
    try:
        c.request("GET", path)
        r = c.getresponse()
        r.read()
        return r.status, r.getheader("Cache-Control") or ""
    finally:
        c.close()


def run(k):
    a = k.args
    t = k.t
    # ---------- công khai và không có token ----------
    anon = Client(a.host, a.port, timeout=10)
    t("system.ping", client=anon, check=lambda d: "" if "pong" in d else "thieu pong")
    t("system.status", client=anon, check=lambda d: "" if "ddns" not in d and d.get("fw") else "lo ddns hoac thieu fw", case="an_ddns_khi_chua_login")
    t("khong.co.lenh", expect="UNKNOWN_CMD", client=anon, case="lenh_la")
    t("auth.challenge", {"user": "ab"}, "BAD_ARG", "ten_qua_ngan", anon)
    t("auth.challenge", {"user": a.user}, "OK", "dung_ten", anon, lambda d: "" if {"nonce", "salt", "iter"} <= set(d) else "thieu truong")
    t("auth.challenge", {"user": "khong-co-that"}, "OK", "ten_la_van_co_salt", anon, lambda d: "" if len(d.get("salt", "")) == 32 else "salt sai")
    for cmd in ("auth.logout", "auth.me", "auth.password", "calendar.range", "calendar.week", "campus.create", "campus.delete",
                "campus.list", "campus.update", "class.create", "class.delete", "class.list", "class.update", "conflict.ack",
                "conflict.check", "conflict.list", "exception.delete", "exception.set", "holiday.create", "holiday.delete",
                "holiday.list", "holiday.update", "occurrence.range", "periods.set", "schedule.create", "schedule.delete",
                "schedule.get", "schedule.list", "schedule.update", "school.create", "school.delete", "school.list",
                "school.update", "subject.create", "subject.delete", "subject.list", "subject.update", "system.backup",
                "system.ddns", "system.restore", "system.wifi", "task.create", "task.delete", "task.list", "task.update",
                "travel.delete", "travel.get", "travel.set", "travel.setDefault", "web.link", "web.rotate"):
        t(cmd, {}, "UNAUTHORIZED", "khong_token", anon)
    anon.close()

    # ---------- đăng nhập, phiên ----------
    login = k.connect()
    k.record("auth.login", "dung_mat_khau", "OK", "OK", "mustChange=" + str(login.get("mustChangePassword")))
    t("auth.me", check=lambda d: "" if d.get("user", {}).get("name") == a.user else "sai user")
    t("system.status", case="co_ddns_khi_login", check=lambda d: "" if "ddns" in d else "thieu ddns")
    other = Client(a.host, a.port, timeout=10)
    other.login(a.user, a.password)
    t("auth.logout", client=other, case="phien_thu_hai")
    t("auth.me", expect="UNAUTHORIZED", client=other, case="token_da_dang_xuat")
    other.close()

    # ---------- nhánh lỗi của lệnh không được chạy thành công ----------
    t("system.wifi", {"ssid": ""}, "BAD_ARG", "ssid_rong")
    t("system.wifi", {"ssid": "ZZ", "pass": "1234567"}, "BAD_ARG", "pass_ngan")
    t("system.ddns", {}, "BAD_ARG", "thieu_domain")
    t("system.ddns", {"domain": "ten sai!"}, "BAD_ARG", "domain_sai")
    t("system.ddns", {"domain": "zzthu"}, "BAD_ARG", "thieu_token")

    # ---------- danh mục ----------
    sid = t("school.create", {"name": "ZZ Truong thu", "shortName": "ZZ", "color": "#123456"})["id"]
    sid_dup = t("school.create", {"name": "ZZ Truong thu", "shortName": "ZZ2", "color": "#123456"}, case="trung_ten_van_nhan")["id"]
    t("school.create", {"shortName": "ZZ"}, "BAD_ARG", "thieu_ten")
    t("school.update", {"id": sid, "name": "ZZ Truong sua", "shortName": "ZZ", "color": "#654321"})
    t("school.update", {"id": 65000, "name": "ZZ Khong co", "shortName": "ZX", "color": "#654321"}, "NOT_FOUND", "id_la")
    t("school.list", check=lambda d: "" if any(s["id"] == sid and s["name"] == "ZZ Truong sua" for s in d["schools"]) else "khong thay truong")
    t("periods.set", {"schoolId": sid, "periods": [{"no": 1, "start": "08:00", "end": "09:00"}, {"no": 2, "start": "08:30", "end": "09:30"}]}, "BAD_ARG", "tiet_chong")
    t("periods.set", {"schoolId": sid, "periods": [{"no": 1, "start": "08:00", "end": "08:50"}, {"no": 2, "start": "09:00", "end": "09:50"},
                                                  {"no": 3, "start": "10:00", "end": "10:50"}]})
    ca = t("campus.create", {"schoolId": sid, "name": "ZZ Co so A", "address": "Thu"})["id"]
    cb = t("campus.create", {"schoolId": sid, "name": "ZZ Co so B", "address": "Thu"})["id"]
    t("campus.create", {"schoolId": 65000, "name": "ZZ Sai", "address": "Thu"}, "NOT_FOUND", "truong_la")
    t("campus.update", {"id": cb, "schoolId": sid, "name": "ZZ Co so B2", "address": "Thu"})
    t("campus.list", {"schoolId": sid}, check=lambda d: "" if len(d["campuses"]) == 2 else "so co so sai")
    hid = t("holiday.create", {"schoolId": sid, "name": "ZZ Nghi", "from": f"{Y}-06-03", "to": f"{Y}-06-04"})["id"]
    t("holiday.create", {"schoolId": sid, "name": "ZZ Sai", "from": f"{Y}-06-05", "to": f"{Y}-06-01"}, "BAD_ARG", "to_truoc_from")
    t("holiday.update", {"id": hid, "schoolId": sid, "name": "ZZ Nghi 2", "from": f"{Y}-06-03", "to": f"{Y}-06-05"})
    t("holiday.list", {"schoolId": sid}, check=lambda d: "" if any(h["id"] == hid for h in d["holidays"]) else "khong thay")
    subj = t("subject.create", {"code": "ZZT01", "name": "ZZ Mon thu", "credits": 3})["id"]
    t("subject.create", {"code": "ZZT01", "name": "ZZ Trung", "credits": 3}, "CONFLICT", "trung_ma")
    t("subject.update", {"id": subj, "code": "ZZT01", "name": "ZZ Mon sua", "credits": 2})
    t("subject.list", check=lambda d: "" if any(s["id"] == subj for s in d["subjects"]) else "khong thay")
    cls = t("class.create", {"code": "ZZC1", "subjectId": subj, "schoolId": sid, "size": 30})["id"]
    t("class.update", {"id": cls, "code": "ZZC1", "subjectId": subj, "schoolId": sid, "size": 31})
    t("class.list", {"schoolId": sid}, check=lambda d: "" if any(c["id"] == cls for c in d["classes"]) else "khong thay")
    t("travel.set", {"campusA": ca, "campusB": cb, "minutes": 60})
    t("travel.set", {"campusA": ca, "campusB": cb, "minutes": 9999}, "BAD_ARG", "phut_qua_lon")
    t("travel.setDefault", {"minutes": 45})
    t("travel.get", check=lambda d: "" if d.get("defaultTravel") == 45 and any(x["minutes"] == 60 for x in d["travel"]) else "sai gia tri")

    # ---------- lịch dạy: trùng giờ và thiếu thời gian di chuyển ----------
    base = {"subject_code": "ZZT01", "subject_name": "ZZ Mon thu", "school_id": sid, "date_from": f"{Y}-03-04", "date_to": f"{Y}-04-29",
            "day_of_week": 1, "weekly": True}
    s1 = t("schedule.create", {**base, "time_mode": 0, "period_start": 1, "period_count": 1, "campus_id": ca, "room": "ZZ1"}, case="theo_tiet")["id"]
    reason = lambda want: (lambda d: "" if any(c.get("reason") == want for c in d.get("conflicts", [])) else "sai reason")
    t("schedule.create", {**base, "subject_code": "ZZT02", "time_mode": 1, "start": "08:30", "end": "09:30", "campus_id": ca}, "CONFLICT", "trung_gio_OVERLAP", check=reason(1))
    t("schedule.create", {**base, "subject_code": "ZZT03", "time_mode": 1, "start": "09:00", "end": "09:50", "campus_id": cb}, "CONFLICT", "thieu_di_chuyen_TRAVEL", check=reason(2))
    s2 = t("schedule.create", {**base, "subject_code": "ZZT03", "time_mode": 1, "start": "09:00", "end": "09:50", "campus_id": cb, "force": True}, case="force_van_luu")["id"]
    t("schedule.create", {**base, "time_mode": 0, "period_start": 7, "period_count": 1}, "BAD_ARG", "tiet_khong_co")
    t("schedule.list", check=lambda d: "" if {s1, s2} <= {s["id"] for s in d["schedules"]} else "khong thay")
    t("schedule.get", {"id": s1}, check=lambda d: "" if d["schedule"]["start"] == "08:00" else "quy doi tiet sai")
    t("schedule.get", {"id": 999999}, "NOT_FOUND", "id_la")
    upd = {"id": s1, **base, "time_mode": 0, "period_start": 1, "period_count": 1, "campus_id": ca, "room": "ZZ1B"}
    t("schedule.update", upd, "CONFLICT", "van_thieu_di_chuyen", check=reason(2))
    t("schedule.update", {**upd, "force": True}, case="force_van_luu", check=lambda d: "" if "removedExceptions" in d else "thieu removedExceptions")
    t("schedule.update", {**upd, "id": 999999}, "NOT_FOUND", "id_la")
    t("school.delete", {"id": sid}, "CONFLICT", "truong_dang_dung")

    # ---------- ngoại lệ, đủ 5 loại ----------
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-03-11", "type": 1}, case="1_huy")
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-03-18", "type": 2, "new_room": "ZZ9"}, case="2_doi_phong")
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-03-25", "type": 3, "new_start": "07:00", "new_end": "07:30"}, case="3_doi_gio")
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-04-01", "type": 4, "new_date": f"{Y}-04-03", "new_start": "13:00", "new_end": "13:50"}, case="4_doi_buoi")
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-04-08", "type": 5, "new_date": f"{Y}-04-10", "new_start": "14:00", "new_end": "14:50"}, case="5_day_bu")
    t("exception.set", {"series_id": s1, "orig_date": f"{Y}-03-18", "type": 2}, "BAD_ARG", "thieu_new_room")
    t("exception.delete", {"series_id": s1, "orig_date": f"{Y}-03-11"})
    t("exception.delete", {"series_id": s1, "orig_date": f"{Y}-03-12"}, "NOT_FOUND", "khong_co_ngoai_le")
    occ = t("occurrence.range", {"from": f"{Y}-03-04", "to": f"{Y}-04-14"}, check=lambda d: "" if any(i["date"] == f"{Y}-04-10" for i in d["items"]) else "thieu buoi bu")
    t("occurrence.range", {"from": f"{Y}-03-04", "to": f"{Y}-07-01"}, "BAD_ARG", "qua_62_ngay")

    # ---------- công việc ----------
    t1 = t("task.create", {"title": "ZZ Viec", "day": f"{Y}-05-06", "start": "10:00", "end": "11:00"})["id"]
    t("task.create", {"title": "ZZ Trung", "day": f"{Y}-05-06", "start": "10:30", "end": "11:30"}, "CONFLICT", "trung_gio")
    t2 = t("task.create", {"title": "ZZ Han", "day": f"{Y}-05-06", "start": "10:30", "deadline": True}, case="han_chot_khong_tinh_trung")["id"]
    t("task.update", {"id": t1, "title": "ZZ Viec sua", "day": f"{Y}-05-06", "start": "12:00", "end": "13:00"})
    t("task.list", check=lambda d: "" if {t1, t2} <= {x["id"] for x in d["tasks"]} else "khong thay")
    t("task.delete", {"id": t2})
    t("task.delete", {"id": t2}, "NOT_FOUND", "da_xoa")

    # ---------- lịch tuần, xung đột ----------
    t("calendar.week", {"date": f"{Y}-03-06"}, check=lambda d: "" if d["from"] == f"{Y}-03-04" and d["conflicts"] else "sai tuan hoac thieu xung dot")
    t("calendar.range", {"from": f"{Y}-03-04", "to": f"{Y}-03-10"}, check=lambda d: "" if d["items"] else "rong")
    t("calendar.range", {"from": f"{Y}-03-04", "to": f"{Y}-07-01"}, "BAD_ARG", "qua_62_ngay")
    cl = t("conflict.list", {"from": f"{Y}-03-04", "to": f"{Y}-03-10"}, check=lambda d: "" if any(c["reason"] == 2 for c in d["conflicts"]) else "thieu xung dot di chuyen")
    t("conflict.check", {"date": f"{Y}-03-04", "start": "08:10", "end": "08:20", "schoolId": sid, "campusId": ca}, case="buoi_moi_trung", check=lambda d: "" if d["conflicts"] else "khong bao trung")
    t("conflict.check", {"date": f"{Y}-03-05", "start": "08:10", "end": "08:20", "schoolId": sid, "campusId": ca}, case="buoi_moi_khong_trung", check=lambda d: "" if not d["conflicts"] else "bao nham")
    src = next(i["sourceId"] for i in occ["items"] if i["date"] == f"{Y}-03-18")
    t("conflict.check", {"sourceId": src, "origDate": f"{Y}-03-18", "date": f"{Y}-03-19", "start": "08:00", "end": "08:50"}, case="doi_buoi_co_san")
    t("conflict.check", {"sourceId": 999999, "origDate": f"{Y}-03-18", "date": f"{Y}-03-19", "start": "08:00", "end": "08:50"}, "NOT_FOUND", "nguon_la")
    if cl.get("conflicts"):
        x = cl["conflicts"][0]
        t("conflict.ack", {"a": x["a"]["sourceId"], "b": x["b"]["sourceId"], "date": x["date"]})
        t("conflict.list", {"from": f"{Y}-03-04", "to": f"{Y}-03-10"}, case="da_xac_nhan", check=lambda d: "" if any(c.get("acked") for c in d["conflicts"]) else "chua danh dau")
    t("conflict.ack", {"a": 999998, "b": 999999, "date": f"{Y}-03-04"}, "NOT_FOUND", "cap_khong_trung")

    # ---------- xoá dữ liệu thử bằng lệnh xoá ----------
    t("schedule.delete", {"id": s2})
    t("schedule.delete", {"id": s1})
    t("schedule.delete", {"id": s1}, "NOT_FOUND", "da_xoa")
    t("task.delete", {"id": t1})
    t("travel.delete", {"campusA": ca, "campusB": cb})
    t("holiday.delete", {"id": hid})
    t("subject.delete", {"id": subj}, "CONFLICT", "mon_con_lop")
    t("class.delete", {"id": cls})
    t("subject.delete", {"id": subj})
    t("campus.delete", {"id": cb})
    t("campus.delete", {"id": ca})
    t("school.delete", {"id": sid})
    t("school.delete", {"id": sid_dup}, case="truong_trung_ten")
    t("school.delete", {"id": sid}, "NOT_FOUND", "da_xoa")

    # ---------- sao lưu, khôi phục nhánh lỗi ----------
    t("system.backup", check=lambda d: "" if any(f["name"] == "schedule.json" for f in d["files"]) else "khong thay file")
    t("system.backup", {"file": "schedule.json", "offset": 0}, case="doc_mau", check=lambda d: "" if "data" in d else "thieu data")
    t("system.backup", {"file": "../secret.json"}, "BAD_ARG", "ten_file_sai")
    t("system.restore", {"file": "../x.json", "offset": 0, "data": ""}, "BAD_ARG", "ten_file_sai")
    t("system.restore", {"file": "zz_thu.json", "offset": 0, "data": "a2hvbmc=", "last": True}, "BAD_ARG", "noi_dung_hong")

    # ---------- link web: khoá thật bị đổi ----------
    old = t("web.link", check=lambda d: "" if len(d.get("key", "")) == 32 else "khoa sai")["key"]
    code, cache = http_status(a, f"/c/{old}")
    k.record("GET /c/<key>", "khoa_dung", "200", str(code), "no-store" if "no-store" in cache else "thieu no-store")
    code, _ = http_status(a, "/fc.js")
    k.record("GET /fc.js", "cong_khai", "200", str(code))
    code, _ = http_status(a, f"/api/{old}/range?from={Y}-03-04&to={Y}-03-10")
    k.record("GET /api/<key>/range", "khoa_dung", "200", str(code))
    code, _ = http_status(a, f"/api/{old}/range?from={Y}-03-04&to={Y}-07-01")
    k.record("GET /api/<key>/range", "qua_62_ngay", "400", str(code))
    new = t("web.rotate", check=lambda d: "" if len(d.get("key", "")) == 32 and d["key"] != old else "khoa khong doi")["key"]
    code, _ = http_status(a, f"/c/{old}")
    k.record("GET /c/<key>", "khoa_cu_sau_rotate", "404", str(code))
    code, _ = http_status(a, f"/c/{new}")
    k.record("GET /c/<key>", "khoa_moi_sau_rotate", "200", str(code))

    # ---------- đổi mật khẩu: giữ nguyên mật khẩu, salt mới ----------
    t("auth.password", {"salt": "zz", "iter": 10000, "key": "zz"}, "BAD_ARG", "salt_sai")
    t("auth.password", new_secret(a.password), case="cung_mat_khau_salt_moi")
    t("auth.me", expect="UNAUTHORIZED", case="token_cu_het_hieu_luc")
    k.c.token = None
    k.connect()
    k.record("auth.login", "dang_nhap_lai_sau_doi", "OK", "OK")


def main():
    ap = argparse.ArgumentParser(description="Chạy mọi lệnh trong docs/protocol.md trên ESP32")
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--user", default="admin")
    ap.add_argument("--snapshot", required=True, help="thư mục ngoài repo để giữ bản sao lưu trước khi chạy")
    args = ap.parse_args()
    args.password = os.environ.get("ESP_PW", "")
    if not args.password:
        raise SystemExit("can dat bien moi truong ESP_PW")

    k = Check(args)
    k.connect()
    os.makedirs(args.snapshot, exist_ok=True)
    files = {f["name"] for f in k.c.request("system.backup", {})["data"]["files"]}
    snap = {}
    for name in DATA_FILES:
        snap[name] = read_backup(k.c, name) if name in files else (EMPTY_ACK if name == "conflict_ack.json" else None)
        if snap[name] is not None:
            with open(os.path.join(args.snapshot, name), "wb") as f:
                f.write(snap[name])
    print("da sao luu:", {n: len(b) for n, b in snap.items() if b is not None})

    error = None
    try:
        run(k)
    except Exception as e:                                # vẫn phải khôi phục dữ liệu dù giữa chừng hỏng
        error = repr(e)
        print("DUNG GIUA CHUNG:", error)

    # ---------- khôi phục đúng dữ liệu trước khi chạy ----------
    time.sleep(2)
    k.connect()
    names = [n for n in DATA_FILES if snap[n] is not None]
    for i, name in enumerate(names):
        r = send_restore(k.c, name, snap[name], reboot=i == len(names) - 1)
        k.record("system.restore", "khoi_phuc_" + name, "OK", "OK" if r.get("ok") else r["error"]["code"])
    k.c.close()
    k.c = None
    time.sleep(3)
    back = wait_back(args, time.perf_counter())
    k.record("system.ping", "len_lai_sau_khoi_phuc", "OK", "OK" if back is not None else "KHONG", f"{back:.0f} ms" if back else "")
    k.connect()
    for name in names:
        same = read_backup(k.c, name) == snap[name]
        k.record("system.backup", "trung_tung_byte_" + name, "OK", "OK" if same else "KHAC")
    left = k.c.request("school.list")["data"]["schools"]
    k.record("school.list", "khong_con_du_lieu_thu", "OK", "OK" if not any(s["name"].startswith("ZZ") for s in left) else "CON")
    k.c.close()

    loadtest.write_csv("protocol_check.csv", ["cmd", "case", "expected", "actual", "passed", "detail"], k.rows)
    bad = [r for r in k.rows if not r[4]]
    cmds = sorted({r[0] for r in k.rows if " " not in r[0] and r[0] != "khong.co.lenh"})
    print(f"\ntong {len(k.rows)} truong hop, dat {len(k.rows) - len(bad)}, sai {len(bad)}; {len(cmds)} lenh")
    for r in bad:
        print("  SAI", r)
    if error:
        print("LOI:", error)
    return 1 if bad or error else 0


if __name__ == "__main__":
    sys.exit(main())
