#!/usr/bin/env bash
set -euo pipefail

# Simple WUHB packer
# Usage: ./wuhb_packer.sh [out_dir] [rpx_path]

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ICON_DIR="$ROOT_DIR/icon"
OUT_DIR="${1:-$ROOT_DIR/out}"
RPX_PATH="${2:-$ROOT_DIR/bin/RSDKv4.rpx}"
# Optional third arg: choice (1 or 2) to skip interactive prompt
CHOICE_ARG="${3:-}"

echo "WUHB Packer - creates a .wuhb containing the RPX and icon"

if [ ! -f "$RPX_PATH" ]; then
  echo "ERROR: RPX not found at $RPX_PATH"
  exit 1
fi

if [ -n "$CHOICE_ARG" ]; then
  CHOICE="$CHOICE_ARG"
else
  echo "Choose icon:"
  echo "  1) Sonic 1"
  echo "  2) Sonic 2"
  # Read from tty if stdin is not interactive (so prompts show when piped)
  if [ -t 0 ]; then
    read -rp "Select 1 or 2: " CHOICE
  elif [ -r /dev/tty ]; then
    read -rp "Select 1 or 2: " CHOICE </dev/tty
  else
    echo "No interactive terminal available; pass choice as third argument (1 or 2)" >&2
    exit 1
  fi
fi

if [ "$CHOICE" != "1" ] && [ "$CHOICE" != "2" ]; then
  echo "Invalid choice: '$CHOICE' (use 1 or 2)" >&2
  exit 1
fi

ICON_NAME="Sonic $CHOICE"
DISPLAY_NAME="Sonic $CHOICE"
SHORT_NAME="Sonic$CHOICE"
PREFERRED_EXTS=(png tga jpg jpeg)
FOUND_ICON=""
FOUND_BANNER=""

# Candidate directories to look for icon/banner files
CANDIDATES=("$ICON_DIR/$ICON_NAME" "$ICON_DIR/Sonic$CHOICE" "$ICON_DIR/sonic$CHOICE" "$ICON_DIR/sonic_$CHOICE" "$ICON_DIR" )
for dir in "${CANDIDATES[@]}"; do
  for ext in "${PREFERRED_EXTS[@]}"; do
    if [ -f "$dir/icon.$ext" ] && [ -z "$FOUND_ICON" ]; then
      FOUND_ICON="$dir/icon.$ext"
    fi
    if [ -f "$dir/banner.$ext" ] && [ -z "$FOUND_BANNER" ]; then
      FOUND_BANNER="$dir/banner.$ext"
    fi
  done
  if [ -n "$FOUND_ICON" ]; then
    break
  fi
done

if [ -z "$FOUND_ICON" ]; then
  echo "ERROR: Could not find icon (icon.png/jpg/etc.) for '$ICON_NAME' in $ICON_DIR or subfolders"
  exit 1
fi

MAGICK_CMD=""
if command -v magick >/dev/null 2>&1; then
  MAGICK_CMD="magick"
elif command -v convert >/dev/null 2>&1; then
  MAGICK_CMD="convert"
fi

PKG_DIR="$OUT_DIR/wuhb_pack_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$PKG_DIR/wiiu/apps/RSDKv4"

TARGET_ICON="$PKG_DIR/wiiu/apps/RSDKv4/icon.png"
if [ -n "$MAGICK_CMD" ]; then
  echo "Converting icon to PNG using $MAGICK_CMD..."
  "$MAGICK_CMD" "$FOUND_ICON" -resize 256x256 "$TARGET_ICON"
else
  echo "No ImageMagick found; copying icon as-is (must be PNG to be usable)."
  cp "$FOUND_ICON" "$TARGET_ICON"
fi

# Handle optional banner (TV/DRC images)
TARGET_BANNER=""
if [ -n "$FOUND_BANNER" ]; then
  TARGET_BANNER="$PKG_DIR/wiiu/apps/RSDKv4/banner.png"
  if [ -n "$MAGICK_CMD" ]; then
    echo "Converting banner to PNG using $MAGICK_CMD..."
    "$MAGICK_CMD" "$FOUND_BANNER" -resize 1280x720 "$TARGET_BANNER"
  else
    cp "$FOUND_BANNER" "$TARGET_BANNER"
  fi
fi



cp "$RPX_PATH" "$PKG_DIR/wiiu/apps/RSDKv4/RSDKv4.rpx"

cat > "$PKG_DIR/wiiu/apps/RSDKv4/metadata.txt" <<EOF
title=$DISPLAY_NAME
game=RSDKv4
source=Sonic $CHOICE
pack_time=$(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF

mkdir -p "$OUT_DIR"
OUT_FILE="$OUT_DIR/Sonic${CHOICE}.wuhb"

# Prefer a user-provided WUHB command, or try to find one on PATH
if [ -z "${WUHB_CMD:-}" ]; then
  for c in wuhb wuhbpack wuhbcreate; do
    if command -v "$c" >/dev/null 2>&1; then
      WUHB_CMD="$c"
      break
    fi
  done
fi

if [ -z "${WUHB_CMD:-}" ]; then
  WUHB_CMD="$(command -v wuhbtool 2>/dev/null || true)"
  if [ -z "$WUHB_CMD" ] && [ -x "/opt/devkitpro/tools/bin/wuhbtool" ]; then
    WUHB_CMD="/opt/devkitpro/tools/bin/wuhbtool"
  fi
fi

if [ -z "${WUHB_CMD:-}" ] || [ ! -x "$WUHB_CMD" ]; then
  echo "ERROR: wuhbtool not found. Install devkitPro tools in WSL or set WUHB_CMD to its path." >&2
  exit 1
fi

RPX_IN="$PKG_DIR/wiiu/apps/RSDKv4/RSDKv4.rpx"
CONTENT_DIR="$PKG_DIR/wiiu"

echo "Creating .wuhb using $WUHB_CMD..."
CMD=("$WUHB_CMD" "$RPX_IN" "$OUT_FILE" --content "$CONTENT_DIR" --icon "$TARGET_ICON" --name "$DISPLAY_NAME" --short-name "$SHORT_NAME" --author "RSDKv4 Packager")
if [ -n "$TARGET_BANNER" ]; then
  CMD+=(--tv-image "$TARGET_BANNER" --drc-image "$TARGET_BANNER")
fi

"${CMD[@]}"
if [ $? -ne 0 ]; then
  echo "ERROR: wuhbtool failed to create package. Check the tool and try again." >&2
  exit 1
fi

echo "Wuhb package created: $OUT_FILE"
