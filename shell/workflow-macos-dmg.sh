#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo " EasyTier Connector - macOS 一键编译打包"
echo "========================================="
echo ""

# 检测系统环境
if [[ "$(uname)" != "Darwin" ]]; then
    echo "错误: 此脚本仅适用于 macOS 系统"
    exit 1
fi

# 提取版本号
VERSION=$(grep 'project(EasyTierConnector VERSION' "$PROJECT_DIR/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')
echo "版本: $VERSION"
echo "架构: $(uname -m)"
echo ""

# 查找 Qt6
echo "[1/5] 查找 Qt6..."
QT_PATH=""
QT_PREFIX_PATH=""

# Homebrew Apple Silicon
if [[ -d "/opt/homebrew/opt/qt6" ]]; then
    QT_PATH="/opt/homebrew/opt/qt6"
    QT_PREFIX_PATH="$QT_PATH"
# Homebrew Intel
elif [[ -d "/usr/local/opt/qt6" ]]; then
    QT_PATH="/usr/local/opt/qt6"
    QT_PREFIX_PATH="$QT_PATH"
fi

if [[ -z "$QT_PATH" ]]; then
    echo "未找到 Homebrew 安装的 Qt6，尝试安装..."
    if ! command -v brew &>/dev/null; then
        echo "错误: 需要 Homebrew，请先安装: https://brew.sh"
        exit 1
    fi
    brew install qt6
    if [[ -d "/opt/homebrew/opt/qt6" ]]; then
        QT_PATH="/opt/homebrew/opt/qt6"
        QT_PREFIX_PATH="$QT_PATH"
    elif [[ -d "/usr/local/opt/qt6" ]]; then
        QT_PATH="/usr/local/opt/qt6"
        QT_PREFIX_PATH="$QT_PATH"
    else
        echo "错误: Qt6 安装失败"
        exit 1
    fi
fi

echo "Qt 路径: $QT_PATH"
echo ""

# 清理旧构建
echo "[2/5] 清理旧构建目录..."
rm -rf "$PROJECT_DIR/build"
echo ""

# CMake 配置
echo "[3/5] CMake 配置..."
mkdir -p "$PROJECT_DIR/build"
cd "$PROJECT_DIR/build"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX_PATH" \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_DIR/Install"

echo ""

# 编译
echo "[4/5] 编译中（使用 $(sysctl -n hw.logicalcpu) 线程）..."
cmake --build . --parallel "$(sysctl -n hw.logicalcpu)"
echo "编译完成."
echo ""

# 安装
echo "[5/5] 安装与打包..."
cmake --install .

# 打包 DMG
bash "$PROJECT_DIR/package/macos/build_dmg.sh"

echo ""
echo "========================================="
echo " 全部完成！"
echo " DMG: Install/EasyTierConnector_v${VERSION}_mac_$(uname -m).dmg"
echo "========================================="
