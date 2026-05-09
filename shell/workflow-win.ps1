$ErrorActionPreference = "Stop"

# 脚本所在目录 → 项目根目录
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Resolve-Path "$ScriptDir\.."
$BuildDir = Join-Path $ProjectDir "build"
$InstallDir = Join-Path $ProjectDir "Install"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " EasyTier Connector - Windows 一键编译打包" -ForegroundColor Cyan
Write-Host "========================================="
Write-Host ""

# ==================== 检测系统环境 ====================
Write-Host "[1/6] 检测系统环境..."

if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Host "错误: 未找到 CMake，请先安装 CMake。" -ForegroundColor Red
    exit 1
}

$qtEnv = $env:CMAKE_PREFIX_PATH
if ($qtEnv) {
    Write-Host "      CMAKE_PREFIX_PATH: $qtEnv"
}
else {
    Write-Host "      提示: CMAKE_PREFIX_PATH 未设置，CMake 将自动搜索 Qt。" -ForegroundColor Yellow
}

Write-Host "      环境就绪。"
Write-Host ""

# ==================== 提取版本号 ====================
Write-Host "[2/6] 提取版本号..."

$content = Get-Content (Join-Path $ProjectDir "CMakeLists.txt") -Raw
$version = if ($content -match 'project\(EasyTierConnector VERSION ([0-9.]+)\)') { $matches[1] }

if (-not $version) {
    Write-Host "错误: 无法从 CMakeLists.txt 提取版本号。" -ForegroundColor Red
    exit 1
}

Write-Host "      版本号: $version"
Write-Host ""

# ==================== 清理旧构建 ====================
Write-Host "[3/6] 清理旧构建目录..."

if (Test-Path $BuildDir) {
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "      已清理。"
}
else {
    Write-Host "      无需清理。"
}
Write-Host ""

# ==================== CMake 配置 ====================
Write-Host "[4/6] CMake 配置..."

$cmakeArgs = @("-S", $ProjectDir, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Release", "-G", "MinGW Makefiles")
if ($qtEnv) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$qtEnv"
}

& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: CMake 配置失败。" -ForegroundColor Red
    exit 1
}

Write-Host "      CMake 配置完成。"
Write-Host ""

# ==================== 编译 ====================
Write-Host "[5/6] 编译中..."

cmake --build $BuildDir --config Release --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 编译失败。" -ForegroundColor Red
    exit 1
}

Write-Host "      编译完成。"
Write-Host ""

# ==================== 安装 ====================
Write-Host "[6/6] 安装..."

cmake --install $BuildDir --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 安装失败。" -ForegroundColor Red
    exit 1
}

Write-Host "      安装完成，产物位于 Install/bin/。"
Write-Host ""

# ==================== Inno Setup 打包 ====================
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " 开始 Inno Setup 打包" -ForegroundColor Cyan
Write-Host "========================================="
Write-Host ""

$packScript = Join-Path $ProjectDir "package\windows\build_win.ps1"

if (-not (Test-Path $packScript)) {
    Write-Host "错误: 打包脚本不存在: $packScript" -ForegroundColor Red
    exit 1
}

$prevErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"

& $packScript --version $version

if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 打包失败。" -ForegroundColor Red
    $ErrorActionPreference = $prevErrorAction
    exit 1
}
$ErrorActionPreference = $prevErrorAction

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " 全部完成！" -ForegroundColor Green
Write-Host " 安装包: Install\EasyTierConnector_v${version}_win_amd64.exe" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
