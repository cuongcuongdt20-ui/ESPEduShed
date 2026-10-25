import { useEffect, useMemo, useState } from 'react'
import { Building2, CalendarDays, Clock3, GraduationCap, MapPinned, School, BookOpen, RefreshCw, Plus, Save, Trash2 } from 'lucide-react'
import type { ModuleRoute } from '../core/routes'
import { api, errorText, usePush } from '../core/api'
import { Card, CardContent, CardHeader, CardTitle } from '../core/components/ui/card'
import { Button } from '../core/components/ui/button'
import { Input, Select, Label } from '../core/components/ui/input'

type Tab = 'schools' | 'periods' | 'holidays' | 'subjects' | 'classes' | 'travel'
type Row = Record<string, any>
const tabs: { id: Tab; label: string; icon: typeof School }[] = [
  { id: 'schools', label: 'Trường & cơ sở', icon: School }, { id: 'periods', label: 'Bảng tiết học', icon: Clock3 },
  { id: 'holidays', label: 'Lịch nghỉ', icon: CalendarDays }, { id: 'subjects', label: 'Môn học', icon: BookOpen },
  { id: 'classes', label: 'Lớp / nhóm học', icon: GraduationCap }, { id: 'travel', label: 'Thời gian di chuyển', icon: MapPinned },
]
function fmtDate(value: string) { return value || '' }
function CatalogPage() {
  const [tab, setTab] = useState<Tab>('schools')
  const [schools, setSchools] = useState<Row[]>([])
  const [campuses, setCampuses] = useState<Row[]>([])
  const [holidays, setHolidays] = useState<Row[]>([])
  const [subjects, setSubjects] = useState<Row[]>([])
  const [classes, setClasses] = useState<Row[]>([])
  const [travel, setTravel] = useState<Row[]>([])
  const [defaultTravel, setDefaultTravel] = useState('30')
  const [selectedSchool, setSelectedSchool] = useState('1')
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState('')
  const [notice, setNotice] = useState('')
  const [form, setForm] = useState<Row>({})
  const [periods, setPeriods] = useState<Row[]>([])
  const [newPeriod, setNewPeriod] = useState({ no: '1', start: '07:00', end: '07:50' })
  const selected = schools.find(s => String(s.id) === selectedSchool)
  const refresh = async () => {
    setLoading(true); setError('')
    try {
      // allSettled: lệnh nào server chưa có (UNKNOWN_CMD) thì bỏ qua phần đó, các phần khác vẫn hiện
      const names = ['school.list', 'holiday.list', 'subject.list', 'class.list', 'travel.get', 'campus.list']
      const results = await Promise.allSettled(names.map(n => api.request<any>(n)))
      const failed = names.filter((_, i) => results[i].status === 'rejected')
      if (failed.length) setError(`Server chưa trả lời được: ${failed.join(', ')}`)
      const [s, h, sub, cls, tr, cp] = results.map(r => (r.status === 'fulfilled' ? r.value ?? {} : {}))
      const ss = s.schools ?? s.items ?? []; setSchools(ss)
      setHolidays(h.holidays ?? h.items ?? []); setSubjects(sub.subjects ?? sub.items ?? [])
      setClasses(cls.classes ?? cls.items ?? []); setTravel(tr.travel ?? tr.items ?? [])
      setDefaultTravel(String(tr.defaultTravel ?? 30)); setCampuses(cp.campuses ?? cp.items ?? [])
      if (ss.length && !ss.some((x: Row) => String(x.id) === selectedSchool)) setSelectedSchool(String(ss[0].id))
    } catch (e) { setError(errorText(e)) } finally { setLoading(false) }
  }
  useEffect(() => { void refresh() }, [])
  usePush(m => { if (['school','campus','periods','holiday','subject','class','travel'].includes(m.entity)) void refresh() })
  useEffect(() => { setPeriods((selected?.periods ?? []).map((p: Row) => ({ ...p, start: String(p.start), end: String(p.end) }))) }, [selectedSchool, schools])
  const sortedPeriods = useMemo(() => [...periods].sort((a,b) => Number(a.no)-Number(b.no)), [periods])
  const periodError = useMemo(() => {
    const seen = new Set<number>(); let previous: Row | undefined
    for (const p of sortedPeriods) {
      const n = Number(p.no), start = String(p.start), end = String(p.end)
      if (!Number.isInteger(n) || n < 1 || n > 255) return 'Số tiết phải là số nguyên dương.'
      if (seen.has(n)) return `Tiết ${n} bị khai báo trùng.`
      seen.add(n)
      if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(start) || !/^([01]\d|2[0-3]):[0-5]\d$/.test(end) || start >= end) return `Giờ bắt đầu/kết thúc của tiết ${n} không hợp lệ.`
      if (previous && start < String(previous.end)) return `Tiết ${n} chồng lấn với tiết ${previous.no}.`
      previous = p
    }
    return ''
  }, [sortedPeriods])
  const save = async (cmd: string, data: Row) => { setError(''); setNotice(''); try { await api.request(cmd, data); setNotice('Đã lưu thay đổi.'); await refresh() } catch (e) { setError(errorText(e)) } }
  const remove = async (cmd: string, data: Row) => { if (!window.confirm('Bạn chắc chắn muốn xóa mục này?')) return; await save(cmd, data) }
  const setField = (key: string, value: any) => setForm((f: Row) => ({ ...f, [key]: value }))
  const resetForm = (initial: Row = {}) => setForm(initial)
  const input = (label: string, key: string, placeholder = '', type = 'text') => <div className="space-y-1" key={key}><Label>{label}</Label><Input type={type} value={form[key] ?? ''} placeholder={placeholder} onChange={e => setField(key, e.target.value)} /></div>
  const schoolOptions = schools.map(s => ({ value: String(s.id), label: s.name }))
  const table = (headers: string[], rows: Row[], cells: ((r: Row) => any)[], onEdit?: (r: Row) => void, onDelete?: (r: Row) => void) => <div className="overflow-x-auto rounded-md border"><table className="w-full min-w-[720px] text-left text-sm"><thead className="bg-muted/70"><tr>{headers.map(h => <th key={h} className="px-3 py-3 font-semibold">{h}</th>)}{(onEdit || onDelete) && <th className="px-3 py-3">Thao tác</th>}</tr></thead><tbody>{rows.map((r,i) => <tr key={r.id ?? `${i}`} className="border-t hover:bg-muted/30">{cells.map((cell,j)=><td key={j} className="px-3 py-3">{cell(r)}</td>)}{(onEdit || onDelete) && <td className="px-3 py-2"><div className="flex gap-2">{onEdit && <Button size="sm" variant="outline" onClick={()=>onEdit(r)}>Sửa</Button>}{onDelete && <Button size="sm" variant="ghost" className="text-destructive" onClick={()=>onDelete(r)}><Trash2 size={15}/></Button>}</div></td>}</tr>)}{rows.length===0 && <tr><td className="px-3 py-8 text-center text-muted-foreground" colSpan={headers.length+1}>Chưa có dữ liệu</td></tr>}</tbody></table></div>
  const renderContent = () => {
    if (tab === 'schools') return <div className="space-y-5">
      <Card><CardHeader><CardTitle className="text-base">Danh sách trường</CardTitle></CardHeader><CardContent className="space-y-4">{table(['Màu','Tên trường','Tên viết tắt','Số cơ sở','Số tiết'], schools, [s=><span className="inline-block h-5 w-5 rounded" style={{background:s.color}}/>,s=>s.name,s=>s.shortName ?? s.short_name,s=>campuses.filter(c=>Number(c.schoolId ?? c.school_id)===Number(s.id)).length,s=>(s.periods??[]).length], s=>{resetForm({id:s.id,name:s.name,shortName:s.shortName??s.short_name,color:s.color});setTab('schools')}, s=>void remove('school.delete',{id:s.id}))}</CardContent></Card>
      <Card><CardHeader><CardTitle className="text-base">Thêm / cập nhật trường</CardTitle></CardHeader><CardContent className="grid gap-3 md:grid-cols-2">{input('Tên trường','name','Ví dụ: Đại học A')}{input('Tên viết tắt','shortName','Ví dụ: DUA')}{input('Mã màu','color','#2E7D32')}<div className="flex items-end gap-2"><Button onClick={()=>void save(form.id?'school.update':'school.create',{...form})}><Save size={16}/> Lưu trường</Button><Button variant="outline" onClick={()=>resetForm()}>Làm mới</Button></div></CardContent></Card>
      <Card><CardHeader><CardTitle className="text-base">Cơ sở trường</CardTitle></CardHeader><CardContent className="space-y-3">{table(['Trường','Tên cơ sở','Địa chỉ'],campuses,[c=>schools.find(s=>Number(s.id)===Number(c.schoolId??c.school_id))?.name??c.schoolId,c=>c.name,c=>c.address],c=>resetForm({campusId:c.id,campusSchoolId:c.schoolId??c.school_id,campusName:c.name,campusAddress:c.address}),c=>void remove('campus.delete',{id:c.id}))}<div className="grid gap-3 md:grid-cols-4"> <div className="space-y-1"><Label>Trường</Label><Select value={String(form.campusSchoolId??selectedSchool)} onChange={e=>setField('campusSchoolId',Number(e.target.value))}>{schoolOptions.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</Select></div>{input('Tên cơ sở','campusName','Cơ sở chính')}{input('Địa chỉ','campusAddress','Địa chỉ cơ sở')}<div className="flex items-end"><Button onClick={()=>void save(form.campusId?'campus.update':'campus.create',{id:form.campusId,schoolId:Number(form.campusSchoolId??selectedSchool),name:form.campusName,address:form.campusAddress??''})}><Plus size={16}/> Lưu cơ sở</Button></div></div></CardContent></Card>
    </div>
    if (tab === 'periods') return <div className="space-y-4"><Card><CardHeader><CardTitle className="text-base">Bảng tiết riêng theo trường</CardTitle></CardHeader><CardContent className="space-y-4"><div className="max-w-md space-y-1"><Label>Chọn trường</Label><Select value={selectedSchool} onChange={e=>setSelectedSchool(e.target.value)}>{schoolOptions.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</Select></div><div className="overflow-x-auto rounded-md border"><table className="w-full text-sm"><thead className="bg-muted/70"><tr><th className="px-3 py-2 text-left">Tiết</th><th className="px-3 py-2 text-left">Bắt đầu</th><th className="px-3 py-2 text-left">Kết thúc</th><th className="px-3 py-2"/></tr></thead><tbody>{sortedPeriods.map((p,i)=><tr className="border-t" key={`${p.no}-${i}`}><td className="px-3 py-2">Tiết <Input className="inline-flex w-20 ml-2" type="number" min="1" value={p.no} onChange={e=>setPeriods(ps=>ps.map((x,j)=>j===i?{...x,no:e.target.value}:x))}/></td><td className="px-3 py-2"><Input type="time" value={p.start} onChange={e=>setPeriods(ps=>ps.map((x,j)=>j===i?{...x,start:e.target.value}:x))}/></td><td className="px-3 py-2"><Input type="time" value={p.end} onChange={e=>setPeriods(ps=>ps.map((x,j)=>j===i?{...x,end:e.target.value}:x))}/></td><td className="px-3 py-2"><Button variant="ghost" size="sm" onClick={()=>setPeriods(ps=>ps.filter((_,j)=>j!==i))}><Trash2 size={15}/></Button></td></tr>)}</tbody></table></div><div className="grid gap-2 rounded-md bg-muted/30 p-3 sm:grid-cols-4"><div className="space-y-1"><Label>Thêm tiết số</Label><Input type="number" min="1" value={newPeriod.no} onChange={e=>setNewPeriod({...newPeriod,no:e.target.value})}/></div><div className="space-y-1"><Label>Bắt đầu</Label><Input type="time" value={newPeriod.start} onChange={e=>setNewPeriod({...newPeriod,start:e.target.value})}/></div><div className="space-y-1"><Label>Kết thúc</Label><Input type="time" value={newPeriod.end} onChange={e=>setNewPeriod({...newPeriod,end:e.target.value})}/></div><div className="flex items-end"><Button variant="outline" onClick={()=>setPeriods(ps=>[...ps,{...newPeriod}])}><Plus size={16}/> Thêm tiết</Button></div></div>{periodError && <p className="text-sm text-destructive">{periodError}</p>}<div className="rounded-md border p-3"><p className="mb-2 font-medium">Xem trước theo trục thời gian</p><div className="flex flex-wrap gap-2">{sortedPeriods.map(p=><div key={p.no} className="min-w-28 rounded border-l-4 bg-muted/50 px-3 py-2" style={{borderLeftColor:selected?.color??'#2563eb'}}><div className="font-semibold">Tiết {p.no}</div><div className="text-xs text-muted-foreground">{p.start} – {p.end}</div></div>)}</div><p className="mt-2 text-xs text-muted-foreground">Số tiết không cần liên tục; ví dụ 1–5, 7–9, 13–16. Một lịch không được kéo qua số tiết bị khuyết.</p></div><Button disabled={!selectedSchool||!!periodError} onClick={()=>void save('periods.set',{schoolId:Number(selectedSchool),periods:sortedPeriods.map(p=>({no:Number(p.no),start:p.start,end:p.end}))})}><Save size={16}/> Lưu bảng tiết</Button></CardContent></Card></div>
    if (tab === 'holidays') return <div className="space-y-4"><Card><CardHeader><CardTitle className="text-base">Lịch nghỉ / ngày lễ</CardTitle></CardHeader><CardContent className="space-y-3">{table(['Tên dịp nghỉ','Phạm vi','Từ ngày','Đến ngày'],holidays,[h=>h.name,h=>Number(h.schoolId??h.school_id)===0?'Tất cả trường':schools.find(s=>s.id===Number(h.schoolId??h.school_id))?.name??h.schoolId,h=>fmtDate(h.fromDate??h.from_date),h=>fmtDate(h.toDate??h.to_date)],h=>resetForm({id:h.id,name:h.name,schoolId:h.schoolId??h.school_id,from:h.fromDate??h.from_date,to:h.toDate??h.to_date}),h=>void remove('holiday.delete',{id:h.id}))}<div className="grid gap-3 md:grid-cols-5"><div className="space-y-1"><Label>Phạm vi</Label><Select value={String(form.schoolId??0)} onChange={e=>setField('schoolId',Number(e.target.value))}><option value="0">Tất cả trường</option>{schoolOptions.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</Select></div>{input('Tên ngày nghỉ','name','Nghỉ lễ')}{input('Từ ngày','from','', 'date')}{input('Đến ngày','to','', 'date')}<div className="flex items-end"><Button onClick={()=>void save(form.id?'holiday.update':'holiday.create',{...form,schoolId:Number(form.schoolId??0),from:form.from,to:form.to})}><Save size={16}/> Lưu</Button></div></div></CardContent></Card></div>
    if (tab === 'subjects') return <div className="space-y-4"><Card><CardHeader><CardTitle className="text-base">Danh mục môn học</CardTitle></CardHeader><CardContent className="space-y-3">{table(['Mã môn','Tên môn học','Số tín chỉ'],subjects,[s=>s.code,s=>s.name,s=>s.credits],s=>resetForm({id:s.id,code:s.code,name:s.name,credits:s.credits}),s=>void remove('subject.delete',{id:s.id}))}<div className="grid gap-3 md:grid-cols-4">{input('Mã môn','code','CS101')}{input('Tên môn','name','Lập trình cơ sở')}{input('Số tín chỉ','credits','3','number')}<div className="flex items-end"><Button onClick={()=>void save(form.id?'subject.update':'subject.create',{...form,credits:Number(form.credits)})}><Save size={16}/> Lưu môn học</Button></div></div></CardContent></Card></div>
    if (tab === 'classes') return <div className="space-y-4"><Card><CardHeader><CardTitle className="text-base">Lớp / nhóm học phần</CardTitle></CardHeader><CardContent className="space-y-3">{table(['Mã lớp','Môn học','Trường','Sĩ số'],classes,[c=>c.code,c=>subjects.find(s=>s.id===Number(c.subjectId??c.subject_id))?.name??c.subjectId,c=>schools.find(s=>s.id===Number(c.schoolId??c.school_id))?.name??c.schoolId,c=>c.size],c=>resetForm({id:c.id,code:c.code,subjectId:c.subjectId??c.subject_id,schoolId:c.schoolId??c.school_id,size:c.size}),c=>void remove('class.delete',{id:c.id}))}<div className="grid gap-3 md:grid-cols-4"><div className="space-y-1"><Label>Môn học</Label><Select value={String(form.subjectId??'')} onChange={e=>setField('subjectId',Number(e.target.value))}><option value="">Chọn môn</option>{subjects.map(s=><option key={s.id} value={s.id}>{s.code} — {s.name}</option>)}</Select></div><div className="space-y-1"><Label>Trường</Label><Select value={String(form.schoolId??selectedSchool)} onChange={e=>setField('schoolId',Number(e.target.value))}>{schoolOptions.map(o=><option key={o.value} value={o.value}>{o.label}</option>)}</Select></div>{input('Mã lớp / nhóm','code','01.2025')}{input('Sĩ số','size','40','number')}<div className="flex items-end"><Button onClick={()=>void save(form.id?'class.update':'class.create',{...form,subjectId:Number(form.subjectId),schoolId:Number(form.schoolId??selectedSchool),size:Number(form.size)})}><Save size={16}/> Lưu lớp</Button></div></div></CardContent></Card></div>
    return <div className="space-y-4"><Card><CardHeader><CardTitle className="text-base">Ma trận thời gian di chuyển giữa các cơ sở</CardTitle></CardHeader><CardContent className="space-y-4"><div className="flex flex-wrap items-end gap-3"><div className="w-48 space-y-1"><Label>Thời gian mặc định (phút)</Label><Input type="number" min="0" value={defaultTravel} onChange={e=>setDefaultTravel(e.target.value)}/></div><Button onClick={()=>void save('travel.setDefault',{minutes:Number(defaultTravel)})}><Save size={16}/> Lưu mặc định</Button></div>{table(['Cơ sở đi','Cơ sở đến','Thời gian (phút)'],travel,[t=>campuses.find(c=>c.id===Number(t.campusA??t.campus_a))?.name??t.campusA,t=>campuses.find(c=>c.id===Number(t.campusB??t.campus_b))?.name??t.campusB,t=>t.minutes],t=>resetForm({campusA:t.campusA??t.campus_a,campusB:t.campusB??t.campus_b,minutes:t.minutes}),t=>void remove('travel.delete',{campusA:t.campusA??t.campus_a,campusB:t.campusB??t.campus_b}))}<div className="grid gap-3 md:grid-cols-4"><div className="space-y-1"><Label>Cơ sở đi</Label><Select value={String(form.campusA??'')} onChange={e=>setField('campusA',Number(e.target.value))}><option value="">Chọn cơ sở</option>{campuses.map(c=><option key={c.id} value={c.id}>{c.name}</option>)}</Select></div><div className="space-y-1"><Label>Cơ sở đến</Label><Select value={String(form.campusB??'')} onChange={e=>setField('campusB',Number(e.target.value))}><option value="">Chọn cơ sở</option>{campuses.map(c=><option key={c.id} value={c.id}>{c.name}</option>)}</Select></div>{input('Số phút','minutes','20','number')}<div className="flex items-end"><Button onClick={()=>void save('travel.set',{campusA:Number(form.campusA),campusB:Number(form.campusB),minutes:Number(form.minutes)})}><Save size={16}/> Lưu thời gian</Button></div></div><p className="text-xs text-muted-foreground">Thời gian giữa hai cơ sở được xem là hai chiều và áp dụng chung cho cả hai hướng.</p></CardContent></Card></div>
  }
  return <div className="space-y-5">
    <div className="flex flex-wrap items-start justify-between gap-3"><div><div className="flex items-center gap-2"><Building2 className="text-primary" size={22}/><h1 className="text-2xl font-semibold tracking-tight">Danh mục đào tạo</h1></div><p className="mt-1 text-sm text-muted-foreground">Quản lý trường, bảng tiết, ngày nghỉ, môn học, lớp và thời gian di chuyển.</p></div><Button variant="outline" onClick={()=>void refresh()} disabled={loading}><RefreshCw size={16} className={loading?'animate-spin':''}/> Làm mới</Button></div>
    {error && <div role="alert" className="rounded-md border border-destructive/40 bg-destructive/5 px-4 py-3 text-sm text-destructive">{error}</div>}{notice && <div className="rounded-md border border-green-600/30 bg-green-600/5 px-4 py-3 text-sm">{notice}</div>}
    <div className="flex flex-wrap gap-2 border-b pb-3">{tabs.map(t=>{const Icon=t.icon;return <Button key={t.id} variant={tab===t.id?'default':'outline'} size="sm" onClick={()=>{setTab(t.id);resetForm()}}><Icon size={15}/>{t.label}</Button>})}</div>
    {renderContent()}
  </div>
}
export const catalogRoutes: ModuleRoute[] = [
  { path: '/schools', title: 'Danh mục đào tạo', element: <CatalogPage />, icon: School, menu: true, order: 20 },
]
