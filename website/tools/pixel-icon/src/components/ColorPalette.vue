<script setup>
import { ref, computed } from 'vue'
import { PAL, PRESET_TILES, tileCss } from '../lib/colors.js'
import {
  customTiles,
  addCustomTile,
  removeCustomTile,
} from '../lib/iconStore.js'

const props = defineProps({
  brushMode: { type: String, default: 'solid' },
  solidColor: { type: Number, default: 0 },
  tile: { type: Array, default: () => [0, 0, 0, 0] },
  activeTileId: { type: String, default: '' },
})

const emit = defineEmits([
  'update:brushMode',
  'update:solidColor',
  'update:tile',
  'update:activeTileId',
])

const editTile = ref([0, 1, 1, 0])
const editCellColor = ref(0)

const allTiles = computed(() => [...PRESET_TILES, ...customTiles.value])

function pickSolid(n) {
  emit('update:brushMode', 'solid')
  emit('update:solidColor', n)
  emit('update:activeTileId', '')
}

function pickTile(t) {
  emit('update:brushMode', 'tile')
  emit('update:tile', [...t.tile])
  emit('update:activeTileId', t.id)
}

function setEditCell(i) {
  const next = [...editTile.value]
  next[i] = editCellColor.value
  editTile.value = next
}

function useEditAsBrush() {
  emit('update:brushMode', 'tile')
  emit('update:tile', [...editTile.value])
  emit('update:activeTileId', 'editor')
}

function saveEditTile() {
  const id = addCustomTile(editTile.value, '自定义')
  emit('update:brushMode', 'tile')
  emit('update:tile', [...editTile.value])
  emit('update:activeTileId', id)
}

function isTileOn(id) {
  return props.brushMode === 'tile' && props.activeTileId === id
}
</script>

<template>
  <div class="palette-inner">
    <div class="palette-head">色盘</div>

    <div class="pal-section">
      <h3>单色</h3>
      <div class="pal-sw">
        <button
          v-for="c in PAL"
          :key="c.id"
          type="button"
          :class="[
            `pal-${['k', 'w', 'y', 'r'][c.id]}`,
            { on: brushMode === 'solid' && solidColor === c.id },
          ]"
          :title="c.name"
          :aria-label="c.name"
          @click="pickSolid(c.id)"
        />
      </div>
    </div>

    <div class="pal-section">
      <h3>2×2 合成色</h3>
      <p class="hint">落笔按偶数格对齐，一次写入 2×2</p>
      <div class="tile-grid">
        <button
          v-for="t in allTiles"
          :key="t.id"
          type="button"
          :class="{ on: isTileOn(t.id) }"
          :style="tileCss(t.tile)"
          :title="t.label"
          @click="pickTile(t)"
          @contextmenu.prevent="
            t.id.startsWith('custom-') && removeCustomTile(t.id)
          "
        />
      </div>
      <p v-if="customTiles.length" class="hint">右键可删除自定义色</p>
    </div>

    <div class="pal-section">
      <h3>自定义 2×2</h3>
      <div class="pal-sw" style="margin-bottom: 8px">
        <button
          v-for="c in PAL"
          :key="'e' + c.id"
          type="button"
          :class="[
            `pal-${['k', 'w', 'y', 'r'][c.id]}`,
            { on: editCellColor === c.id },
          ]"
          :title="'编辑色 ' + c.name"
          @click="editCellColor = c.id"
        />
      </div>
      <div class="mini2x2">
        <button
          v-for="(cell, i) in editTile"
          :key="i"
          type="button"
          :style="{ background: PAL[cell].hex }"
          @click="setEditCell(i)"
        />
      </div>
      <div class="mini-actions">
        <button type="button" @click="useEditAsBrush">用作笔刷</button>
        <button type="button" @click="saveEditTile">加入色盘</button>
      </div>
    </div>
  </div>
</template>
