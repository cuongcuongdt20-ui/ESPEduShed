// Truy cập từ xa: link web xem lịch trên điện thoại (web.link, web.rotate) và tên miền DuckDNS (system.ddns).
// Ở ngoài mạng nhà, app và điện thoại vào ESP32 qua <tên>.duckdns.org khi router đã mở cổng 5000 và 8080.
import { useState, type FormEvent } from 'react'
import { Copy, RefreshCw } from 'lucide-react'
import { QRCodeSVG } from 'qrcode.react'
import { api, errorText, useRequest } from '../api'
import { useSession } from '../session'
import { useToast } from '../components/toast'
import { useConfirm } from '../components/confirm'
import { Button } from '../components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card'
import { Input, Label } from '../components/ui/input'

interface WebLink {
  key: string
  port: number
}

interface Ddns {
  domain: string
  lastOk?: boolean
  lastAgoS?: number
}

const HOST_KEY = 'publicHost'

function savedHost(): string {
  try {
    return localStorage.getItem(HOST_KEY) ?? ''
  } catch {
    return ''
  }
}

export function RemotePage() {
  const s = useSession()
  const toast = useToast()
  const confirm = useConfirm()
  const link = useRequest<WebLink>('web.link')
  const status = useRequest<{ ip: string; ddns?: Ddns }>('system.status')
  const ddns = status.data?.ddns
  const [host, setHost] = useState(savedHost)
  const [domain, setDomain] = useState('')
  const [token, setToken] = useState('')
  const [busy, setBusy] = useState(false)

  // Không nhập thì dùng tên DuckDNS (nếu đã bật), không có thì IP của ESP32 trong mạng nhà
  const usedHost = host.trim() || (ddns?.domain ? `${ddns.domain}.duckdns.org` : status.data?.ip || s.host || '')
  const url = link.data ? `http://${usedHost}:${link.data.port}/c/${link.data.key}` : ''

  function changeHost(v: string) {
    setHost(v)
    try {
      localStorage.setItem(HOST_KEY, v.trim())
    } catch {
      // không lưu được thì chỉ dùng trong phiên này
    }
  }

  async function copy() {
    try {
      await navigator.clipboard.writeText(url)
      toast.ok('Đã sao chép link')
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  async function rotate() {
    if (!(await confirm('Đổi link xem lịch? Link cũ trên các điện thoại sẽ không mở được nữa.'))) return
    try {
      await api.request('web.rotate')
      link.reload()
      toast.ok('Đã đổi link')
    } catch (e) {
      toast.error(errorText(e))
    }
  }

  async function saveDdns(e: FormEvent) {
    e.preventDefault()
    setBusy(true)
    try {
      await api.request('system.ddns', { domain: domain.trim().toLowerCase(), token: token.trim() })
      setToken('')
      toast.ok(domain.trim() ? 'Đã lưu DuckDNS, ESP32 sẽ cập nhật ngay' : 'Đã tắt DuckDNS')
      setTimeout(status.reload, 3000)
    } catch (err) {
      toast.error(errorText(err))
    } finally {
      setBusy(false)
    }
  }

  return (
    <div className="max-w-2xl space-y-5">
      <Card>
        <CardHeader>
          <CardTitle>Xem lịch trên điện thoại</CardTitle>
          <CardDescription>
            Trang web chỉ đọc chạy trên ESP32. Ai có link là xem được lịch, nên chỉ gửi link cho chính bạn.
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-3">
          <div className="space-y-1">
            <Label htmlFor="host">Địa chỉ dùng trong link</Label>
            <Input
              id="host"
              placeholder={ddns?.domain ? `${ddns.domain}.duckdns.org` : status.data?.ip ?? 'ten.duckdns.org'}
              value={host}
              onChange={(e) => changeHost(e.target.value)}
            />
            <p className="text-xs text-muted-foreground">
              Để trống thì dùng tên DuckDNS bên dưới, hoặc IP trong mạng nhà nếu chưa bật. Muốn mở từ 4G hay mạng khác cần
              tên DDNS và mở cổng {link.data?.port ?? 8080} trên router.
            </p>
          </div>
          {link.error && <p className="text-sm text-destructive">{link.error}</p>}
          {url && <p className="break-all rounded-md border bg-muted/40 px-3 py-2 font-mono text-xs">{url}</p>}
          {url && (
            <div className="flex flex-col items-center gap-2 py-1">
              {/* Nền trắng cố định để camera quét được cả khi app ở giao diện tối; mã sinh ngay trong app, link không gửi đi đâu */}
              <div className="rounded-md border bg-white">
                <QRCodeSVG value={url} size={208} level="M" marginSize={4} title="Mã QR của link xem lịch" />
              </div>
              <p className="text-xs text-muted-foreground">Quét bằng camera điện thoại để mở link. Mã QR cũng có giá trị như link, đừng chia sẻ.</p>
            </div>
          )}
          <div className="flex gap-2">
            <Button size="sm" onClick={copy} disabled={!url}>
              <Copy className="h-4 w-4" /> Sao chép link
            </Button>
            <Button size="sm" variant="outline" onClick={rotate} disabled={!link.data}>
              <RefreshCw className="h-4 w-4" /> Đổi link
            </Button>
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Tên miền động DuckDNS</CardTitle>
          <CardDescription>
            ESP32 tự báo IP công khai của router cho DuckDNS mỗi 5 phút. Bỏ qua nếu router đã có sẵn DDNS.
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-3">
          <p className="text-sm">
            {ddns?.domain ? (
              <>
                Đang dùng <b>{ddns.domain}.duckdns.org</b>
                {ddns.lastOk === undefined
                  ? ', chưa cập nhật lần nào.'
                  : ddns.lastOk
                    ? `, cập nhật được ${Math.round((ddns.lastAgoS ?? 0) / 60)} phút trước.`
                    : ', lần cập nhật gần nhất bị lỗi (kiểm tra token).'}
              </>
            ) : (
              'Chưa bật.'
            )}
          </p>
          <form className="grid gap-3 sm:grid-cols-[1fr_1fr_auto] sm:items-end" onSubmit={saveDdns}>
            <div className="space-y-1">
              <Label htmlFor="domain">Tên miền</Label>
              <Input id="domain" placeholder="ten (bỏ .duckdns.org)" value={domain} onChange={(e) => setDomain(e.target.value)} />
            </div>
            <div className="space-y-1">
              <Label htmlFor="token">Token</Label>
              <Input id="token" type="password" value={token} onChange={(e) => setToken(e.target.value)} />
            </div>
            <Button type="submit" disabled={busy || (!!domain.trim() && !token.trim())}>
              {domain.trim() ? 'Lưu' : 'Tắt'}
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  )
}
