#!/bin/bash
set -euo pipefail

# 项目根目录
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# 解析参数
VERSION=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        *)
            echo "用法: $0 --version x.x.x"
            exit 1
            ;;
    esac
done

if [[ -z "$VERSION" ]]; then
    echo "错误: --version 参数为必填项"
    echo "用法: $0 --version x.x.x"
    exit 1
fi

echo "=== EasyTier Connector AUR 打包 ==="
echo "版本号: $VERSION"

# AUR 输出目录
AUR_DIR="$SCRIPT_DIR"

# 清理旧的 PKGBUILD 和 .SRCINFO
rm -f "$AUR_DIR/PKGBUILD" "$AUR_DIR/.SRCINFO"

# 生成 PKGBUILD
cat > "$AUR_DIR/PKGBUILD" <<EOF
# Maintainer: Myqfeng <viagrahuang@outlook.com>

pkgname=easytier-connector
pkgver=$VERSION
pkgrel=1
pkgdesc="EasyTier Web Connector based on Qt6"
arch=('x86_64')
url="https://gitee.com/myqfeng/et-connector"
license=('LGPL3')
depends=('qt6-base' 'qt6-svg')
makedepends=('cmake' 'git')
source=("\${pkgname}-\${pkgver}.tar.gz::https://gitee.com/viah6341/etc-download/releases/download/\${pkgver}/v\${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "\${srcdir}/et-connector"
    mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .
}

package() {
    cd "\${srcdir}/et-connector"
    
    # 安装主程序到 /opt/etconnector
    install -Dm755 "build/EasyTierConnector" "\${pkgdir}/opt/etconnector/EasyTierConnector"
    
    # 安装 etcore 预编译二进制
    install -Dm755 "etcore/linux/easytier-cli" "\${pkgdir}/opt/etconnector/etcore/easytier-cli"
    install -Dm755 "etcore/linux/easytier-deamon" "\${pkgdir}/opt/etconnector/etcore/easytier-deamon"
    
    # 安装图标
    install -Dm644 "assets/favicon.svg" "\${pkgdir}/opt/etconnector/favicon.svg"
    install -Dm644 "assets/favicon.png" "\${pkgdir}/opt/etconnector/favicon.png"
    
    # 安装桌面文件（Exec 指向 /usr/bin symlink，Icon 指向 /opt 下的图标）
    install -Dm644 /dev/stdin "\${pkgdir}/usr/share/applications/etconnector.desktop" <<DESKTOP_EOF
[Desktop Entry]
Type=Application
Name=ET Connector
Comment=EasyTier Web Connector
Exec=/usr/bin/EasyTierConnector
Icon=/opt/etconnector/favicon.png
Terminal=false
Categories=Utility;Qt;Web;Network;Internet;
DESKTOP_EOF
    
    # 创建 /usr/bin symlink（方便命令行调用）
    install -dm755 "\${pkgdir}/usr/bin"
    ln -sf "/opt/etconnector/EasyTierConnector" "\${pkgdir}/usr/bin/EasyTierConnector"
}
EOF

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
