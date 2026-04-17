# EasyTier 控制台连接器

基于 Qt6 的 Windows 系统托盘应用程序，用于管理 EasyTier 远程控制台连接。应用程序常驻系统托盘，通过 `easytier-cli.exe` 将 EasyTier Core 注册为 Windows 系统服务进行管理，提供连接管理、开机自启等功能。

## 功能特性

### 系统托盘菜单
| 功能 | 说明 |
|------|------|
| 状态显示 | 实时显示：未启动 / 连接中 / 已连接 |
| 连接控制 | 一键启动/停止 EasyTier Core |
| Web 控制台 | 快速打开 https://console.easytier.net/ |
| 设置 | 配置连接地址密钥 |
| 开机自启 | Windows 注册表实现（无需管理员权限） |
| 自动回连 | 程序启动时自动连接 |
| 关于软件 | 显示版本信息 |

### 核心特性
- 双击托盘图标打开 Web 控制台
- 连接状态持久化保存
- 系统托盘常驻运行
- 进程生命周期管理（启动超时 30s，停止超时 10s）
- 修改密钥后自动重连
- 启动/停止进度提示

## 项目结构

```
et-web-connector/
├── src/                      # 源代码
│   ├── main.cpp              # 程序入口，命令行解析，单实例检测
│   ├── SystemTray.h/cpp      # 系统托盘主类
│   ├── ETRunService.h/cpp    # EasyTier 系统服务管理器（静态类）
│   ├── ConfigManager.h/cpp   # JSON 配置文件管理
│   ├── SettingsDialog.h/cpp  # 连接密钥设置对话框
│   └── AboutDialog.h/cpp     # 关于软件对话框
├── assets/                   # 图标资源 (SVG/ICO)
├── etcore/                   # EasyTier Core 依赖
│   ├── easytier-core.exe     # 核心程序
│   ├── easytier-cli.exe      # 命令行管理工具
│   ├── wintun.dll            # Wintun 驱动
│   ├── Packet.dll            # 网络包处理
│   └── WinDivert64.sys       # WinDivert 驱动
├── docs/                     # 使用文档
│   ├── 快速开始.md
│   ├── 配置说明.md
│   ├── 常见问题.md
│   └── 开发指南.md
├── resources.qrc             # Qt 资源文件定义
├── app.manifest              # Windows 应用清单 (UAC/DPI/兼容性)
└── CMakeLists.txt            # CMake 构建配置
```

## 构建说明

### 环境要求
- Qt 6.11 或更高版本
- CMake 3.16+
- llvm-mingw 或 MSVC 编译器

### 构建步骤

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置 CMake（指定 Qt 路径）
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt6.11

# 3. 编译
cmake --build . --config Release

# 4. 安装（输出到 Install/bin/ 目录，自动部署 Qt 运行时）
cmake --install .
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--auto-start` | 开机自启模式启动（不显示启动通知） |
| `--help` | 显示帮助信息 |
| `--version` | 显示版本号 |

## 配置说明

配置文件位于 `%LOCALAPPDATA%/EasyTier/config.json`：

```json
{
  "connectionKey": "your-connection-key",
  "autoStart": false,
  "autoReconnect": false
}
```

| 配置项 | 说明 |
|--------|------|
| `connectionKey` | EasyTier 连接地址密钥 |
| `autoStart` | 开机自启开关 |
| `autoReconnect` | 自动回连开关 |

## 运行原理

### 进程管理
应用程序管理 `etcore/easytier-core.exe` 进程：

```
启动参数: --config-server <key> --hostname <name> --secure-mode true
启动判定: 标准输出包含 "Successful"
启动超时: 30 秒
停止超时: 10 秒
```

### 状态机

```
NotStarted ──start()──> Starting ──success──> Running
     ↑                    │                       │
     │                failed                 stop()
     │                    ↓                       ↓
     └──────────────── NotStarted <─── Stopping ──┘
```

## 依赖说明

| 文件 | 说明 |
|------|------|
| `easytier-core.exe` | EasyTier 核心程序 |
| `easytier-cli.exe` | EasyTier 命令行管理工具 |
| `wintun.dll` | Windows TUN 驱动 |
| `Packet.dll` | WinPcap 数据包捕获库 |
| `WinDivert64.sys` | Windows 网络数据包拦截驱动 |

## 许可证

本项目仅供学习和研究使用。