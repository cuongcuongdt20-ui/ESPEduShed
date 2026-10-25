// Các thao tác ghi do màn hình lịch gọi sang module của TV3. Gom ở một chỗ để chỉnh khi TV3 đổi tên trường.
import { api } from '../core/api'
import type { Item } from './types'

const EXC_MOVE = 4                       // exc_type_t của TV3 (docs/protocol.md mục 4)

/**
 * Dời một buổi sang ngày / giờ khác bằng exception.set (TV3):
 * {series_id, orig_date, type: 4, new_date, new_start, new_end, force}.
 * force: true vì người dùng đã xem cảnh báo trùng (conflict.check) và xác nhận trước khi gọi.
 */
export function moveOccurrence(it: Item, date: string, start: string, end: string): Promise<unknown> {
  return api.request('exception.set', {
    series_id: it.sourceId, orig_date: it.origDate, type: EXC_MOVE, new_date: date, new_start: start, new_end: end,
    force: true,
  })
}
