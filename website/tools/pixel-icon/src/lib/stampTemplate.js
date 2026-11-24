import { layoutBg } from './bgLayout.js'

/** 与 tools/rasterize_icons.py nearest 一致的四色量化 */
export function nearestBwry(r, g, b, a) {
  if (a < 40) return 1
  if (r > 180 && g > 140 && b < 90) return 2 // 黄
  if (r > 180 && g < 100 && b < 100) return 3 // 红
  const luma = 0.299 * r + 0.587 * g + 0.114 * b
  return luma < 140 ? 0 : 1
}

/**
 * 按当前模板在绘制区的位置/缩放，采样并量化为四色像素
 * @returns {Uint8Array} 与画布同尺寸的色索引
 */
export function stampTemplateToPixels(img, w, h, cell, scale, bgX, bgY) {
  const out = new Uint8Array(w * h)
  out.fill(1)
  if (!img?.naturalWidth) return out

  // 源图拉到离屏 canvas 便于取色
  const src = document.createElement('canvas')
  src.width = img.naturalWidth
  src.height = img.naturalHeight
  const sctx = src.getContext('2d', { willReadFrequently: true })
  sctx.drawImage(img, 0, 0)
  const srcData = sctx.getImageData(0, 0, src.width, src.height).data
  const nw = src.width
  const nh = src.height

  const L = layoutBg(img, w, h, cell, scale, bgX, bgY)
  const { ox, oy, dw, dh } = L
  if (!dw || !dh) return out

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      // 逻辑像素中心 → CSS
      const cx = (x + 0.5) * cell
      const cy = (y + 0.5) * cell
      const u = (cx - ox) / dw
      const v = (cy - oy) / dh
      if (u < 0 || v < 0 || u >= 1 || v >= 1) {
        out[y * w + x] = 1
        continue
      }
      const sx = Math.min(nw - 1, Math.max(0, Math.floor(u * nw)))
      const sy = Math.min(nh - 1, Math.max(0, Math.floor(v * nh)))
      const i = (sy * nw + sx) * 4
      out[y * w + x] = nearestBwry(
        srcData[i],
        srcData[i + 1],
        srcData[i + 2],
        srcData[i + 3],
      )
    }
  }
  return out
}
