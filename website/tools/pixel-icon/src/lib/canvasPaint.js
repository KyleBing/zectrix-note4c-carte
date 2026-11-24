/**
 * 落笔工具：单像素 / 对齐偶数格的 2×2 stamp
 */

/** 单像素写入 */
export function paintPixel(pixels, w, h, x, y, color) {
  if (x < 0 || y < 0 || x >= w || y >= h) return false
  const i = y * w + x
  if (pixels[i] === color) return false
  pixels[i] = color
  return true
}

/**
 * 2×2 stamp：对齐到偶数格 (x&~1, y&~1)
 * tile 行优先 [TL, TR, BL, BR]
 */
export function stamp2x2(pixels, w, h, x, y, tile) {
  const ax = x & ~1
  const ay = y & ~1
  let changed = false
  const cells = [
    [ax, ay, tile[0]],
    [ax + 1, ay, tile[1]],
    [ax, ay + 1, tile[2]],
    [ax + 1, ay + 1, tile[3]],
  ]
  for (const [cx, cy, col] of cells) {
    if (paintPixel(pixels, w, h, cx, cy, col)) changed = true
  }
  return changed
}

/** 橡皮：2×2 或单像素填白 */
export function eraseAt(pixels, w, h, x, y, use2x2) {
  if (use2x2) return stamp2x2(pixels, w, h, x, y, [1, 1, 1, 1])
  return paintPixel(pixels, w, h, x, y, 1)
}

/**
 * 以画布中心为基点，展开对称 / 旋转副本坐标
 * mirrorH=左右 mirrorV=上下；rotateN=旋转份数（1=关闭）
 */
export function expandSymmetryPoints(x, y, w, h, { mirrorH = false, mirrorV = false, rotateN = 1 } = {}) {
  const n = Math.max(1, rotateN | 0)
  const cx = (w - 1) / 2
  const cy = (h - 1) / 2
  const map = new Map()
  const add = (px, py) => {
    const rx = Math.round(px)
    const ry = Math.round(py)
    if (rx < 0 || ry < 0 || rx >= w || ry >= h) return
    map.set(`${rx},${ry}`, [rx, ry])
  }
  for (let k = 0; k < n; k++) {
    const ang = (2 * Math.PI * k) / n
    const dx = x - cx
    const dy = y - cy
    const px = cx + dx * Math.cos(ang) - dy * Math.sin(ang)
    const py = cy + dx * Math.sin(ang) + dy * Math.cos(ang)
    const irx = Math.round(px)
    const iry = Math.round(py)
    add(irx, iry)
    if (mirrorH) add(w - 1 - irx, iry)
    if (mirrorV) add(irx, h - 1 - iry)
    if (mirrorH && mirrorV) add(w - 1 - irx, h - 1 - iry)
  }
  return [...map.values()]
}

/** 画线（Bresenham），每步调用 stampFn */
export function paintLine(x0, y0, x1, y1, stampFn) {
  let dx = Math.abs(x1 - x0)
  let dy = Math.abs(y1 - y0)
  const sx = x0 < x1 ? 1 : -1
  const sy = y0 < y1 ? 1 : -1
  let err = dx - dy
  let x = x0
  let y = y0
  for (;;) {
    stampFn(x, y)
    if (x === x1 && y === y1) break
    const e2 = 2 * err
    if (e2 > -dy) {
      err -= dy
      x += sx
    }
    if (e2 < dx) {
      err += dx
      y += sy
    }
  }
}

/** 从源复制到新尺寸，裁剪或白底填充 */
export function resizePixels(src, sw, sh, nw, nh, fill = 1) {
  const out = new Uint8Array(nw * nh)
  out.fill(fill)
  const cw = Math.min(sw, nw)
  const ch = Math.min(sh, nh)
  for (let y = 0; y < ch; y++) {
    for (let x = 0; x < cw; x++) {
      out[y * nw + x] = src[y * sw + x]
    }
  }
  return out
}

const UNDO_MAX = 30

export function createUndoStack() {
  /** @type {Uint8Array[]} */
  const stack = []
  return {
    push(pixels) {
      stack.push(new Uint8Array(pixels))
      if (stack.length > UNDO_MAX) stack.shift()
    },
    pop() {
      return stack.pop() || null
    },
    get length() {
      return stack.length
    },
    clear() {
      stack.length = 0
    },
  }
}
