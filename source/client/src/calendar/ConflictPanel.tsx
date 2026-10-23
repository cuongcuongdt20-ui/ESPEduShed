// Bảng xung đột trong khoảng đang xem (TV4): bấm để nhảy tới buổi, "Đã biết" gọi conflict.ack.
import { AlertTriangle, CheckCheck } from 'lucide-react'
import { api, errorText, useRequest } from '../core/api'
import { useToast } from '../core/components/toast'
import { Button } from '../core/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'
import { reasonText, type ConflictRow } from './types'

interface Props {
  from: string
  to: string
  onJump: (date: string) => void
}

function fmtDay(d: string): string {
  return `${d.slice(8, 10)}/${d.slice(5, 7)}`
}

export function ConflictPanel({ from, to, onJump }: Props) {
  const toast = useToast()
  const { data, error, reload } = useRequest<{ conflicts: ConflictRow[]; truncated?: boolean }>(
    'conflict.list',
    { from, to },
    [from, to],
    ['schedule', 'task', 'exception', 'school', 'holiday', 'conflict'],
  )
  const rows = data?.conflicts ?? []
  const open = rows.filter((r) => !r.acked).length

  async function ack(r: ConflictRow) {
    try {
      await api.request('conflict.ack', { a: r.a.sourceId, b: r.b.sourceId, date: r.date })
      toast.ok('Đã đánh dấu đã biết')
      reload()
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  return (
    <Card>
      <CardHeader className="pb-2">
        <CardTitle className="flex items-center gap-2 text-base">
          <AlertTriangle className={open ? 'h-4 w-4 text-destructive' : 'h-4 w-4 text-muted-foreground'} />
          Xung đột {open > 0 ? `(${open})` : ''}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-2 text-sm">
        {error && <p className="text-destructive">{error}</p>}
        {!error && rows.length === 0 && <p className="text-muted-foreground">Không có xung đột trong khoảng đang xem.</p>}
        {rows.map((r, i) => (
          <div
            key={`${r.a.sourceId}-${r.b.sourceId}-${r.date}-${i}`}
            className={`rounded-md border p-2 ${r.acked ? 'opacity-60' : 'border-destructive/60'}`}
          >
            <button className="w-full text-left" onClick={() => onJump(r.date)} title="Xem trên lịch">
              <div className="font-medium">
                {fmtDay(r.date)} · {r.start}–{r.end} · {reasonText(r.reason)}
              </div>
              <div className="text-muted-foreground">
                {r.a.title} ({r.a.start}–{r.a.end})
              </div>
              <div className="text-muted-foreground">
                {r.b.title} ({r.b.start}–{r.b.end})
              </div>
            </button>
            <div className="mt-1 flex justify-end">
              {r.acked ? (
                <span className="text-xs text-muted-foreground">Đã biết</span>
              ) : (
                <Button size="sm" variant="outline" onClick={() => ack(r)}>
                  <CheckCheck className="h-3 w-3" /> Đã biết
                </Button>
              )}
            </div>
          </div>
        ))}
        {data?.truncated && <p className="text-xs text-muted-foreground">Còn nhiều xung đột hơn, chỉ hiện một phần.</p>}
      </CardContent>
    </Card>
  )
}
