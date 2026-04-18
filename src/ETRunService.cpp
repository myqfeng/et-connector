/**
 * @file ETRunService.cpp
 * @brief EasyTier Core 系统服务管理器实现
 */

#include "ETRunService.h"
#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>
#include <QSettings>
#include <QProcess>
#include <QFile>
#include <iostream>

#ifdef Q_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#endif

/**
 * @brief 以管理员权限执行命令（通过 UAC 提权）
 *
 * 使用 ShellExecuteExW 配合 "runas" 动词触发 UAC 提示框。
 * 用户确认后以管理员权限运行程序。
 *
 * @param program 程序路径
 * @param arguments 参数列表
 * @param workingDir 工作目录
 * @return 是否成功启动提权进程
 */
static bool executeElevated(const QString &program,
                            const QStringList &arguments,
                            const QString &workingDir)
{
    // 转换为 Windows 原生路径格式
    QString nativeProgram = QDir::toNativeSeparators(program);
    QString nativeWorkingDir = QDir::toNativeSeparators(workingDir);
    QString args = arguments.join(' ');
    
    std::clog << "ETRunService: 执行 UAC 提权命令: " << nativeProgram.toStdString() << " " << args.toStdString() << std::endl;
    
    // 初始化 SHELLEXECUTEINFOW 结构体
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.hwnd = nullptr;
    sei.lpVerb = L"runas";  // UAC 提权动词
    sei.lpFile = reinterpret_cast<LPCWSTR>(nativeProgram.utf16());
    sei.lpParameters = args.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(args.utf16());
    sei.lpDirectory = nativeWorkingDir.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(nativeWorkingDir.utf16());
    sei.nShow = SW_HIDE;  // 隐藏窗口
    
    // 执行 ShellExecuteExW
    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        std::cerr << "ETRunService: ShellExecuteExW 失败, 错误码: " << error << std::endl;
        return false;
    }
    
    // 等待进程完成
    if (sei.hProcess) {
        DWORD waitResult = WaitForSingleObject(sei.hProcess, 120000);  // 2分钟超时
        
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(sei.hProcess, 1);
            CloseHandle(sei.hProcess);
            std::cerr << "ETRunService: UAC 提权命令执行超时" << std::endl;
            return false;
        }
        
        // 获取退出码
        DWORD exitCode = 0;
        if (GetExitCodeProcess(sei.hProcess, &exitCode)) {
            std::clog << "ETRunService: UAC 提权命令退出码: " << exitCode << std::endl;
        }
        
        CloseHandle(sei.hProcess);
        
        // 用户取消 UAC 时，exitCode 通常为 1223 (ERROR_CANCELLED)
        if (exitCode == 1223) {
            std::cerr << "ETRunService: 用户取消了 UAC 授权" << std::endl;
            return false;
        }
        
        return exitCode == 0;
    }
    
    return true;
}

QString ETRunService::getCliPath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir::toNativeSeparators(appDir + "/etcore/easytier-cli.exe");
}

QString ETRunService::getWorkingDirectory()
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir::toNativeSeparators(appDir + "/etcore");
}

CommandResult ETRunService::executeCommand(const QString &command, 
                                           const QStringList &args,
                                           const int timeoutMs)
{
    CommandResult result;
    
    QProcess process;
    process.setWorkingDirectory(getWorkingDirectory());
    process.start(command, args);
    
    if (!process.waitForStarted(5000)) {
        result.errorString = QString("无法启动进程: %1").arg(process.errorString());
        std::cerr << "ETRunService: " << result.errorString.toStdString() << std::endl;
        return result;
    }
    
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        result.errorString = "命令执行超时";
        std::cerr << "ETRunService: " << result.errorString.toStdString() << std::endl;
        return result;
    }
    
    result.exitCode = process.exitCode();
    result.output = QString::fromUtf8(
        process.readAllStandardOutput() + process.readAllStandardError()
    );
    result.success = (result.exitCode == 0);
    
    return result;
}

bool ETRunService::isServiceInstalled()
{
    QSettings settings(
        R"(HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services)", 
        QSettings::NativeFormat
    );
    
    return settings.childGroups().contains(SERVICE_NAME);
}

bool ETRunService::start(const QString &connectionKey)
{
    if (connectionKey.isEmpty()) {
        std::cerr << "ETRunService::start: 连接密钥为空" << std::endl;
        return false;
    }
    
    QString cliPath = getCliPath();
    if (!QFile::exists(cliPath)) {
        std::cerr << "ETRunService::start: 找不到 easytier-cli.exe: " << cliPath.toStdString() << std::endl;
        return false;
    }
    
    QString workDir = getWorkingDirectory();
    
    // 检查服务是否已在运行
    if (isRunning()) {
        std::clog << "ETRunService: 服务已在运行中" << std::endl;
        return true;
    }
    
    // 如果服务未安装，合并安装+启动为一条 cmd /c 命令，只需一次 UAC 授权
    if (!isServiceInstalled()) {
        const QString hostname = QSysInfo::machineHostName();
        const QString workPath = getWorkingDirectory()+ "/easytier-deamon.exe";
        
        // 构造安装命令
        QString installCmd = QString("\"%1\" service install --core-path \"%2\" --display-name \"%3\" -- -w \"%4\" --hostname \"%5\" --secure-mode true")
                                 .arg(cliPath, workPath, SERVICE_NAME, connectionKey, hostname);
        // 构造启动命令
        QString startCmd = QString("\"%1\" service start").arg(cliPath);
        
        // 使用 cmd /c 串联：install 成功后才执行 start (&&)
        QStringList args;
        args << "/c" << QString("\"%1 && %2\"").arg(installCmd, startCmd);
        
        std::clog << "ETRunService: 安装并启动服务 (需要UAC授权)" << std::endl;
        
        if (!executeElevated("cmd.exe", args, workDir)) {
            std::cerr << "ETRunService: 安装并启动服务失败" << std::endl;
            return false;
        }
        
        std::clog << "ETRunService: 安装并启动服务成功" << std::endl;
    } else {
        // 服务已安装：直接启动（只需一次 UAC 授权）
        QStringList startArgs;
        startArgs << "service" << "start";
        
        std::clog << "ETRunService: 启动服务 (需要UAC授权), 参数: " << startArgs.join(" ").toStdString() << std::endl;
        
        if (!executeElevated(cliPath, startArgs, workDir)) {
            std::cerr << "ETRunService: 服务启动失败" << std::endl;
            return false;
        }
        
        std::clog << "ETRunService: 服务启动成功" << std::endl;
    }
    
    return true;
}

bool ETRunService::stop()
{
    QString cliPath = getCliPath();
    if (!QFile::exists(cliPath)) {
        std::cerr << "ETRunService::stop: 找不到 easytier-cli.exe: " << cliPath.toStdString() << std::endl;
        return false;
    }
    
    QString workDir = getWorkingDirectory();
    
    // 合并停止+卸载为一条 cmd /c 命令，只需一次 UAC 授权
    // 使用 & 串联：无论停止是否成功，都尝试卸载（与原逻辑一致）
    QString stopCmd = QString("\"%1\" service stop").arg(cliPath);
    QString uninstallCmd = QString("\"%1\" service uninstall").arg(cliPath);
    
    QStringList args;
    args << "/c" << QString("\"%1 & %2\"").arg(stopCmd, uninstallCmd);
    
    std::clog << "ETRunService: 停止并卸载服务 (需要UAC授权)" << std::endl;
    
    if (!executeElevated("cmd.exe", args, workDir)) {
        std::cerr << "ETRunService: 停止并卸载服务失败" << std::endl;
        return false;
    }
    
    std::clog << "ETRunService: 停止并卸载服务成功" << std::endl;
    return true;
}

bool ETRunService::isRunning()
{
    // 创建系统进程快照，TH32CS_SNAPPROCESS 表示包含所有进程信息
    // 参数 dwProcessID=0 表示当前进程，对 TH32CS_SNAPPROCESS 无实际影响
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "ETRunService::isRunning: CreateToolhelp32Snapshot 失败" << std::endl;
        return false;
    }

    // PROCESSENTRY32W 是进程信息的宽字符版本结构体
    // 必须在调用 Process32FirstW 之前设置 dwSize 为结构体大小，否则调用失败
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;
    // Process32FirstW 获取快照中的第一个进程信息
    if (Process32FirstW(snapshot, &entry)) {
        do {
            // _wcsicmp 宽字符不区分大小写比较，匹配进程名
            if (_wcsicmp(entry.szExeFile, L"easytier-deamon.exe") == 0) {
                found = true;
                break;
            }
            // Process32NextW 遍历快照中的下一个进程
        } while (Process32NextW(snapshot, &entry));
    }

    // 关闭快照句柄，释放系统资源
    CloseHandle(snapshot);
    return found;
}