// Route của TV4: trang lịch tuần là trang chủ ("/"). Phần giao diện nằm trong CalendarPage.tsx.
import { CalendarDays } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { CalendarPage } from './CalendarPage'

export const calendarRoutes: ModuleRoute[] = [
  { path: '/', title: 'Lịch tuần', element: <CalendarPage />, icon: CalendarDays, menu: true, order: 0 },
]
