# EasyTier 控制台连接器

## 项目概述

基于 Qt6 的 Windows 系统托盘应用程序，用于管理 EasyTier 远程控制台连接。应用程序常驻系统托盘，通过 `easytier-cli.exe` 将 EasyTier Core 注册为 Windows 系统服务进行管理，提供连接管理、开机自启等功能。

### 技术栈
- **语言**: C++17
- **框架**: Qt 6.11+ (Core, Gui, Widgets, Network)
- **构建系统**: CMake 3.16+
- **平台**: Windows (使用 llvm-mingw 或 MSVC)
- **目标**: 通过 `etcore/easytier-cli.exe` 管理 EasyTier 系统服务

### 核心架构

```
┌─────────────────────────────────────────────────────────┐
│                      SystemTray                          │
│  (系统托盘主类 - UI状态管理、菜单、信号槽协调)           │
├─────────────────────────────────────────────────────────┤
│   ConfigManager     │    ETRunService                    │
│   (配置持久化)      │    (系统服务生命周期管理)          │
├─────────────────────────────────────────────────────────┤
│   SettingsDialog    │    AboutDialog                     │
│   (设置对话框)      │    (关于对话框)                    │
└─────────────────────────────────────────────────────────┘
```

## 目录结构

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
├── Install/                  # 安装输出目录 (windeployqt 部署)
├── resources.qrc             # Qt 资源文件定义
├── app.manifest              # Windows 应用清单 (UAC/DPI/兼容性)
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

# 安装 (输出到 Install/bin/ 目录，自动部署 Qt 运行时)
cmake --install .
```

### 命令行参数
- `--auto-start`: 开机自启模式启动（不显示启动通知）
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
- 启动/停止连接（调用 `ETRunService` 静态方法）
- 打开 Web 控制台 (https://console.easytier.net/)
- 设置连接地址与密钥
- 开机自启（Windows 计划任务 `schtasks.exe`）
- 双击托盘图标打开 Web 控制台

**单实例检测**: 使用 `QLocalServer`/`QLocalSocket`，服务名称 `QtETWebConnector-By-Myqfeng`

### ETRunService (系统服务管理器)
通过 `easytier-cli.exe` 将 EasyTier Core 注册为 Windows 系统服务进行管理。所有方法为静态方法，不继承 QObject。

**Windows 服务信息**:
- 服务名称: `QtETWebConnector`
- 服务安装后默认开机自启

**核心操作** (均通过 `easytier-cli.exe` 执行):
- `start(key)`: 如果服务未安装则先安装后启动；安装/启动均需 UAC 提权
- `stop()`: 先停止再卸载服务；均需 UAC 提权
- `isRunning()`: 执行 `easytier-cli node` 命令，输出包含 "running" 则为运行中
- `isServiceInstalled()`: 检查注册表 `HKLM\SYSTEM\CurrentControlSet\Services`

**UAC 提权**: 使用 `ShellExecuteExW` 配合 `"runas"` 动词触发 UAC 提示框，等待进程完成（2分钟超时）

**服务安装参数**:
```
easytier-cli.exe service install --display-name QtETWebConnector -- -w <key> --hostname <name> --secure-mode true
```

**命令执行**: `executeCommand()` 使用 `QProcess` 同步执行命令，支持超时控制（默认30秒）

### ConfigManager (配置管理)
使用 JSON 文件存储配置，位于 `%LOCALAPPDATA%/EasyTier/conf.json`：
- `connectionKey`: 连接地址与密钥
- `autoStart`: 开机自启

配置路径优先级：`AppLocalDataLocation` > `AppDataLocation` > `GenericDataLocation` > 当前目录

**旧版设置迁移**: 启动时自动从 QSettings (`HKEY_CURRENT_USER\Software\EasyTier\Connector`) 迁移旧配置到新 JSON 系统

## 开发约定

### 代码风格
- Qt 信号槽使用新语法 (函数指针)
- 成员变量使用 `m_` 前缀
- 使用 `std::clog`/`std::cerr` 进行日志输出
- 异常处理使用 try-catch 包装关键代码

### 编译宏
- `IS_NOT_ET_PRO`: 定义后切换为非 Pro 版本模式（UI 文案变化，Web 控制台禁用）

### 信号槽连接模式
- 同步连接 (默认): UI 状态更新
- 对话框使用 `QPointer` 管理生命周期，避免悬空指针

### Windows 特定功能
- 开机自启: 使用 `schtasks.exe` 创建计划任务 (`/rl highest` 以管理员权限运行)
- 服务管理: `easytier-cli.exe` (install/start/stop/uninstall)
- UAC 提权: `ShellExecuteExW` + `runas` 动词
- 注册表: 兼容旧版设置迁移 (`HKEY_CURRENT_USER\Software\EasyTier\Connector`)
- 应用清单: `asInvoker` 权限级别，PerMonitorV2 DPI 感知，Windows 7-11 兼容性声明

### 安装部署
- CMake 安装前缀设为项目根目录的 `Install/`
- 构建后自动复制 `etcore/` 目录到输出目录
- 安装时通过 `windeployqt` 最小化部署 Qt 运行时（排除 Quick、D3D、翻译、软件渲染）

## 常见问题

### 服务启动失败
1. 检查 `etcore/easytier-cli.exe` 是否存在
2. 检查连接密钥是否有效
3. 检查 UAC 授权是否被用户取消
4. 检查 `easytier-core.exe` 依赖的 DLL 是否完整

### 修改密钥后重启服务
- 密钥变更时自动停止服务 → 等待 500ms → 使用新密钥安装并启动服务
- 停止和启动均需 UAC 授权

### 图标不显示
- 确保 `assets/favicon.ico` 存在 (需从 SVG 转换)
- CMakeLists.txt 会在构建时检查并给出警告

### 程序提示"已在运行"
- 基于 QLocalServer 的单实例检测，确保同一时间只有一个实例运行