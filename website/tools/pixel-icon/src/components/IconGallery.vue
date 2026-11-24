<script setup>
import { watch, nextTick, ref } from 'vue'
import { pixelsToImageData } from '../lib/colors.js'
import { icons, selectIcon } from '../lib/iconStore.js'

const emit = defineEmits(['close', 'pick'])

const canvasMap = ref({})

function setCanvasEl(id, el) {
  if (el) canvasMap.value[id] = el
  else delete canvasMap.value[id]
}

function paint(icon) {
  const el = canvasMap.value[icon.id]
  if (!el) return
  const scale = Math.max(2, Math.floor(140 / Math.max(icon.w, icon.h)))
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

watch(
  icons,
  () => {
    nextTick(() => icons.value.forEach(paint))
  },
  { deep: true, immediate: true },
)

function pick(icon) {
  selectIcon(icon.id)
  emit('pick', icon.id)
  emit('close')
}
</script>

<template>
  <div class="gallery">
    <div class="gallery-head">
      <h2>全部图标（{{ icons.length }}）</h2>
      <button type="button" @click="emit('close')">返回编辑</button>
    </div>
    <div class="gallery-grid">
      <div
        v-for="icon in icons"
        :key="icon.id"
        class="gallery-card"
        @click="pick(icon)"
      >
        <canvas :ref="(el) => setCanvasEl(icon.id, el)" />
        <div class="name">{{ icon.name }}</div>
        <div class="sz">{{ icon.w }}×{{ icon.h }}</div>
      </div>
    </div>
  </div>
</template>
