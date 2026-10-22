// Biểu mẫu lịch dạy định kỳ (TV3).
import { get } from "../core/api.js";
import { h } from "../core/ui.js";

export async function scheduleForm(el, id) {
  const schools = await get("/api/schools");
  // TODO(TV3):
  //  - các trường: môn (hoặc chọn lớp có sẵn), mã lớp, tín chỉ, trường, cơ sở, phòng, sĩ số, thứ,
  //    kiểu giờ (theo tiết / giờ trực tiếp), ngày bắt đầu, ngày kết thúc, lặp hàng tuần / một lần, ghi chú
  //  - chọn trường thì hiện bảng tiết của trường đó, chọn tiết thì hiện giờ quy đổi ngay
  //  - lưu: POST /api/schedules hoặc PUT /api/schedules/{id}
  //  - server trả 409 (CONFLICT): hiện danh sách err.data.conflicts, nút "Vẫn lưu" gửi lại với force: true
  el.append(
    h("div", { class: "card" },
      h("h2", {}, id ? `Sửa lịch dạy #${id}` : "Thêm lịch dạy"),
      h("label", {}, "Trường ", h("select", {}, schools.map((s) => h("option", { value: s.id }, s.name)))),
      h("p", { class: "sch-hint" }, "Chưa làm"))
  );
}
