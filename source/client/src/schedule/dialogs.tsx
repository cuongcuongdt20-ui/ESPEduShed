// Hộp thoại của TV3: lịch dạy định kỳ, công việc, sửa một buổi (ngoại lệ), danh sách trùng có nút "Vẫn lưu".
// Mở qua editor.ts; ScheduleDialogHost được gắn một lần trong App.tsx nên mở được từ trang lịch lẫn trang danh sách.
import { useEffect, useState, type ReactNode } from 'react'
import { AlertTriangle, Save, Trash2, X } from 'lucide-react'
import { api, ApiError, errorText, requestAll } from '../core/api'
import { useToast } from '../core/components/toast'
import { useConfirm } from '../core/components/confirm'
import { Button } from '../core/components/ui/button'
import { ColorPicker } from '../core/components/color-picker'
import { Input, Label, Select } from '../core/components/ui/input'
import { onEditorRequest, openEditor, type EditorRequest } from './editor'

type Row = Record<string, any>

export const DAY_NAMES = ['', 'Thứ Hai', 'Thứ Ba', 'Thứ Tư', 'Thứ Năm', 'Thứ Sáu', 'Thứ Bảy', 'Chủ nhật']
export const TASK_TYPES = ['Cá nhân', 'Họp', 'Coi thi', 'Chấm thi', 'Hướng dẫn', 'Công tác', 'Nộp báo cáo']
export const TASK_STATUS = ['Chưa làm', 'Đang làm', 'Xong', 'Huỷ']

function pad(n: number) {
  return String(n).padStart(2, '0')
}
function ymd(d: Date) {
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`
}
function parseYmd(s: string) {
  const [y, m, d] = s.split('-').map(Number)
  return new Date(y, m - 1, d)
}
export function weekdayOf(s: string): number {
  const w = parseYmd(s).getDay()
  return w === 0 ? 7 : w
}
function addDays(s: string, n: number) {
  const d = parseYmd(s)
  d.setDate(d.getDate() + n)
  return ymd(d)
}

// ---------- khung chung ----------

function Modal({ title, onClose, children }: { title: string; onClose: () => void; children: ReactNode }) {
  return (
    <div className="fixed inset-0 z-40 flex items-center justify-center bg-black/40 p-4" onMouseDown={onClose}>
      <div
        className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-lg border bg-background p-5 shadow-lg"
        onMouseDown={(e) => e.stopPropagation()}
      >
        <div className="mb-4 flex items-center justify-between">
          <h2 className="text-lg font-semibold">{title}</h2>
          <Button variant="ghost" size="icon" onClick={onClose} aria-label="Đóng">
            <X className="h-4 w-4" />
          </Button>
        </div>
        {children}
      </div>
    </div>
  )
}

function Field({ label, children, wide }: { label: string; children: ReactNode; wide?: boolean }) {
  return (
    <div className={`space-y-1 ${wide ? 'sm:col-span-2' : ''}`}>
      <Label>{label}</Label>
      {children}
    </div>
  )
}

// Danh sách trùng do server trả (ApiError CONFLICT, data.conflicts), kèm nút lưu đè
function ConflictList({ items, onForce }: { items: Row[]; onForce: () => void }) {
  return (
    <div className="space-y-2 rounded-md border border-destructive/50 bg-destructive/5 p-3 text-sm">
      <div className="flex items-center gap-2 font-medium text-destructive">
        <AlertTriangle className="h-4 w-4" /> Server báo trùng lịch
      </div>
      {items.map((c, i) => (
        <div key={i}>
          {c.date} {c.start}–{c.end}: {c.reason === 2 ? 'không đủ thời gian di chuyển với' : 'trùng giờ với'}{' '}
          <b>{c.with?.title}</b> ({c.with?.start}–{c.with?.end})
        </div>
      ))}
      <Button size="sm" variant="destructive" onClick={onForce}>
        Vẫn lưu
      </Button>
    </div>
  )
}

// Gửi lệnh ghi; CONFLICT thì trả danh sách trùng để hiện, lỗi khác ném tiếp
async function send(cmd: string, data: Row): Promise<{ ok: true; res: Row } | { ok: false; conflicts: Row[] }> {
  try {
    return { ok: true, res: await api.request<Row>(cmd, data) }
  } catch (e) {
    if (e instanceof ApiError && e.code === 'CONFLICT' && Array.isArray(e.data?.conflicts))
      return { ok: false, conflicts: e.data.conflicts }
    throw e
  }
}

// ---------- lịch dạy định kỳ ----------

function SeriesForm({ id, prefill, onClose }: { id?: number; prefill?: Row; onClose: () => void }) {
  const toast = useToast()
  const confirm = useConfirm()
  const [schools, setSchools] = useState<Row[]>([])
  const [campuses, setCampuses] = useState<Row[]>([])
  const [conflicts, setConflicts] = useState<Row[] | null>(null)
  const [busy, setBusy] = useState(false)
  const [f, setF] = useState<Row>(() => {
    const date = String(prefill?.date ?? ymd(new Date()))
    const byTime = !!prefill?.start
    return {
      subject_code: '', subject_name: '', class_code: '', room: '', note: '', color: '',
      school_id: '', campus_id: '0', weekly: true, day_of_week: weekdayOf(date),
      time_mode: byTime ? 1 : 0, period_start: '', period_count: 1,
      start: prefill?.start ?? '07:00', end: prefill?.end && prefill.end !== '23:59' ? prefill.end : '08:30',
      date_from: date, date_to: addDays(date, 7 * 12),
    }
  })
  const set = (k: string, v: unknown) => setF((x) => ({ ...x, [k]: v }))

  useEffect(() => {
    requestAll<Row>('school.list', 'schools').then((schools) => {
      setSchools(schools)
      setF((x) => (x.school_id ? x : { ...x, school_id: String(schools[0]?.id ?? '') }))
    }).catch((e) => toast.error(errorText(e)))
    requestAll<Row>('campus.list', 'campuses').then(setCampuses).catch(() => {})
    if (id)
      api.request<Row>('schedule.get', { id }).then(({ schedule: s }) =>
        setF({
          ...s, school_id: String(s.school_id), campus_id: String(s.campus_id ?? 0),
          period_start: String(s.period_start), period_count: s.period_count,
        }),
      ).catch((e) => toast.error(errorText(e)))
  }, [id, toast])

  const school = schools.find((s) => String(s.id) === String(f.school_id))
  const periods: Row[] = [...(school?.periods ?? [])].sort((a, b) => a.no - b.no)
  const first = periods.find((p) => String(p.no) === String(f.period_start))
  const last = periods.find((p) => p.no === Number(f.period_start) + Number(f.period_count) - 1)

  async function save(force = false) {
    setBusy(true)
    setConflicts(null)
    const data: Row = {
      subject_code: f.subject_code.trim(), subject_name: f.subject_name.trim(), class_code: f.class_code ?? '',
      room: f.room ?? '', note: f.note ?? '', color: f.color ?? '', school_id: Number(f.school_id), campus_id: Number(f.campus_id || 0),
      weekly: !!f.weekly, day_of_week: Number(f.day_of_week), time_mode: Number(f.time_mode),
      date_from: f.date_from, date_to: f.weekly ? f.date_to : f.date_from, force,
    }
    if (data.time_mode === 0) Object.assign(data, { period_start: Number(f.period_start), period_count: Number(f.period_count) })
    else Object.assign(data, { start: f.start, end: f.end })
    if (id) data.id = id
    try {
      const r = await send(id ? 'schedule.update' : 'schedule.create', data)
      if (!r.ok) setConflicts(r.conflicts)
      else {
        const removed = Number(r.res.removedExceptions ?? 0)
        toast.ok(removed ? `Đã lưu; bỏ ${removed} thay đổi buổi không còn hợp lệ` : 'Đã lưu lịch dạy')
        onClose()
      }
    } catch (e) {
      toast.error(errorText(e))
    } finally {
      setBusy(false)
    }
  }

  async function remove() {
    if (!id || !(await confirm('Xoá lịch dạy này và mọi thay đổi buổi của nó?'))) return
    try {
      await api.request('schedule.delete', { id })
      toast.ok('Đã xoá lịch dạy')
      onClose()
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  return (
    <Modal title={id ? 'Sửa lịch dạy' : 'Thêm lịch dạy'} onClose={onClose}>
      <div className="grid gap-3 sm:grid-cols-2">
        <Field label="Mã học phần"><Input value={f.subject_code} onChange={(e) => set('subject_code', e.target.value)} placeholder="INT1433" /></Field>
        <Field label="Tên học phần"><Input value={f.subject_name} onChange={(e) => set('subject_name', e.target.value)} placeholder="Lập trình mạng" /></Field>
        <Field label="Lớp / nhóm"><Input value={f.class_code} onChange={(e) => set('class_code', e.target.value)} placeholder="N01" /></Field>
        <Field label="Phòng"><Input value={f.room} onChange={(e) => set('room', e.target.value)} placeholder="A2-301" /></Field>
        <Field label="Trường">
          <Select value={f.school_id} onChange={(e) => setF((x) => ({ ...x, school_id: e.target.value, campus_id: '0', period_start: '' }))}>
            {schools.map((s) => <option key={s.id} value={s.id}>{s.name}</option>)}
          </Select>
        </Field>
        <Field label="Cơ sở">
          <Select value={f.campus_id} onChange={(e) => set('campus_id', e.target.value)}>
            <option value="0">Không ghi (trực tuyến)</option>
            {campuses.filter((c) => String(c.schoolId) === String(f.school_id)).map((c) => <option key={c.id} value={c.id}>{c.name}</option>)}
          </Select>
        </Field>
        <Field label="Lặp lại">
          <Select value={f.weekly ? '1' : '0'} onChange={(e) => set('weekly', e.target.value === '1')}>
            <option value="1">Hàng tuần</option>
            <option value="0">Một lần</option>
          </Select>
        </Field>
        {f.weekly ? (
          <Field label="Thứ">
            <Select value={f.day_of_week} onChange={(e) => set('day_of_week', Number(e.target.value))}>
              {DAY_NAMES.slice(1).map((d, i) => <option key={d} value={i + 1}>{d}</option>)}
            </Select>
          </Field>
        ) : <div />}
        <Field label={f.weekly ? 'Từ ngày' : 'Ngày'}><Input type="date" value={f.date_from} onChange={(e) => set('date_from', e.target.value)} /></Field>
        {f.weekly && <Field label="Đến ngày"><Input type="date" value={f.date_to} onChange={(e) => set('date_to', e.target.value)} /></Field>}
        <Field label="Cách nhập giờ" wide>
          <Select value={f.time_mode} onChange={(e) => set('time_mode', Number(e.target.value))}>
            <option value={0}>Theo tiết của trường</option>
            <option value={1}>Theo giờ thực</option>
          </Select>
        </Field>
        {Number(f.time_mode) === 0 ? (
          <>
            <Field label="Tiết bắt đầu">
              <Select value={f.period_start} onChange={(e) => set('period_start', e.target.value)}>
                <option value="">Chọn tiết</option>
                {periods.map((p) => <option key={p.no} value={p.no}>Tiết {p.no} ({p.start}–{p.end})</option>)}
              </Select>
            </Field>
            <Field label="Số tiết"><Input type="number" min={1} max={10} value={f.period_count} onChange={(e) => set('period_count', e.target.value)} /></Field>
            <p className="text-xs text-muted-foreground sm:col-span-2">
              {first && last ? `Giờ quy đổi: ${first.start}–${last.end}` : 'Chọn tiết có trong bảng tiết của trường; khoảng tiết không được vắt qua tiết bị khuyết.'}
            </p>
          </>
        ) : (
          <>
            <Field label="Bắt đầu"><Input type="time" value={f.start} onChange={(e) => set('start', e.target.value)} /></Field>
            <Field label="Kết thúc"><Input type="time" value={f.end} onChange={(e) => set('end', e.target.value)} /></Field>
          </>
        )}
        <Field label="Ghi chú" wide><Input value={f.note} onChange={(e) => set('note', e.target.value)} /></Field>
        <Field label="Màu trên lịch" wide><ColorPicker value={f.color ?? ''} onChange={(c) => set('color', c)} noneLabel="Theo màu trường" /></Field>
      </div>
      {conflicts && <div className="mt-3"><ConflictList items={conflicts} onForce={() => void save(true)} /></div>}
      <div className="mt-4 flex justify-between">
        {id ? <Button variant="ghost" className="text-destructive" onClick={() => void remove()}><Trash2 className="h-4 w-4" /> Xoá</Button> : <span />}
        <Button disabled={busy} onClick={() => void save()}><Save className="h-4 w-4" /> Lưu</Button>
      </div>
    </Modal>
  )
}

// ---------- công việc ----------

function TaskForm({ id, prefill, onClose }: { id?: number; prefill?: Row; onClose: () => void }) {
  const toast = useToast()
  const confirm = useConfirm()
  const [conflicts, setConflicts] = useState<Row[] | null>(null)
  const [busy, setBusy] = useState(false)
  const [f, setF] = useState<Row>(() => ({
    title: '', type: 0, status: 0, content: '', place: '', note: '', color: '', until: '',
    day: prefill?.date ?? ymd(new Date()), deadline: false,
    start: prefill?.start ?? '09:00', end: prefill?.end && prefill.end !== '23:59' ? prefill.end : '10:00',
  }))
  const set = (k: string, v: unknown) => setF((x) => ({ ...x, [k]: v }))

  useEffect(() => {
    if (!id) return
    requestAll<Row>('task.list', 'tasks').then((tasks) => {
      const t = tasks.find((x: Row) => x.id === id)
      if (t) setF({ ...t, until: t.until ?? '' })
      else toast.error('Không tìm thấy công việc')
    }).catch((e) => toast.error(errorText(e)))
  }, [id, toast])

  async function save(force = false) {
    setBusy(true)
    setConflicts(null)
    const data: Row = {
      title: f.title.trim(), type: Number(f.type), status: Number(f.status), content: f.content ?? '', place: f.place ?? '',
      note: f.note ?? '', color: f.color ?? '', day: f.day, until: f.until || null, start: f.start, deadline: !!f.deadline, force,
    }
    if (!f.deadline) data.end = f.end
    if (id) data.id = id
    try {
      const r = await send(id ? 'task.update' : 'task.create', data)
      if (!r.ok) setConflicts(r.conflicts)
      else {
        toast.ok('Đã lưu công việc')
        onClose()
      }
    } catch (e) {
      toast.error(errorText(e))
    } finally {
      setBusy(false)
    }
  }

  async function remove() {
    if (!id || !(await confirm('Xoá công việc này?'))) return
    try {
      await api.request('task.delete', { id })
      toast.ok('Đã xoá công việc')
      onClose()
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  return (
    <Modal title={id ? 'Sửa công việc' : 'Thêm công việc'} onClose={onClose}>
      <div className="grid gap-3 sm:grid-cols-2">
        <Field label="Tên công việc" wide><Input value={f.title} onChange={(e) => set('title', e.target.value)} placeholder="Nộp báo cáo bài tập lớn" /></Field>
        <Field label="Loại">
          <Select value={f.type} onChange={(e) => set('type', Number(e.target.value))}>
            {TASK_TYPES.map((t, i) => <option key={t} value={i}>{t}</option>)}
          </Select>
        </Field>
        <Field label="Trạng thái">
          <Select value={f.status} onChange={(e) => set('status', Number(e.target.value))}>
            {TASK_STATUS.map((t, i) => <option key={t} value={i}>{t}</option>)}
          </Select>
        </Field>
        <Field label="Ngày"><Input type="date" value={f.day} onChange={(e) => set('day', e.target.value)} /></Field>
        <Field label="Kiểu thời gian">
          <Select value={f.deadline ? '1' : '0'} onChange={(e) => set('deadline', e.target.value === '1')}>
            <option value="0">Có giờ bắt đầu – kết thúc</option>
            <option value="1">Hạn chót</option>
          </Select>
        </Field>
        <Field label={f.deadline ? 'Hạn lúc' : 'Bắt đầu'}><Input type="time" value={f.start} onChange={(e) => set('start', e.target.value)} /></Field>
        {!f.deadline && <Field label="Kết thúc"><Input type="time" value={f.end} onChange={(e) => set('end', e.target.value)} /></Field>}
        <Field label="Lặp hàng tuần đến (bỏ trống: một lần)"><Input type="date" value={f.until} onChange={(e) => set('until', e.target.value)} /></Field>
        <Field label="Địa điểm"><Input value={f.place} onChange={(e) => set('place', e.target.value)} /></Field>
        <Field label="Nội dung" wide><Input value={f.content} onChange={(e) => set('content', e.target.value)} /></Field>
        <Field label="Ghi chú" wide><Input value={f.note} onChange={(e) => set('note', e.target.value)} /></Field>
        <Field label="Màu trên lịch" wide><ColorPicker value={f.color ?? ''} onChange={(c) => set('color', c)} noneLabel="Mặc định (xám)" /></Field>
      </div>
      {conflicts && <div className="mt-3"><ConflictList items={conflicts} onForce={() => void save(true)} /></div>}
      <div className="mt-4 flex justify-between">
        {id ? <Button variant="ghost" className="text-destructive" onClick={() => void remove()}><Trash2 className="h-4 w-4" /> Xoá</Button> : <span />}
        <Button disabled={busy} onClick={() => void save()}><Save className="h-4 w-4" /> Lưu</Button>
      </div>
    </Modal>
  )
}

// ---------- một buổi của lịch dạy (ngoại lệ) ----------

const EXC_ACTIONS = [
  { type: 1, label: 'Huỷ buổi này' },
  { type: 2, label: 'Đổi phòng' },
  { type: 3, label: 'Đổi giờ (cùng ngày)' },
  { type: 4, label: 'Dời sang ngày khác' },
  { type: 5, label: 'Huỷ buổi này và dạy bù' },
]

function OccurrenceDialog({ sourceId, origDate, date, kind, onClose }: {
  sourceId: number; origDate: string; date?: string; kind?: number; onClose: () => void
}) {
  const toast = useToast()
  const [s, setS] = useState<Row | null>(null)
  const [error, setError] = useState('')
  const [conflicts, setConflicts] = useState<Row[] | null>(null)
  const visibleDate = date ?? origDate
  const [f, setF] = useState<Row>({ type: 1, new_room: '', new_date: visibleDate, new_start: '07:00', new_end: '09:00', note: '' })
  const set = (k: string, v: unknown) => setF((x) => ({ ...x, [k]: v }))

  useEffect(() => {
    let alive = true
    Promise.all([
      api.request<Row>('schedule.get', { id: sourceId }),
      api.request<Row>('occurrence.range', { from: visibleDate, to: visibleDate }),
    ]).then(([r, range]) => {
      if (!alive) return
      setS(r.schedule)
      // Giờ thực của buổi (đã áp dụng ngoại lệ). Buổi gốc đã huỷ và buổi dạy bù cùng ngày có chung sourceId,
      // origDate nên phân biệt thêm bằng ngày đang hiện và kind.
      const occ = (range.items ?? []).find((x: Row) => x.sourceId === sourceId && x.origDate === origDate
        && x.date === visibleDate && (kind === undefined || x.kind === kind))
      const slot = occ ?? r.schedule
      if (slot.start && slot.end) setF((x) => ({ ...x, new_start: slot.start, new_end: slot.end }))
    }).catch((e) => { if (alive) setError(errorText(e)) })
    return () => { alive = false }
  }, [sourceId, origDate, visibleDate, kind])

  async function save(force = false) {
    setConflicts(null)
    const type = Number(f.type)
    const data: Row = { series_id: sourceId, orig_date: origDate, type, note: f.note, force }
    if (type === 2) data.new_room = f.new_room
    if (type >= 3) Object.assign(data, { new_start: f.new_start, new_end: f.new_end })
    if (type >= 4) data.new_date = f.new_date
    try {
      const r = await send('exception.set', data)
      if (!r.ok) setConflicts(r.conflicts)
      else {
        toast.ok('Đã lưu thay đổi của buổi')
        onClose()
      }
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  async function reset() {
    try {
      await api.request('exception.delete', { series_id: sourceId, orig_date: origDate })
      toast.ok('Buổi đã trở lại như lịch gốc')
      onClose()
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  const type = Number(f.type)
  return (
    <Modal title="Thay đổi một buổi" onClose={onClose}>
      {error && <p className="text-sm text-destructive">{error}</p>}
      {s && (
        <div className="space-y-3">
          <p className="text-sm">
            <b>{s.subject_name}</b> {s.class_code && `· ${s.class_code}`} · buổi gốc ngày {origDate} · phòng {s.room || '—'}
          </p>
          <div className="grid gap-3 sm:grid-cols-2">
            <Field label="Thay đổi" wide>
              <Select value={f.type} onChange={(e) => set('type', Number(e.target.value))}>
                {EXC_ACTIONS.map((a) => <option key={a.type} value={a.type}>{a.label}</option>)}
              </Select>
            </Field>
            {type === 2 && <Field label="Phòng mới"><Input value={f.new_room} onChange={(e) => set('new_room', e.target.value)} /></Field>}
            {type >= 4 && <Field label={type === 5 ? 'Ngày dạy bù' : 'Ngày mới'}><Input type="date" value={f.new_date} onChange={(e) => set('new_date', e.target.value)} /></Field>}
            {type >= 3 && (
              <>
                <Field label="Bắt đầu"><Input type="time" value={f.new_start} onChange={(e) => set('new_start', e.target.value)} /></Field>
                <Field label="Kết thúc"><Input type="time" value={f.new_end} onChange={(e) => set('new_end', e.target.value)} /></Field>
              </>
            )}
            <Field label="Ghi chú" wide><Input value={f.note} onChange={(e) => set('note', e.target.value)} /></Field>
          </div>
          {conflicts && <ConflictList items={conflicts} onForce={() => void save(true)} />}
          <div className="flex flex-wrap justify-between gap-2">
            <div className="flex gap-2">
              <Button variant="outline" size="sm" onClick={() => void reset()}>Bỏ thay đổi của buổi này</Button>
              <Button variant="outline" size="sm" onClick={() => openEditor({ mode: 'series', id: sourceId })}>Sửa cả chuỗi</Button>
            </div>
            <Button onClick={() => void save()}><Save className="h-4 w-4" /> Lưu</Button>
          </div>
        </div>
      )}
    </Modal>
  )
}

// ---------- gắn một lần trong App.tsx ----------

export function ScheduleDialogHost() {
  const [req, setReq] = useState<EditorRequest | null>(null)
  useEffect(() => onEditorRequest(setReq), [])
  if (!req) return null
  const close = () => setReq(null)
  if (req.mode === 'series') return <SeriesForm key={`series-${req.id ?? 'new'}`} id={req.id} prefill={req.prefill} onClose={close} />
  if (req.mode === 'task') return <TaskForm key={`task-${req.id ?? 'new'}`} id={req.id} prefill={req.prefill} onClose={close} />
  return (
    <OccurrenceDialog key={`occ-${req.sourceId}-${req.origDate}-${req.date ?? ''}-${req.kind ?? ''}`}
      sourceId={req.sourceId} origDate={req.origDate} date={req.date} kind={req.kind} onClose={close} />
  )
}
