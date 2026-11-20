import { existsSync, readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vitepress'

const rootDir = resolve(dirname(fileURLToPath(import.meta.url)), '../..')
const versionHPath = resolve(rootDir, 'firmware/include/app_version.h')
const versionH = existsSync(versionHPath) ? readFileSync(versionHPath, 'utf-8') : ''
const pick = (key: string, fallback = '') =>
  versionH.match(new RegExp(`${key}\\s*=\\s*"([^"]+)"`))?.[1] ?? fallback

const APP_VERSION = pick('APP_VERSION', '0.1.0')
const APP_UPDATE_TIME = pick('APP_UPDATE_TIME', '')
const APP_AUTHOR = pick('APP_AUTHOR', 'KyleBing')
const DOC_VERSION = `v${APP_VERSION}`

// GitHub Pages project site base
const SITE_ROOT = process.env.GITHUB_ACTIONS ? '/zectrix-note4c-display' : ''
const base = SITE_ROOT ? `${SITE_ROOT}/` : '/'

export default defineConfig({
  title: 'Carte',
  description: 'Carte · ZECTRIX NOTE4C 四色墨水屏开放固件与硬件文档',
  base,
  outDir: '.vitepress/dist',
  lastUpdated: true,
  cleanUrls: true,
  lang: 'zh-CN',

  vite: {
    server: { port: 3124 },
    preview: { port: 3124 },
  },

  head: [
    ['meta', { name: 'theme-color', content: '#111827' }],
    ['link', { rel: 'icon', type: 'image/svg+xml', href: `${base}assets/favicon.svg` }],
    ['link', { rel: 'apple-touch-icon', href: `${base}assets/favicon.svg` }],
  ],

  markdown: {
    config(md) {
      const render = md.render.bind(md)
      md.render = (src, env) =>
        render(
          src
            .replaceAll('{{APP_VERSION}}', APP_VERSION)
            .replaceAll('{{APP_UPDATE_TIME}}', APP_UPDATE_TIME)
            .replaceAll('{{APP_AUTHOR}}', APP_AUTHOR)
            .replaceAll('{{DOC_VERSION}}', DOC_VERSION),
          env,
        )
    },
  },

  themeConfig: {
    logo: '/assets/favicon.svg',
    siteTitle: 'Carte',
    socialLinks: [
      { icon: 'github', link: 'https://github.com/KyleBing/zectrix-note4c-display' },
    ],
    search: { provider: 'local' },
    footer: {
      message: 'Carte · ZECTRIX NOTE4C 四色墨水屏开放固件',
      copyright: `${DOC_VERSION} · ${APP_AUTHOR}`,
    },
    nav: [
      { text: '首页', link: '/' },
      { text: '固件功能', link: '/features/' },
      { text: '从零开发', link: '/guide/getting-started' },
      {
        text: '硬件',
        items: [
          { text: '硬件参数', link: '/hardware/note4c' },
          { text: '硬件速览', link: '/hardware/overview' },
        ],
      },
      {
        text: DOC_VERSION,
        items: [
          {
            text: 'Releases',
            link: 'https://github.com/KyleBing/zectrix-note4c-display/releases',
          },
        ],
      },
    ],
    sidebar: [
      {
        text: '开始',
        items: [
          { text: '简介', link: '/' },
          { text: '固件功能', link: '/features/' },
          { text: '从零开发', link: '/guide/getting-started' },
        ],
      },
      {
        text: '硬件',
        items: [
          { text: '硬件参数（完整）', link: '/hardware/note4c' },
          { text: '硬件速览', link: '/hardware/overview' },
        ],
      },
    ],
    editLink: {
      pattern: 'https://github.com/KyleBing/zectrix-note4c-display/edit/main/docs/:path',
      text: '在 GitHub 上编辑此页',
    },
  },
})
