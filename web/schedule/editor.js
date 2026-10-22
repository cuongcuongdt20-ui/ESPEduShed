// Điểm nối DUY NHẤT giữa trang lịch (TV4) và phần sửa lịch (TV3).
// TV4 gọi khi người dùng bấm vào một khối trên lịch tuần.
import { exceptionDialog } from "./exception-dialog.js";

/**
 * @param {number} sourceId id lịch dạy hoặc công việc
 * @param {number} kind     1 = lịch dạy, 2 = công việc, 3 = buổi dạy bù
 * @param {string} origDate ngày gốc của buổi "YYYY-MM-DD"
 */
export function openScheduleEditor(sourceId, kind, origDate) {
  if (kind === 2) {
    location.hash = `#/tasks/${sourceId}`;
    return;
  }
  // TODO(TV3): hỏi "Sửa buổi này" (mở hộp thoại ngoại lệ) hay "Sửa cả chuỗi" (mở biểu mẫu lịch)
  exceptionDialog(sourceId, origDate);
}
