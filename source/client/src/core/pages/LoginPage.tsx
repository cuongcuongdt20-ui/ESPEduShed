// Đăng nhập (TV1). Server chỉ có một tài khoản nên chỉ hỏi mật khẩu. Mật khẩu chỉ đi xuống main process để tính
// PBKDF2/HMAC, không đi trên mạng.
import { useState, type FormEvent } from 'react'
import { useNavigate } from 'react-router-dom'
import { api, errorText } from '../api'
import { useSession } from '../session'
import { Button } from '../components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card'
import { Input, Label } from '../components/ui/input'

export function LoginPage() {
  const s = useSession()
  const navigate = useNavigate()
  const [password, setPassword] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)

  async function submit(e: FormEvent) {
    e.preventDefault()
    setBusy(true)
    setError(null)
    try {
      const r = await api.login(password)
      await s.refresh()
      navigate(r.mustChangePassword ? '/change-password' : '/')
    } catch (err) {
      setError(errorText(err))
    } finally {
      setBusy(false)
      setPassword('')
    }
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-muted/40 p-6">
      <Card className="w-full max-w-sm">
        <CardHeader>
          <CardTitle>Đăng nhập</CardTitle>
          <CardDescription>
            Server {s.host}:{s.port}.{' '}
            <button className="underline" onClick={() => navigate('/connect')}>
              Đổi server
            </button>
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form className="space-y-3" onSubmit={submit}>
            <div className="space-y-1">
              <Label htmlFor="password">Mật khẩu</Label>
              <Input
                id="password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                autoFocus
                autoComplete="current-password"
              />
            </div>
            {error && <p className="text-sm text-destructive">{error}</p>}
            <Button type="submit" className="w-full" disabled={busy || !password}>
              Đăng nhập
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
