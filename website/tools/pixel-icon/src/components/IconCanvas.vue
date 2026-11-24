<script setup>
import { ref, computed, watch, onMounted, onBeforeUnmount } from 'vue'
import { pixelsToImageData, PAL } from '../lib/colors.js'
import {
  paintPixel,
  stamp2x2,
  eraseAt,
  paintLine,
  createUndoStack,
  expandSymmetryPoints,
} from '../lib/canvasPaint.js'
import { activeIcon, commitPixels } from '../lib/iconStore.js'
import { layoutBg, zoomAbout, clampBgScale, templateVisibleCenter } from '../lib/bgLayout.js'
import { stampTemplateToPixels } from '../lib/stampTemplate.js'

const props = defineProps({
  tool: { type: String, default: 'brush' },
  brushMode: { type: String, default: 'solid' },
  solidColor: { type: Number, default: 0 },
  tile: { type: Array, default: () => [0, 0, 0, 0] },
  cellSize: { type: Number, default: 14 },
  showGrid: { type: Boolean, default: true },
  bgImage: { type: Object, default: null },
  bgOpacity: { type: Number, default: 0.45 },
  bgScale: { type: Number, default: 1 },
  bgX: { type: Number, default: 0 },
  bgY: { type: Number, default: 0 },
  bgDrag: { type: Boolean, default: false },
  whiteTransparent: { type: Boolean, default: true },
  // 绘制层透明度（临摹时调低以透出模板）
  paintOpacity: { type: Number, default: 1 },
  // 空格拖动画布时禁止绘制
  spaceHeld: { type: Boolean, default: false },
  // 对称 / 旋转辅助（以画布中心为基点）
  mirrorH: { type: Boolean, default: false },
  mirrorV: { type: Boolean, default: false },
  rotateN: { type: Number, default: 1 },
})

const emit = defineEmits([
  'eyedrop',
  'undo-state',
  'update:bgScale',
  'update:bgX',
  'update:bgY',
])

const canvasRef = ref(null)
const overlayRef = ref(null)
const localPixels = ref(null)
const undo = createUndoStack()
let drawing = false
let lastX = -1
let lastY = -1
let strokeDirty = false

const draggingBg = ref(false)
let dragStartClientX = 0
let dragStartClientY = 0
let dragStartBgX = 0
let dragStartBgY = 0

// 避免外部改 scale 时与内部锚点缩放互相抢
let applyingZoom = false
let lastScale = props.bgScale

const canvasCursor = computed(() => {
  if (props.spaceHeld) return 'grab'
  if (props.bgDrag && props.bgImage) {
    return draggingBg.value ? 'grabbing' : 'grab'
  }
  return 'crosshair'
})

function syncFromStore() {
  const icon = activeIcon.value
  if (!icon) return
  localPixels.value = new Uint8Array(icon.pixels)
  undo.clear()
  emit('undo-state', 0)
  redraw()
}

function pushUndo() {
  if (!localPixels.value) return
  undo.push(localPixels.value)
  emit('undo-state', undo.length)
}

function undoOnce() {
  const prev = undo.pop()
  if (!prev || !localPixels.value) return
  localPixels.value = new Uint8Array(prev)
  commitPixels(localPixels.value)
  emit('undo-state', undo.length)
  redraw()
}

/** 清空画板为白（可撤销） */
function clearBoard() {
  const icon = activeIcon.value
  if (!icon || !localPixels.value) return
  pushUndo()
  localPixels.value.fill(1)
  commitPixels(localPixels.value)
  redraw()
}

function drawBackground(ctx, w, h, s) {
  const img = props.bgImage
  if (!img || !img.complete || !img.naturalWidth) return
  const { ox, oy, dw, dh } = layoutBg(
    img,
    w,
    h,
    s,
    props.bgScale,
    props.bgX,
    props.bgY,
  )
  ctx.save()
  ctx.globalAlpha = Math.max(0, Math.min(1, props.bgOpacity))
  ctx.imageSmoothingEnabled = true
  ctx.drawImage(img, ox, oy, dw, dh)
  ctx.restore()
}

function drawPixels(ctx, w, h, s) {
  const px = localPixels.value
  const alpha = Math.max(0, Math.min(1, props.paintOpacity))
  ctx.save()
  ctx.globalAlpha = alpha
  // 有模板且白透出：只画非白，便于临摹
  if (props.whiteTransparent && props.bgImage) {
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        const ink = px[y * w + x] & 3
        if (ink === 1) continue
        ctx.fillStyle = PAL[ink].hex
        ctx.fillRect(x * s, y * s, s, s)
      }
    }
    ctx.restore()
    return
  }
  const tiny = document.createElement('canvas')
  tiny.width = w
  tiny.height = h
  tiny.getContext('2d').putImageData(pixelsToImageData(px, w, h, 1), 0, 0)
  ctx.imageSmoothingEnabled = false
  ctx.drawImage(tiny, 0, 0, w * s, h * s)
  ctx.restore()
}

function redraw() {
  const icon = activeIcon.value
  const cv = canvasRef.value
  const ov = overlayRef.value
  if (!icon || !cv || !localPixels.value) return
  const { w, h } = icon
  const s = props.cellSize
  cv.width = w * s
  cv.height = h * s
  const ctx = cv.getContext('2d')
  ctx.clearRect(0, 0, cv.width, cv.height)
  // 有模板且绘制层半透明/白透出时用浅底，避免白底挡模板
  if (props.bgImage && (props.whiteTransparent || props.paintOpacity < 1)) {
    ctx.fillStyle = '#d8d4ca'
    ctx.fillRect(0, 0, cv.width, cv.height)
  } else {
    ctx.fillStyle = '#ffffff'
    ctx.fillRect(0, 0, cv.width, cv.height)
  }
  drawBackground(ctx, w, h, s)
  drawPixels(ctx, w, h, s)

  if (ov) {
    ov.width = cv.width
    ov.height = cv.height
    const octx = ov.getContext('2d')
    octx.clearRect(0, 0, ov.width, ov.height)
    if (props.showGrid && s >= 8) {
      octx.strokeStyle = 'rgba(0,0,0,0.12)'
      octx.lineWidth = 1
      octx.beginPath()
      for (let x = 0; x <= w; x++) {
        octx.moveTo(x * s + 0.5, 0)
        octx.lineTo(x * s + 0.5, h * s)
      }
      for (let y = 0; y <= h; y++) {
        octx.moveTo(0, y * s + 0.5)
        octx.lineTo(w * s, y * s + 0.5)
      }
      octx.stroke()
    }
    // 对称/旋转开启时画中心十字
    const symOn =
      props.mirrorH || props.mirrorV || (props.rotateN | 0) > 1
    if (symOn) {
      const cx = (w / 2) * s
      const cy = (h / 2) * s
      octx.strokeStyle = 'rgba(200, 40, 20, 0.55)'
      octx.lineWidth = 1
      octx.setLineDash([4, 3])
      octx.beginPath()
      octx.moveTo(cx + 0.5, 0)
      octx.lineTo(cx + 0.5, h * s)
      octx.moveTo(0, cy + 0.5)
      octx.lineTo(w * s, cy + 0.5)
      octx.stroke()
      octx.setLineDash([])
      octx.fillStyle = 'rgba(200, 40, 20, 0.7)'
      octx.fillRect(cx - 2, cy - 2, 4, 4)
    }
  }
}

/** 缩放：默认以模板在可绘制区域内的中心为锚点 */
function applyScaleAbout(newScale, ax, ay) {
  const icon = activeIcon.value
  const img = props.bgImage
  if (!icon || !img) return
  const s = props.cellSize
  let anchorX = ax
  let anchorY = ay
  if (anchorX == null || anchorY == null) {
    const c = templateVisibleCenter(
      img,
      icon.w,
      icon.h,
      s,
      props.bgScale,
      props.bgX,
      props.bgY,
    )
    anchorX = c.ax
    anchorY = c.ay
  }
  const next = zoomAbout(
    img,
    icon.w,
    icon.h,
    s,
    props.bgScale,
    newScale,
    props.bgX,
    props.bgY,
    anchorX,
    anchorY,
  )
  applyingZoom = true
  emit('update:bgScale', next.scale)
  emit('update:bgX', next.bgX)
  emit('update:bgY', next.bgY)
  lastScale = next.scale
  queueMicrotask(() => {
    applyingZoom = false
  })
}

/** 按当前模板在绘制区的样子排印为四色像素 */
function stampTemplate() {
  const icon = activeIcon.value
  const img = props.bgImage
  if (!icon || !img || !localPixels.value) return false
  pushUndo()
  const stamped = stampTemplateToPixels(
    img,
    icon.w,
    icon.h,
    props.cellSize,
    props.bgScale,
    props.bgX,
    props.bgY,
  )
  localPixels.value = stamped
  commitPixels(stamped)
  redraw()
  return true
}

function ptrToCell(e) {
  const cv = canvasRef.value
  const icon = activeIcon.value
  if (!cv || !icon) return null
  const r = cv.getBoundingClientRect()
  const x = Math.floor(((e.clientX - r.left) / r.width) * icon.w)
  const y = Math.floor(((e.clientY - r.top) / r.height) * icon.h)
  if (x < 0 || y < 0 || x >= icon.w || y >= icon.h) return null
  return [x, y]
}

function paintOne(px, w, h, x, y) {
  if (props.tool === 'erase') {
    return eraseAt(px, w, h, x, y, props.brushMode === 'tile')
  }
  if (props.brushMode === 'tile') {
    return stamp2x2(px, w, h, x, y, props.tile)
  }
  return paintPixel(px, w, h, x, y, props.solidColor)
}

function applyAt(x, y) {
  const icon = activeIcon.value
  const px = localPixels.value
  if (!icon || !px) return false
  const pts = expandSymmetryPoints(x, y, icon.w, icon.h, {
    mirrorH: props.mirrorH,
    mirrorV: props.mirrorV,
    rotateN: props.rotateN,
  })
  let changed = false
  for (const [px_, py_] of pts) {
    if (paintOne(px, icon.w, icon.h, px_, py_)) changed = true
  }
  return changed
}

let activePointerId = null

function pointerInCanvas(e) {
  const cv = canvasRef.value
  if (!cv) return false
  const r = cv.getBoundingClientRect()
  return (
    e.clientX >= r.left &&
    e.clientX < r.right &&
    e.clientY >= r.top &&
    e.clientY < r.bottom
  )
}

function releasePointer(e) {
  const cv = canvasRef.value
  if (cv && activePointerId != null) {
    try {
      if (cv.hasPointerCapture?.(activePointerId)) {
        cv.releasePointerCapture(activePointerId)
      }
    } catch {
      /* ignore */
    }
  }
  activePointerId = null
}

function onPointerDown(e) {
  // 空格平移由外层处理
  if (props.spaceHeld) return

  if (props.bgDrag && props.bgImage) {
    draggingBg.value = true
    dragStartClientX = e.clientX
    dragStartClientY = e.clientY
    dragStartBgX = props.bgX
    dragStartBgY = props.bgY
    activePointerId = e.pointerId
    canvasRef.value?.setPointerCapture(e.pointerId)
    return
  }

  const cell = ptrToCell(e)
  if (!cell) return
  const [x, y] = cell

  if (props.tool === 'eyedrop') {
    const icon = activeIcon.value
    const ink = localPixels.value[y * icon.w + x]
    emit('eyedrop', ink)
    return
  }

  pushUndo()
  drawing = true
  strokeDirty = false
  lastX = x
  lastY = y
  activePointerId = e.pointerId
  canvasRef.value?.setPointerCapture(e.pointerId)
  if (applyAt(x, y)) strokeDirty = true
  if (strokeDirty) redraw()
}

function onPointerMove(e) {
  // 指针离开绘制区：结束拖动并丢掉按下状态
  if ((draggingBg.value || drawing) && !pointerInCanvas(e)) {
    endStroke(e)
    return
  }
  if (draggingBg.value) {
    const s = props.cellSize || 1
    const dx = (e.clientX - dragStartClientX) / s
    const dy = (e.clientY - dragStartClientY) / s
    emit('update:bgX', dragStartBgX + dx)
    emit('update:bgY', dragStartBgY + dy)
    return
  }
  if (!drawing) return
  const cell = ptrToCell(e)
  if (!cell) {
    endStroke(e)
    return
  }
  const [x, y] = cell
  if (x === lastX && y === lastY) return
  paintLine(lastX, lastY, x, y, (px, py) => {
    if (applyAt(px, py)) strokeDirty = true
  })
  lastX = x
  lastY = y
  if (strokeDirty) redraw()
}

function endStroke(e) {
  releasePointer(e)
  if (draggingBg.value) {
    draggingBg.value = false
    return
  }
  if (!drawing) return
  drawing = false
  if (strokeDirty && localPixels.value) {
    commitPixels(localPixels.value)
  } else {
    undo.pop()
    emit('undo-state', undo.length)
  }
  strokeDirty = false
}

function onPointerLeave(e) {
  if (draggingBg.value || drawing) endStroke(e)
}

/** Alt+滚轮：缩放模板；普通滚轮交给外层缩放绘制区 */
function zoomTemplateByFactor(factor) {
  if (!props.bgImage || !activeIcon.value) return
  applyScaleAbout(clampBgScale(props.bgScale * factor))
}

watch(
  () => activeIcon.value?.id,
  () => syncFromStore(),
)

// 换图时同步缩放基准，避免沿用旧 lastScale
watch(
  () => props.bgImage,
  () => {
    lastScale = props.bgScale
  },
)

// 侧栏滑条改缩放：以模板在可绘制区域内的中心为锚点
watch(
  () => props.bgScale,
  (ns) => {
    if (applyingZoom) {
      lastScale = ns
      return
    }
    if (Math.abs(ns - lastScale) < 1e-6) return
    if (!props.bgImage || !activeIcon.value) {
      lastScale = ns
      return
    }
    const s = props.cellSize
    const c = templateVisibleCenter(
      props.bgImage,
      activeIcon.value.w,
      activeIcon.value.h,
      s,
      lastScale,
      props.bgX,
      props.bgY,
    )
    const next = zoomAbout(
      props.bgImage,
      activeIcon.value.w,
      activeIcon.value.h,
      s,
      lastScale,
      ns,
      props.bgX,
      props.bgY,
      c.ax,
      c.ay,
    )
    lastScale = ns
    applyingZoom = true
    emit('update:bgX', next.bgX)
    emit('update:bgY', next.bgY)
    queueMicrotask(() => {
      applyingZoom = false
    })
  },
)

watch(
  () => [
    props.cellSize,
    props.showGrid,
    props.bgOpacity,
    props.bgScale,
    props.bgX,
    props.bgY,
    props.bgImage,
    props.whiteTransparent,
    props.paintOpacity,
    props.mirrorH,
    props.mirrorV,
    props.rotateN,
  ],
  () => redraw(),
)

onMounted(() => {
  syncFromStore()
  lastScale = props.bgScale
  window.addEventListener('keydown', onKey)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', onKey)
})

function onKey(e) {
  if ((e.metaKey || e.ctrlKey) && e.key === 'z') {
    e.preventDefault()
    undoOnce()
  }
}

defineExpose({
  undoOnce,
  clearBoard,
  stampTemplate,
  redraw,
  syncFromStore,
  applyScaleAbout,
  zoomTemplateByFactor,
})
</script>

<template>
  <div class="canvas-stage">
    <canvas
      ref="canvasRef"
      :style="{ cursor: canvasCursor }"
      @pointerdown="onPointerDown"
      @pointermove="onPointerMove"
      @pointerup="endStroke"
      @pointercancel="endStroke"
      @pointerleave="onPointerLeave"
    />
    <canvas
      ref="overlayRef"
      class="grid-ov"
      style="position: absolute; inset: 0; pointer-events: none"
    />
  </div>
</template>
