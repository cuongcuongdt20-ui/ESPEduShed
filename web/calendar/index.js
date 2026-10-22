// Trang của TV4: lịch tuần (trang mặc định), lịch ngày, danh sách sắp tới.
import { weekView } from "./week.js";
import { dayView } from "./day.js";
import { listView } from "./list.js";

export const routes = [
  { path: "#/week", title: "Lịch tuần", menu: true, render: (el, p, q) => weekView(el, q.date) },
  { path: "#/day", title: "Lịch ngày", menu: true, render: (el, p, q) => dayView(el, q.date) },
  { path: "#/upcoming", title: "Sắp tới", menu: true, render: (el) => listView(el) },
];
