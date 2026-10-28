// Đăng nhập phía client (TV1).
//  - Mật khẩu chỉ dùng trong main process để tính key; không gửi lên mạng, không lưu xuống đĩa.
//  - key (tương đương mật khẩu) chỉ giữ trong RAM để tự đăng nhập lại khi ESP khởi động lại (phiên trên ESP mất).
//  - token lưu xuống đĩa đã mã hoá bằng safeStorage (DPAPI trên Windows) để mở lại app không phải đăng nhập.
import fs from 'node:fs'
import path from 'node:path'
import { app, safeStorage } from 'electron'
import { deriveKey, makeProof, newSecret } from './crypto'
import { ApiError, TcpClient } from './tcp-client'
import type { LoginResult, User } from './shared'

interface Cached {
  user: string
  key: Buffer
}

let cached: Cached | null = null
export let currentUser: User | null = null
export let mustChangePassword = false

const tokenFile = () => path.join(app.getPath('userData'), 'session.bin')

function saveToken(client: TcpClient): void {
  if (!client.token || !safeStorage.isEncryptionAvailable()) return
  const plain = JSON.stringify({ host: client.host, port: client.port, token: client.token })
  fs.writeFileSync(tokenFile(), safeStorage.encryptString(plain))
}

function clearToken(): void {
  fs.rmSync(tokenFile(), { force: true })
}

// Token đã lưu của đúng server này (nếu có). Gọi auth.me để biết nó còn hiệu lực không.
export async function restoreSession(client: TcpClient): Promise<boolean> {
  try {
    if (!safeStorage.isEncryptionAvailable() || !fs.existsSync(tokenFile())) return false
    const saved = JSON.parse(safeStorage.decryptString(fs.readFileSync(tokenFile())))
    if (saved.host !== client.host || saved.port !== client.port) return false
    client.token = saved.token
    const me = await client.request('auth.me')
    currentUser = me.user
    mustChangePassword = me.mustChangePassword
    return true
  } catch {
    client.token = null
    return false
  }
}

async function loginWithKey(client: TcpClient, user: string, keyFor: (salt: string, iter: number) => Promise<Buffer>): Promise<LoginResult> {
  const ch = await client.request('auth.challenge', { user })
  const key = await keyFor(ch.salt, ch.iter)
  const res = await client.request('auth.login', { user, nonce: ch.nonce, proof: makeProof(key, ch.nonce) })
  client.token = res.token
  cached = { user, key }
  currentUser = res.user
  mustChangePassword = res.mustChangePassword
  saveToken(client)
  return { user: res.user, mustChangePassword: res.mustChangePassword }
}

// Server chỉ có một tài khoản, tên cố định
export const USER_NAME = 'admin'

export function login(client: TcpClient, password: string): Promise<LoginResult> {
  return loginWithKey(client, USER_NAME, (salt, iter) => deriveKey(password, salt, iter))
}

// Sau khi nối lại: phiên trên ESP có thể đã mất (ESP khởi động lại). Dùng key trong RAM để đăng nhập lại.
export async function relogin(client: TcpClient): Promise<boolean> {
  if (!cached) return false
  const { user, key } = cached
  try {
    await loginWithKey(client, user, async () => key)
    return true
  } catch {
    cached = null                      // mật khẩu đã đổi ở máy khác: phải đăng nhập lại bằng tay
    return false
  }
}

export async function logout(client: TcpClient): Promise<void> {
  try {
    if (client.token) await client.request('auth.logout')
  } catch {
    // mất kết nối cũng vẫn xoá phiên phía client
  }
  client.token = null
  cached = null
  currentUser = null
  mustChangePassword = false
  clearToken()
}

// Đổi mật khẩu. Server huỷ mọi phiên sau khi đổi: đăng nhập lại bằng mật khẩu mới
export async function changePassword(client: TcpClient, newPassword: string): Promise<void> {
  if (newPassword.length < 8) throw new ApiError({ code: 'BAD_ARG', message: 'Mật khẩu tối thiểu 8 ký tự', field: 'password' })
  await client.request('auth.password', await newSecret(newPassword))
  client.token = null
  await login(client, newPassword)
}
