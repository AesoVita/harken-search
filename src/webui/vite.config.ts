import path from 'path';
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Harken Search WebUI Vite 配置
// build 时输出单文件 HTML（嵌入所有 CSS/JS），方便 C++ 通过 NavigateToString 加载
export default defineConfig({
  plugins: [
    react(),
    viteSingleFile(),  // 将 JS/CSS 内联到 HTML 中
  ],
  build: {
    outDir: 'dist',
    assetsInlineLimit: Infinity,  // 内联所有资源
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
      },
    },
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, 'src'),
    },
  },
  server: {
    port: 5173,
    host: '0.0.0.0',
  },
});
