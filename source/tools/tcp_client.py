"""Gửi lệnh tới server ESPEduShed từ dòng lệnh (TV1). Chỉ dùng thư viện chuẩn của Python.

Ví dụ:
  python tcp_client.py --host 192.168.1.37 --cmd system.ping
  python tcp_client.py --host 192.168.1.37 --password Ab3dEf9h --cmd web.link
  python tcp_client.py --host ten.duckdns.org --password Ab3dEf9h --cmd calendar.week --data '{"date":"2026-09-02"}'
  python tcp_client.py --host 192.168.1.37 --password Ab3dEf9h --listen 60        # in tin đẩy trong 60 giây
  python tcp_client.py --host 192.168.1.37 --password Ab3dEf9h --new-password Moi12345
"""
import argparse
import json
import sys
import time

from espproto import Client, new_secret


def show(frame):
    print(json.dumps(frame, ensure_ascii=False, indent=2))


def main():
    ap = argparse.ArgumentParser(description="Client dòng lệnh cho giao thức TCP của ESPEduShed")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--user", default="admin", help="tên tài khoản (server chỉ có một tài khoản: admin)")
    ap.add_argument("--password", help="có mật khẩu thì đăng nhập trước khi gửi lệnh")
    ap.add_argument("--cmd", help="tên lệnh, ví dụ system.status")
    ap.add_argument("--data", help="JSON cho trường data")
    ap.add_argument("--listen", type=float, default=0, help="số giây chờ và in tin đẩy")
    ap.add_argument("--new-password", help="đổi mật khẩu của tài khoản đang đăng nhập")
    args = ap.parse_args()

    c = Client(args.host, args.port)
    try:
        if args.password is not None:
            res = c.login(args.user, args.password)
            show(res)
            if not res.get("ok"):
                return 1
        if args.new_password:
            show(c.request("auth.password", new_secret(args.new_password)))
            c.token = None
            res = c.login(args.user, args.new_password)
            show(res)
        if args.cmd:
            data = json.loads(args.data) if args.data else None
            t0 = time.perf_counter()
            res = c.request(args.cmd, data)
            show(res)
            print(f"# thoi gian khu hoi: {(time.perf_counter() - t0) * 1000:.1f} ms", file=sys.stderr)
        if args.listen > 0:
            end = time.monotonic() + args.listen
            while time.monotonic() < end:
                try:
                    show(c.recv_frame(timeout=min(15, end - time.monotonic())))
                except TimeoutError:
                    c.request("system.ping")            # giữ kết nối: server đóng nếu im lặng 60 giây
                except OSError as e:
                    if "timed out" in str(e):
                        c.request("system.ping")
                    else:
                        raise
    finally:
        c.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
