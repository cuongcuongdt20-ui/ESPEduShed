// Trạng thái server (TV1): lấy system.status mỗi 5 giây, đo thời gian khứ hồi của system.ping.
import { useEffect, useState } from 'react'
import { api, errorText } from '../api'
import { Card, CardContent, CardHeader, CardTitle } from '../components/ui/card'

interface Status {
  name: string
  fw: string
  mode: string
  ip: string
  rssi?: number
  uptimeS: number
  timeSynced: boolean
  clients: number
  maxClients: number
  sessions: number
  heapFree?: number
  heapMin?: number
  requests: number
  errors: number
  maxHandlerUs?: number
  dataUsed?: number
  dataTotal?: number
}

const kb = (n?: number) => (n === undefined ? '—' : `${(n / 1024).toFixed(1)} KB`)

export function StatusPage() {
  const [st, setSt] = useState<Status | null>(null)
  const [rtt, setRtt] = useState<number | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    let alive = true
    async function tick() {
      try {
        const t0 = performance.now()
        await api.request('system.ping')
        const t1 = performance.now()
        const s = await api.request<Status>('system.status')
        if (!alive) return
        setRtt(t1 - t0)
        setSt(s)
        setError(null)
      } catch (e) {
        if (alive) setError(errorText(e))
      }
    }
    tick()
    const t = setInterval(tick, 5000)
    return () => {
      alive = false
      clearInterval(t)
    }
  }, [])

  const rows: [string, string][] = st
    ? [
        ['Firmware', `${st.name} ${st.fw}`],
        ['Chế độ Wi-Fi', st.mode === 'ap' ? 'AP cài đặt' : 'STA'],
        ['Địa chỉ IP', st.ip],
        ['RSSI', st.rssi === undefined ? '—' : `${st.rssi} dBm`],
        ['Thời gian chạy', `${Math.floor(st.uptimeS / 3600)} giờ ${Math.floor((st.uptimeS % 3600) / 60)} phút`],
        ['Đã đồng bộ giờ (SNTP)', st.timeSynced ? 'Có' : 'Chưa'],
        ['Kết nối TCP', `${st.clients}/${st.maxClients}`],
        ['Phiên đăng nhập', String(st.sessions)],
        ['RAM trống / thấp nhất', `${kb(st.heapFree)} / ${kb(st.heapMin)}`],
        ['Yêu cầu / lỗi', `${st.requests} / ${st.errors}`],
        ['Handler chậm nhất', st.maxHandlerUs === undefined ? '—' : `${(st.maxHandlerUs / 1000).toFixed(2)} ms`],
        ['Bộ nhớ /data', `${kb(st.dataUsed)} / ${kb(st.dataTotal)}`],
        ['Khứ hồi system.ping (đo từ app)', rtt === null ? '—' : `${rtt.toFixed(1)} ms`],
      ]
    : []

  return (
    <Card className="max-w-2xl">
      <CardHeader>
        <CardTitle>Trạng thái server</CardTitle>
      </CardHeader>
      <CardContent>
        {error && <p className="mb-3 text-sm text-destructive">{error}</p>}
        <dl className="grid grid-cols-[220px_1fr] gap-y-2 text-sm">
          {rows.map(([k, v]) => (
            <div key={k} className="contents">
              <dt className="text-muted-foreground">{k}</dt>
              <dd className="font-medium">{v}</dd>
            </div>
          ))}
        </dl>
      </CardContent>
    </Card>
  )
}
