// Tiện ích giao diện dùng chung (TV1).

/** Tạo phần tử: h("button", {class: "btn", onclick: f}, "Lưu") */
export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
    else if (k === "class") el.className = v;
    else if (v !== false && v != null) el.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children.flat()) if (c != null && c !== false) el.append(c instanceof Node ? c : String(c));
  return el;
}

let toastTimer;
/** Thông báo ngắn ở góc màn hình; type: "ok" | "error" */
export function toast(message, type = "ok") {
  const el = document.getElementById("toast");
  el.textContent = message;
  el.className = "toast " + type;
  el.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => (el.hidden = true), 3000);
}

/** Ngày dạng "YYYY-MM-DD" theo giờ máy client */
export function isoDate(d) {
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`;
}
