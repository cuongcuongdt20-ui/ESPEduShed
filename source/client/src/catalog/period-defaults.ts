// Giá trị gợi ý khi thêm tiết (TV2): mỗi tiết 50 phút, nghỉ 10 phút giữa hai tiết (07:00–07:50, 08:00–08:50).
// Chỉ là mặc định trên form, người dùng vẫn sửa được; server không áp đặt độ dài tiết.
export const PERIOD_MINUTES = 50
export const BREAK_MINUTES = 10
const LAST_MINUTE = 23 * 60 + 59

function toMinutes(hm: string): number | null {
  const m = /^(\d{1,2}):(\d{2})$/.exec(hm)
  return m ? Number(m[1]) * 60 + Number(m[2]) : null
}

function toHm(minutes: number): string {
  const t = Math.min(minutes, LAST_MINUTE)
  return `${String(Math.floor(t / 60)).padStart(2, '0')}:${String(t % 60).padStart(2, '0')}`
}

// Giờ kết thúc mặc định của tiết bắt đầu lúc `start`; start sai định dạng thì trả nguyên
export function defaultEnd(start: string): string {
  const s = toMinutes(start)
  return s === null ? start : toHm(s + PERIOD_MINUTES)
}

// Tiết gợi ý tiếp theo: số sau tiết lớn nhất, bắt đầu sau giờ kết thúc của tiết đó 10 phút
export function nextPeriod(periods: { no: unknown; end: unknown }[]): { no: string; start: string; end: string } {
  let last: { no: unknown; end: unknown } | null = null
  for (const p of periods) if (!last || Number(p.no) > Number(last.no)) last = p
  const lastEnd = last ? toMinutes(String(last.end)) : null
  const start = lastEnd === null ? '07:00' : toHm(lastEnd + BREAK_MINUTES)
  return { no: String(last ? Number(last.no) + 1 : 1), start, end: defaultEnd(start) }
}
