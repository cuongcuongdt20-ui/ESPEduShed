// Hộp xác nhận trong app (TV1). Dùng: const confirm = useConfirm(); if (!(await confirm('Xoá mục này?'))) return
// Không dùng window.confirm(): trên Windows, sau hộp thoại gốc của Electron, cửa sổ mất focus bàn phím và các ô nhập
// không gõ được nữa cho tới khi chuyển sang cửa sổ khác rồi quay lại.
import { createContext, useCallback, useContext, useEffect, useRef, useState, type ReactNode } from 'react'
import { Button } from './ui/button'

type ConfirmFn = (message: string) => Promise<boolean>

const Ctx = createContext<ConfirmFn | null>(null)

export function ConfirmProvider({ children }: { children: ReactNode }) {
  const [pending, setPending] = useState<{ message: string; resolve: (ok: boolean) => void } | null>(null)
  const okRef = useRef<HTMLButtonElement>(null)

  const confirm = useCallback<ConfirmFn>((message) => new Promise((resolve) => setPending({ message, resolve })), [])

  const answer = (ok: boolean) => {
    pending?.resolve(ok)
    setPending(null)
  }

  useEffect(() => {
    if (!pending) return
    okRef.current?.focus()
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') answer(false)
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [pending])

  return (
    <Ctx.Provider value={confirm}>
      {children}
      {pending && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 p-4" onMouseDown={() => answer(false)}>
          <div
            role="alertdialog"
            aria-modal="true"
            className="w-full max-w-md rounded-lg border bg-background p-5 shadow-lg"
            onMouseDown={(e) => e.stopPropagation()}
          >
            <p className="whitespace-pre-line text-sm">{pending.message}</p>
            <div className="mt-4 flex justify-end gap-2">
              <Button variant="outline" onClick={() => answer(false)}>Huỷ</Button>
              <Button ref={okRef} onClick={() => answer(true)}>Đồng ý</Button>
            </div>
          </div>
        </div>
      )}
    </Ctx.Provider>
  )
}

export function useConfirm(): ConfirmFn {
  const c = useContext(Ctx)
  if (!c) throw new Error('useConfirm phải nằm trong ConfirmProvider')
  return c
}
