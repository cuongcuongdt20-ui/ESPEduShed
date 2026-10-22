// Trang của TV3: danh sách lịch dạy, công việc, biểu mẫu nhập.
import { h } from "../core/ui.js";
import { scheduleForm } from "./schedule-form.js";
import { taskForm } from "./task-form.js";

async function schedulesPage(el) {
  // TODO(TV3): danh sách lịch dạy (GET /api/schedules), nút thêm / sửa / xoá
  el.append(h("div", { class: "card" }, h("h2", {}, "Lịch giảng dạy"),
    h("a", { class: "btn primary", href: "#/schedules/new" }, "Thêm lịch dạy")));
}

async function tasksPage(el) {
  // TODO(TV3): danh sách công việc (GET /api/tasks), lọc theo trạng thái
  el.append(h("div", { class: "card" }, h("h2", {}, "Công việc"),
    h("a", { class: "btn primary", href: "#/tasks/new" }, "Thêm công việc")));
}

export const routes = [
  { path: "#/schedules", title: "Lịch dạy", menu: true, render: schedulesPage },
  { path: "#/schedules/new", title: "Thêm lịch dạy", render: (el) => scheduleForm(el, null) },
  { path: "#/schedules/:id", title: "Sửa lịch dạy", render: (el, p) => scheduleForm(el, Number(p.id)) },
  { path: "#/tasks", title: "Công việc", menu: true, render: tasksPage },
  { path: "#/tasks/new", title: "Thêm công việc", render: (el) => taskForm(el, null) },
  { path: "#/tasks/:id", title: "Sửa công việc", render: (el, p) => taskForm(el, Number(p.id)) },
];
