// Test ghép khung TCP phía client (TV1). Chạy: npm test  (cần Node.js >= 22.6)
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { FrameDecoder, MAX_PAYLOAD, encodeFrame } from '../frame.ts'

test('một khung nguyên vẹn', () => {
  const d = new FrameDecoder()
  assert.deepEqual(d.push(encodeFrame({ id: 1, ok: true })), [{ id: 1, ok: true }])
})

test('khung bị cắt từng byte, kể cả 4 byte độ dài', () => {
  const d = new FrameDecoder()
  const f = encodeFrame({ push: 'changed', entity: 'schedule', id: 5 })
  const got: unknown[] = []
  for (let i = 0; i < f.length; i++) got.push(...d.push(f.subarray(i, i + 1)))
  assert.deepEqual(got, [{ push: 'changed', entity: 'schedule', id: 5 }])
})

test('nhiều khung dính trong một lần nhận, khung cuối bị cắt', () => {
  const d = new FrameDecoder()
  const all = Buffer.concat([encodeFrame({ id: 1 }), encodeFrame({ id: 2 }), encodeFrame({ id: 3, text: 'Lập trình mạng' })])
  const cut = all.length - 5
  assert.deepEqual(d.push(all.subarray(0, cut)), [{ id: 1 }, { id: 2 }])
  assert.deepEqual(d.push(all.subarray(cut)), [{ id: 3, text: 'Lập trình mạng' }])
})

test('độ dài khung sai thì báo lỗi', () => {
  const head = Buffer.alloc(4)
  head.writeUInt32BE(MAX_PAYLOAD + 1, 0)
  assert.throws(() => new FrameDecoder().push(head))
  assert.throws(() => new FrameDecoder().push(Buffer.alloc(4)))
})

test('không mã hoá khung quá 8 KB', () => {
  assert.throws(() => encodeFrame({ s: 'x'.repeat(MAX_PAYLOAD) }))
})
