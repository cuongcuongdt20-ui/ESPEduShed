"""Thư viện dùng chung cho các công cụ Python (TV1): ghép khung TCP và đăng nhập challenge–response.

Khung: 4 byte độ dài big-endian + JSON UTF-8, tối đa 8192 byte (README mục 5.3.1).
Chỉ dùng thư viện chuẩn của Python.
"""
import hashlib
import hmac
import json
import os
import socket
import struct
import time

MAX_PAYLOAD = 8192


def encode_frame(obj) -> bytes:
    body = json.dumps(obj, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if not 0 < len(body) <= MAX_PAYLOAD:
        raise ValueError(f"khung {len(body)} byte vượt giới hạn {MAX_PAYLOAD}")
    return struct.pack(">I", len(body)) + body


class FrameReader:
    """Ghép khung từ luồng byte TCP: nhận mẩu bất kỳ, trả về các khung đã đủ."""

    def __init__(self):
        self.buf = bytearray()

    def feed(self, data: bytes):
        self.buf += data
        frames = []
        while len(self.buf) >= 4:
            n = struct.unpack(">I", self.buf[:4])[0]
            if n == 0 or n > MAX_PAYLOAD:
                raise ValueError(f"độ dài khung sai: {n}")
            if len(self.buf) < 4 + n:
                break
            frames.append(json.loads(self.buf[4:4 + n].decode("utf-8")))
            del self.buf[:4 + n]
        return frames


def derive_key(password: str, salt_hex: str, iterations: int) -> bytes:
    return hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), bytes.fromhex(salt_hex), iterations, 32)


def make_proof(key: bytes, nonce: str) -> str:
    return hmac.new(key, nonce.encode("ascii"), hashlib.sha256).hexdigest()


def new_secret(password: str, iterations: int = 10000):
    """Sinh {salt, iter, key} để tạo tài khoản hoặc đổi mật khẩu (server không bao giờ thấy mật khẩu)."""
    salt = os.urandom(16).hex()
    return {"salt": salt, "iter": iterations, "key": derive_key(password, salt, iterations).hex()}


class Client:
    """Client TCP đồng bộ, đủ dùng cho công cụ dòng lệnh và đo tải."""

    def __init__(self, host: str, port: int = 5000, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.reader = FrameReader()
        self.pending = []           # khung đã nhận nhưng chưa ai lấy (tin đẩy, phản hồi lệch thứ tự)
        self.next_id = 1
        self.token = None

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def send_raw(self, data: bytes):
        self.sock.sendall(data)

    def recv_frame(self, timeout: float = 5.0):
        if self.pending:
            return self.pending.pop(0)
        self.sock.settimeout(timeout)
        while True:
            data = self.sock.recv(4096)
            if not data:
                raise ConnectionError("server đóng kết nối")
            frames = self.reader.feed(data)
            if frames:
                self.pending.extend(frames[1:])
                return frames[0]

    def request(self, cmd: str, data=None, timeout: float = 5.0):
        """Gửi một lệnh và chờ đúng phản hồi có cùng id. Tin đẩy nhận trong lúc chờ được giữ lại."""
        rid = self.next_id
        self.next_id += 1
        msg = {"id": rid, "cmd": cmd}
        if self.token:
            msg["token"] = self.token
        if data is not None:
            msg["data"] = data
        self.sock.sendall(encode_frame(msg))
        deadline = time.monotonic() + timeout
        held = []
        try:
            while True:
                left = deadline - time.monotonic()
                if left <= 0:
                    raise TimeoutError(f"không có phản hồi cho {cmd} sau {timeout} giây")
                frame = self.recv_frame(left)
                if frame.get("id") == rid and "push" not in frame:
                    return frame
                held.append(frame)
        finally:
            self.pending = held + self.pending

    def login(self, user: str, password: str):
        ch = self.request("auth.challenge", {"user": user})
        if not ch.get("ok"):
            return ch
        d = ch["data"]
        key = derive_key(password, d["salt"], d["iter"])
        res = self.request("auth.login", {"user": user, "nonce": d["nonce"], "proof": make_proof(key, d["nonce"])})
        if res.get("ok"):
            self.token = res["data"]["token"]
        return res
