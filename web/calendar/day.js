// Lịch theo ngày (TV4), mặc định trên điện thoại.
import { h } from "../core/ui.js";

export async function dayView(el, date) {
  // TODO(TV4): một cột giờ của ngày đã chọn, dùng lại layoutDay; điều hướng ngày trước / ngày sau
  el.append(h("div", { class: "card" }, h("h2", {}, `Lịch ngày ${date || "hôm nay"}`), h("p", {}, "Chưa làm")));
}
