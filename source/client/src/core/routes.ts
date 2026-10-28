// Kiểu route mà mỗi module khai báo trong <module>/routes.tsx (TV1). Thêm trang mới không cần sửa App.tsx.
import type { ComponentType, ReactElement } from 'react'

export interface ModuleRoute {
  path: string                           // ví dụ "/schools", "/schools/:id/periods"
  title: string                          // tiêu đề trên thanh bên
  element: ReactElement
  icon?: ComponentType<{ className?: string }>
  menu?: boolean                         // hiện trên thanh bên
  order?: number                         // thứ tự trên thanh bên (nhỏ đứng trước)
}
