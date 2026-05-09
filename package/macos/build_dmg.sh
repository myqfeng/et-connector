#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

APP_PATH="$PROJECT_DIR/Install/EasyTierConnector.app"
if [[ ! -d "$APP_PATH" ]]; then
    echo "错误: 找不到 $APP_PATH，请先完成构建安装"
    exit 1
fi

VERSION=$(grep 'project(EasyTierConnector VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')
ARCH=$(uname -m)
DMG_NAME="EasyTierConnector_v${VERSION}_mac_${ARCH}.dmg"
OUTPUT_DIR="$PROJECT_DIR/Install"

echo "=== EasyTier Connector DMG 打包 ==="
echo "版本: $VERSION"
echo "架构: $ARCH"

TMP_DIR=$(mktemp -d)
trap "rm -rf $TMP_DIR" EXIT

cp -R "$APP_PATH" "$TMP_DIR/"
ln -s /Applications "$TMP_DIR/Applications"

echo "正在创建 DMG..."
hdiutil create -volname "EasyTierConnector" \
    -srcfolder "$TMP_DIR" \
    -ov -format UDZO \
    "$OUTPUT_DIR/$DMG_NAME"

echo "完成: $OUTPUT_DIR/$DMG_NAME"
