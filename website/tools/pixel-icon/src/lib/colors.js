// 与相册编辑器对齐的 BWRY 四色
export const PAL = [
  { id: 0, name: '黑', hex: '#111111' },
  { id: 1, name: '白', hex: '#ffffff' },
  { id: 2, name: '黄', hex: '#F6C749' },
  { id: 3, name: '红', hex: '#8b2010' },
]

export const SIZE_PRESETS = [16, 24, 32, 48, 50, 64, 90]

/** @typedef {[number, number, number, number]} Tile2x2  行优先：TL TR BL BR */

/** 生成常用二色 2×2 抖动预设 */
export function buildPresetTiles() {
  /** @type {{ id: string, label: string, tile: Tile2x2 }[]} */
  const out = []
  for (let a = 0; a < 4; a++) {
    for (let b = a + 1; b < 4; b++) {
      const na = PAL[a].name
      const nb = PAL[b].name
      out.push(
        { id: `chk-${a}-${b}`, label: `${na}/${nb} 棋盘`, tile: [a, b, b, a] },
        { id: `v-${a}-${b}`, label: `${na}/${nb} 竖条`, tile: [a, b, a, b] },
        { id: `h-${a}-${b}`, label: `${na}/${nb} 横条`, tile: [a, a, b, b] },
        { id: `chk2-${a}-${b}`, label: `${nb}/${na} 棋盘`, tile: [b, a, a, b] },
      )
    }
  }
  // 纯色 2×2（等同加粗单色）
  for (let c = 0; c < 4; c++) {
    out.push({
      id: `solid2-${c}`,
      label: `${PAL[c].name} 2×2`,
      tile: [c, c, c, c],
    })
  }
  return out
}

export const PRESET_TILES = buildPresetTiles()

/** 把色索引数组画到 canvas（最近邻） */
export function pixelsToImageData(pixels, w, h, scale = 1) {
  const sw = w * scale
  const sh = h * scale
  const data = new Uint8ClampedArray(sw * sh * 4)
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const ink = pixels[y * w + x] & 3
      const { hex } = PAL[ink]
      const r = parseInt(hex.slice(1, 3), 16)
      const g = parseInt(hex.slice(3, 5), 16)
      const b = parseInt(hex.slice(5, 7), 16)
      for (let dy = 0; dy < scale; dy++) {
        for (let dx = 0; dx < scale; dx++) {
          const o = ((y * scale + dy) * sw + (x * scale + dx)) * 4
          data[o] = r
          data[o + 1] = g
          data[o + 2] = b
          data[o + 3] = 255
        }
      }
    }
  }
  return new ImageData(data, sw, sh)
}

/** 在离屏 canvas 上渲染缩略图，返回 dataURL */
export function pixelsToDataUrl(pixels, w, h, scale = 4) {
  const c = document.createElement('canvas')
  c.width = w * scale
  c.height = h * scale
  const ctx = c.getContext('2d')
  ctx.putImageData(pixelsToImageData(pixels, w, h, scale), 0, 0)
  return c.toDataURL('image/png')
}

/** 导出 PNG Blob（可选放大倍数） */
export function exportPngBlob(pixels, w, h, scale = 1) {
  const c = document.createElement('canvas')
  c.width = w * scale
  c.height = h * scale
  const ctx = c.getContext('2d')
  ctx.putImageData(pixelsToImageData(pixels, w, h, scale), 0, 0)
  return new Promise((resolve) => c.toBlob((b) => resolve(b), 'image/png'))
}

/** 渲染 2×2 tile 为 CSS 四格背景（用于色盘按钮） */
export function tileCss(tile) {
  const [tl, tr, bl, br] = tile
  return {
    background: `
      linear-gradient(to right, ${PAL[tl].hex} 50%, ${PAL[tr].hex} 50%) top / 100% 50% no-repeat,
      linear-gradient(to right, ${PAL[bl].hex} 50%, ${PAL[br].hex} 50%) bottom / 100% 50% no-repeat
    `,
  }
}
