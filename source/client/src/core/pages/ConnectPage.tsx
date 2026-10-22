// Màn hình kết nối (TV1): tìm ESP32 trong LAN bằng UDP (discovery.ts của TV2) hoặc nhập IP.
import { useState, type FormEvent } from 'react'
import { useNavigate } from 'react-router-dom'
import { Radar, Server } from 'lucide-react'
import type { ServerInfo } from '../../../electron/shared'
import { api, errorText } from '../api'
import { useSession } from '../session'
import { Button } from '../components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card'
import { Input, Label } from '../components/ui/input'

const LAST_KEY = 'lastServer'

function lastServer(): { host: string; port: number } {
  try {
    return JSON.parse(localStorage.getItem(LAST_KEY) ?? '') as { host: string; port: number }
  } catch {
    return { host: '192.168.4.1', port: 5000 }
  }
}

export function ConnectPage() {
  const s = useSession()
  const navigate = useNavigate()
  const [host, setHost] = useState(lastServer().host)
  const [port, setPort] = useState(String(lastServer().port))
  const [found, setFound] = useState<ServerInfo[] | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)

  async function connect(h: string, p: number) {
    setBusy(true)
    setError(null)
    try {
      await api.connect(h, p)
      try {
        localStorage.setItem(LAST_KEY, JSON.stringify({ host: h, port: p }))
      } catch {
        // bỏ qua: lần sau phải nhập lại
      }
      await s.refresh()
      navigate('/')
    } catch (e) {
      setError(errorText(e))
    } finally {
      setBusy(false)
    }
  }

  async function scan() {
    setBusy(true)
    setError(null)
    try {
      setFound(await api.discover(1500))
    } catch (e) {
      setError(errorText(e))
    } finally {
      setBusy(false)
    }
  }

  function submit(e: FormEvent) {
    e.preventDefault()
    connect(host.trim(), Number(port))
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-muted/40 p-6">
      <Card className="w-full max-w-md">
        <CardHeader>
          <CardTitle>Kết nối tới ESPEduShed</CardTitle>
          <CardDescription>Server chạy trên ESP32-S3 trong cùng mạng Wi-Fi, giao thức TCP cổng 5000.</CardDescription>
        </CardHeader>
        <CardContent className="space-y-5">
          <div className="space-y-2">
            <Button variant="outline" className="w-full" onClick={scan} disabled={busy}>
              <Radar className="h-4 w-4" /> Tìm server trong mạng LAN (UDP)
            </Button>
            {found && found.length === 0 && (
              <p className="text-sm text-muted-foreground">Không tìm thấy server nào. Hãy nhập IP in trên cổng serial của ESP32.</p>
            )}
            {found?.map((f) => (
              <button
                key={f.ip}
                className="flex w-full items-center gap-3 rounded-md border p-3 text-left text-sm hover:bg-accent"
                onClick={() => connect(f.ip, f.port)}
              >
                <Server className="h-4 w-4" />
                <span className="flex-1">
                  {f.name} — {f.ip}:{f.port}
                </span>
                <span className="text-xs text-muted-foreground">fw {f.fw}</span>
              </button>
            ))}
          </div>
          <form className="space-y-3" onSubmit={submit}>
            <div className="grid grid-cols-[1fr_100px] gap-3">
              <div className="space-y-1">
                <Label htmlFor="host">Địa chỉ IP</Label>
                <Input id="host" value={host} onChange={(e) => setHost(e.target.value)} placeholder="192.168.1.37" />
              </div>
              <div className="space-y-1">
                <Label htmlFor="port">Cổng</Label>
                <Input id="port" value={port} onChange={(e) => setPort(e.target.value)} inputMode="numeric" />
              </div>
            </div>
            {error && <p className="text-sm text-destructive">{error}</p>}
            <Button type="submit" className="w-full" disabled={busy}>
              Kết nối
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
