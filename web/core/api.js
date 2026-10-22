// Gọi REST API của ESP32 (TV1). Mọi module dùng các hàm này, không tự gọi fetch.
const TOKEN_KEY = "ld_token";

export function getToken() {
  try {
    return localStorage.getItem(TOKEN_KEY);
  } catch {
    return null;
  }
}

export function setToken(token) {
  try {
    if (token) localStorage.setItem(TOKEN_KEY, token);
    else localStorage.removeItem(TOKEN_KEY);
  } catch {
    /* trình duyệt chặn lưu trữ: chỉ đăng nhập được trong phiên này */
  }
}

/** Lỗi API: có status (HTTP), code (BAD_ARG, CONFLICT...), field và data (toàn bộ JSON trả về). */
export class ApiError extends Error {
  constructor(status, data) {
    super(data?.error?.message || `HTTP ${status}`);
    this.status = status;
    this.code = data?.error?.code;
    this.field = data?.error?.field;
    this.data = data;
  }
}

export async function api(method, path, body) {
  const headers = {};
  const token = getToken();
  if (token) headers.Authorization = "Bearer " + token;
  if (body !== undefined) headers["Content-Type"] = "application/json";
  const res = await fetch(path, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) });
  if (res.status === 204) return null;
  const data = await res.json().catch(() => null);
  if (!res.ok) {
    if (res.status === 401) location.hash = "#/login";
    throw new ApiError(res.status, data);
  }
  return data;
}

export const get = (path) => api("GET", path);
export const post = (path, body) => api("POST", path, body);
export const put = (path, body) => api("PUT", path, body);
export const del = (path) => api("DELETE", path);
