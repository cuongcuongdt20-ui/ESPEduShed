// Điểm nối giữa màn hình lịch (TV4) và phần sửa lịch (TV3). BẢN GIẢ do TV1 tạo.
// TODO(TV3): mở hộp thoại sửa thật (sửa buổi này / sửa cả chuỗi), tạo lịch mới từ vùng kéo trên lịch.

/** kind: 1 = lịch dạy, 2 = công việc, 3 = buổi dạy bù; origDate: ngày gốc của buổi "YYYY-MM-DD" */
export function openScheduleEditor(sourceId: number, kind: number, origDate: string): void {
  console.info('openScheduleEditor chưa làm (TV3)', { sourceId, kind, origDate })
}

/** date "YYYY-MM-DD", start / end "HH:MM" */
export function openNewSchedule(date: string, start: string, end: string): void {
  console.info('openNewSchedule chưa làm (TV3)', { date, start, end })
}
