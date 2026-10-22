// Trang của TV1: đăng nhập, cài đặt, trạng thái thiết bị.
import { get, post, setToken } from "./api.js";
import { h, toast } from "./ui.js";

async function loginPage(el) {
  const pass = h("input", { type: "password", placeholder: "Mật khẩu", autocomplete: "current-password" });
  const form = h("form", { class: "card narrow" },
    h("h2", {}, "Đăng nhập"), pass, h("button", { class: "btn primary" }, "Đăng nhập"));
  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    try {
      const r = await post("/api/auth/login", { password: pass.value });
      setToken(r.token);
      location.hash = "#/week";
    } catch (err) {
      toast(err.message, "error");
    }
  });
  el.append(form);
}

async function settingsPage(el) {
  // TODO(TV1): đổi mật khẩu, tải bản sao lưu (/api/backup), khôi phục (/api/restore), đăng xuất
  el.append(h("div", { class: "card" }, h("h2", {}, "Cài đặt"), h("p", {}, "Chưa làm")));
}

async function statusPage(el) {
  const s = await get("/api/system/status");
  // TODO(TV1): trình bày đẹp hơn, tự làm mới mỗi 5 giây
  el.append(h("div", { class: "card" }, h("h2", {}, "Trạng thái thiết bị"), h("pre", {}, JSON.stringify(s, null, 2))));
}

export const routes = [
  { path: "#/login", title: "Đăng nhập", render: loginPage },
  { path: "#/settings", title: "Cài đặt", menu: true, render: settingsPage },
  { path: "#/status", title: "Thiết bị", menu: true, render: statusPage },
];
