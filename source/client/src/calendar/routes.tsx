// Route của TV4. BẢN GIẢ do TV1 tạo: trang lịch tuần là trang chủ ("/"); hiện đọc calendar.week dạng danh sách
// để minh hoạ cách gọi api và nhận tin đẩy.
// TODO(TV4): CalendarPage bằng FullCalendar, EventBlock, ConflictPanel, FilterBar, Legend, export.ts (README mục B4)
import { CalendarDays } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { useRequest } from '../core/api'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'

interface Item {
  sourceId: number
  date: string
  start: string
  end: string
  title: string
  room: string
  school: string
  color: string
}

function today(): string {
  const d = new Date()
  const p = (n: number) => String(n).padStart(2, '0')
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`
}

function CalendarPage() {
  const date = today()                      // gửi ngày của máy: ESP có thể chưa đồng bộ giờ
  const { data, error } = useRequest<{ from: string; to: string; items: Item[] }>(
    'calendar.week',
    { date },
    [date],
    ['schedule', 'task', 'exception', 'school'],
  )
  return (
    <Card className="max-w-3xl">
      <CardHeader>
        <CardTitle>
          Lịch tuần {data ? `${data.from} → ${data.to}` : ''}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-2 text-sm">
        {error && <p className="text-destructive">{error}</p>}
        {data?.items.length === 0 && <p className="text-muted-foreground">Tuần này chưa có lịch.</p>}
        {data?.items.map((it, i) => (
          <div key={i} className="flex items-center gap-3 rounded-md border p-2" style={{ borderLeft: `4px solid ${it.color}` }}>
            <span className="w-24 text-muted-foreground">{it.date}</span>
            <span className="w-28">
              {it.start}–{it.end}
            </span>
            <span className="flex-1 font-medium">{it.title}</span>
            <span className="text-muted-foreground">
              {it.school} · {it.room}
            </span>
          </div>
        ))}
        <p className="text-muted-foreground">Giao diện thời khoá biểu FullCalendar: đang làm (TV4)</p>
      </CardContent>
    </Card>
  )
}

export const calendarRoutes: ModuleRoute[] = [
  { path: '/', title: 'Lịch tuần', element: <CalendarPage />, icon: CalendarDays, menu: true, order: 0 },
]
