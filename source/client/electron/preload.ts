// Preload (TV1): cầu nối an toàn giữa giao diện và main process. Chỉ mở đúng các kênh cần thiết.
// Giao diện không gọi trực tiếp window.espRaw mà dùng `api` trong src/core/api.ts (có ném ApiError).
import { contextBridge, ipcRenderer } from 'electron'
import type { ConnState, PushMessage } from './shared'

function subscribe<T>(channel: string, cb: (v: T) => void): () => void {
  const listener = (_e: Electron.IpcRendererEvent, v: T) => cb(v)
  ipcRenderer.on(channel, listener)
  return () => ipcRenderer.removeListener(channel, listener)
}

contextBridge.exposeInMainWorld('espRaw', {
  connect: (host: string, port: number) => ipcRenderer.invoke('api:connect', host, port),
  login: (user: string, password: string) => ipcRenderer.invoke('api:login', user, password),
  logout: () => ipcRenderer.invoke('api:logout'),
  request: (cmd: string, data?: object) => ipcRenderer.invoke('api:request', cmd, data),
  discover: (timeoutMs?: number) => ipcRenderer.invoke('api:discover', timeoutMs),
  session: () => ipcRenderer.invoke('api:session'),
  changePassword: (password: string, user?: string) => ipcRenderer.invoke('api:changePassword', password, user),
  createUser: (user: string, role: string, password: string) => ipcRenderer.invoke('api:createUser', user, role, password),
  printToPdf: (defaultName: string) => ipcRenderer.invoke('api:printToPdf', defaultName),
  onPush: (cb: (msg: PushMessage) => void) => subscribe('api:push', cb),
  onConnection: (cb: (state: ConnState) => void) => subscribe('api:state', cb),
})
