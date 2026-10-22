// Router theo hash (TV1). Mỗi module tự khai báo route trong <module>/index.js;
// thêm trang mới KHÔNG cần sửa file này, chỉ sửa index.js của module mình.
import { routes as coreRoutes } from "./pages.js";
import { routes as catalogRoutes } from "../catalog/index.js";
import { routes as scheduleRoutes } from "../schedule/index.js";
import { routes as calendarRoutes } from "../calendar/index.js";
import { connectWs } from "./ws.js";

const DEFAULT_ROUTE = "#/week";
const all = [...calendarRoutes, ...scheduleRoutes, ...catalogRoutes, ...coreRoutes];

function buildMenu() {
  const menu = document.getElementById("menu");
  menu.replaceChildren(
    ...all.filter((r) => r.menu).map((r) => {
      const a = document.createElement("a");
      a.href = r.path;
      a.textContent = r.title;
      return a;
    })
  );
}

// "#/schools/3?x=1" → route "#/schools/:id", params {id: "3"}, query {x: "1"}
function match(hash) {
  const [path, qs] = hash.split("?");
  const query = Object.fromEntries(new URLSearchParams(qs || ""));
  for (const r of all) {
    const keys = [];
    const re = new RegExp("^" + r.path.replace(/:(\w+)/g, (_, k) => (keys.push(k), "([^/]+)")) + "$");
    const m = path.match(re);
    if (m) return { route: r, params: Object.fromEntries(keys.map((k, i) => [k, decodeURIComponent(m[i + 1])])), query };
  }
  return null;
}

async function render() {
  const hash = location.hash || DEFAULT_ROUTE;
  const found = match(hash);
  const view = document.getElementById("view");
  if (!found) {
    location.hash = DEFAULT_ROUTE;
    return;
  }
  document.querySelectorAll("#menu a").forEach((a) => a.classList.toggle("active", hash.startsWith(a.getAttribute("href"))));
  view.replaceChildren();
  try {
    await found.route.render(view, found.params, found.query);
  } catch (e) {
    view.textContent = "Lỗi: " + e.message;
  }
}

buildMenu();
window.addEventListener("hashchange", render);
connectWs();
render();
