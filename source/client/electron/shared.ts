// Kiểu dữ liệu dùng chung giữa main process và giao diện (TV1). Chỉ chứa type, không có mã chạy.

export type ConnState = 'connecting' | 'online' | 'offline'
export interface User {
  id: number
  name: string
}

export interface LoginResult {
  user: User
  mustChangePassword: boolean
}

export interface ServerInfo {
  name: string
  ip: string
  port: number
  fw: string
}

export interface SessionInfo {
  host: string | null
  port: number | null
  state: ConnState
  user: User | null
  mustChangePassword: boolean
}

export interface ApiErrorData {
  code: string                 // BAD_ARG, CONFLICT, UNAUTHORIZED, OFFLINE, TIMEOUT, ...
  message: string
  field: string | null
  data?: unknown               // ví dụ {conflicts:[...]} khi code = CONFLICT
}

// Kết quả qua IPC: không ném lỗi qua ranh giới IPC vì Electron chỉ giữ lại message, mất code và field
export type IpcResult<T> = { ok: true; data: T } | { ok: false; error: ApiErrorData }

export interface PushMessage {
  push: 'changed'
  entity: string
  id: number
}

// Hợp đồng window.api giữa TV1 và các thành viên khác (README mục B1)
export interface EspApi {
  connect(host: string, port: number): Promise<void>
  login(password: string): Promise<LoginResult>
  logout(): Promise<void>
  request<T = any>(cmd: string, data?: object): Promise<T>
  onPush(cb: (msg: PushMessage) => void): () => void
  onConnection(cb: (state: ConnState) => void): () => void
  discover(timeoutMs?: number): Promise<ServerInfo[]>
  // Thêm của TV1 cho đổi mật khẩu và in ấn
  session(): Promise<SessionInfo>
  changePassword(newPassword: string): Promise<void>
  printToPdf(defaultName: string): Promise<string | null>
}
