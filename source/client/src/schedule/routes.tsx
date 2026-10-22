// Route của TV3. BẢN GIẢ do TV1 tạo.
// TODO(TV3): ScheduleList, ScheduleForm, TaskForm, ExceptionDialog, ConflictList (README mục B3)
import { ClipboardList } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'

function SchedulesPage() {
  return (
    <Card className="max-w-3xl">
      <CardHeader>
        <CardTitle>Lịch dạy và công việc</CardTitle>
      </CardHeader>
      <CardContent className="text-sm text-muted-foreground">Đang làm (TV3)</CardContent>
    </Card>
  )
}

export const scheduleRoutes: ModuleRoute[] = [
  { path: '/schedules', title: 'Lịch dạy và công việc', element: <SchedulesPage />, icon: ClipboardList, menu: true, order: 10 },
]
