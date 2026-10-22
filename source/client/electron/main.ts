// Main process (TV1): tạo cửa sổ, giữ kết nối TCP tới ESP32, mở các kênh IPC cho giao diện.
// Giao diện không có quyền Node.js (contextIsolation, sandbox): mọi thao tác mạng đi qua các kênh IPC dưới đây.
import fs from 'node:fs'
import path from 'node:path'
import { app, BrowserWindow, dialog, ipcMain, type IpcMainInvokeEvent } from 'electron'
import * as auth from './auth'
import { discover } from './discovery'
import { ApiError, TcpClient } from './tcp-client'
import type { IpcResult, Role } from './shared'

const client = new TcpClient()
let win: BrowserWindow | null = null

// Bọc handler: luôn trả {ok, data | error} vì Electron chỉ giữ lại message khi ném lỗi qua IPC
function handle<A extends unknown[], T>(channel: string, fn: (...args: A) => Promise<T>): void {
  ipcMain.handle(channel, async (_e: IpcMainInvokeEvent, ...args: unknown[]): Promise<IpcResult<T>> => {
    try {
      return { ok: true, data: await fn(...(args as A)) }
    } catch (e) {
      if (e instanceof ApiError) return { ok: false, error: e.info }
      return { ok: false, error: { code: 'INTERNAL', message: e instanceof Error ? e.message : String(e), field: null } }
    }
  })
}

function badArg(message: string, field: string): ApiError {
  return new ApiError({ code: 'BAD_ARG', message, field })
}

// Token hết hiệu lực (ESP khởi động lại, hoặc hết hạn): tự đăng nhập lại bằng key trong RAM rồi gửi lại một lần
async function requestWithRelogin(cmd: string, data?: object): Promise<unknown> {
  try {
    return await client.request(cmd, data)
  } catch (e) {
    if (e instanceof ApiError && e.info.code === 'UNAUTHORIZED' && (await auth.relogin(client))) {
      return client.request(cmd, data)
    }
    throw e
  }
}

function registerIpc(): void {
  handle('api:connect', async (host: unknown, port: unknown) => {
    if (typeof host !== 'string' || !/^[A-Za-z0-9.-]{1,253}$/.test(host)) throw badArg('Địa chỉ server không hợp lệ', 'host')
    if (!Number.isInteger(port) || (port as number) < 1 || (port as number) > 65535) throw badArg('Cổng không hợp lệ', 'port')
    await client.connect(host, port as number)
    await auth.restoreSession(client)
  })
  handle('api:login', (user: unknown, password: unknown) => {
    if (typeof user !== 'string' || typeof password !== 'string') throw badArg('Thiếu tài khoản hoặc mật khẩu', 'user')
    return auth.login(client, user, password)
  })
  handle('api:logout', () => auth.logout(client))
  handle('api:request', async (cmd: unknown, data: unknown) => {
    if (typeof cmd !== 'string' || !/^[a-z]+\.[a-z_]+$/.test(cmd)) throw badArg('Tên lệnh không hợp lệ', 'cmd')
    if (cmd === 'auth.login' || cmd === 'auth.challenge') throw badArg('Dùng api.login để đăng nhập', 'cmd')
    if (data !== undefined && (data === null || typeof data !== 'object')) throw badArg('data phải là object', 'data')
    return requestWithRelogin(cmd, data as object | undefined)
  })
  handle('api:discover', (timeoutMs: unknown) => discover(typeof timeoutMs === 'number' ? timeoutMs : 1500))
  handle('api:session', async () => ({
    host: client.host,
    port: client.port,
    state: client.state,
    user: auth.currentUser,
    mustChangePassword: auth.mustChangePassword,
  }))
  handle('api:changePassword', (password: unknown, user: unknown) => {
    if (typeof password !== 'string') throw badArg('Thiếu mật khẩu mới', 'password')
    return auth.changePassword(client, password, typeof user === 'string' && user ? user : undefined)
  })
  handle('api:createUser', (user: unknown, role: unknown, password: unknown) => {
    if (typeof user !== 'string' || typeof password !== 'string') throw badArg('Thiếu tài khoản hoặc mật khẩu', 'user')
    if (role !== 'admin' && role !== 'teacher' && role !== 'student') throw badArg('Vai trò không hợp lệ', 'role')
    return auth.createUser(client, user, role as Role, password)
  })
  // In / xuất PDF trang đang xem (TV4 dùng cho thời khoá biểu tuần)
  handle('api:printToPdf', async (defaultName: unknown) => {
    if (!win) return null
    const name = typeof defaultName === 'string' ? defaultName.replace(/[\\/:*?"<>|]/g, '_') : 'lich.pdf'
    const r = await dialog.showSaveDialog(win, { defaultPath: name, filters: [{ name: 'PDF', extensions: ['pdf'] }] })
    if (r.canceled || !r.filePath) return null
    const pdf = await win.webContents.printToPDF({ landscape: true, printBackground: true })
    fs.writeFileSync(r.filePath, pdf)
    return r.filePath
  })
}

function wireClientEvents(): void {
  client.on('push', (msg) => win?.webContents.send('api:push', msg))
  client.on('state', (state) => win?.webContents.send('api:state', state))
  client.on('reconnected', async () => {
    // Nối lại xong: token còn dùng được thì thôi, mất (ESP khởi động lại) thì đăng nhập lại bằng key trong RAM
    try {
      if (client.token) await client.request('auth.me')
    } catch {
      await auth.relogin(client)
    }
    win?.webContents.send('api:state', client.state)
  })
}

function createWindow(): void {
  win = new BrowserWindow({
    width: 1280,
    height: 820,
    minWidth: 960,
    minHeight: 640,
    title: 'ESPEduShed',
    webPreferences: {
      preload: path.join(__dirname, '../preload/index.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  })
  win.setMenuBarVisibility(false)
  // Không cho giao diện mở trang lạ hay cửa sổ mới
  win.webContents.on('will-navigate', (e, url) => {
    if (!url.startsWith('file://') && !url.startsWith(process.env.ELECTRON_RENDERER_URL ?? '\0')) e.preventDefault()
  })
  win.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  if (process.env.ELECTRON_RENDERER_URL) win.loadURL(process.env.ELECTRON_RENDERER_URL)
  else win.loadFile(path.join(__dirname, '../renderer/index.html'))
  win.on('closed', () => (win = null))
}

app.whenReady().then(() => {
  registerIpc()
  wireClientEvents()
  createWindow()
})

app.on('window-all-closed', () => {
  client.disconnect()
  app.quit()
})
