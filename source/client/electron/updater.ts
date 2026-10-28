// Tự cập nhật (TV1): khi mở app, hỏi GitHub Releases của repo xem có bản mới không (cấu hình publish trong
// electron-builder.yml). Có thì tải ngầm, tải xong hỏi người dùng khởi động lại để cài. Chỉ chạy ở bản đã đóng gói.
import { app, BrowserWindow, dialog } from 'electron'
import { autoUpdater } from 'electron-updater'

const CHECK_EVERY_MS = 4 * 60 * 60 * 1000      // app mở lâu (cả buổi dạy) thì vẫn nhận bản mới

export function startAutoUpdate(getWindow: () => BrowserWindow | null): void {
  if (!app.isPackaged) return                  // npm run dev: không có app-update.yml, bỏ qua

  autoUpdater.autoDownload = true
  autoUpdater.autoInstallOnAppQuit = true       // người dùng chọn "Để sau" thì cài khi tắt app
  autoUpdater.on('error', (e) => console.warn('[update]', e?.message ?? e))   // mất mạng Internet: bỏ qua, lần sau thử lại
  autoUpdater.on('update-downloaded', async (info) => {
    const win = getWindow()
    const opts = {
      type: 'info' as const,
      title: 'Có bản cập nhật',
      message: `Đã tải ESPEduShed ${info.version}. Khởi động lại để cài ngay?`,
      buttons: ['Khởi động lại', 'Để sau'],
      defaultId: 0,
      cancelId: 1,
    }
    const r = win ? await dialog.showMessageBox(win, opts) : await dialog.showMessageBox(opts)
    if (r.response === 0) autoUpdater.quitAndInstall()
  })

  const check = () => autoUpdater.checkForUpdates().catch(() => undefined)
  check()
  setInterval(check, CHECK_EVERY_MS)
}
