/** 背景图布局：scale=1 时短边贴合画布短边；最大建议 20 */

export const BG_SCALE_MIN = 0.05
export const BG_SCALE_MAX = 20

export function clampBgScale(n) {
  return Math.max(BG_SCALE_MIN, Math.min(BG_SCALE_MAX, n))
}

/**
 * @returns {{ ox: number, oy: number, dw: number, dh: number, cw: number, ch: number }}
 * ox/oy/dw/dh 为 CSS 像素；bgX/bgY 为逻辑像素（÷ cellSize）
 */
export function layoutBg(img, w, h, cell, scale, bgX, bgY) {
  const cw = w * cell
  const ch = h * cell
  if (!img?.naturalWidth) {
    return { ox: 0, oy: 0, dw: 0, dh: 0, cw, ch }
  }
  const base = Math.min(cw, ch)
  const k =
    clampBgScale(scale) *
    (base / Math.min(img.naturalWidth, img.naturalHeight))
  const dw = img.naturalWidth * k
  const dh = img.naturalHeight * k
  return {
    ox: bgX * cell,
    oy: bgY * cell,
    dw,
    dh,
    cw,
    ch,
  }
}

/** scale=1 时居中放置的 bgX/bgY */
export function centeredBgPos(img, w, h, cell, scale = 1) {
  const { dw, dh } = layoutBg(img, w, h, cell, scale, 0, 0)
  return {
    bgX: (w * cell - dw) / 2 / cell,
    bgY: (h * cell - dh) / 2 / cell,
  }
}

/**
 * 以 CSS 锚点 (ax, ay) 为中心改缩放，返回新的 bgX/bgY
 * 保证锚点下的图像内容不动
 */
export function zoomAbout(img, w, h, cell, oldScale, newScale, bgX, bgY, ax, ay) {
  const oldL = layoutBg(img, w, h, cell, oldScale, bgX, bgY)
  const ns = clampBgScale(newScale)
  if (!oldL.dw || !oldL.dh) {
    const c = centeredBgPos(img, w, h, cell, ns)
    return { scale: ns, bgX: c.bgX, bgY: c.bgY }
  }
  const u = (ax - oldL.ox) / oldL.dw
  const v = (ay - oldL.oy) / oldL.dh
  const neu = layoutBg(img, w, h, cell, ns, 0, 0)
  const newOx = ax - u * neu.dw
  const newOy = ay - v * neu.dh
  return {
    scale: ns,
    bgX: newOx / cell,
    bgY: newOy / cell,
  }
}

/** 模板与画布相交区域的中心（CSS），用作缩放锚点 */
export function templateVisibleCenter(img, w, h, cell, scale, bgX, bgY) {
  const L = layoutBg(img, w, h, cell, scale, bgX, bgY)
  const { ox, oy, dw, dh, cw, ch } = L
  if (!dw || !dh) return { ax: cw / 2, ay: ch / 2 }
  const left = Math.max(0, ox)
  const top = Math.max(0, oy)
  const right = Math.min(cw, ox + dw)
  const bottom = Math.min(ch, oy + dh)
  if (right <= left || bottom <= top) {
    return { ax: ox + dw / 2, ay: oy + dh / 2 }
  }
  return { ax: (left + right) / 2, ay: (top + bottom) / 2 }
}
