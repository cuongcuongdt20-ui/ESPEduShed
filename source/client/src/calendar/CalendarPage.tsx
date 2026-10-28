// Trang lịch tuần (TV4): FullCalendar, khối sự kiện tự vẽ, bảng xung đột, lọc, chú giải, kéo thả, in PDF.
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import FullCalendar from '@fullcalendar/react'
import timeGridPlugin from '@fullcalendar/timegrid'
import listPlugin from '@fullcalendar/list'
import interactionPlugin from '@fullcalendar/interaction'
import viLocale from '@fullcalendar/core/locales/vi'
import type { DatesSetArg, EventContentArg, EventDropArg, EventInput, EventSourceFuncArg } from '@fullcalendar/core'
import type { DateSelectArg, EventClickArg } from '@fullcalendar/core'
import { AlertTriangle, Printer } from 'lucide-react'
import { api, errorText, usePush } from '../core/api'
import { useToast } from '../core/components/toast'
import { useConfirm } from '../core/components/confirm'
import { Button } from '../core/components/ui/button'
import { openNewSchedule, openScheduleEditor } from '../schedule/editor'
import { ConflictPanel } from './ConflictPanel'
import { moveOccurrence } from './actions'
import {
  addDays, hm, reasonText, ymd,
  KIND_MAKEUP, KIND_TASK, KIND_TEACH, STATUS_CANCELLED, STATUS_HOLIDAY,
  type CheckRow, type Item, type RangeData,
} from './types'
import './calendar.css'

const PUSH_ENTITIES = ['schedule', 'task', 'exception', 'school', 'holiday', 'conflict']

function isCancelled(it: Item): boolean {
  return it.status === STATUS_CANCELLED || it.status === STATUS_HOLIDAY
}

function hourOf(t: string): number {
  return Number(t.slice(0, 2))
}

// Khối sự kiện tự vẽ: môn, trường, lớp, phòng, giờ; biểu tượng cảnh báo khi trùng
function EventBlock({ arg }: { arg: EventContentArg }) {
  const it = arg.event.extendedProps.item as Item
  const conflict = !!it.conflicts?.length
  if (arg.view.type.startsWith('list')) {
    return (
      <span>
        {conflict && <AlertTriangle className="mr-1 inline h-3 w-3 text-destructive" />}
        {it.title}
        {it.room ? ` · ${it.room}` : ''}
        {it.school ? ` · ${it.school}` : ''}
        {it.status === STATUS_CANCELLED ? ' (đã huỷ)' : it.status === STATUS_HOLIDAY ? ' (nghỉ lễ)' : ''}
      </span>
    )
  }
  return (
    <div className="ev-body h-full overflow-hidden px-1 text-[11px] leading-tight">
      <div className="flex items-center gap-1 font-semibold">
        {conflict && <AlertTriangle className="h-3 w-3 shrink-0 text-destructive" />}
        <span className="truncate">{it.title}</span>
      </div>
      <div>
        {it.start === it.end ? `Hạn ${it.start}` : `${it.start}–${it.end}`}
      </div>
      {(it.school || it.room) && (
        <div className="truncate">
          {it.school}
          {it.school && it.room ? ' · ' : ''}
          {it.room}
        </div>
      )}
      {it.classCode && <div className="truncate">{it.classCode}</div>}
    </div>
  )
}

export function CalendarPage() {
  const toast = useToast()
  const confirm = useConfirm()
  const calRef = useRef<FullCalendar>(null)
  const [range, setRange] = useState<{ from: string; to: string } | null>(null)
  const [slot, setSlot] = useState({ min: 6, max: 22 })
  const [schools, setSchools] = useState<{ name: string; color: string }[]>([])
  const [truncated, setTruncated] = useState(false)
  const [fSchool, setFSchool] = useState('')
  const [fKind, setFKind] = useState('')
  const filter = useRef({ school: '', kind: '' })
  filter.current = { school: fSchool, kind: fKind }

  const refetch = useCallback(() => calRef.current?.getApi().refetchEvents(), [])
  useEffect(() => {
    refetch()
  }, [fSchool, fKind, refetch])
  usePush((m) => {
    if (PUSH_ENTITIES.includes(m.entity)) refetch()
  })

  // events dạng hàm: gọi calendar.range theo khoảng đang xem (FullCalendar: end là đầu ngày kế tiếp)
  const loadEvents = useCallback(async (arg: EventSourceFuncArg): Promise<EventInput[]> => {
    try {
      const data = await api.request<RangeData>('calendar.range', { from: ymd(arg.start), to: ymd(addDays(arg.end, -1)) })
      setTruncated(!!data.truncated)
      // trục giờ 06:00–22:00, mở rộng khi có buổi nằm ngoài
      let min = 6, max = 22
      for (const it of data.items) {
        min = Math.min(min, hourOf(it.start))
        max = Math.max(max, hourOf(it.end) + (it.end.slice(3) === '00' ? 0 : 1))
      }
      setSlot((s) => (s.min === min && s.max === max ? s : { min, max: Math.min(max, 24) }))
      const seen = new Map<string, string>()
      for (const it of data.items) if (it.school && !seen.has(it.school)) seen.set(it.school, it.color)
      setSchools((old) => {
        const merged = new Map(old.map((s) => [s.name, s.color]))
        seen.forEach((c, n) => merged.set(n, c))
        return merged.size === old.length ? old : [...merged].map(([name, color]) => ({ name, color }))
      })
      const f = filter.current
      return data.items
        .filter((it) => (!f.school || it.school === f.school) && (!f.kind || String(it.kind) === f.kind))
        .map((it) => {
          const conflict = !!it.conflicts?.length
          const classes = ['ev']
          if (conflict) classes.push('ev-conflict')
          if (it.kind === KIND_MAKEUP) classes.push('ev-makeup')
          if (it.kind === KIND_TASK) classes.push('ev-task')
          if (isCancelled(it)) classes.push('ev-cancel')
          const movable = it.kind !== KIND_TASK && !isCancelled(it)
          return {
            id: `${it.sourceId}:${it.origDate}:${it.start}`,
            title: it.title,
            start: `${it.date}T${it.start}`,
            // Hạn chót (start == end): bỏ end để FullCalendar vẽ khối 30 phút thay vì khối 0 phút không thấy được
            end: it.start === it.end ? undefined : `${it.date}T${it.end}`,
            backgroundColor: conflict ? 'rgba(250, 204, 21, 0.35)' : `${it.color}33`,
            borderColor: conflict ? '#dc2626' : it.kind === KIND_MAKEUP ? '#9333ea' : it.color,
            classNames: classes,
            startEditable: movable,
            durationEditable: false,
            extendedProps: { item: it },
          }
        })
    } catch (e) {
      toast.error(errorText(e))
      return []
    }
  }, [toast])

  function onDates(arg: DatesSetArg) {
    setRange({ from: ymd(arg.start), to: ymd(addDays(arg.end, -1)) })
  }

  function onClick(arg: EventClickArg) {
    const it = arg.event.extendedProps.item as Item
    openScheduleEditor(it.sourceId, it.kind, it.origDate, it.date)
  }

  function onSelect(arg: DateSelectArg) {
    const end = ymd(arg.start) === ymd(arg.end) ? hm(arg.end) : '23:59'
    openNewSchedule(ymd(arg.start), hm(arg.start), end)
    arg.view.calendar.unselect()
  }

  // Kéo thả dời một buổi: kiểm tra trùng (conflict.check) → xác nhận → exception.set
  async function onDrop(info: EventDropArg) {
    const it = info.event.extendedProps.item as Item
    const s = info.event.start
    const e = info.event.end
    if (!s || !e || ymd(s) !== ymd(e)) {
      info.revert()
      toast.error('Buổi phải nằm trong một ngày')
      return
    }
    const date = ymd(s), start = hm(s), end = hm(e)
    try {
      const chk = await api.request<{ conflicts: CheckRow[] }>('conflict.check', {
        sourceId: it.sourceId, kind: it.kind, origDate: it.origDate, curDate: it.date, date, start, end,
      })
      if (chk.conflicts.length > 0) {
        const lines = chk.conflicts.map((c) => `• ${c.with.title} (${c.with.start}–${c.with.end}): ${reasonText(c.reason)}`)
        if (!(await confirm(`Dời "${it.title}" sang ${date} ${start}–${end} sẽ gây xung đột:\n${lines.join('\n')}\n\nVẫn dời?`))) {
          info.revert()
          return
        }
      }
      await moveOccurrence(it, date, start, end)
      toast.ok('Đã dời buổi')
    } catch (err) {
      info.revert()
      toast.error(errorText(err))
    }
  }

  async function onPrint() {
    try {
      const path = await api.printToPdf('lich-tuan.pdf')
      if (path) toast.ok(`Đã lưu ${path}`)
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  const jump = (date: string) => calRef.current?.getApi().gotoDate(date)
  const pad2 = (n: number) => String(n).padStart(2, '0')
  const slotMin = useMemo(() => `${pad2(slot.min)}:00:00`, [slot.min])
  const slotMax = useMemo(() => `${pad2(slot.max)}:00:00`, [slot.max])

  return (
    <div className="flex flex-col gap-3 xl:flex-row">
      <div className="min-w-0 flex-1 space-y-2">
        <div className="esp-print-hide flex flex-wrap items-center gap-2 text-sm">
          <select
            className="h-8 rounded-md border border-input bg-background px-2"
            value={fSchool}
            onChange={(e) => setFSchool(e.target.value)}
            aria-label="Lọc theo trường"
          >
            <option value="">Mọi trường</option>
            {schools.map((s) => (
              <option key={s.name} value={s.name}>
                {s.name}
              </option>
            ))}
          </select>
          <select
            className="h-8 rounded-md border border-input bg-background px-2"
            value={fKind}
            onChange={(e) => setFKind(e.target.value)}
            aria-label="Lọc theo loại"
          >
            <option value="">Mọi loại</option>
            <option value={KIND_TEACH}>Lịch dạy</option>
            <option value={KIND_TASK}>Công việc</option>
            <option value={KIND_MAKEUP}>Dạy bù</option>
          </select>
          <Button size="sm" variant="outline" onClick={onPrint}>
            <Printer className="h-4 w-4" /> In / PDF
          </Button>
          {truncated && <span className="text-destructive">Tuần này có quá nhiều buổi, chỉ hiện một phần.</span>}
        </div>

        <div className="esp-print-hide flex flex-wrap items-center gap-3 text-xs text-muted-foreground">
          {schools.map((s) => (
            <span key={s.name} className="flex items-center gap-1">
              <i className="inline-block h-3 w-3 rounded-sm border" style={{ background: `${s.color}33`, borderColor: s.color }} />
              {s.name}
            </span>
          ))}
          <span className="flex items-center gap-1">
            <i className="inline-block h-3 w-3 rounded-sm border-2 border-red-600" style={{ background: 'rgba(250,204,21,.35)' }} />
            Trùng lịch
          </span>
          <span className="flex items-center gap-1">
            <i className="inline-block h-3 w-3 rounded-sm border-l-4 border-purple-600 bg-muted" />
            Dạy bù
          </span>
          <span className="flex items-center gap-1">
            <i className="inline-block h-3 w-3 rounded-sm border border-dashed bg-muted" />
            Công việc
          </span>
          <span className="flex items-center gap-1 line-through opacity-60">Đã huỷ / nghỉ lễ</span>
        </div>

        <div className="esp-cal h-[calc(100vh-13rem)] min-h-[480px]">
          <FullCalendar
            ref={calRef}
            plugins={[timeGridPlugin, listPlugin, interactionPlugin]}
            initialView="timeGridWeek"
            locale={viLocale}
            firstDay={1}
            headerToolbar={{ left: 'prev,next today', center: 'title', right: 'timeGridWeek,timeGridDay,listWeek' }}
            height="100%"
            allDaySlot={false}
            nowIndicator
            slotMinTime={slotMin}
            slotMaxTime={slotMax}
            slotDuration="00:30:00"
            defaultTimedEventDuration="00:30:00"
            slotLabelFormat={{ hour: '2-digit', minute: '2-digit', hour12: false }}
            eventTimeFormat={{ hour: '2-digit', minute: '2-digit', hour12: false }}
            events={loadEvents}
            eventContent={(arg) => <EventBlock arg={arg} />}
            eventClick={onClick}
            selectable
            select={onSelect}
            eventDrop={onDrop}
            datesSet={onDates}
          />
        </div>
      </div>

      <div className="esp-print-hide w-full shrink-0 xl:w-80">
        {range && <ConflictPanel from={range.from} to={range.to} onJump={jump} />}
      </div>
    </div>
  )
}
