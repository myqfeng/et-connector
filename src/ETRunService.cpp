#include "ETRunService.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QSysInfo>
#include <QSettings>

ETRunService::ETRunService(QObject *parent)
    : QObject(parent)
{
}

ETRunService::~ETRunService()
= default;

bool ETRunService::start(const QString &connectionKey)
{
    if (connectionKey.isEmpty()) {
        qDebug() << "ETRunService::start: 连接密钥为空";
        return false;
    }
    
    QString cliPath = getCliPath();
    if (!QFile::exists(cliPath)) {
        qDebug() << "ETRunService::start: 找不到 easytier-cli.exe:" << cliPath;
        return false;
    }
    
    QString output;
    int exitCode;
    
    // 如果服务未安装，先安装
    if (!isServiceInstalled()) {
        const QString &hostname = QSysInfo::machineHostName();
        
        // 安装服务
        QStringList installArgs;
        installArgs << "service" << "install"
                    << "--display-name" << "QtET Web Connector"
                    << "--"
                    << "--config-server" << connectionKey
                    << "--hostname" << hostname
                    << "--secure-mode" << "true";
        
        qDebug() << "安装 EasyTier 服务:" << installArgs;
        exitCode = executeCommand(cliPath, installArgs, output);
        if (exitCode != 0) {
            qDebug() << "服务安装失败:" << output;
            return false;
        }
        qDebug() << "服务安装成功";
    }
    
    // 启动服务
    QStringList startArgs;
    startArgs << "service" << "start";
    
    qDebug() << "启动 EasyTier 服务:" << startArgs;
    exitCode = executeCommand(cliPath, startArgs, output);
    
    if (exitCode == 0 || output.contains("already running") || output.contains("已运行")) {
        qDebug() << "服务启动成功";
        return true;
    }
    
    qDebug() << "服务启动失败:" << output;
    return false;
}

bool ETRunService::stop()
{
    const QString &cliPath = getCliPath();
    if (!QFile::exists(cliPath)) {
        qDebug() << "ETRunService::stop: 找不到 easytier-cli.exe:" << cliPath;
        return false;
    }
    
    QString output;

    // 停止服务
    QStringList stopArgs;
    stopArgs << "service" << "stop";
    
    qDebug() << "停止 EasyTier 服务:" << stopArgs;
    int exitCode = executeCommand(cliPath, stopArgs, output);
    qDebug() << "停止服务结果:" << exitCode << output;
    
    // 卸载服务
    QStringList uninstallArgs;
    uninstallArgs << "service" << "uninstall";
    
    qDebug() << "卸载 EasyTier 服务:" << uninstallArgs;
    exitCode = executeCommand(cliPath, uninstallArgs, output);
    qDebug() << "卸载服务结果:" << exitCode << output;
    
    return true;
}

bool ETRunService::isRunning()
{
    const QString &cliPath = getCliPath();
    if (!QFile::exists(cliPath)) {
        return false;
    }
    
    QString output;
    QStringList args;
    args << "service" << "status";
    
    executeCommand(cliPath, args, output);
    
    return output.contains("running") || output.contains("运行中") || output.contains("Running");
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

int ETRunService::executeCommand(const QString &command, const QStringList &args, QString &output)
{
    QProcess process;
    process.setWorkingDirectory(getWorkingDirectory());
    process.start(command, args);
    
    if (!process.waitForFinished(30000)) {  // 30秒超时
        process.kill();
        process.waitForFinished(3000);
        output = "操作超时";
        return -1;
    }
    
    output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    return process.exitCode();
}

bool ETRunService::isServiceInstalled()
{
    QSettings settings(R"(HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services)", QSettings::NativeFormat);
    return settings.childGroups().contains("QtET Web Connector");
}
