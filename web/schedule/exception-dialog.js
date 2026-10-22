// Hộp thoại ngoại lệ cho MỘT buổi (TV3): huỷ, đổi phòng, đổi giờ, dời ngày, thêm buổi bù.
import { h } from "../core/ui.js";

export function exceptionDialog(seriesId, origDate) {
  const dlg = h("dialog", { class: "sch-dialog" },
    h("h3", {}, `Buổi ngày ${origDate} (lịch #${seriesId})`),
    // TODO(TV3): các lựa chọn ngoại lệ, gửi POST /api/schedules/{id}/exceptions
    h("p", {}, "Chưa làm"),
    h("button", { class: "btn", onclick: () => dlg.close() }, "Đóng"));
  dlg.addEventListener("close", () => dlg.remove());
  document.body.append(dlg);
  dlg.showModal();
}
