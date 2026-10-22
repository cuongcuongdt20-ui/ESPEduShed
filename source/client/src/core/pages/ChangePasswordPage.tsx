// Đổi mật khẩu (TV1). Bắt buộc ở lần đăng nhập đầu (mustChangePassword).
import { useState, type FormEvent } from 'react'
import { useNavigate } from 'react-router-dom'
import { api, errorText } from '../api'
import { useSession } from '../session'
import { useToast } from '../components/toast'
import { Button } from '../components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card'
import { Input, Label } from '../components/ui/input'

export function ChangePasswordPage() {
  const s = useSession()
  const navigate = useNavigate()
  const toast = useToast()
  const [pw, setPw] = useState('')
  const [pw2, setPw2] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const mismatch = pw2.length > 0 && pw !== pw2

  async function submit(e: FormEvent) {
    e.preventDefault()
    if (pw !== pw2) return
    setBusy(true)
    setError(null)
    try {
      await api.changePassword(pw)
      await s.refresh()
      toast.ok('Đã đổi mật khẩu')
      navigate('/')
    } catch (err) {
      setError(errorText(err))
    } finally {
      setBusy(false)
    }
  }

  return (
    <div className="mx-auto max-w-sm pt-10">
      <Card>
        <CardHeader>
          <CardTitle>Đổi mật khẩu</CardTitle>
          <CardDescription>
            {s.mustChangePassword ? 'Đây là lần đăng nhập đầu, bạn cần đặt mật khẩu mới.' : 'Mật khẩu tối thiểu 8 ký tự.'}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form className="space-y-3" onSubmit={submit}>
            <div className="space-y-1">
              <Label htmlFor="pw">Mật khẩu mới</Label>
              <Input id="pw" type="password" value={pw} onChange={(e) => setPw(e.target.value)} autoComplete="new-password" />
            </div>
            <div className="space-y-1">
              <Label htmlFor="pw2">Nhập lại</Label>
              <Input id="pw2" type="password" value={pw2} onChange={(e) => setPw2(e.target.value)} autoComplete="new-password" />
            </div>
            {mismatch && <p className="text-sm text-destructive">Hai lần nhập không khớp</p>}
            {error && <p className="text-sm text-destructive">{error}</p>}
            <Button type="submit" className="w-full" disabled={busy || pw.length < 8 || pw !== pw2}>
              Lưu mật khẩu
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
