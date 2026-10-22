// Route của TV1 trong khung app
import { Activity, KeyRound, Users } from 'lucide-react'
import type { ModuleRoute } from './routes'
import { ChangePasswordPage } from './pages/ChangePasswordPage'
import { StatusPage } from './pages/StatusPage'
import { UsersPage } from './pages/UsersPage'

export const coreRoutes: ModuleRoute[] = [
  { path: '/users', title: 'Tài khoản', element: <UsersPage />, icon: Users, menu: true, minRole: 'admin', order: 90 },
  { path: '/status', title: 'Trạng thái server', element: <StatusPage />, icon: Activity, menu: true, order: 91 },
  { path: '/change-password', title: 'Đổi mật khẩu', element: <ChangePasswordPage />, icon: KeyRound, menu: true, order: 92 },
]
