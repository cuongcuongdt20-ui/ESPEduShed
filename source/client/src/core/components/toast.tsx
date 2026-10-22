// Thông báo ngắn góc màn hình (TV1). Dùng: const toast = useToast(); toast.error('...'); toast.ok('Đã lưu')
import { createContext, useCallback, useContext, useState, type ReactNode } from 'react'
import { cn } from '../lib/utils'

type Tone = 'ok' | 'error'
interface Item {
  id: number
  tone: Tone
  text: string
}

const Ctx = createContext<{ ok: (t: string) => void; error: (t: string) => void } | null>(null)
let nextId = 1

export function ToastProvider({ children }: { children: ReactNode }) {
  const [items, setItems] = useState<Item[]>([])
  const push = useCallback((tone: Tone, text: string) => {
    const id = nextId++
    setItems((xs) => [...xs, { id, tone, text }])
    setTimeout(() => setItems((xs) => xs.filter((x) => x.id !== id)), 4000)
  }, [])
  const value = { ok: (t: string) => push('ok', t), error: (t: string) => push('error', t) }
  return (
    <Ctx.Provider value={value}>
      {children}
      <div className="fixed bottom-4 right-4 z-50 flex flex-col gap-2">
        {items.map((x) => (
          <div
            key={x.id}
            className={cn(
              'max-w-sm rounded-md px-4 py-3 text-sm shadow-lg',
              x.tone === 'ok' ? 'bg-emerald-600 text-white' : 'bg-destructive text-destructive-foreground',
            )}
          >
            {x.text}
          </div>
        ))}
      </div>
    </Ctx.Provider>
  )
}

export function useToast() {
  const c = useContext(Ctx)
  if (!c) throw new Error('useToast phải nằm trong ToastProvider')
  return c
}
