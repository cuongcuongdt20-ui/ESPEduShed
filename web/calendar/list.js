// Danh sách buổi dạy và công việc sắp tới (TV4).
import { h } from "../core/ui.js";

export async function listView(el) {
  // TODO(TV4): GET /api/calendar/range 14 ngày tới, nhóm theo ngày, đánh dấu xung đột
  el.append(h("div", { class: "card" }, h("h2", {}, "Sắp tới"), h("p", {}, "Chưa làm")));
}
