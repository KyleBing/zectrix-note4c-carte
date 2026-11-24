<script setup>
import { watch, nextTick, ref } from 'vue'
import { pixelsToImageData } from '../lib/colors.js'
import {
  icons,
  activeId,
  selectIcon,
  deleteIcon,
  duplicateIcon,
  renameIcon,
} from '../lib/iconStore.js'

const emit = defineEmits(['new', 'resize-as'])

const canvasMap = ref({})

function setCanvasEl(id, el) {
  if (el) canvasMap.value[id] = el
  else delete canvasMap.value[id]
}

function paintThumb(icon) {
  const el = canvasMap.value[icon.id]
  if (!el) return
  const scale = Math.max(1, Math.floor(96 / Math.max(icon.w, icon.h)))
  el.width = icon.w * scale
  el.height = icon.h * scale
  const ctx = el.getContext('2d')
  ctx.imageSmoothingEnabled = false
  const tiny = document.createElement('canvas')
  tiny.width = icon.w
  tiny.height = icon.h
  tiny.getContext('2d').putImageData(
    pixelsToImageData(icon.pixels, icon.w, icon.h, 1),
    0,
    0,
  )
  ctx.clearRect(0, 0, el.width, el.height)
  ctx.drawImage(tiny, 0, 0, el.width, el.height)
}

function refreshAll() {
  nextTick(() => {
    icons.value.forEach(paintThumb)
  })
}

watch(icons, refreshAll, { deep: true, immediate: true })
watch(activeId, refreshAll)

function onRename(icon) {
  const name = prompt('重命名', icon.name)
  if (name != null) renameIcon(icon.id, name)
}

function onDelete(icon) {
  if (icons.value.length <= 1) return
  if (confirm(`删除「${icon.name}」？`)) deleteIcon(icon.id)
}
</script>

<template>
  <aside class="panel panel-sidebar">
    <div class="sidebar-head">图标</div>
    <div class="sidebar-actions">
      <button type="button" @click="emit('new')">新建</button>
      <button type="button" @click="duplicateIcon(activeId)">复制</button>
    </div>
    <div class="sidebar-actions">
      <button type="button" @click="emit('resize-as')">另存尺寸</button>
    </div>
    <ul class="thumb-list">
      <li
        v-for="icon in icons"
        :key="icon.id"
        class="thumb-item"
        :class="{ on: icon.id === activeId }"
        @click="selectIcon(icon.id)"
        @dblclick="onRename(icon)"
      >
        <canvas :ref="(el) => setCanvasEl(icon.id, el)" />
        <span class="thumb-name">{{ icon.name }}</span>
        <span class="thumb-size">{{ icon.w }}×{{ icon.h }}</span>
        <div class="sidebar-actions" style="padding: 4px 0 0" @click.stop>
          <button type="button" @click="onRename(icon)">名</button>
          <button type="button" @click="onDelete(icon)">删</button>
        </div>
      </li>
    </ul>
  </aside>
</template>
