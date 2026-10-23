// Các thao tác ghi do màn hình lịch gọi sang module của TV3. Gom ở một chỗ để chỉnh khi TV3 chốt tên trường.
import { api } from '../core/api'
import type { Item } from './types'

/**
 * Dời một buổi sang ngày / giờ khác bằng exception.set (TV3).
 * GIẢ ĐỊNH về `data` (chờ TV3 ghi vào docs/protocol.md mục 4): {sourceId, origDate, type: 'move', date, start, end, force}.
 * `force: true` vì người dùng đã xem cảnh báo trùng (conflict.check) và xác nhận.
 */
export function moveOccurrence(it: Item, date: string, start: string, end: string): Promise<unknown> {
  return api.request('exception.set', { sourceId: it.sourceId, origDate: it.origDate, type: 'move', date, start, end, force: true })
}
