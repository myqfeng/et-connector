# EasyTier 控制台连接器

基于 Qt6 的 Windows 系统托盘应用程序，用于 EasyTier 远程控制台连接管理。

## 功能特性

### 系统托盘菜单
| 功能 | 说明 |
|------|------|
| 状态显示 | 实时显示：未启动 / 连接中 / 已连接 |
| 连接控制 | 一键启动/停止 EasyTier Core |
| Web 控制台 | 快速打开 https://console.easytier.net/ |
| 设置 | 配置连接地址密钥 |
| 开机自启 | Windows 计划任务实现 |
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
│   ├── main.cpp              # 程序入口
│   ├── SystemTray.h/cpp      # 系统托盘主类
│   ├── ETRunWorkerWin.h/cpp  # EasyTier Core 进程管理器
│   ├── ConfigManager.h/cpp   # 配置管理（JSON）
│   ├── SettingsDialog.h/cpp  # 设置对话框
│   └── AboutDialog.h/cpp     # 关于对话框
├── assets/                   # 图标资源 (SVG/ICO)
├── etcore/                   # EasyTier Core 依赖
│   ├── easytier-core.exe     # 核心程序
│   ├── wintun.dll            # Wintun 驱动
│   ├── Packet.dll            # 网络包处理
│   └── WinDivert64.sys       # WinDivert 驱动
├── resources.qrc             # Qt 资源文件
├── app.manifest              # Windows 应用清单
└── CMakeLists.txt            # CMake 配置
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

# 4. 运行
./EasyTierConnector.exe
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--auto-start` | 开机自启模式启动（不显示启动消息） |
| `--help` | 显示帮助信息 |
| `--version` | 显示版本号 |

## 配置说明

配置文件位于 `%APPDATA%/EasyTier/config.json`：

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
| `wintun.dll` | Windows TUN 驱动 |
| `Packet.dll` | WinPcap 数据包捕获库 |
| `WinDivert64.sys` | Windows 网络数据包拦截驱动 |

## 许可证

本项目仅供学习和研究使用。