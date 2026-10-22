// Tính toán challenge–response phía client (TV1). Không phụ thuộc Electron, test được bằng node --test.
//   key   = PBKDF2-HMAC-SHA256(mật khẩu, salt, iter, 32 byte)
//   proof = HMAC-SHA256(key, nonce)            (nonce là chuỗi hex, băm nguyên chuỗi ASCII)
import crypto from 'node:crypto'

export const DEFAULT_ITER = 10000

export function deriveKey(password: string, saltHex: string, iter: number): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    crypto.pbkdf2(password, Buffer.from(saltHex, 'hex'), iter, 32, 'sha256', (err, key) => (err ? reject(err) : resolve(key)))
  })
}

export function makeProof(key: Buffer, nonce: string): string {
  return crypto.createHmac('sha256', key).update(nonce, 'ascii').digest('hex')
}

// {salt, iter, key} gửi khi tạo tài khoản hoặc đổi mật khẩu: server không bao giờ nhận mật khẩu
export async function newSecret(password: string, iter = DEFAULT_ITER): Promise<{ salt: string; iter: number; key: string }> {
  const salt = crypto.randomBytes(16).toString('hex')
  const key = await deriveKey(password, salt, iter)
  return { salt, iter, key: key.toString('hex') }
}
