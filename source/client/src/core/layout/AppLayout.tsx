// Khung app (TV1): thanh bên với menu của các module, thanh trạng thái kết nối, chế độ sáng / tối.
import { useEffect, useState, type ReactNode } from 'react'
import { NavLink, useNavigate } from 'react-router-dom'
import { LogOut, Moon, Sun, Wifi, WifiOff } from 'lucide-react'
import { api } from '../api'
import type { ModuleRoute } from '../routes'
import { useSession } from '../session'
import { Badge } from '../components/ui/card'
import { Button } from '../components/ui/button'
import { cn } from '../lib/utils'

function useTheme(): [boolean, () => void] {
  const [dark, setDark] = useState(() => {
    try {
      return localStorage.getItem('theme') === 'dark'
    } catch {
      return false
    }
  })
  useEffect(() => {
    document.documentElement.classList.toggle('dark', dark)
    try {
      localStorage.setItem('theme', dark ? 'dark' : 'light')
    } catch {
      // không lưu được thì chỉ áp dụng trong phiên này
    }
  }, [dark])
  return [dark, () => setDark((d) => !d)]
}

export function AppLayout({ routes, children }: { routes: ModuleRoute[]; children: ReactNode }) {
  const s = useSession()
  const navigate = useNavigate()
  const [dark, toggleTheme] = useTheme()
  const menu = routes.filter((r) => r.menu)

  async function logout() {
    await api.logout()
    await s.refresh()
    navigate('/login')
  }

  return (
    <div className="flex h-screen">
      <aside className="flex w-60 flex-col border-r bg-card">
        <div className="px-5 py-4">
          <div className="text-lg font-bold">ESPEduShed</div>
          <div className="text-xs text-muted-foreground">Lịch giảng dạy trên ESP32-S3</div>
        </div>
        <nav className="flex flex-1 flex-col gap-1 overflow-y-auto px-3">
          {menu.map((r) => (
            <NavLink
              key={r.path}
              to={r.path}
              className={({ isActive }) =>
                cn(
                  'flex items-center gap-3 rounded-md px-3 py-2 text-sm text-muted-foreground hover:bg-accent hover:text-accent-foreground',
                  isActive && 'bg-accent font-medium text-accent-foreground',
                )
              }
            >
              {r.icon && <r.icon className="h-4 w-4" />}
              {r.title}
            </NavLink>
          ))}
        </nav>
        <div className="border-t p-3 text-sm">
          <div className="flex gap-1">
            <Button variant="ghost" size="icon" title="Đổi giao diện sáng / tối" onClick={toggleTheme}>
              {dark ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
            </Button>
            <Button variant="ghost" size="sm" className="flex-1 justify-start" onClick={logout}>
              <LogOut className="h-4 w-4" /> Đăng xuất
            </Button>
          </div>
        </div>
      </aside>
      <div className="flex flex-1 flex-col overflow-hidden">
        <header className="flex h-11 items-center justify-end gap-3 border-b px-5 text-sm">
          <span className="text-muted-foreground">
            {s.host}:{s.port}
          </span>
          {s.state === 'online' ? (
            <Badge tone="success">
              <Wifi className="mr-1 h-3 w-3" /> Đã kết nối
            </Badge>
          ) : s.state === 'connecting' ? (
            <Badge tone="warning">Đang nối lại…</Badge>
          ) : (
            <Badge tone="danger">
              <WifiOff className="mr-1 h-3 w-3" /> Mất kết nối
            </Badge>
          )}
        </header>
        <main className="flex-1 overflow-y-auto p-6">{children}</main>
      </div>
    </div>
  )
}
