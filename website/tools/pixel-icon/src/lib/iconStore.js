import { ref, computed, watch } from 'vue'
import { resizePixels } from './canvasPaint.js'
import { gcTemplates } from './templateDb.js'

const STORAGE_KEY = 'note4c-pixel-icon-v1'
const CUSTOM_TILES_KEY = 'note4c-pixel-icon-custom-tiles-v1'

function uid() {
  return `i_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 7)}`
}

function blankPixels(w, h, fill = 1) {
  const p = new Uint8Array(w * h)
  p.fill(fill)
  return p
}

/** 每图标的模板视图（图片本体按 templateId 存在 IndexedDB，可共用） */
export function defaultTemplateView() {
  return {
    scale: 1,
    opacity: 0.45,
    x: 0,
    y: 0,
    drag: false,
    whiteTransparent: true,
  }
}

function normalizeTemplateView(raw) {
  const d = defaultTemplateView()
  if (!raw || typeof raw !== 'object') return d
  return {
    scale: Number(raw.scale) || d.scale,
    opacity: raw.opacity != null ? Number(raw.opacity) : d.opacity,
    x: Number(raw.x) || 0,
    y: Number(raw.y) || 0,
    drag: !!raw.drag,
    whiteTransparent:
      raw.whiteTransparent != null ? !!raw.whiteTransparent : d.whiteTransparent,
  }
}

function serializeIcon(icon) {
  return {
    id: icon.id,
    name: icon.name,
    w: icon.w,
    h: icon.h,
    // 持久化：色索引数组转普通数组
    pixels: Array.from(icon.pixels),
    templateId: icon.templateId || null,
    templateView: normalizeTemplateView(icon.templateView),
  }
}

function deserializeIcon(raw) {
  return {
    id: raw.id,
    name: raw.name || '未命名',
    w: raw.w,
    h: raw.h,
    pixels: new Uint8Array(raw.pixels),
    templateId: raw.templateId || null,
    templateView: normalizeTemplateView(raw.templateView),
  }
}

function loadIcons() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) return null
    const data = JSON.parse(raw)
    if (!Array.isArray(data?.icons) || !data.icons.length) return null
    return {
      icons: data.icons.map(deserializeIcon),
      activeId: data.activeId || data.icons[0].id,
    }
  } catch {
    return null
  }
}

function loadCustomTiles() {
  try {
    const raw = localStorage.getItem(CUSTOM_TILES_KEY)
    if (!raw) return []
    const arr = JSON.parse(raw)
    if (!Array.isArray(arr)) return []
    return arr.filter(
      (t) => t && Array.isArray(t.tile) && t.tile.length === 4,
    )
  } catch {
    return []
  }
}

const saved = loadIcons()
const defaultIcon = {
  id: uid(),
  name: 'icon-1',
  w: 32,
  h: 32,
  pixels: blankPixels(32, 32),
  templateId: null,
  templateView: defaultTemplateView(),
}

export const icons = ref(saved?.icons ?? [defaultIcon])
const initialActive =
  saved?.activeId && icons.value.some((i) => i.id === saved.activeId)
    ? saved.activeId
    : icons.value[0].id
export const activeId = ref(initialActive)
export const customTiles = ref(loadCustomTiles())

export const activeIcon = computed(
  () => icons.value.find((i) => i.id === activeId.value) || icons.value[0],
)

let persistTimer = null
// 防抖写入 localStorage
function schedulePersist() {
  clearTimeout(persistTimer)
  persistTimer = setTimeout(() => {
    const payload = {
      activeId: activeId.value,
      icons: icons.value.map(serializeIcon),
    }
    localStorage.setItem(STORAGE_KEY, JSON.stringify(payload))
  }, 250)
}

watch([icons, activeId], schedulePersist, { deep: true })
watch(
  customTiles,
  () => {
    localStorage.setItem(CUSTOM_TILES_KEY, JSON.stringify(customTiles.value))
  },
  { deep: true },
)

// 启动时清理无引用模板
gcTemplates(icons.value.map((i) => i.templateId)).catch(() => {})

export function selectIcon(id) {
  if (icons.value.some((i) => i.id === id)) activeId.value = id
}

export function createIcon(w, h, name) {
  const n = icons.value.length + 1
  const icon = {
    id: uid(),
    name: name || `icon-${n}`,
    w,
    h,
    pixels: blankPixels(w, h),
    templateId: null,
    templateView: defaultTemplateView(),
  }
  icons.value.push(icon)
  activeId.value = icon.id
  return icon
}

export function duplicateIcon(id) {
  const src = icons.value.find((i) => i.id === id)
  if (!src) return null
  // 复制像素；模板引用同一 templateId（不重复存图）
  const icon = {
    id: uid(),
    name: `${src.name}-copy`,
    w: src.w,
    h: src.h,
    pixels: new Uint8Array(src.pixels),
    templateId: src.templateId || null,
    templateView: normalizeTemplateView(src.templateView),
  }
  icons.value.push(icon)
  activeId.value = icon.id
  return icon
}

export function deleteIcon(id) {
  if (icons.value.length <= 1) return false
  const idx = icons.value.findIndex((i) => i.id === id)
  if (idx < 0) return false
  icons.value.splice(idx, 1)
  if (activeId.value === id) {
    activeId.value = icons.value[Math.max(0, idx - 1)].id
  }
  // 删除后回收无引用模板文件
  gcTemplates(icons.value.map((i) => i.templateId)).catch(() => {})
  return true
}

export function renameIcon(id, name) {
  const icon = icons.value.find((i) => i.id === id)
  if (!icon) return
  icon.name = name.trim() || icon.name
}

/** 把像素写回当前图标（画布变更后调用） */
export function commitPixels(pixels) {
  const icon = activeIcon.value
  if (!icon) return
  icon.pixels = new Uint8Array(pixels)
}

/** 写入当前图标的模板 id / 视图 */
export function setIconTemplate(id, templateId, view) {
  const icon = icons.value.find((i) => i.id === id)
  if (!icon) return
  icon.templateId = templateId
  if (view) icon.templateView = normalizeTemplateView(view)
}

export function patchIconTemplateView(id, partial) {
  const icon = icons.value.find((i) => i.id === id)
  if (!icon) return
  icon.templateView = normalizeTemplateView({
    ...icon.templateView,
    ...partial,
  })
}

/** 另存为新尺寸：裁剪或白底填充；模板引用保持 */
export function resizeAsNew(nw, nh, name) {
  const src = activeIcon.value
  if (!src) return null
  const icon = {
    id: uid(),
    name: name || `${src.name}-${nw}x${nh}`,
    w: nw,
    h: nh,
    pixels: resizePixels(src.pixels, src.w, src.h, nw, nh, 1),
    templateId: src.templateId || null,
    templateView: normalizeTemplateView(src.templateView),
  }
  icons.value.push(icon)
  activeId.value = icon.id
  return icon
}

export function addCustomTile(tile, label) {
  const id = `custom-${Date.now().toString(36)}`
  customTiles.value.push({
    id,
    label: label || '自定义',
    tile: [...tile],
  })
  return id
}

export function removeCustomTile(id) {
  const i = customTiles.value.findIndex((t) => t.id === id)
  if (i >= 0) customTiles.value.splice(i, 1)
}
