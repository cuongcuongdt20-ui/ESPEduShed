// Route của TV2. BẢN GIẢ do TV1 tạo: trang trường đã đọc school.list để minh hoạ cách gọi api.
// TODO(TV2): SchoolsPage, PeriodsEditor, HolidaysPage, SubjectsPage, ClassesPage, TravelMatrix (README mục B2)
import { School } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { useRequest } from '../core/api'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'

interface SchoolRow {
  id: number
  name: string
  shortName: string
  color: string
  periods: { no: number; start: string; end: string }[]
}

function SchoolsPage() {
  const { data, error } = useRequest<{ schools: SchoolRow[] }>('school.list', undefined, [], ['school'])
  return (
    <Card className="max-w-3xl">
      <CardHeader>
        <CardTitle>Trường và bảng tiết</CardTitle>
      </CardHeader>
      <CardContent className="space-y-3 text-sm">
        {error && <p className="text-destructive">{error}</p>}
        {data?.schools.map((s) => (
          <div key={s.id} className="flex items-center gap-3">
            <span className="h-3 w-3 rounded-full" style={{ background: s.color }} />
            <span className="font-medium">{s.name}</span>
            <span className="text-muted-foreground">
              {s.periods.map((p) => `T${p.no} ${p.start}–${p.end}`).join(' · ')}
            </span>
          </div>
        ))}
        <p className="text-muted-foreground">Đang làm (TV2)</p>
      </CardContent>
    </Card>
  )
}

export const catalogRoutes: ModuleRoute[] = [
  { path: '/schools', title: 'Trường và tiết học', element: <SchoolsPage />, icon: School, menu: true, order: 20 },
]
