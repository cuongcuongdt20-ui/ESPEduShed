// Lịch tuần dạng thời khoá biểu (TV4).
import { get } from "../core/api.js";
import { h, isoDate } from "../core/ui.js";
import { openScheduleEditor } from "../schedule/editor.js";
import { layoutDay } from "./layout.js";

const DAY_NAMES = ["Thứ 2", "Thứ 3", "Thứ 4", "Thứ 5", "Thứ 6", "Thứ 7", "CN"];
const START_HOUR = 6;
const END_HOUR = 22;

const toMin = (hm) => Number(hm.slice(0, 2)) * 60 + Number(hm.slice(3, 5));

function addDays(date, days) {
  const d = new Date(date + "T00:00:00");
  d.setDate(d.getDate() + days);
  return isoDate(d);
}

const shiftWeek = (date, weeks) => addDays(date, 7 * weeks);

export async function weekView(el, date) {
  const anchor = date || isoDate(new Date());      // gửi ngày của client: ESP có thể chưa đồng bộ giờ
  const data = await get(`/api/calendar/week?date=${anchor}`);

  const nav = h("div", { class: "cal-nav" },
    h("a", { class: "btn", href: `#/week?date=${shiftWeek(anchor, -1)}` }, "‹ Tuần trước"),
    h("a", { class: "btn", href: "#/week" }, "Tuần này"),
    h("a", { class: "btn", href: `#/week?date=${shiftWeek(anchor, 1)}` }, "Tuần sau ›"),
    h("span", { class: "cal-range" }, `${data.from} → ${data.to}`));

  // TODO(TV4): trục giờ tự mở rộng khi có buổi ngoài 06:00–22:00, chú giải màu, lọc theo trường / loại,
  //            buổi huỷ / nghỉ lễ hiện mờ gạch ngang, viền cảnh báo khi trùng, chuyển sang lịch ngày trên điện thoại
  const minutes = (END_HOUR - START_HOUR) * 60;
  const grid = h("div", { class: "cal-week" });
  grid.append(h("div", { class: "cal-head" }));
  DAY_NAMES.forEach((n) => grid.append(h("div", { class: "cal-head" }, n)));

  const hours = h("div", { class: "cal-hours" });
  for (let hr = START_HOUR; hr < END_HOUR; hr++) hours.append(h("div", { class: "cal-hour" }, `${hr}:00`));
  grid.append(hours);

  for (let i = 0; i < 7; i++) {
    const col = h("div", { class: "cal-col" });
    const day = addDays(data.from, i);
    const items = data.items.filter((it) => it.date === day);
    for (const { item, lane, lanes } of layoutDay(items)) {
      const top = ((toMin(item.start) - START_HOUR * 60) / minutes) * 100;
      const height = ((toMin(item.end) - toMin(item.start)) / minutes) * 100;
      col.append(h("div", {
        class: "cal-block",
        style: `top:${top}%;height:${height}%;left:${(lane / lanes) * 100}%;width:${100 / lanes}%;background:${item.color}`,
        onclick: () => openScheduleEditor(item.sourceId, item.kind, item.origDate),
      }, h("b", {}, item.title), h("div", {}, `${item.schoolShort} · ${item.classCode}`),
        h("div", {}, `${item.room} · ${item.start}–${item.end}`)));
    }
    grid.append(col);
  }
  el.append(nav, grid);

  // Máy khác sửa lịch: tải lại tuần đang xem (bỏ qua nếu người dùng đã chuyển trang)
  window.addEventListener("ld:changed", () => {
    if (!grid.isConnected) return;
    el.replaceChildren();
    weekView(el, date);
  }, { once: true });
}
