#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo " EasyTier Connector - Debian 一键编译打包"
echo "========================================="
echo ""

# ==================== 系统环境检测 ====================
echo "[1/7] 检测系统环境..."

if ! command -v apt-get &>/dev/null; then
    echo "错误: 当前系统不支持 apt-get 包管理器，仅适用于 Debian 系发行版。"
    exit 1
fi

if [[ $EUID -ne 0 ]]; then
    echo "错误: 请使用 sudo 运行此脚本（需要安装系统包）。"
    echo "用法: sudo ./shell/workflow-linux-deb.sh"
    exit 1
fi

echo "      系统支持 apt-get，环境就绪。"

# ==================== 安装构建依赖 ====================
echo ""
echo "[2/7] 安装构建依赖..."

apt-get update -qq

DEPS=(
    build-essential
    cmake
    qt6-base-dev
    libqt6svg6-dev
    libgl1-mesa-dev
    dpkg-dev
)

for dep in "${DEPS[@]}"; do
    if dpkg -s "$dep" &>/dev/null; then
        echo "      [已安装] $dep"
    else
        echo "      [安装中] $dep"
        apt-get install -y -qq "$dep"
    fi
done

echo "      依赖安装完成。"

# ==================== 提取版本号 ====================
echo ""
echo "[3/7] 提取版本号..."

cd "$PROJECT_DIR"
VERSION=$(sed -n 's/.*project(EasyTierConnector VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)

if [[ -z "$VERSION" ]]; then
    echo "错误: 无法从 CMakeLists.txt 提取版本号。"
    exit 1
fi

echo "      版本号: $VERSION"

# ==================== 清理旧构建 ====================
echo ""
echo "[4/7] 清理旧构建目录..."

rm -rf "$PROJECT_DIR/build"
echo "      已清理。"

# ==================== CMake 配置 ====================
echo ""
echo "[5/7] CMake 配置..."

mkdir -p "$PROJECT_DIR/build"
cd "$PROJECT_DIR/build"

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PROJECT_DIR}/Install"

echo "      CMake 配置完成。"

# ==================== 编译 ====================
echo ""
echo "[6/7] 编译中（使用 $(nproc) 线程）..."

cmake --build . --parallel "$(nproc)"

echo "      编译完成。"

# ==================== 安装 ====================
echo ""
echo "[7/7] 安装..."

cmake --install .

echo "      安装完成，产物位于 Install/bin/。"

# ==================== DEB 打包 ====================
echo ""
echo "========================================="
echo " 开始 DEB 打包"
echo "========================================="
echo ""

DEB_SCRIPT="$PROJECT_DIR/package/linux/deb/build_deb.sh"

if [[ ! -f "$DEB_SCRIPT" ]]; then
    echo "错误: 打包脚本不存在: $DEB_SCRIPT"
    exit 1
fi

bash "$DEB_SCRIPT" --version "$VERSION"

echo ""
echo "========================================="
echo " 全部完成！"
echo " DEB 包: Install/EasyTierConnector_v${VERSION}_linux_amd64.deb"
echo "========================================="
