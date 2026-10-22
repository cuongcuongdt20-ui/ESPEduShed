// Cấu hình build (TV1): main process (electron/main.ts), preload (electron/preload.ts), giao diện React (src/).
import { resolve } from 'node:path'
import { defineConfig, externalizeDepsPlugin } from 'electron-vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  main: {
    plugins: [externalizeDepsPlugin()],
    build: { rollupOptions: { input: { index: resolve(__dirname, 'electron/main.ts') } } },
  },
  preload: {
    plugins: [externalizeDepsPlugin()],
    build: { rollupOptions: { input: { index: resolve(__dirname, 'electron/preload.ts') } } },
  },
  renderer: {
    root: '.',
    resolve: { alias: { '@': resolve(__dirname, 'src') } },
    plugins: [react()],
    build: { rollupOptions: { input: { index: resolve(__dirname, 'index.html') } } },
  },
})
