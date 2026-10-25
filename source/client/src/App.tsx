// Gốc giao diện (TV1): chọn màn hình theo trạng thái phiên, gắn route của các module.
//   chưa nối server → /connect;  chưa đăng nhập → /login;  bị bắt đổi mật khẩu → /change-password
import { HashRouter, Navigate, Route, Routes, useLocation } from 'react-router-dom'
import { ToastProvider } from './core/components/toast'
import { coreRoutes } from './core/core-routes'
import { AppLayout } from './core/layout/AppLayout'
import { ConnectPage } from './core/pages/ConnectPage'
import { LoginPage } from './core/pages/LoginPage'
import type { ModuleRoute } from './core/routes'
import { SessionProvider, useSession } from './core/session'
import { calendarRoutes } from './calendar/routes'
import { catalogRoutes } from './catalog/routes'
import { scheduleRoutes } from './schedule/routes'
import { ScheduleDialogHost } from './schedule/dialogs'

const allRoutes: ModuleRoute[] = [...calendarRoutes, ...scheduleRoutes, ...catalogRoutes, ...coreRoutes].sort(
  (a, b) => (a.order ?? 50) - (b.order ?? 50),
)

function Shell() {
  const s = useSession()
  const loc = useLocation()
  if (!s.ready) return null
  if (!s.host) return loc.pathname === '/connect' ? <ConnectPage /> : <Navigate to="/connect" />
  if (loc.pathname === '/connect') return <ConnectPage />
  if (!s.user) return loc.pathname === '/login' ? <LoginPage /> : <Navigate to="/login" />
  if (s.mustChangePassword && loc.pathname !== '/change-password') return <Navigate to="/change-password" />

  return (
    <AppLayout routes={allRoutes}>
      <Routes>
        {allRoutes
          .filter((r) => !r.minRole || s.hasRole(r.minRole))
          .map((r) => (
            <Route key={r.path} path={r.path} element={r.element} />
          ))}
        <Route path="*" element={<Navigate to="/" />} />
      </Routes>
      <ScheduleDialogHost />
    </AppLayout>
  )
}

export default function App() {
  return (
    <HashRouter>
      <ToastProvider>
        <SessionProvider>
          <Shell />
        </SessionProvider>
      </ToastProvider>
    </HashRouter>
  )
}
