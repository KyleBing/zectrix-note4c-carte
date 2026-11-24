<script setup>
import { ref } from 'vue'
import { BG_SCALE_MIN, BG_SCALE_MAX } from '../lib/bgLayout.js'

defineProps({
  hasImage: { type: Boolean, default: false },
  opacity: { type: Number, default: 0.45 },
  paintOpacity: { type: Number, default: 1 },
  scale: { type: Number, default: 1 },
  drag: { type: Boolean, default: false },
  whiteTransparent: { type: Boolean, default: true },
})

const emit = defineEmits([
  'update:opacity',
  'update:paintOpacity',
  'update:scale',
  'update:drag',
  'update:whiteTransparent',
  'load',
  'clear',
  'stamp',
])

const fileRef = ref(null)

function onFile(e) {
  const f = e.target.files?.[0]
  if (!f) return
  const url = URL.createObjectURL(f)
  const img = new Image()
  img.onload = () => emit('load', { img, blob: f, objectUrl: url })
  img.onerror = () => URL.revokeObjectURL(url)
  img.src = url
  e.target.value = ''
}

function pick() {
  fileRef.value?.click()
}

function onScaleInput(e) {
  emit('update:scale', +e.target.value)
}
</script>

<template>
  <div class="pal-section trace-panel">
    <h3>背景描摹</h3>
    <input
      ref="fileRef"
      type="file"
      accept="image/*"
      hidden
      @change="onFile"
    />
    <div class="mini-actions">
      <button type="button" @click="pick">导入图片</button>
      <button type="button" :disabled="!hasImage" @click="emit('clear')">
        清除
      </button>
    </div>
    <template v-if="hasImage">
      <div class="mini-actions" style="margin-top: 6px">
        <button type="button" @click="emit('stamp')">排印到画板</button>
      </div>
      <p class="hint">按当前缩放/位置采样，量化为黑白黄红写入画板（可撤销）。</p>
      <label class="trace-check">
        <input
          type="checkbox"
          :checked="drag"
          @change="emit('update:drag', $event.target.checked)"
        />
        拖动模板
      </label>
      <p v-if="drag" class="hint">开启后在画布上拖动参考图；关掉再描像素。</p>
      <label class="trace-row">
        <span>模板</span>
        <input
          type="range"
          min="0.05"
          max="1"
          step="0.05"
          :value="opacity"
          @input="emit('update:opacity', +$event.target.value)"
        />
        <em>{{ Math.round(opacity * 100) }}%</em>
      </label>
      <label class="trace-row">
        <span>绘制</span>
        <input
          type="range"
          min="0.05"
          max="1"
          step="0.05"
          :value="paintOpacity"
          @input="emit('update:paintOpacity', +$event.target.value)"
        />
        <em>{{ Math.round(paintOpacity * 100) }}%</em>
      </label>
      <p class="hint">调低「绘制」透明度可透出模板临摹。</p>
      <label class="trace-row">
        <span>缩放</span>
        <input
          type="range"
          :min="BG_SCALE_MIN"
          :max="BG_SCALE_MAX"
          step="0.05"
          :value="scale"
          @input="onScaleInput"
        />
        <em>{{ Number(scale).toFixed(2) }}×</em>
      </label>
      <p class="hint">1×=贴合画布；最大 20×。Alt+滚轮也可缩放模板。</p>
      <label class="trace-check">
        <input
          type="checkbox"
          :checked="whiteTransparent"
          @change="emit('update:whiteTransparent', $event.target.checked)"
        />
        白像素透出背景
      </label>
      <p class="hint">白像素当透明，便于沿图描边；导出仍按白写出。</p>
    </template>
    <p v-else class="hint">导入参考图后会保存在浏览器；复制图标时共用同一模板文件。</p>
  </div>
</template>
