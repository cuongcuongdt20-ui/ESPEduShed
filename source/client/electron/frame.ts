// Ghép khung TCP phía client (TV1): 4 byte độ dài big-endian + JSON UTF-8, tối đa 8192 byte.
// Cùng quy tắc với frame.c trên ESP32. Không phụ thuộc Electron, test được bằng node --test.

export const MAX_PAYLOAD = 8192

export function encodeFrame(obj: unknown): Buffer {
  const body = Buffer.from(JSON.stringify(obj), 'utf8')
  if (body.length === 0 || body.length > MAX_PAYLOAD) {
    throw new Error(`Khung ${body.length} byte vượt giới hạn ${MAX_PAYLOAD}`)
  }
  const head = Buffer.alloc(4)
  head.writeUInt32BE(body.length, 0)
  return Buffer.concat([head, body])
}

// TCP là luồng byte: một khung có thể đến thành nhiều lần 'data', hoặc nhiều khung trong một lần.
export class FrameDecoder {
  private buf = Buffer.alloc(0)

  // Nhận một mẩu byte bất kỳ, trả về các khung đã đủ. Ném lỗi khi độ dài sai: luồng byte đã lệch, phải đóng kết nối.
  push(chunk: Buffer): unknown[] {
    this.buf = this.buf.length ? Buffer.concat([this.buf, chunk]) : chunk
    const frames: unknown[] = []
    while (this.buf.length >= 4) {
      const n = this.buf.readUInt32BE(0)
      if (n === 0 || n > MAX_PAYLOAD) throw new Error(`Độ dài khung sai: ${n}`)
      if (this.buf.length < 4 + n) break
      frames.push(JSON.parse(this.buf.subarray(4, 4 + n).toString('utf8')))
      this.buf = this.buf.subarray(4 + n)
    }
    return frames
  }

  reset(): void {
    this.buf = Buffer.alloc(0)
  }
}
