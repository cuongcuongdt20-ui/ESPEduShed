// Quản lý tài khoản (TV1, chỉ admin): danh sách, tạo, đặt lại mật khẩu, xoá.
import { useState, type FormEvent } from 'react'
import { KeyRound, Trash2, UserPlus } from 'lucide-react'
import type { Role } from '../../../electron/shared'
import { api, errorText, useRequest } from '../api'
import { useSession } from '../session'
import { useToast } from '../components/toast'
import { useConfirm } from '../components/confirm'
import { Button } from '../components/ui/button'
import { Badge, Card, CardContent, CardHeader, CardTitle, Table, Td, Th } from '../components/ui/card'
import { Input, Label, Select } from '../components/ui/input'

interface UserRow {
  id: number
  name: string
  role: Role
  mustChangePassword: boolean
}

const ROLE_TEXT: Record<Role, string> = { admin: 'Quản trị', teacher: 'Giảng viên', student: 'Sinh viên' }

export function UsersPage() {
  const s = useSession()
  const toast = useToast()
  const confirm = useConfirm()
  const { data, error, reload } = useRequest<{ users: UserRow[] }>('auth.users', undefined, [], ['user'])
  const [name, setName] = useState('')
  const [role, setRole] = useState<Role>('teacher')
  const [password, setPassword] = useState('')
  const [busy, setBusy] = useState(false)
  const [resetFor, setResetFor] = useState<UserRow | null>(null)   // Electron không hỗ trợ window.prompt
  const [resetPw, setResetPw] = useState('')

  async function create(e: FormEvent) {
    e.preventDefault()
    setBusy(true)
    try {
      await api.createUser(name.trim(), role, password)
      toast.ok(`Đã tạo ${name}. Người dùng phải đổi mật khẩu ở lần đăng nhập đầu.`)
      setName('')
      setPassword('')
      reload()
    } catch (err) {
      toast.error(errorText(err))
    } finally {
      setBusy(false)
    }
  }

  async function resetPassword(e: FormEvent) {
    e.preventDefault()
    if (!resetFor) return
    try {
      await api.changePassword(resetPw, resetFor.name)
      toast.ok(`Đã đặt lại mật khẩu ${resetFor.name}. Người đó phải đổi lại ở lần đăng nhập tới.`)
      setResetFor(null)
      setResetPw('')
      reload()
    } catch (err) {
      toast.error(errorText(err))
    }
  }

  async function remove(u: UserRow) {
    if (!(await confirm(`Xoá tài khoản ${u.name}?`))) return
    try {
      await api.request('auth.remove', { user: u.name })
      toast.ok(`Đã xoá ${u.name}`)
      reload()
    } catch (err) {
      toast.error(errorText(err))
    }
  }

  return (
    <div className="grid gap-6 lg:grid-cols-[1fr_320px]">
      <Card>
        <CardHeader>
          <CardTitle>Tài khoản</CardTitle>
        </CardHeader>
        <CardContent>
          {error && <p className="text-sm text-destructive">{error}</p>}
          <Table>
            <thead>
              <tr>
                <Th>Tên đăng nhập</Th>
                <Th>Vai trò</Th>
                <Th>Trạng thái</Th>
                <Th className="w-28" />
              </tr>
            </thead>
            <tbody>
              {data?.users.map((u) => (
                <tr key={u.id}>
                  <Td className="font-medium">{u.name}</Td>
                  <Td>{ROLE_TEXT[u.role]}</Td>
                  <Td>{u.mustChangePassword ? <Badge tone="warning">Chờ đổi mật khẩu</Badge> : <Badge tone="success">Hoạt động</Badge>}</Td>
                  <Td className="text-right">
                    <Button variant="ghost" size="icon" title="Đặt lại mật khẩu" onClick={() => setResetFor(u)}>
                      <KeyRound className="h-4 w-4" />
                    </Button>
                    <Button
                      variant="ghost"
                      size="icon"
                      title="Xoá"
                      disabled={u.id === s.user?.id}
                      onClick={() => remove(u)}
                    >
                      <Trash2 className="h-4 w-4" />
                    </Button>
                  </Td>
                </tr>
              ))}
            </tbody>
          </Table>
          {resetFor && (
            <form className="mt-4 flex items-end gap-3 rounded-md border p-3" onSubmit={resetPassword}>
              <div className="flex-1 space-y-1">
                <Label htmlFor="reset">Mật khẩu mới cho {resetFor.name}</Label>
                <Input id="reset" type="password" value={resetPw} onChange={(e) => setResetPw(e.target.value)} autoFocus />
              </div>
              <Button type="submit" disabled={resetPw.length < 8}>
                Đặt lại
              </Button>
              <Button type="button" variant="ghost" onClick={() => setResetFor(null)}>
                Huỷ
              </Button>
            </form>
          )}
        </CardContent>
      </Card>
      <Card>
        <CardHeader>
          <CardTitle>Tạo tài khoản</CardTitle>
        </CardHeader>
        <CardContent>
          <form className="space-y-3" onSubmit={create}>
            <div className="space-y-1">
              <Label htmlFor="name">Tên đăng nhập</Label>
              <Input id="name" value={name} onChange={(e) => setName(e.target.value)} placeholder="gv.lan" />
            </div>
            <div className="space-y-1">
              <Label htmlFor="role">Vai trò</Label>
              <Select id="role" value={role} onChange={(e) => setRole(e.target.value as Role)}>
                <option value="teacher">Giảng viên</option>
                <option value="student">Sinh viên</option>
                <option value="admin">Quản trị</option>
              </Select>
            </div>
            <div className="space-y-1">
              <Label htmlFor="pw">Mật khẩu ban đầu</Label>
              <Input id="pw" type="password" value={password} onChange={(e) => setPassword(e.target.value)} autoComplete="new-password" />
            </div>
            <Button type="submit" className="w-full" disabled={busy || name.length < 3 || password.length < 8}>
              <UserPlus className="h-4 w-4" /> Tạo tài khoản
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
