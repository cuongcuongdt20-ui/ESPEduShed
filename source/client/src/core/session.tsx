// Trạng thái phiên dùng chung cho toàn app (TV1): đang nối server nào, ai đang đăng nhập.
import { createContext, useCallback, useContext, useEffect, useState, type ReactNode } from 'react'
import type { Role, SessionInfo } from '../../electron/shared'
import { api } from './api'

interface SessionCtx extends SessionInfo {
  ready: boolean
  refresh: () => Promise<void>
  hasRole: (min: Role) => boolean
}

const RANK: Record<Role, number> = { student: 1, teacher: 2, admin: 3 }
const Ctx = createContext<SessionCtx | null>(null)

export function SessionProvider({ children }: { children: ReactNode }) {
  const [info, setInfo] = useState<SessionInfo>({ host: null, port: null, state: 'offline', user: null, mustChangePassword: false })
  const [ready, setReady] = useState(false)

  const refresh = useCallback(async () => {
    setInfo(await api.session())
    setReady(true)
  }, [])

  useEffect(() => {
    refresh()
    return api.onConnection(() => refresh())       // nối lại xong main process có thể đã tự đăng nhập lại
  }, [refresh])

  const hasRole = (min: Role) => !!info.user && RANK[info.user.role] >= RANK[min]
  return <Ctx.Provider value={{ ...info, ready, refresh, hasRole }}>{children}</Ctx.Provider>
}

export function useSession(): SessionCtx {
  const c = useContext(Ctx)
  if (!c) throw new Error('useSession phải nằm trong SessionProvider')
  return c
}
