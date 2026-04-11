# EasyTier 控制台连接器

## 项目概述

这是一个基于 Qt6 的 Windows 系统托盘应用程序，用于管理 EasyTier 远程控制台连接。应用程序常驻系统托盘，提供连接管理、开机自启、自动回连等功能。

### 技术栈
- **语言**: C++17
- **框架**: Qt 6.11+ (Core, Gui, Widgets)
- **构建系统**: CMake 3.16+
- **平台**: Windows (使用 llvm-mingw 或 MSVC)
- **目标**: 管理 `etcore/easytier-core.exe` 进程

### 核心架构

```
┌─────────────────────────────────────────────────────────┐
│                      SystemTray                          │
│  (系统托盘主类 - UI状态管理、菜单、信号槽协调)           │
├─────────────────────────────────────────────────────────┤
│   ConfigManager     │    ETRunWorkerWin                  │
│   (配置持久化)      │    (进程生命周期管理)              │
├─────────────────────────────────────────────────────────┤
│   SettingsDialog    │    AboutDialog                     │
│   (设置对话框)      │    (关于对话框)                    │
└─────────────────────────────────────────────────────────┘
```

## 目录结构

```
et-web-connector/
├── src/                      # 源代码
│   ├── main.cpp              # 程序入口，命令行解析
│   ├── SystemTray.h/cpp      # 系统托盘主类
│   ├── ETRunWorkerWin.h/cpp  # EasyTier Core 进程管理器
│   ├── ConfigManager.h/cpp   # JSON配置文件管理
│   ├── SettingsDialog.h/cpp  # 连接密钥设置对话框
│   └── AboutDialog.h/cpp     # 关于软件对话框
├── assets/                   # 图标资源 (SVG/ICO)
├── etcore/                   # EasyTier Core 依赖
│   ├── easytier-core.exe     # 核心程序
│   ├── wintun.dll            # Wintun 驱动
│   ├── Packet.dll            # 网络包处理
│   └── WinDivert64.sys       # WinDivert 驱动
├── build/                    # 构建输出目录
├── resources.qrc             # Qt 资源文件定义
├── app.manifest              # Windows 应用清单
└── CMakeLists.txt            # CMake 构建配置
```

## 构建与运行

### 环境要求
- Qt 6.11 或更高版本
- CMake 3.16+
- llvm-mingw 或 MSVC 编译器

### 构建命令

```bash
# 创建构建目录
mkdir build && cd build

# 配置 (指定Qt路径)
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt6.11

# 编译
cmake --build . --config Release

# 运行
./EasyTierConnector.exe
```

### 命令行参数
- `--auto-start`: 开机自启模式启动（不显示启动消息）
- `--help`: 显示帮助
- `--version`: 显示版本

## 核心组件说明

### SystemTray (系统托盘主类)
负责整体协调，管理 UI 状态和信号槽连接：

**状态管理**:
- `ConnectionState::NotStarted` - 未启动
- `ConnectionState::Connecting` - 连接中
- `ConnectionState::Connected` - 已连接

**菜单功能**:
- 启动/停止连接
- 打开 Web 控制台 (https://console.easytier.net/)
- 设置连接地址密钥
- 开机自启（Windows 计划任务）
- 自动回连

### ETRunWorkerWin (进程管理器)
管理 `easytier-core.exe` 进程的生命周期：

**状态机**:
```
NotStarted -> Starting -> Running -> Stopping -> NotStarted
```

**关键特性**:
- 启动超时: 30秒
- 停止超时: 10秒
- 启动成功判定: 标准输出包含 "Successful"
- 进程参数: `--config-server <key> --hostname <name> --secure-mode true`

### ConfigManager (配置管理)
使用 JSON 文件存储配置，位于 `%APPDATA%/EasyTier/config.json`：
- `connectionKey`: 连接地址密钥
- `autoStart`: 开机自启
- `autoReconnect`: 自动回连

## 开发约定

### 代码风格
- Qt 信号槽使用新语法 (函数指针)
- 成员变量使用 `m_` 前缀
- 使用 `qDebug()` 进行日志输出
- 异常处理使用 try-catch 包装关键代码

### 信号槽连接模式
- 同步连接 (默认): UI 状态更新
- 异步延迟: 进程停止后重连 (`QTimer::singleShot`)

### Windows 特定功能
- 开机自启: 使用 `schtasks.exe` 创建计划任务
- 进程管理: `QProcess` (terminate/kill)
- 注册表: 兼容旧版设置 (`HKEY_CURRENT_USER\Software\EasyTier\Connector`)

## 常见问题

### 进程启动失败
1. 检查 `etcore/easytier-core.exe` 是否存在
2. 检查连接密钥是否有效
3. 检查端口是否被占用

### 修改密钥后无法重连
- 已实现: 停止后延迟 500ms 再重连，确保资源释放

### 图标不显示
- 确保 `assets/favicon.ico` 存在 (需从 SVG 转换)
