"""Server giả lập ESP32 (TV1) để làm app Electron và thử công cụ khi chưa có phần cứng.

Cài đặt đúng giao thức của firmware cho phần TV1: khung 4 byte + JSON, auth.* (challenge–response, token, chặn dò
mật khẩu), system.ping/status, tin đẩy, giới hạn 8 client, đóng kết nối 30 giây chưa đăng nhập / 60 giây im lặng,
UDP discovery cổng 5001. Có thêm dữ liệu mẫu cho school.list và calendar.week/range để các thành viên khác làm
giao diện. Đây KHÔNG phải server của đề tài: server thật chạy trên ESP32 (source/server).

  python fake_server.py                 # admin có mật khẩu ngẫu nhiên, in ra màn hình, phải đổi lần đầu
  python fake_server.py --dev           # admin / admin123, không bắt đổi mật khẩu
"""
import argparse
import asyncio
import datetime as dt
import hashlib
import hmac
import json
import os
import secrets
import struct
import time

from espproto import MAX_PAYLOAD, derive_key

MAX_CLIENTS = 8
UNAUTH_TIMEOUT = 30
IDLE_TIMEOUT = 60
CHALLENGE_TTL = 30
SESSION_TTL = 7 * 24 * 3600
ROLES = {"student": 1, "teacher": 2, "admin": 3}
FAKE_SECRET = os.urandom(32)


class Err(Exception):
    def __init__(self, code, message, field=None):
        super().__init__(message)
        self.code, self.message, self.field = code, message, field


class State:
    def __init__(self, dev: bool):
        self.users = {}           # name -> dict
        self.next_id = 1
        self.sessions = {}        # token -> (user_id, expires)
        self.blocked = {}         # ip -> (failures, first, blocked_until)
        self.clients = set()
        self.requests = 0
        self.errors = 0
        password = "admin123" if dev else secrets.token_urlsafe(6)[:8]
        salt = os.urandom(16).hex()
        self.add_user("admin", "admin", salt, 10000, derive_key(password, salt, 10000).hex(), must_change=not dev)
        print("=" * 50)
        print(f"  Tai khoan admin: admin / {password}" + ("" if dev else "  (phai doi mat khau lan dau)"))
        print("=" * 50)

    def add_user(self, name, role, salt, it, key, must_change=True):
        if name in self.users:
            raise Err("CONFLICT", "Ten dang nhap da ton tai", "user")
        u = {"id": self.next_id, "name": name, "role": role, "salt": salt, "iter": it, "key": key,
             "mustChange": must_change}
        self.next_id += 1
        self.users[name] = u
        return u

    def user_by_id(self, uid):
        return next((u for u in self.users.values() if u["id"] == uid), None)

    def is_blocked(self, ip):
        e = self.blocked.get(ip)
        return bool(e and time.monotonic() < e[2])

    def failure(self, ip):
        now = time.monotonic()
        n, first, until = self.blocked.get(ip, (0, now, 0))
        if n == 0 or now - first > 60:
            n, first = 0, now
        n += 1
        if n >= 5:
            self.blocked[ip] = (0, now, now + 60)
        else:
            self.blocked[ip] = (n, first, until)


class Conn:
    def __init__(self, state, reader, writer):
        self.state, self.reader, self.writer = state, reader, writer
        self.ip = writer.get_extra_info("peername")[0]
        self.user_id = 0
        self.challenge = None
        self.connected = self.last_rx = time.monotonic()

    def send(self, obj):
        body = json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.writer.write(struct.pack(">I", len(body)) + body)

    # ---------------------------------------------------------------- lệnh

    def cmd_ping(self, ctx, d):
        return {"pong": int(time.monotonic() * 1000)}

    def cmd_status(self, ctx, d):
        s = self.state
        return {"name": "espedushed", "fw": "fake-1.0.0", "mode": "sta", "ip": "127.0.0.1", "uptimeS": int(time.monotonic()),
                "timeSynced": True, "clients": len(s.clients), "maxClients": MAX_CLIENTS, "sessions": len(s.sessions),
                "requests": s.requests, "errors": s.errors, "fake": True}

    def cmd_challenge(self, ctx, d):
        if self.state.is_blocked(self.ip):
            raise Err("BUSY", "Sai qua nhieu lan, thu lai sau 60 giay")
        name = d.get("user")
        if not isinstance(name, str) or not 3 <= len(name) <= 32:
            raise Err("BAD_ARG", "Ten dang nhap 3..32 ky tu", "user")
        nonce = os.urandom(16).hex()
        self.challenge = (nonce, name, time.monotonic() + CHALLENGE_TTL)
        u = self.state.users.get(name)
        salt = u["salt"] if u else hmac.new(FAKE_SECRET, name.encode(), hashlib.sha256).digest()[:16].hex()
        return {"nonce": nonce, "salt": salt, "iter": u["iter"] if u else 10000}

    def cmd_login(self, ctx, d):
        s = self.state
        if s.is_blocked(self.ip):
            raise Err("BUSY", "Sai qua nhieu lan, thu lai sau 60 giay")
        ch, self.challenge = self.challenge, None          # nonce dùng một lần
        name, nonce, proof = d.get("user"), d.get("nonce"), d.get("proof", "")
        u = s.users.get(name)
        ok = bool(ch and u and ch[0] == nonce and ch[1] == name and time.monotonic() < ch[2])
        if ok:
            expected = hmac.new(bytes.fromhex(u["key"]), nonce.encode(), hashlib.sha256).hexdigest()
            ok = hmac.compare_digest(expected, str(proof).lower())
        if not ok:
            s.failure(self.ip)
            raise Err("UNAUTHORIZED", "Sai tai khoan hoac mat khau, hoac phien dang nhap da het han")
        s.blocked.pop(self.ip, None)
        token = os.urandom(16).hex()
        s.sessions[token] = (u["id"], time.monotonic() + SESSION_TTL)
        self.user_id = u["id"]
        expires = (dt.date.today() + dt.timedelta(days=7)).isoformat()
        return {"token": token, "expiresAt": expires, "user": {"id": u["id"], "name": u["name"], "role": u["role"]},
                "mustChangePassword": u["mustChange"]}

    def cmd_logout(self, ctx, d):
        self.state.sessions.pop(ctx["token"], None)
        self.user_id = 0
        return {}

    @staticmethod
    def read_secret(d):
        salt, key, it = d.get("salt", ""), d.get("key", ""), d.get("iter")
        if len(salt) != 32:
            raise Err("BAD_ARG", "salt phai la 32 ky tu hex", "salt")
        if len(key) != 64:
            raise Err("BAD_ARG", "key phai la 64 ky tu hex", "key")
        if not isinstance(it, int) or not 1000 <= it <= 200000:
            raise Err("BAD_ARG", "iter trong khoang 1000..200000", "iter")
        return salt, it, key

    def cmd_password(self, ctx, d):
        s = self.state
        salt, it, key = self.read_secret(d)
        me = s.user_by_id(ctx["user_id"])
        target = me
        if d.get("user"):
            target = s.users.get(d["user"])
            if not target:
                raise Err("NOT_FOUND", "Khong co tai khoan nay", "user")
            if target is not me and me["role"] != "admin":
                raise Err("FORBIDDEN", "Chi admin duoc dat lai mat khau nguoi khac", "user")
        target.update(salt=salt, iter=it, key=key, mustChange=target is not me)
        for t in [t for t, (uid, _) in s.sessions.items() if uid == target["id"]]:
            del s.sessions[t]
        if target is me:
            self.user_id = 0
        return {}

    def cmd_register(self, ctx, d):
        if d.get("role") not in ROLES:
            raise Err("BAD_ARG", "role la admin, teacher hoac student", "role")
        salt, it, key = self.read_secret(d)
        u = self.state.add_user(d.get("user"), d["role"], salt, it, key)
        push_all(self.state, "user", u["id"])
        return {"id": u["id"]}

    def cmd_me(self, ctx, d):
        u = self.state.user_by_id(ctx["user_id"])
        return {"user": {"id": u["id"], "name": u["name"], "role": u["role"]}, "mustChangePassword": u["mustChange"]}

    def cmd_users(self, ctx, d):
        return {"users": [{"id": u["id"], "name": u["name"], "role": u["role"], "mustChangePassword": u["mustChange"]}
                          for u in self.state.users.values()]}

    def cmd_remove(self, ctx, d):
        s = self.state
        u = s.users.get(d.get("user"))
        if not u:
            raise Err("NOT_FOUND", "Khong co tai khoan nay", "user")
        if u["id"] == ctx["user_id"]:
            raise Err("BAD_ARG", "Khong tu xoa tai khoan dang dung", "user")
        del s.users[u["name"]]
        push_all(s, "user", u["id"])
        return {}

    def cmd_school_list(self, ctx, d):
        return {"schools": [
            {"id": 1, "name": "Truong A", "shortName": "A", "color": "#2E7D32",
             "periods": [{"no": 3, "start": "07:10", "end": "08:10"}, {"no": 4, "start": "08:15", "end": "09:15"}]},
            {"id": 2, "name": "Truong B", "shortName": "B", "color": "#1565C0",
             "periods": [{"no": 3, "start": "08:50", "end": "09:40"}]}]}

    def sample_range(self, start: dt.date, end: dt.date):
        items, day = [], start
        while day <= end:
            if day.isoweekday() == 3:                  # thứ Tư: 2 buổi mẫu trùng giờ
                items.append({"sourceId": 1, "kind": 1, "status": 0, "date": day.isoformat(), "origDate": day.isoformat(),
                              "start": "07:10", "end": "09:15", "title": "Lap trinh mang", "room": "A2-301",
                              "classCode": "N01", "school": "A", "color": "#2E7D32"})
                items.append({"sourceId": 2, "kind": 1, "status": 0, "date": day.isoformat(), "origDate": day.isoformat(),
                              "start": "08:50", "end": "09:40", "title": "Mang may tinh", "room": "B-105",
                              "classCode": "L02", "school": "B", "color": "#1565C0"})
            day += dt.timedelta(days=1)
        return {"from": start.isoformat(), "to": end.isoformat(), "items": items, "conflicts": []}

    def cmd_week(self, ctx, d):
        try:
            day = dt.date.fromisoformat(d["date"]) if d.get("date") else dt.date.today()
        except ValueError:
            raise Err("BAD_ARG", "Sai ngay", "date")
        start = day - dt.timedelta(days=day.isoweekday() - 1)
        return self.sample_range(start, start + dt.timedelta(days=6))

    def cmd_range(self, ctx, d):
        try:
            a, b = dt.date.fromisoformat(d["from"]), dt.date.fromisoformat(d["to"])
        except (KeyError, ValueError):
            raise Err("BAD_ARG", "Thieu hoac sai from/to", "from")
        if (b - a).days + 1 > 62 or b < a:
            raise Err("BAD_ARG", "Khoang ngay qua dai hoac qua nhieu buoi")
        return self.sample_range(a, b)

    COMMANDS = {
        "system.ping": ("cmd_ping", 0), "system.status": ("cmd_status", 0),
        "auth.challenge": ("cmd_challenge", 0), "auth.login": ("cmd_login", 0),
        "auth.logout": ("cmd_logout", 1), "auth.password": ("cmd_password", 1), "auth.me": ("cmd_me", 1),
        "auth.register": ("cmd_register", 3), "auth.users": ("cmd_users", 3), "auth.remove": ("cmd_remove", 3),
        "school.list": ("cmd_school_list", 1), "calendar.week": ("cmd_week", 1), "calendar.range": ("cmd_range", 1),
    }

    # ---------------------------------------------------------------- xử lý một khung

    def handle(self, payload: bytes):
        s = self.state
        s.requests += 1
        rid = 0
        try:
            try:
                req = json.loads(payload.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                raise Err("BAD_JSON", "Noi dung khong phai JSON object")
            if not isinstance(req, dict):
                raise Err("BAD_JSON", "Noi dung khong phai JSON object")
            rid = req.get("id", 0) if isinstance(req.get("id"), (int, float)) else 0
            name = req.get("cmd")
            if not isinstance(name, str):
                raise Err("BAD_ARG", "Thieu truong cmd", "cmd")
            if name not in self.COMMANDS:
                raise Err("UNKNOWN_CMD", name)
            fn, min_role = self.COMMANDS[name]
            ctx = {"user_id": 0, "role": 0, "token": None}
            token = req.get("token")
            sess = s.sessions.get(token) if isinstance(token, str) else None
            if sess and sess[1] > time.monotonic():
                u = s.user_by_id(sess[0])
                if u:
                    ctx = {"user_id": u["id"], "role": ROLES[u["role"]], "token": token, "must": u["mustChange"]}
                    self.user_id = u["id"]
            if min_role > 0:
                if not ctx["user_id"]:
                    raise Err("UNAUTHORIZED", "Chua dang nhap hoac token het han")
                if ctx.get("must") and name not in ("auth.password", "auth.logout", "auth.me", "system.ping", "system.status"):
                    raise Err("FORBIDDEN", "Phai doi mat khau truoc")
                if ctx["role"] < min_role:
                    raise Err("FORBIDDEN", "Khong du quyen")
            data = req.get("data") or {}
            if not isinstance(data, dict):
                raise Err("BAD_ARG", "Truong data phai la object", "data")
            self.send({"id": rid, "ok": True, "data": getattr(self, fn)(ctx, data)})
        except Err as e:
            s.errors += 1
            self.send({"id": rid, "ok": False, "error": {"code": e.code, "message": e.message, "field": e.field}})


def push_all(state, entity, eid):
    for c in list(state.clients):
        if c.user_id:
            c.send({"push": "changed", "entity": entity, "id": eid})


async def serve_client(state, reader, writer):
    conn = Conn(state, reader, writer)
    if len(state.clients) >= MAX_CLIENTS:
        conn.send({"id": 0, "ok": False, "error": {"code": "BUSY", "message": "Server da du ket noi", "field": None}})
        await writer.drain()
        writer.close()
        return
    state.clients.add(conn)
    print(f"+ {conn.ip} ({len(state.clients)}/{MAX_CLIENTS})")
    buf = bytearray()
    try:
        while True:
            now = time.monotonic()
            limit = IDLE_TIMEOUT - (now - conn.last_rx)
            if not conn.user_id:
                limit = min(limit, UNAUTH_TIMEOUT - (now - conn.connected))
            if limit <= 0:
                break
            try:
                data = await asyncio.wait_for(reader.read(4096), timeout=limit)
            except asyncio.TimeoutError:
                break
            if not data:
                break
            conn.last_rx = time.monotonic()
            buf += data
            while len(buf) >= 4:
                n = struct.unpack(">I", buf[:4])[0]
                if n == 0 or n > MAX_PAYLOAD:
                    raise ConnectionError("khung sai do dai")
                if len(buf) < 4 + n:
                    break
                payload = bytes(buf[4:4 + n])
                del buf[:4 + n]
                conn.handle(payload)
            await writer.drain()
    except (ConnectionError, OSError) as e:
        print(f"! {conn.ip}: {e}")
    finally:
        state.clients.discard(conn)
        writer.close()
        print(f"- {conn.ip} ({len(state.clients)}/{MAX_CLIENTS})")


class Discovery(asyncio.DatagramProtocol):
    def __init__(self, port):
        self.port = port

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        if data.strip() == b"ESPEDUSHED_DISCOVER":
            reply = {"name": "espedushed", "ip": "127.0.0.1", "port": self.port, "fw": "fake-1.0.0"}
            self.transport.sendto(json.dumps(reply).encode(), addr)


async def main():
    ap = argparse.ArgumentParser(description="Server giả lập ESP32 cho ESPEduShed")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--udp-port", type=int, default=5001)
    ap.add_argument("--dev", action="store_true", help="admin/admin123, không bắt đổi mật khẩu")
    args = ap.parse_args()
    state = State(args.dev)
    server = await asyncio.start_server(lambda r, w: serve_client(state, r, w), "0.0.0.0", args.port)
    loop = asyncio.get_running_loop()
    await loop.create_datagram_endpoint(lambda: Discovery(args.port), local_addr=("0.0.0.0", args.udp_port))
    print(f"Fake ESPEduShed: TCP {args.port}, UDP discovery {args.udp_port}")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
