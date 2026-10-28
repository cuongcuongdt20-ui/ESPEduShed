// Trạng thái phiên dùng chung cho toàn app (TV1): đang nối server nào, đã đăng nhập chưa.
import { createContext, useCallback, useContext, useEffect, useState, type ReactNode } from 'react'
import type { SessionInfo } from '../../electron/shared'
import { api } from './api'

interface SessionCtx extends SessionInfo {
  ready: boolean
  refresh: () => Promise<void>
}

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

  return <Ctx.Provider value={{ ...info, ready, refresh }}>{children}</Ctx.Provider>
}

export function useSession(): SessionCtx {
  const c = useContext(Ctx)
  if (!c) throw new Error('useSession phải nằm trong SessionProvider')
  return c
}
