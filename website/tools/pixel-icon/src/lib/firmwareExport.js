/**
 * 与 tools/rasterize_icons.py pack_row / icon_bitmaps.h 一致的 2bpp 打包
 * 色：0 黑 / 1 白 / 2 黄 / 3 红；每字节 4 像素，高位在前
 */

/** 单行像素 → 字节（不足 4 的倍数用 padColor 填） */
export function packRow(pixels, padColor = 1) {
  const w = pixels.length
  const bpr = Math.ceil(w / 4)
  const padByte = padColor === 0 ? 0x00 : 0x55
  const out = new Array(bpr).fill(padByte)
  for (let x = 0; x < w; x++) {
    let v = pixels[x]
    if (v == null) v = padColor
    const byteI = x >> 2
    const shift = 6 - 2 * (x & 3)
    out[byteI] = (out[byteI] & ~(0x03 << shift)) | ((v & 3) << shift)
  }
  return out
}

/** 整图像素索引 → 2bpp 字节数组 */
export function packBitmap(pixels, w, h, padColor = 1) {
  const rows = []
  for (let y = 0; y < h; y++) {
    const row = []
    for (let x = 0; x < w; x++) row.push(pixels[y * w + x] & 3)
    rows.push(...packRow(row, padColor))
  }
  return rows
}

/** 图标名 → C 标识符（Pascal 风格后缀） */
export function toCIdent(name) {
  const raw = String(name || 'Icon')
    .replace(/[^a-zA-Z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '')
  const parts = (raw || 'Icon').split('_').filter(Boolean)
  const pascal = parts
    .map((p) => p.charAt(0).toUpperCase() + p.slice(1))
    .join('')
  const body = /^[0-9]/.test(pascal) ? `Icon${pascal}` : pascal
  return `k${body}`
}

function formatBytes(data, per = 16) {
  const lines = []
  for (let i = 0; i < data.length; i += per) {
    const chunk = data
      .slice(i, i + per)
      .map((b) => `0x${b.toString(16).toUpperCase().padStart(2, '0')}`)
      .join(', ')
    lines.push(`  ${chunk},`)
  }
  return lines.join('\n')
}

/**
 * 导出单个图标为可 include 的 .h 片段（含 PROGMEM）
 * @param {{ name: string, w: number, h: number, pixels: Uint8Array|number[] }} icon
 * @param {{ padColor?: number, blackBg?: boolean }} opts
 */
export function iconToHeader(icon, opts = {}) {
  const padColor = opts.blackBg ? 0 : (opts.padColor ?? 1)
  const ident = toCIdent(icon.name)
  const data = packBitmap(icon.pixels, icon.w, icon.h, padColor)
  const wName = `${ident}_W`
  const hName = `${ident}_H`
  return [
    `// Pixel icon export — 2bpp BWRY, same packing as tools/rasterize_icons.py`,
    `#pragma once`,
    `#include <Arduino.h>`,
    `#include <pgmspace.h>`,
    ``,
    `static constexpr int ${wName} = ${icon.w};`,
    `static constexpr int ${hName} = ${icon.h};`,
    // 方图时兼容现有 blit2bpp(src) 用法
    icon.w === icon.h
      ? `static constexpr int ${ident}_SRC = ${icon.w};`
      : `// 非正方形：绘制时请分别使用 ${wName} / ${hName}`,
    ``,
    `static const uint8_t ${ident}[] PROGMEM = {`,
    formatBytes(data),
    `};`,
    ``,
  ]
    .filter((line) => line !== false)
    .join('\n')
}

/** 多个图标合并为一个 .h */
export function iconsToHeader(icons, opts = {}) {
  const padColor = opts.blackBg ? 0 : (opts.padColor ?? 1)
  const parts = [
    `// Pixel icon export — 2bpp BWRY, same packing as tools/rasterize_icons.py`,
    `#pragma once`,
    `#include <Arduino.h>`,
    `#include <pgmspace.h>`,
    ``,
  ]
  const names = []
  for (const icon of icons) {
    const ident = toCIdent(icon.name)
    names.push(ident)
    const data = packBitmap(icon.pixels, icon.w, icon.h, padColor)
    parts.push(`static constexpr int ${ident}_W = ${icon.w};`)
    parts.push(`static constexpr int ${ident}_H = ${icon.h};`)
    if (icon.w === icon.h) {
      parts.push(`static constexpr int ${ident}_SRC = ${icon.w};`)
    }
    parts.push(`static const uint8_t ${ident}[] PROGMEM = {`)
    parts.push(formatBytes(data))
    parts.push(`};`)
    parts.push(``)
  }
  if (names.length > 1) {
    parts.push(`static const uint8_t *const kPixelIconBits[] = {`)
    for (const n of names) parts.push(`  ${n},`)
    parts.push(`};`)
    parts.push(``)
  }
  return parts.join('\n')
}

export function downloadText(filename, text) {
  const blob = new Blob([text], { type: 'text/plain;charset=utf-8' })
  const a = document.createElement('a')
  a.href = URL.createObjectURL(blob)
  a.download = filename
  a.click()
  URL.revokeObjectURL(a.href)
}
