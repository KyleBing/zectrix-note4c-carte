<script setup>
import { ref, watch, nextTick } from 'vue'
import { pixelsToImageData } from '../lib/colors.js'
import { activeIcon } from '../lib/iconStore.js'

const c1 = ref(null)
const c2 = ref(null)

function paint(el, scale) {
  const icon = activeIcon.value
  if (!el || !icon) return
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

function refresh() {
  nextTick(() => {
    paint(c1.value, 1)
    paint(c2.value, 2)
  })
}

watch(activeIcon, refresh, { deep: true, immediate: true })
</script>

<template>
  <div class="icon-preview-bar">
    <div class="prev-block">
      <span class="prev-lab">1:1</span>
      <div class="prev-frame">
        <canvas ref="c1" />
      </div>
    </div>
    <div class="prev-block">
      <span class="prev-lab">2×</span>
      <div class="prev-frame">
        <canvas ref="c2" />
      </div>
    </div>
  </div>
</template>
