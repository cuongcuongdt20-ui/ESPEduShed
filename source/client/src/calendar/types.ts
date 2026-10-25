// Kiểu dữ liệu của lệnh calendar.* và conflict.* (docs/protocol.md mục 5)
export interface Item {
  sourceId: number
  kind: number            // 1 = lịch dạy, 2 = công việc, 3 = dạy bù
  status: number          // 0 bình thường, 1 đã đổi, 2 đã huỷ, 3 nghỉ lễ
  date: string            // "YYYY-MM-DD"
  origDate: string
  start: string           // "HH:MM"
  end: string
  title: string
  room: string
  classCode: string
  school: string
  color: string           // "#rrggbb"
  conflicts?: number[]    // chỉ số trong RangeData.conflicts, chỉ các xung đột chưa xác nhận
}

export interface Conflict {
  a: number
  b: number
  date: string
  start: string
  end: string
  reason: number          // 1 = trùng giờ, 2 = thiếu thời gian di chuyển
  acked: boolean
}

export interface RangeData {
  from: string
  to: string
  items: Item[]
  conflicts: Conflict[]
  truncated?: boolean
}

export interface Side {
  sourceId: number
  kind: number
  title: string
  start: string
  end: string
}

export interface ConflictRow {
  a: Side
  b: Side
  date: string
  start: string
  end: string
  reason: number
  acked: boolean
}

export interface CheckRow {
  with: Side
  date: string
  start: string
  end: string
  reason: number
}

export const KIND_TEACH = 1
export const KIND_TASK = 2
export const KIND_MAKEUP = 3
export const STATUS_CANCELLED = 2
export const STATUS_HOLIDAY = 3

export function pad(n: number): string {
  return String(n).padStart(2, '0')
}

export function ymd(d: Date): string {
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`
}

export function hm(d: Date): string {
  return `${pad(d.getHours())}:${pad(d.getMinutes())}`
}

export function addDays(d: Date, n: number): Date {
  const r = new Date(d)
  r.setDate(r.getDate() + n)
  return r
}

export function reasonText(reason: number): string {
  return reason === 2 ? 'Không đủ thời gian di chuyển' : 'Trùng giờ'
}
