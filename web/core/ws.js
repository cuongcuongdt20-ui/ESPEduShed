// WebSocket tới ESP32 (TV1). Nhận {"t":"changed","entity":"...","id":n} rồi phát sự kiện "ld:changed"
// trên window; trang nào cần cập nhật thì tự nghe:  window.addEventListener("ld:changed", e => ...)
let ws;

export function connectWs() {
  const dot = document.getElementById("conn");
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => dot?.classList.add("on");
  ws.onclose = () => {
    dot?.classList.remove("on");
    setTimeout(connectWs, 2000);            // ESP khởi động lại hoặc mất Wi-Fi: tự nối lại
  };
  ws.onmessage = (e) => {
    let m;
    try {
      m = JSON.parse(e.data);
    } catch {
      return;
    }
    if (m.t === "changed") window.dispatchEvent(new CustomEvent("ld:changed", { detail: m }));
  };
}
