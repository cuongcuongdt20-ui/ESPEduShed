// Test gợi ý giờ tiết học trên form danh mục. Chạy: npm test
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { defaultEnd, nextPeriod } from '../../src/catalog/period-defaults.ts'

test('tiết mặc định dài 50 phút', () => {
  assert.equal(defaultEnd('07:00'), '07:50')
  assert.equal(defaultEnd('08:30'), '09:20')
})

test('giờ kết thúc không vượt quá 23:59', () => {
  assert.equal(defaultEnd('23:30'), '23:59')
})

test('giờ sai định dạng thì giữ nguyên', () => {
  assert.equal(defaultEnd(''), '')
})

test('chưa có tiết nào: gợi ý tiết 1 lúc 07:00–07:50', () => {
  assert.deepEqual(nextPeriod([]), { no: '1', start: '07:00', end: '07:50' })
})

test('tiết sau cách tiết trước 10 phút: 07:00–07:50 rồi 08:00–08:50', () => {
  assert.deepEqual(nextPeriod([{ no: '1', end: '07:50' }]), { no: '2', start: '08:00', end: '08:50' })
})

test('dựa vào tiết có số lớn nhất, kể cả khi danh sách chưa sắp và số tiết không liên tục', () => {
  assert.deepEqual(nextPeriod([{ no: 13, end: '18:50' }, { no: 2, end: '08:45' }]), { no: '14', start: '19:00', end: '19:50' })
})
