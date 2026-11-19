#!/usr/bin/env bash
# 刷 NOTE4C：编译并上传固件（可选字库 / 串口监视）
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FW="$ROOT/firmware"
ENV=note4c

usage() {
  cat <<'EOF'
用法: ./flash.sh [命令]

  （无参数）  编译并刷固件
  all         编译并刷固件 + LittleFS 字库（首次推荐）
  fs          只刷 LittleFS（data/ 字库等）
  build       只编译
  mon         打开串口监视（115200）

可选环境变量:
  UPLOAD_PORT=/dev/cu.usbmodemXXXX   指定串口
EOF
}

need_pio() {
  if command -v pio >/dev/null 2>&1; then
    PIO=(pio)
  elif [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then
    PIO=("$HOME/.platformio/penv/bin/pio")
  else
    echo "找不到 pio。请先安装 PlatformIO CLI。" >&2
    exit 1
  fi
}

run_pio() {
  need_pio
  # `pio run -d <dir> -e <env> ...`（-d/-e 必须跟在 run 后面）
  local args=(run -d "$FW" -e "$ENV")
  if [[ -n "${UPLOAD_PORT:-}" ]]; then
    args+=(--upload-port "$UPLOAD_PORT")
  fi
  # 其余参数：如 -t upload / -t uploadfs
  args+=("$@")
  echo "+ ${PIO[*]} ${args[*]}"
  "${PIO[@]}" "${args[@]}"
}

cmd="${1:-}"
case "$cmd" in
  -h|--help|help) usage; exit 0 ;;
  ""|fw|upload)
    run_pio -t upload
    echo "固件已刷完。"
    ;;
  all)
    run_pio -t upload
    run_pio -t uploadfs
    echo "固件 + 字库已刷完。"
    ;;
  fs|uploadfs)
    run_pio -t uploadfs
    echo "LittleFS 已刷完。"
    ;;
  build)
    run_pio
    echo "编译完成。"
    ;;
  mon|monitor)
    need_pio
    if [[ -n "${UPLOAD_PORT:-}" ]]; then
      echo "+ ${PIO[*]} device monitor -b 115200 -p $UPLOAD_PORT"
      "${PIO[@]}" device monitor -b 115200 -p "$UPLOAD_PORT"
    else
      echo "+ ${PIO[*]} device monitor -b 115200"
      "${PIO[@]}" device monitor -b 115200
    fi
    ;;
  *)
    echo "未知命令: $cmd" >&2
    usage >&2
    exit 1
    ;;
esac
