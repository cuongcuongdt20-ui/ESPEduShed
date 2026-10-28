// Route của TV1 trong khung app
import { Activity, Globe, KeyRound } from 'lucide-react'
import type { ModuleRoute } from './routes'
import { ChangePasswordPage } from './pages/ChangePasswordPage'
import { RemotePage } from './pages/RemotePage'
import { StatusPage } from './pages/StatusPage'

export const coreRoutes: ModuleRoute[] = [
  { path: '/remote', title: 'Truy cập từ xa', element: <RemotePage />, icon: Globe, menu: true, order: 90 },
  { path: '/status', title: 'Trạng thái server', element: <StatusPage />, icon: Activity, menu: true, order: 91 },
  { path: '/change-password', title: 'Đổi mật khẩu', element: <ChangePasswordPage />, icon: KeyRound, menu: true, order: 92 },
]
