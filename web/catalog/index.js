// Trang của TV2: trường, bảng tiết, ngày nghỉ, môn học, lớp, thời gian di chuyển.
import { get } from "../core/api.js";
import { h } from "../core/ui.js";
import { periodEditor } from "./periods.js";

async function schoolsPage(el) {
  const schools = await get("/api/schools");
  // TODO(TV2): thêm / sửa / xoá trường, cơ sở, chọn màu
  el.append(
    h("div", { class: "card" },
      h("h2", {}, "Trường"),
      h("ul", { class: "cat-list" },
        schools.map((s) => h("li", {},
          h("span", { class: "cat-dot", style: `background:${s.color}` }), ` ${s.name} (${s.shortName}) `,
          h("a", { href: `#/schools/${s.id}/periods` }, "Bảng tiết")))))
  );
}

async function periodsPage(el, params) {
  await periodEditor(el, Number(params.id));
}

async function holidaysPage(el) {
  // TODO(TV2): danh sách ngày nghỉ, thêm khoảng ngày nghỉ cho một trường hoặc mọi trường
  el.append(h("div", { class: "card" }, h("h2", {}, "Ngày nghỉ"), h("p", {}, "Chưa làm")));
}

async function subjectsPage(el) {
  // TODO(TV2): môn học (mã học phần, tên, tín chỉ) và lớp / nhóm học phần (mã lớp, môn, trường, sĩ số)
  el.append(h("div", { class: "card" }, h("h2", {}, "Môn học và lớp"), h("p", {}, "Chưa làm")));
}

async function travelPage(el) {
  // TODO(TV2): ma trận thời gian di chuyển giữa các cơ sở
  el.append(h("div", { class: "card" }, h("h2", {}, "Thời gian di chuyển"), h("p", {}, "Chưa làm")));
}

export const routes = [
  { path: "#/schools", title: "Trường", menu: true, render: schoolsPage },
  { path: "#/schools/:id/periods", title: "Bảng tiết", render: periodsPage },
  { path: "#/holidays", title: "Ngày nghỉ", menu: true, render: holidaysPage },
  { path: "#/subjects", title: "Môn & lớp", menu: true, render: subjectsPage },
  { path: "#/travel", title: "Di chuyển", render: travelPage },
];
