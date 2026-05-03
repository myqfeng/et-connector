#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 解析参数
VERSION=""
REL="1"
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --rel)
            REL="$2"
            shift 2
            ;;
        *)
            echo "用法: $0 --version x.x.x [--rel x]"
            exit 1
            ;;
    esac
done

if [[ -z "$VERSION" ]]; then
    echo "错误: --version 参数为必填项"
    echo "用法: $0 --version x.x.x [--rel x]"
    exit 1
fi

echo "=== EasyTier Connector AUR 打包 ==="
echo "版本号: $VERSION"
echo "pkgrel: $REL"

# AUR 输出目录
AUR_DIR="$SCRIPT_DIR"

# 清理旧的 PKGBUILD 和 .SRCINFO
rm -f "$AUR_DIR/PKGBUILD" "$AUR_DIR/.SRCINFO"

# 使用占位符 + sed 替换，避免 heredoc 中 $ 被 shell 提前展开
cat > "$AUR_DIR/PKGBUILD" <<'EOF'
# Maintainer: Myqfeng <viagrahuang@outlook.com>

pkgname=easytier-connector
pkgver=__VERSION__
pkgrel=__REL__
pkgdesc="EasyTier Web Connector based on Qt6"
arch=('x86_64')
url="https://gitee.com/myqfeng/et-connector"
license=('LGPL3')
depends=('qt6-base' 'qt6-svg')
makedepends=('cmake')
source=("${pkgname}-${pkgver}.tar.gz::https://gitee.com/viah6341/etc-download/releases/download/${pkgver}/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "${srcdir}/et-connector_v${pkgver}"
    rm -rf build Install
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_INSTALL_PREFIX="${srcdir}/et-connector_v${pkgver}/Install"
    cmake --build .
    cmake --install .
}

package() {
    cd "${srcdir}/et-connector_v${pkgver}"

    # 从 Install/bin 复制主程序和 etcore 到 /opt/etconnector
    install -Dm755 "Install/bin/EasyTierConnector" "${pkgdir}/opt/etconnector/EasyTierConnector"
    install -Dm755 "Install/bin/etcore/easytier-cli" "${pkgdir}/opt/etconnector/etcore/easytier-cli"
    install -Dm755 "Install/bin/etcore/easytier-deamon" "${pkgdir}/opt/etconnector/etcore/easytier-deamon"

    # 从 deb 目录复制图标
    install -Dm644 "package/linux/deb/opt/etconnector/favicon.png" "${pkgdir}/opt/etconnector/favicon.png"

    # 从 deb 目录复制桌面文件
    install -Dm644 "package/linux/deb/usr/share/applications/etconnector.desktop" "${pkgdir}/usr/share/applications/etconnector.desktop"

    # 创建 /usr/bin symlink
    install -dm755 "${pkgdir}/usr/bin"
    ln -sf "/opt/etconnector/EasyTierConnector" "${pkgdir}/usr/bin/EasyTierConnector"
}
EOF

# 替换占位符
sed -i "s/__VERSION__/$VERSION/g" "$AUR_DIR/PKGBUILD"
sed -i "s/__REL__/$REL/g" "$AUR_DIR/PKGBUILD"

# 生成 .SRCINFO
echo "正在生成 .SRCINFO..."
cd "$AUR_DIR"
makepkg --printsrcinfo > .SRCINFO

echo ""
echo "完成！"
echo "AUR 文件已生成到: $AUR_DIR"
echo "  - PKGBUILD"
echo "  - .SRCINFO"
echo ""
echo "发布步骤:"
echo "  1. git clone ssh://aur@aur.archlinux.org/easytier-connector.git"
echo "  2. 将 PKGBUILD 和 .SRCINFO 复制到 AUR 仓库目录"
echo "  3. git add . && git commit -m \"Update to v$VERSION\" && git push"
