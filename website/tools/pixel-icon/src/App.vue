<script setup>
import { ref, computed, watch, onMounted, onBeforeUnmount, nextTick } from 'vue'
import IconCanvas from './components/IconCanvas.vue'
import ColorPalette from './components/ColorPalette.vue'
import IconSidebar from './components/IconSidebar.vue'
import IconGallery from './components/IconGallery.vue'
import TracePanel from './components/TracePanel.vue'
import IconPreview from './components/IconPreview.vue'
import { SIZE_PRESETS, exportPngBlob, PAL } from './lib/colors.js'
import {
  activeIcon,
  activeId,
  icons,
  createIcon,
  resizeAsNew,
  setIconTemplate,
  patchIconTemplateView,
} from './lib/iconStore.js'
import {
  iconToHeader,
  iconsToHeader,
  downloadText,
  toCIdent,
} from './lib/firmwareExport.js'
import { centeredBgPos } from './lib/bgLayout.js'
import {
  templateUid,
  putTemplateBlob,
  getTemplateBlob,
  gcTemplates as gcTemplateBlobs,
} from './lib/templateDb.js'

const tool = ref('brush')
const brushMode = ref('solid')
const solidColor = ref(0)
const tile = ref([0, 1, 1, 0])
const activeTileId = ref('chk-0-1')
const cellSize = ref(14)
const CELL_MIN = 4
const CELL_MAX = 64
const showGrid = ref(true)
// 绘制区平移 / 空格拖动
const wrapRef = ref(null)
const panX = ref(0)
const panY = ref(0)
const spaceHeld = ref(false)
const viewPanning = ref(false)
let panStartX = 0
let panStartY = 0
let panOriginX = 0
let panOriginY = 0
const mirrorH = ref(false) // 左右对称
const mirrorV = ref(false) // 上下对称
const rotateN = ref(1) // 旋转份数，1=关
const undoLen = ref(0)
const showGallery = ref(false)
const exportScale = ref(1)
const exportPad = ref('white') // white | black — 行尾填充色

const showNewModal = ref(false)
const showResizeModal = ref(false)
const newW = ref(32)
const newH = ref(32)
const newName = ref('')

const canvasRef = ref(null)

// 背景描摹（按当前图标；图文件存 IndexedDB，多图标可共用 templateId）
const bgImage = ref(null)
const bgOpacity = ref(0.45)
const bgScale = ref(1) // 1×=贴合画布，最大 20×
const bgX = ref(0)
const bgY = ref(0)
const bgDrag = ref(false)
const whiteTransparent = ref(true)
const paintOpacity = ref(1)
/** @type {Map<string, HTMLImageElement>} */
const templateImageCache = new Map()
let restoringTemplate = false

const sizeLabel = computed(() => {
  const i = activeIcon.value
  return i ? `${i.w}×${i.h}` : ''
})

const brushLabel = computed(() => {
  if (tool.value === 'erase') return '橡皮'
  if (tool.value === 'eyedrop') return '取色'
  if (brushMode.value === 'tile') return '2×2'
  return PAL[solidColor.value]?.name || '笔刷'
})

function openNew() {
  newW.value = 32
  newH.value = 32
  newName.value = ''
  showNewModal.value = true
}

function confirmNew() {
  const w = clampSize(newW.value)
  const h = clampSize(newH.value)
  createIcon(w, h, newName.value || undefined)
  showNewModal.value = false
}

function openResize() {
  const i = activeIcon.value
  newW.value = i?.w || 32
  newH.value = i?.h || 32
  newName.value = ''
  showResizeModal.value = true
}

function confirmResize() {
  const w = clampSize(newW.value)
  const h = clampSize(newH.value)
  resizeAsNew(w, h, newName.value || undefined)
  showResizeModal.value = false
}

function clampSize(n) {
  return Math.max(2, Math.min(256, Math.floor(Number(n) || 32)))
}

function setPreset(n) {
  newW.value = n
  newH.value = n
}

function onEyedrop(ink) {
  solidColor.value = ink
  brushMode.value = 'solid'
  activeTileId.value = ''
  tool.value = 'brush'
}

async function doExportPng() {
  const icon = activeIcon.value
  if (!icon) return
  const blob = await exportPngBlob(
    icon.pixels,
    icon.w,
    icon.h,
    exportScale.value,
  )
  if (!blob) return
  const a = document.createElement('a')
  a.href = URL.createObjectURL(blob)
  a.download = `${icon.name || 'icon'}@${exportScale.value}x.png`
  a.click()
  URL.revokeObjectURL(a.href)
}

function exportOpts() {
  return { blackBg: exportPad.value === 'black' }
}

// 导出当前图标为固件 2bpp .h
function doExportHeader() {
  const icon = activeIcon.value
  if (!icon) return
  const text = iconToHeader(icon, exportOpts())
  downloadText(`${toCIdent(icon.name)}.h`, text)
}

// 导出全部暂存图标
function doExportAllHeaders() {
  const text = iconsToHeader(icons.value, exportOpts())
  downloadText('pixel_icons.h', text)
}

function applyTemplateView(view) {
  restoringTemplate = true
  bgScale.value = view.scale
  bgOpacity.value = view.opacity
  bgX.value = view.x
  bgY.value = view.y
  bgDrag.value = view.drag
  whiteTransparent.value = view.whiteTransparent
  queueMicrotask(() => {
    restoringTemplate = false
  })
}

function currentTemplateView() {
  return {
    scale: bgScale.value,
    opacity: bgOpacity.value,
    x: bgX.value,
    y: bgY.value,
    drag: bgDrag.value,
    whiteTransparent: whiteTransparent.value,
  }
}

function flushTemplateViewToIcon(iconId) {
  if (!iconId) return
  patchIconTemplateView(iconId, currentTemplateView())
}

async function imageFromBlob(blob) {
  const url = URL.createObjectURL(blob)
  try {
    const img = new Image()
    await new Promise((resolve, reject) => {
      img.onload = resolve
      img.onerror = reject
      img.src = url
    })
    // 保留 object URL 供缓存复用；由 cache 生命周期管理
    img._objectUrl = url
    return img
  } catch (err) {
    URL.revokeObjectURL(url)
    throw err
  }
}

async function resolveTemplateImage(templateId) {
  if (!templateId) return null
  if (templateImageCache.has(templateId)) {
    return templateImageCache.get(templateId)
  }
  const blob = await getTemplateBlob(templateId)
  if (!blob) return null
  const img = await imageFromBlob(blob)
  templateImageCache.set(templateId, img)
  return img
}

async function restoreTemplateForIcon(icon) {
  if (!icon) {
    bgImage.value = null
    return
  }
  applyTemplateView(icon.templateView || {})
  if (!icon.templateId) {
    bgImage.value = null
    return
  }
  try {
    const img = await resolveTemplateImage(icon.templateId)
    // 切换过程中 active 可能已变
    if (activeId.value !== icon.id) return
    bgImage.value = img
  } catch {
    if (activeId.value === icon.id) bgImage.value = null
  }
}

async function onBgLoad(payload) {
  const img = payload?.img || payload
  const blob = payload?.blob
  if (!img) return
  const icon = activeIcon.value
  if (!icon) return

  const id = templateUid()
  if (blob) {
    await putTemplateBlob(id, blob)
    templateImageCache.set(id, img)
  }

  const cell = cellSize.value
  const pos = centeredBgPos(img, icon.w, icon.h, cell, 1)
  restoringTemplate = true
  bgImage.value = img
  bgOpacity.value = 0.45
  bgScale.value = 1
  bgX.value = pos.bgX
  bgY.value = pos.bgY
  bgDrag.value = true
  whiteTransparent.value = true
  setIconTemplate(icon.id, id, currentTemplateView())
  // 旧无引用模板回收
  gcTemplateBlobs(icons.value.map((i) => i.templateId)).catch(() => {})
  queueMicrotask(() => {
    restoringTemplate = false
  })
}

function onBgClear() {
  const icon = activeIcon.value
  if (icon) {
    setIconTemplate(icon.id, null, {
      ...currentTemplateView(),
      drag: false,
      scale: 1,
      x: 0,
      y: 0,
    })
  }
  bgImage.value = null
  bgDrag.value = false
  bgScale.value = 1
  bgX.value = 0
  bgY.value = 0
  gcTemplateBlobs(icons.value.map((i) => i.templateId)).catch(() => {})
}

function onUndoState(n) {
  undoLen.value = n
}

function doUndo() {
  canvasRef.value?.undoOnce()
}

function doClear() {
  if (!confirm('清空当前画板？')) return
  canvasRef.value?.clearBoard()
}

function doStampTemplate() {
  if (!bgImage.value) return
  if (!confirm('用当前模板排印覆盖画板？（四色量化，可撤销）')) return
  canvasRef.value?.stampTemplate()
}

watch(activeIcon, () => {
  undoLen.value = 0
})

// 切换图标：先写回上一张的模板视图，再恢复目标模板（同 templateId 共用文件）
watch(
  activeId,
  async (id, prevId) => {
    if (prevId) flushTemplateViewToIcon(prevId)
    const icon = icons.value.find((i) => i.id === id)
    await restoreTemplateForIcon(icon)
  },
)

// 模板视图变更写回当前图标
watch(
  [bgScale, bgOpacity, bgX, bgY, bgDrag, whiteTransparent],
  () => {
    if (restoringTemplate) return
    flushTemplateViewToIcon(activeId.value)
  },
)

function clampCell(n) {
  return Math.max(CELL_MIN, Math.min(CELL_MAX, Math.round(n)))
}

/** 将画板在视口中居中 */
function centerView() {
  const wrap = wrapRef.value
  const icon = activeIcon.value
  if (!wrap || !icon) return
  const cw = icon.w * cellSize.value
  const ch = icon.h * cellSize.value
  panX.value = (wrap.clientWidth - cw) / 2
  panY.value = (wrap.clientHeight - ch) / 2
}

function onViewWheel(e) {
  e.preventDefault()
  const factor = e.deltaY < 0 ? 1.1 : 1 / 1.1
  // Alt+滚轮：缩放模板
  if (e.altKey && bgImage.value) {
    canvasRef.value?.zoomTemplateByFactor(factor)
    return
  }
  const wrap = wrapRef.value
  if (!wrap) return
  const rect = wrap.getBoundingClientRect()
  const mx = e.clientX - rect.left
  const my = e.clientY - rect.top
  const old = cellSize.value
  const neu = clampCell(old * factor)
  if (neu === old) return
  // 以指针为缩放中心调整平移
  const wx = mx - panX.value
  const wy = my - panY.value
  const ratio = neu / old
  panX.value = mx - wx * ratio
  panY.value = my - wy * ratio
  cellSize.value = neu
}

let wrapPointerId = null

function releaseWrapPointer() {
  const wrap = wrapRef.value
  if (wrap && wrapPointerId != null) {
    try {
      if (wrap.hasPointerCapture?.(wrapPointerId)) {
        wrap.releasePointerCapture(wrapPointerId)
      }
    } catch {
      /* ignore */
    }
  }
  wrapPointerId = null
}

function pointerInWrap(e) {
  const wrap = wrapRef.value
  if (!wrap) return false
  const r = wrap.getBoundingClientRect()
  return (
    e.clientX >= r.left &&
    e.clientX < r.right &&
    e.clientY >= r.top &&
    e.clientY < r.bottom
  )
}

function onWrapPointerDown(e) {
  if (!spaceHeld.value) return
  e.preventDefault()
  e.stopPropagation()
  viewPanning.value = true
  panStartX = e.clientX
  panStartY = e.clientY
  panOriginX = panX.value
  panOriginY = panY.value
  wrapPointerId = e.pointerId
  wrapRef.value?.setPointerCapture?.(e.pointerId)
}

function onWrapPointerMove(e) {
  if (!viewPanning.value) return
  // 离开绘制视口则结束平移并释放按下
  if (!pointerInWrap(e)) {
    onWrapPointerUp()
    return
  }
  panX.value = panOriginX + (e.clientX - panStartX)
  panY.value = panOriginY + (e.clientY - panStartY)
}

function onWrapPointerUp() {
  viewPanning.value = false
  releaseWrapPointer()
}

function onSpaceKeyDown(e) {
  if (e.repeat) return
  const tag = (e.target?.tagName || '').toLowerCase()
  if (tag === 'input' || tag === 'textarea' || tag === 'select') return
  // B 笔刷 / E 橡皮
  if (e.key === 'b' || e.key === 'B') {
    tool.value = 'brush'
    return
  }
  if (e.key === 'e' || e.key === 'E') {
    tool.value = 'erase'
    return
  }
  if (e.code !== 'Space') return
  e.preventDefault()
  spaceHeld.value = true
}

function onSpaceKeyUp(e) {
  if (e.code !== 'Space') return
  spaceHeld.value = false
  viewPanning.value = false
}

watch(
  () => activeIcon.value?.id,
  () => {
    nextTick(() => centerView())
  },
)

onMounted(() => {
  nextTick(() => centerView())
  // 启动恢复当前图标模板
  restoreTemplateForIcon(activeIcon.value)
  window.addEventListener('keydown', onSpaceKeyDown)
  window.addEventListener('keyup', onSpaceKeyUp)
  window.addEventListener('blur', () => {
    spaceHeld.value = false
    viewPanning.value = false
  })
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', onSpaceKeyDown)
  window.removeEventListener('keyup', onSpaceKeyUp)
})
</script>

<template>
  <div class="app-shell">
    <header class="app-header">
      <h1>像素图标 · NOTE4C 四色</h1>
      <button
        type="button"
        :class="{ on: showGallery }"
        @click="showGallery = true"
      >
        查看全部
      </button>
      <div class="spacer" />
      <div class="export-group">
        <label style="font-size: 12px; opacity: 0.85">
          填充
          <select v-model="exportPad">
            <option value="white">白底 pad</option>
            <option value="black">黑底 pad</option>
          </select>
        </label>
        <label style="font-size: 12px; opacity: 0.85">
          PNG
          <select v-model.number="exportScale">
            <option :value="1">1×</option>
            <option :value="2">2×</option>
            <option :value="4">4×</option>
          </select>
        </label>
        <button type="button" class="btn" @click="doExportPng">PNG</button>
        <button type="button" class="btn on" @click="doExportHeader">
          固件 .h
        </button>
        <button type="button" class="btn" @click="doExportAllHeaders">
          全部 .h
        </button>
      </div>
    </header>

    <div class="app-body">
      <IconSidebar @new="openNew" @resize-as="openResize" />

      <main class="panel-main">
        <div class="toolbar">
          <button
            type="button"
            :class="{ on: tool === 'brush' }"
            title="笔刷 (B)"
            @click="tool = 'brush'"
          >
            笔刷
          </button>
          <button
            type="button"
            :class="{ on: tool === 'erase' }"
            title="橡皮 (E)"
            @click="tool = 'erase'"
          >
            橡皮
          </button>
          <button
            type="button"
            :class="{ on: tool === 'eyedrop' }"
            @click="tool = 'eyedrop'"
          >
            取色
          </button>
          <button type="button" :disabled="!undoLen" @click="doUndo">
            撤销 ({{ undoLen }})
          </button>
          <button type="button" @click="doClear">清空</button>
          <button
            type="button"
            :class="{ on: showGrid }"
            @click="showGrid = !showGrid"
          >
            网格
          </button>
          <span class="toolbar-sep" />
          <button
            type="button"
            :class="{ on: mirrorH }"
            title="左右对称"
            @click="mirrorH = !mirrorH"
          >
            左右对称
          </button>
          <button
            type="button"
            :class="{ on: mirrorV }"
            title="上下对称"
            @click="mirrorV = !mirrorV"
          >
            上下对称
          </button>
          <label title="以中心为基点同时绘制多份">
            旋转
            <select v-model.number="rotateN">
              <option :value="1">关</option>
              <option :value="2">2 份</option>
              <option :value="3">3 份</option>
              <option :value="4">4 份</option>
              <option :value="6">6 份</option>
              <option :value="8">8 份</option>
            </select>
          </label>
          <label>
            放大
            <input
              v-model.number="cellSize"
              type="number"
              :min="CELL_MIN"
              :max="CELL_MAX"
            />
          </label>
          <button type="button" title="画板居中" @click="centerView">居中</button>
          <span class="meta">
            {{ sizeLabel }} · {{ brushLabel }} · 滚轮缩放 · 空格拖动
          </span>
        </div>
        <div
          ref="wrapRef"
          class="canvas-wrap"
          :class="{ 'is-space': spaceHeld, 'is-panning': viewPanning }"
          @wheel.prevent="onViewWheel"
          @pointerdown.capture="onWrapPointerDown"
          @pointermove="onWrapPointerMove"
          @pointerup="onWrapPointerUp"
          @pointercancel="onWrapPointerUp"
          @pointerleave="onWrapPointerUp"
        >
          <div
            class="canvas-world"
            :style="{ transform: `translate(${panX}px, ${panY}px)` }"
          >
            <IconCanvas
              ref="canvasRef"
              :tool="tool"
              :brush-mode="brushMode"
              :solid-color="solidColor"
              :tile="tile"
              :cell-size="cellSize"
              :show-grid="showGrid"
              :bg-image="bgImage"
              :bg-opacity="bgOpacity"
              v-model:bg-scale="bgScale"
              v-model:bg-x="bgX"
              v-model:bg-y="bgY"
              :bg-drag="bgDrag"
              :white-transparent="whiteTransparent"
              :paint-opacity="paintOpacity"
              :space-held="spaceHeld"
              :mirror-h="mirrorH"
              :mirror-v="mirrorV"
              :rotate-n="rotateN"
              @eyedrop="onEyedrop"
              @undo-state="onUndoState"
            />
          </div>
          <IconPreview />
        </div>
      </main>

      <aside class="panel panel-right">
        <ColorPalette
          v-model:brush-mode="brushMode"
          v-model:solid-color="solidColor"
          v-model:tile="tile"
          v-model:active-tile-id="activeTileId"
        />
        <TracePanel
          :has-image="!!bgImage"
          v-model:opacity="bgOpacity"
          v-model:paint-opacity="paintOpacity"
          v-model:scale="bgScale"
          v-model:drag="bgDrag"
          v-model:white-transparent="whiteTransparent"
          @load="onBgLoad"
          @clear="onBgClear"
          @stamp="doStampTemplate"
        />
      </aside>
    </div>

    <IconGallery v-if="showGallery" @close="showGallery = false" />

    <div v-if="showNewModal" class="modal-backdrop" @click.self="showNewModal = false">
      <div class="modal">
        <h3>新建图标</h3>
        <label>名称</label>
        <input v-model="newName" type="text" placeholder="icon-n" />
        <label>宽度</label>
        <input v-model.number="newW" type="number" min="2" max="256" />
        <label>高度</label>
        <input v-model.number="newH" type="number" min="2" max="256" />
        <div class="presets">
          <button
            v-for="n in SIZE_PRESETS"
            :key="n"
            type="button"
            @click="setPreset(n)"
          >
            {{ n }}
          </button>
        </div>
        <div class="modal-actions">
          <button type="button" class="cancel" @click="showNewModal = false">
            取消
          </button>
          <button type="button" class="ok" @click="confirmNew">创建</button>
        </div>
      </div>
    </div>

    <div
      v-if="showResizeModal"
      class="modal-backdrop"
      @click.self="showResizeModal = false"
    >
      <div class="modal">
        <h3>另存为新尺寸</h3>
        <p class="hint">原图裁剪或白底填充到新尺寸，生成新图标</p>
        <label>名称</label>
        <input v-model="newName" type="text" placeholder="自动" />
        <label>宽度</label>
        <input v-model.number="newW" type="number" min="2" max="256" />
        <label>高度</label>
        <input v-model.number="newH" type="number" min="2" max="256" />
        <div class="presets">
          <button
            v-for="n in SIZE_PRESETS"
            :key="'r' + n"
            type="button"
            @click="setPreset(n)"
          >
            {{ n }}
          </button>
        </div>
        <div class="modal-actions">
          <button type="button" class="cancel" @click="showResizeModal = false">
            取消
          </button>
          <button type="button" class="ok" @click="confirmResize">创建</button>
        </div>
      </div>
    </div>
  </div>
</template>
