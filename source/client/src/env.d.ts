// Kiểu của cầu nối do electron/preload.ts mở ra (TV1). Module khác dùng `api` trong core/api.ts.
import type { ConnState, IpcResult, LoginResult, PushMessage, ServerInfo, SessionInfo } from '../electron/shared'

declare global {
  interface Window {
    espRaw: {
      connect(host: string, port: number): Promise<IpcResult<void>>
      login(user: string, password: string): Promise<IpcResult<LoginResult>>
      logout(): Promise<IpcResult<void>>
      request(cmd: string, data?: object): Promise<IpcResult<any>>
      discover(timeoutMs?: number): Promise<IpcResult<ServerInfo[]>>
      session(): Promise<IpcResult<SessionInfo>>
      changePassword(password: string, user?: string): Promise<IpcResult<void>>
      createUser(user: string, role: string, password: string): Promise<IpcResult<{ id: number }>>
      printToPdf(defaultName: string): Promise<IpcResult<string | null>>
      onPush(cb: (msg: PushMessage) => void): () => void
      onConnection(cb: (state: ConnState) => void): () => void
    }
  }
}

export {}
