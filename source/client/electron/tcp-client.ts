// Client TCP (TV1), chạy ở main process bằng module net của Node.js.
//  - một kết nối lâu dài tới ESP32, ghép khung bằng FrameDecoder
//  - mỗi yêu cầu có id tăng dần; phản hồi được ghép lại theo id, hết hạn sau 10 giây
//  - tin đẩy (không có id) phát qua sự kiện 'push'
//  - gửi system.ping mỗi 15 giây (server đóng kết nối im lặng quá 60 giây)
//  - mất kết nối thì tự nối lại sau 2 giây, phát sự kiện 'reconnected'
import net from 'node:net'
import { EventEmitter } from 'node:events'
import { FrameDecoder, encodeFrame } from './frame'
import type { ApiErrorData, ConnState } from './shared'

const REQUEST_TIMEOUT_MS = 10000      // đủ cho mạng di động chậm khi vào từ ngoài qua DDNS
const PING_INTERVAL_MS = 15000
const RECONNECT_DELAY_MS = 2000

export class ApiError extends Error {
  readonly info: ApiErrorData

  constructor(info: ApiErrorData) {
    super(info.message)
    this.info = info
  }
}

interface Pending {
  resolve: (data: any) => void
  reject: (err: ApiError) => void
  timer: NodeJS.Timeout
  cmd: string
}

export class TcpClient extends EventEmitter {
  host: string | null = null
  port: number | null = null
  token: string | null = null
  state: ConnState = 'offline'

  private sock: net.Socket | null = null
  private decoder = new FrameDecoder()
  private nextId = 1
  private pending = new Map<number, Pending>()
  private pingTimer: NodeJS.Timeout | null = null
  private reconnectTimer: NodeJS.Timeout | null = null
  private wanted = false               // người dùng muốn giữ kết nối (false sau disconnect())
  private everConnected = false

  // Kết nối tới host:port. Promise xong khi kết nối lần đầu thành công; lỗi thì không tự nối lại.
  connect(host: string, port: number): Promise<void> {
    this.disconnect()
    this.host = host
    this.port = port
    this.wanted = true
    this.everConnected = false
    return new Promise((resolve, reject) => {
      this.open(resolve, reject)
    })
  }

  disconnect(): void {
    this.wanted = false
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer)
    this.reconnectTimer = null
    const sock = this.sock
    this.sock = null                     // onClose của socket cũ sẽ bỏ qua, nên dọn dẹp ngay tại đây
    this.stopPing()
    this.failPending('Đã ngắt kết nối')
    sock?.destroy()
    this.setState('offline')
  }

  private failPending(message: string): void {
    for (const [id, p] of this.pending) {
      clearTimeout(p.timer)
      p.reject(new ApiError({ code: 'OFFLINE', message, field: null }))
      this.pending.delete(id)
    }
  }

  async request<T = any>(cmd: string, data?: object, timeoutMs = REQUEST_TIMEOUT_MS): Promise<T> {
    if (!this.sock || this.state !== 'online') {
      throw new ApiError({ code: 'OFFLINE', message: 'Chưa kết nối tới server', field: null })
    }
    const id = this.nextId++
    const msg: Record<string, unknown> = { id, cmd }
    if (this.token) msg.token = this.token
    if (data !== undefined) msg.data = data
    const frame = encodeFrame(msg)
    return new Promise<T>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id)
        reject(new ApiError({ code: 'TIMEOUT', message: `Server không trả lời ${cmd} sau ${timeoutMs / 1000} giây`, field: null }))
      }, timeoutMs)
      this.pending.set(id, { resolve, reject, timer, cmd })
      this.sock!.write(frame)
    })
  }

  private open(onFirst?: () => void, onFirstError?: (e: Error) => void): void {
    this.setState('connecting')
    this.decoder.reset()
    const sock = net.createConnection({ host: this.host!, port: this.port! })
    this.sock = sock
    sock.setNoDelay(true)                       // tin nhỏ: gửi ngay, không gom
    sock.setKeepAlive(true, 30000)

    sock.once('connect', () => {
      const reconnect = this.everConnected
      this.everConnected = true
      this.setState('online')
      this.startPing()
      if (reconnect) this.emit('reconnected')
      onFirst?.()
    })
    sock.on('data', (chunk) => this.onData(chunk))
    sock.on('error', (err) => {
      if (!this.everConnected && onFirstError) {
        onFirstError(new ApiError({ code: 'OFFLINE', message: `Không kết nối được ${this.host}:${this.port} (${err.message})`, field: null }))
        onFirstError = undefined
        this.wanted = false
      }
    })
    sock.on('close', () => this.onClose(sock))
  }

  private onData(chunk: Buffer): void {
    let frames: unknown[]
    try {
      frames = this.decoder.push(chunk)
    } catch {
      this.sock?.destroy()                       // luồng byte đã lệch: đóng để nối lại từ đầu
      return
    }
    for (const f of frames as any[]) {
      if (f && typeof f.push === 'string') {
        this.emit('push', f)
        continue
      }
      const p = f && this.pending.get(f.id)
      if (!p) {
        // id 0: server từ chối kết nối (BUSY) trước khi nhận yêu cầu nào
        if (f?.ok === false && f.error) this.emit('server-error', f.error)
        continue
      }
      clearTimeout(p.timer)
      this.pending.delete(f.id)
      if (f.ok) p.resolve(f.data ?? {})
      else p.reject(new ApiError({ code: f.error?.code ?? 'INTERNAL', message: f.error?.message ?? '', field: f.error?.field ?? null, data: f.data }))
    }
  }

  private onClose(sock: net.Socket): void {
    if (this.sock !== sock) return
    this.sock = null
    this.stopPing()
    this.failPending('Mất kết nối tới server')
    this.setState('offline')
    if (this.wanted && this.everConnected) {
      this.reconnectTimer = setTimeout(() => {
        this.reconnectTimer = null
        if (this.wanted) this.open()
      }, RECONNECT_DELAY_MS)
    }
  }

  private startPing(): void {
    this.stopPing()
    this.pingTimer = setInterval(() => {
      this.request('system.ping').catch(() => {})   // không có phản hồi: TCP keep-alive và 'close' sẽ xử lý
    }, PING_INTERVAL_MS)
  }

  private stopPing(): void {
    if (this.pingTimer) clearInterval(this.pingTimer)
    this.pingTimer = null
  }

  private setState(s: ConnState): void {
    if (this.state === s) return
    this.state = s
    this.emit('state', s)
  }
}
