// API cho giao diện (TV1). Mọi module gọi server qua `api` ở đây, không dùng window.espRaw trực tiếp.
//
//   const data = await api.request('school.list')
//   try { await api.request('schedule.create', form) } catch (e) { if (e instanceof ApiError && e.code === 'CONFLICT') ... }
//   useEffect(() => api.onPush((m) => { if (m.entity === 'schedule') reload() }), [])
import { useCallback, useEffect, useRef, useState } from 'react'
import type { ConnState, EspApi, IpcResult, PushMessage } from '../../electron/shared'

export class ApiError extends Error {
  code: string
  field: string | null
  data: any

  constructor(code: string, message: string, field: string | null = null, data?: unknown) {
    super(message)
    this.code = code
    this.field = field
    this.data = data
  }
}

async function unwrap<T>(p: Promise<IpcResult<T>>): Promise<T> {
  const r = await p
  if (r.ok) return r.data
  throw new ApiError(r.error.code, r.error.message, r.error.field, r.error.data)
}

const raw = () => window.espRaw

export const api: EspApi = {
  connect: (host, port) => unwrap(raw().connect(host, port)),
  login: (password) => unwrap(raw().login(password)),
  logout: () => unwrap(raw().logout()),
  request: (cmd, data) => unwrap(raw().request(cmd, data)),
  onPush: (cb) => raw().onPush(cb),
  onConnection: (cb) => raw().onConnection(cb),
  discover: (timeoutMs) => unwrap(raw().discover(timeoutMs)),
  session: () => unwrap(raw().session()),
  changePassword: (password) => unwrap(raw().changePassword(password)),
  printToPdf: (name) => unwrap(raw().printToPdf(name)),
}

// Thông báo lỗi dễ đọc cho người dùng
export function errorText(e: unknown): string {
  if (e instanceof ApiError) return e.field ? `${e.message} (${e.field})` : e.message
  return e instanceof Error ? e.message : String(e)
}

// Gọi một lệnh khi component hiện ra (và khi deps đổi); tự gọi lại khi server đẩy tin `changed` của các entity
// trong reloadOn. Trả {data, error, loading, reload}.
export function useRequest<T = any>(cmd: string, data?: object, deps: unknown[] = [], reloadOn: string[] = []) {
  const [state, setState] = useState<{ data: T | null; error: string | null; loading: boolean }>({
    data: null,
    error: null,
    loading: true,
  })
  const dataRef = useRef(data)
  dataRef.current = data
  const reload = useCallback(() => {
    setState((s) => ({ ...s, loading: true }))
    api
      .request<T>(cmd, dataRef.current)
      .then((d) => setState({ data: d, error: null, loading: false }))
      .catch((e) => setState({ data: null, error: errorText(e), loading: false }))
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [cmd, ...deps])
  useEffect(reload, [reload])
  usePush((m) => {
    if (reloadOn.includes(m.entity)) reload()
  })
  return { ...state, reload }
}

// Lệnh danh sách có phân trang (server trả tối đa ~6 KB mỗi lần, kèm "next" nếu còn): gọi lần lượt các trang và
// gộp mảng `key` lại. Dùng cho school.list, campus.list, holiday.list, subject.list, class.list, schedule.list, task.list.
export async function requestAll<T = any>(cmd: string, key: string, data: object = {}): Promise<T[]> {
  const items: T[] = []
  let offset = 0
  for (let page = 0; page < 100; page++) {
    const r = await api.request<any>(cmd, { ...data, offset })
    items.push(...((r?.[key] ?? []) as T[]))
    if (typeof r?.next !== 'number' || r.next <= offset) break
    offset = r.next
  }
  return items
}

// Như useRequest nhưng cho lệnh danh sách có phân trang: data là mảng đã gộp mọi trang
export function useList<T = any>(cmd: string, key: string, reloadOn: string[] = []) {
  const [state, setState] = useState<{ data: T[] | null; error: string | null; loading: boolean }>({
    data: null,
    error: null,
    loading: true,
  })
  const reload = useCallback(() => {
    setState((s) => ({ ...s, loading: true }))
    requestAll<T>(cmd, key)
      .then((d) => setState({ data: d, error: null, loading: false }))
      .catch((e) => setState({ data: null, error: errorText(e), loading: false }))
  }, [cmd, key])
  useEffect(reload, [reload])
  usePush((m) => {
    if (reloadOn.includes(m.entity)) reload()
  })
  return { ...state, reload }
}

export function usePush(cb: (msg: PushMessage) => void): void {
  const ref = useRef(cb)
  ref.current = cb
  useEffect(() => api.onPush((m) => ref.current(m)), [])
}

export function useConnection(): ConnState {
  const [state, setState] = useState<ConnState>('offline')
  useEffect(() => {
    api.session().then((s) => setState(s.state)).catch(() => {})
    return api.onConnection(setState)
  }, [])
  return state
}
