// Test challenge–response phía client (TV1): khớp với vector chuẩn của PBKDF2-HMAC-SHA256 và HMAC-SHA256.
import { test } from 'node:test'
import assert from 'node:assert/strict'
import crypto from 'node:crypto'
import { deriveKey, makeProof, newSecret } from '../crypto.ts'

test('PBKDF2-HMAC-SHA256 khớp vector chuẩn (RFC 7914 mục 11, c = 1)', async () => {
  // P = "passwd", S = "salt", c = 1, dkLen = 32: 55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc
  const key = await deriveKey('passwd', Buffer.from('salt').toString('hex'), 1)
  assert.equal(key.toString('hex'), '55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc')
})

test('proof = HMAC-SHA256(key, nonce dạng chuỗi ASCII)', () => {
  const key = Buffer.alloc(32, 7)
  const nonce = '00112233445566778899aabbccddeeff'
  assert.equal(makeProof(key, nonce), crypto.createHmac('sha256', key).update(nonce).digest('hex'))
})

test('newSecret sinh salt 16 byte và key 32 byte, không chứa mật khẩu', async () => {
  const s = await newSecret('MatKhau123', 1000)
  assert.match(s.salt, /^[0-9a-f]{32}$/)
  assert.match(s.key, /^[0-9a-f]{64}$/)
  assert.equal(s.iter, 1000)
  assert.equal((await deriveKey('MatKhau123', s.salt, 1000)).toString('hex'), s.key)
})
