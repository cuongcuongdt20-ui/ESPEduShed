// Biểu mẫu công việc (TV3).
import { h } from "../core/ui.js";

export async function taskForm(el, id) {
  // TODO(TV3): loại (họp bộ môn, coi thi, chấm thi, hướng dẫn, công tác, cá nhân), tên, nội dung,
  //            ngày, giờ bắt đầu, giờ kết thúc, lặp hàng tuần tới ngày, địa điểm / cơ sở, trạng thái, ghi chú.
  //            Lưu: POST /api/tasks hoặc PUT /api/tasks/{id}; xử lý 409 như lịch dạy.
  el.append(h("div", { class: "card" }, h("h2", {}, id ? `Sửa công việc #${id}` : "Thêm công việc"),
    h("p", { class: "sch-hint" }, "Chưa làm")));
}
