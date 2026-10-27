// Route của TV3: danh sách lịch dạy định kỳ và công việc của người đang đăng nhập.
// Thêm / sửa mở hộp thoại trong dialogs.tsx (qua editor.ts), cùng hộp thoại mà trang lịch của TV4 dùng.
import { ClipboardList, Plus } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { useList } from '../core/api'
import { useSession } from '../core/session'
import { Button } from '../core/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'
import { DAY_NAMES, TASK_STATUS, TASK_TYPES } from './dialogs'
import { openEditor } from './editor'

type Row = Record<string, any>

function fmtDay(d: string) {
  return d ? `${d.slice(8, 10)}/${d.slice(5, 7)}/${d.slice(0, 4)}` : ''
}

function Table({ headers, rows, cells, onOpen, empty }: {
  headers: string[]; rows: Row[]; cells: ((r: Row) => React.ReactNode)[]; onOpen: (r: Row) => void; empty: string
}) {
  return (
    <div className="overflow-x-auto rounded-md border">
      <table className="w-full min-w-[640px] text-left text-sm">
        <thead className="bg-muted/70">
          <tr>{headers.map((h) => <th key={h} className="px-3 py-2 font-semibold">{h}</th>)}</tr>
        </thead>
        <tbody>
          {rows.map((r) => (
            <tr key={r.id} className="cursor-pointer border-t hover:bg-muted/30" onClick={() => onOpen(r)} title="Bấm để sửa">
              {cells.map((c, i) => <td key={i} className="px-3 py-2">{c(r)}</td>)}
            </tr>
          ))}
          {rows.length === 0 && (
            <tr><td colSpan={headers.length} className="px-3 py-6 text-center text-muted-foreground">{empty}</td></tr>
          )}
        </tbody>
      </table>
    </div>
  )
}

function SchedulesPage() {
  const { hasRole } = useSession()
  const teacher = hasRole('teacher')
  const series = useList<Row>('schedule.list', 'schedules', ['schedule', 'exception'])
  const tasks = useList<Row>('task.list', 'tasks', ['task'])
  const schools = useList<Row>('school.list', 'schools', ['school'])
  const schoolName = (id: number) => schools.data?.find((s) => s.id === id)?.shortName ?? id

  return (
    <div className="space-y-5">
      {teacher && (
        <Card>
          <CardHeader className="flex flex-row items-center justify-between">
            <CardTitle className="text-base">Lịch dạy định kỳ</CardTitle>
            <Button size="sm" onClick={() => openEditor({ mode: 'series' })}><Plus className="h-4 w-4" /> Thêm lịch dạy</Button>
          </CardHeader>
          <CardContent>
            {series.loading && !series.data && <p className="mb-2 text-sm text-muted-foreground">Đang tải lịch dạy…</p>}
            {series.error && <p className="mb-2 text-sm text-destructive">{series.error}</p>}
            <Table
              headers={['Học phần', 'Lớp', 'Trường', 'Thứ', 'Giờ', 'Phòng', 'Thời gian áp dụng']}
              rows={series.data ?? []}
              cells={[
                (s) => <><b>{s.subject_code}</b> {s.subject_name}</>,
                (s) => s.class_code,
                (s) => schoolName(s.school_id),
                (s) => (s.weekly ? DAY_NAMES[s.day_of_week] : 'Một lần'),
                (s) => (s.time_mode === 0 ? `Tiết ${s.period_start}–${s.period_start + s.period_count - 1}` : `${s.start}–${s.end}`),
                (s) => s.room,
                (s) => (s.weekly ? `${fmtDay(s.date_from)} – ${fmtDay(s.date_to)}` : fmtDay(s.date_from)),
              ]}
              onOpen={(s) => openEditor({ mode: 'series', id: s.id })}
              empty="Chưa có lịch dạy. Bấm Thêm lịch dạy, hoặc kéo vùng trống trên trang Lịch tuần."
            />
          </CardContent>
        </Card>
      )}

      <Card>
        <CardHeader className="flex flex-row items-center justify-between">
          <CardTitle className="text-base">Công việc và hạn chót</CardTitle>
          <Button size="sm" onClick={() => openEditor({ mode: 'task' })}><Plus className="h-4 w-4" /> Thêm công việc</Button>
        </CardHeader>
        <CardContent>
          {tasks.loading && !tasks.data && <p className="mb-2 text-sm text-muted-foreground">Đang tải công việc…</p>}
          {tasks.error && <p className="mb-2 text-sm text-destructive">{tasks.error}</p>}
          <Table
            headers={['Công việc', 'Loại', 'Ngày', 'Giờ', 'Lặp', 'Trạng thái']}
            rows={[...(tasks.data ?? [])].sort((a, b) => (a.day + a.start).localeCompare(b.day + b.start))}
            cells={[
              (t) => t.title,
              (t) => TASK_TYPES[t.type] ?? t.type,
              (t) => fmtDay(t.day),
              (t) => (t.deadline ? `Hạn ${t.start}` : `${t.start}–${t.end}`),
              (t) => (t.until ? `Hàng tuần đến ${fmtDay(t.until)}` : 'Một lần'),
              (t) => TASK_STATUS[t.status] ?? t.status,
            ]}
            onOpen={(t) => openEditor({ mode: 'task', id: t.id })}
            empty="Chưa có công việc."
          />
        </CardContent>
      </Card>
    </div>
  )
}

export const scheduleRoutes: ModuleRoute[] = [
  { path: '/schedules', title: 'Lịch dạy và công việc', element: <SchedulesPage />, icon: ClipboardList, menu: true, order: 10 },
]
