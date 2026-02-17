#!/usr/bin/env bash
set -eux
PKG_DIR="$(pwd)/out/wuhb_pack_test_Sonic2"
APP_INTERNAL="RSDKv4_Sonic2"
mkdir -p "$PKG_DIR/wiiu/apps/$APP_INTERNAL"
if [ -f "icon/Sonic 2/icon.png" ]; then cp "icon/Sonic 2/icon.png" "$PKG_DIR/wiiu/apps/$APP_INTERNAL/icon.png"; fi
if [ -f "icon/Sonic 2/banner.png" ]; then cp "icon/Sonic 2/banner.png" "$PKG_DIR/wiiu/apps/$APP_INTERNAL/banner.png"; fi
make -f Makefile.wiiu PACKAGED_GAME=2
cp bin/RSDKv4.rpx "$PKG_DIR/wiiu/apps/$APP_INTERNAL/RSDKv4.rpx"
ls -la "$PKG_DIR/wiiu/apps/$APP_INTERNAL"
