// Điểm nối giữa màn hình lịch (TV4) và phần sửa lịch (TV3). Gọi được từ bất kỳ đâu (không cần là component):
// hàm ở đây chỉ phát yêu cầu, ScheduleDialogHost (dialogs.tsx, gắn sẵn trong App.tsx) mở hộp thoại tương ứng.

export type EditorRequest =
  | { mode: 'series'; id?: number; prefill?: Record<string, unknown> }       // lịch dạy: thêm mới hoặc sửa cả chuỗi
  | { mode: 'task'; id?: number; prefill?: Record<string, unknown> }         // công việc
  | { mode: 'occurrence'; sourceId: number; origDate: string }               // một buổi: huỷ, đổi phòng, đổi giờ, dời, bù

type Listener = (req: EditorRequest) => void
const listeners = new Set<Listener>()

export function onEditorRequest(cb: Listener): () => void {
  listeners.add(cb)
  return () => {
    listeners.delete(cb)
  }
}

export function openEditor(req: EditorRequest): void {
  listeners.forEach((cb) => cb(req))
}

/** kind: 1 = lịch dạy, 2 = công việc, 3 = buổi dạy bù; origDate: ngày gốc của buổi "YYYY-MM-DD" */
export function openScheduleEditor(sourceId: number, kind: number, origDate: string): void {
  if (kind === 2) openEditor({ mode: 'task', id: sourceId })
  else openEditor({ mode: 'occurrence', sourceId, origDate })
}

/** Kéo vùng trống trên lịch: date "YYYY-MM-DD", start / end "HH:MM". Giảng viên tạo lịch dạy, sinh viên tạo công việc. */
export function openNewSchedule(date: string, start: string, end: string): void {
  openEditor({ mode: 'series', prefill: { date, start, end } })
}
