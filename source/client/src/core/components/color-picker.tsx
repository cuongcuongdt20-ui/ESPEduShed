// Chọn màu từ bảng màu có sẵn (không nhập mã màu). Server lưu "#RRGGBB"; "" nghĩa là không đặt màu riêng.
import type { ReactNode } from 'react'
import { Check } from 'lucide-react'
import { cn } from '../lib/utils'

export const PALETTE: { value: string; name: string }[] = [
  { value: '#DC2626', name: 'Đỏ' },
  { value: '#EA580C', name: 'Cam' },
  { value: '#D97706', name: 'Vàng đậm' },
  { value: '#65A30D', name: 'Xanh nõn chuối' },
  { value: '#16A34A', name: 'Xanh lá' },
  { value: '#0D9488', name: 'Xanh ngọc' },
  { value: '#0891B2', name: 'Xanh lơ' },
  { value: '#2563EB', name: 'Xanh dương' },
  { value: '#4F46E5', name: 'Chàm' },
  { value: '#9333EA', name: 'Tím' },
  { value: '#DB2777', name: 'Hồng' },
  { value: '#57534E', name: 'Nâu xám' },
]

interface Props {
  value: string
  onChange: (color: string) => void
  // Có thì thêm ô đầu tiên "không đặt màu riêng" với nhãn này, ví dụ "Theo màu trường"
  noneLabel?: string
}

export function ColorPicker({ value, onChange, noneLabel }: Props) {
  const current = (value ?? '').toUpperCase()
  // Màu cũ nhập bằng mã, không có trong bảng: vẫn hiện để không mất khi lưu lại
  const extra = current && !PALETTE.some((c) => c.value === current) ? [{ value: current, name: 'Màu đang dùng' }] : []
  const swatch = (color: string, name: string, content?: ReactNode) => {
    const active = current === color
    return (
      <button
        key={color || 'none'}
        type="button"
        title={name}
        aria-label={name}
        aria-pressed={active}
        onClick={() => onChange(color)}
        className={cn(
          'flex h-7 w-7 items-center justify-center rounded-full border-2 transition-transform hover:scale-110',
          active ? 'border-foreground' : 'border-transparent',
        )}
        style={color ? { background: color } : undefined}
      >
        {content ?? (active && <Check className="h-4 w-4 text-white" />)}
      </button>
    )
  }
  return (
    <div className="flex flex-wrap items-center gap-1.5">
      {noneLabel !== undefined &&
        swatch('', noneLabel, (
          <span className={cn('h-full w-full rounded-full border border-dashed border-muted-foreground',
            !current && 'bg-muted')} />
        ))}
      {[...PALETTE, ...extra].map((c) => swatch(c.value, c.name))}
      <span className="ml-1 text-xs text-muted-foreground">
        {current ? ([...PALETTE, ...extra].find((c) => c.value === current)?.name ?? '') : noneLabel}
      </span>
    </div>
  )
}
