// UDP broadcast discovery for ESP32-S3. Keep this API stable for the connection screen.
import { createSocket, type RemoteInfo, type Socket } from 'node:dgram'
import { networkInterfaces } from 'node:os'
import type { ServerInfo } from './shared'

const PORT = 5001
const REQUEST = Buffer.from('ESPEDUSHED_DISCOVER', 'utf8')

function broadcasts(): string[] {
  const result = new Set<string>(['255.255.255.255'])
  for (const addresses of Object.values(networkInterfaces())) {
    for (const a of addresses ?? []) {
      if (a.family !== 'IPv4' || a.internal || !a.netmask) continue
      const ip = a.address.split('.').map(Number)
      const mask = a.netmask.split('.').map(Number)
      if (ip.length !== 4 || mask.length !== 4 || [...ip, ...mask].some(n => !Number.isInteger(n) || n < 0 || n > 255)) continue
      result.add(ip.map((v, i) => (v | (~mask[i] & 255))).join('.'))
    }
  }
  return [...result]
}

export function discover(timeoutMs = 1500): Promise<ServerInfo[]> {
  return new Promise(resolve => {
    const socket: Socket = createSocket({ type: 'udp4', reuseAddr: true })
    const found = new Map<string, ServerInfo>()
    let settled = false
    const finish = () => {
      if (settled) return
      settled = true
      try { socket.close() } catch { /* already closed */ }
      resolve([...found.values()])
    }
    socket.on('message', (message: Buffer, remote: RemoteInfo) => {
      try {
        const parsed = JSON.parse(message.toString('utf8')) as Partial<ServerInfo>
        if (typeof parsed.name !== 'string' || typeof parsed.ip !== 'string' || !Number.isInteger(parsed.port) || typeof parsed.fw !== 'string') return
        // ESP chưa có IP thì gửi "0.0.0.0": dùng địa chỉ nguồn của gói UDP, là địa chỉ thật đã trả lời
        const claimed = parsed.ip.trim()
        const ip = claimed && claimed !== '0.0.0.0' ? claimed : remote.address
        if (!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip)) return
        found.set(ip, { name: parsed.name, ip, port: parsed.port!, fw: parsed.fw })
      } catch { /* ignore malformed or unrelated UDP packets */ }
    })
    socket.on('error', finish)
    socket.bind(0, '0.0.0.0', () => {
      try { socket.setBroadcast(true) } catch { finish(); return }
      for (const address of broadcasts()) socket.send(REQUEST, PORT, address, () => {})
      setTimeout(finish, Math.max(100, Math.min(timeoutMs, 10000))).unref()
    })
  })
}
