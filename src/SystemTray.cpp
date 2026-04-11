#include "SystemTray.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <QThread>

SystemTray::SystemTray(QObject *parent)
    : QObject(parent)
    , m_connectionState(ConnectionState::NotStarted)
    , m_autoStart(false)
    , m_autoReconnect(false)
    , m_isAutoStartMode(false)
    , m_progressDialog(nullptr)
    , m_stopProgressDialog(nullptr)
    , m_reconnectAfterStop(false)
{
    qDebug() << "SystemTray::SystemTray() 开始构造";
    
    try {
        // 初始化配置管理器
        m_configManager = new ConfigManager(this);
        qDebug() << "ConfigManager 创建完成";
        
        bool loadResult = m_configManager->loadConfig();
        qDebug() << "ConfigManager::loadConfig() 结果:" << loadResult;
    
    // 兼容旧的设置
    m_settings = new QSettings("EasyTier", "Connector", this);
    loadSettings();
    
    // 如果旧设置存在，优先使用新设置并保存到ConfigManager
    if (!m_settings->value("connectionKey", "").toString().isEmpty()) {
        m_connectionKey = m_settings->value("connectionKey", "").toString();
        m_autoStart = m_settings->value("autoStart", false).toBool();
        m_autoReconnect = m_settings->value("autoReconnect", false).toBool();
        m_configManager->setConnectionKey(m_connectionKey);
        m_configManager->setAutoStart(m_autoStart);
        m_configManager->setAutoReconnect(m_autoReconnect);
        m_configManager->saveConfig();
    }
    
    // 重新加载ConfigManager的配置以确保是最新的
    m_configManager->loadConfig();
    qDebug() << "ConfigManager 重新加载完成";
    
    loadSettings();
    qDebug() << "loadSettings() 完成";
    
    // 创建系统托盘图标
    m_trayIcon = new QSystemTrayIcon(this);
    qDebug() << "QSystemTrayIcon 创建完成";
    
    // 检查系统托盘图标是否支持
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "系统托盘不可用";
        // 不要在这里返回，继续执行，但记录错误
    }
    
    QIcon icon(":/assets/favicon.svg");
    if (icon.isNull()) {
        qDebug() << "无法加载图标资源";
    } else {
        qDebug() << "图标资源加载成功";
    }
    m_trayIcon->setIcon(icon);
    qDebug() << "设置图标完成";
    
    m_trayIcon->setToolTip("EasyTier 控制台连接器");
    qDebug() << "设置工具提示完成";
    
    // 检查系统托盘是否支持上下文菜单
    if (!QSystemTrayIcon::supportsMessages()) {
        qDebug() << "系统托盘不支持消息显示";
    }
    
    // 创建菜单
    m_menu = new QMenu();
    qDebug() << "QMenu 创建完成";
    
    setupMenu();
    qDebug() << "setupMenu() 完成";
    
    m_trayIcon->setContextMenu(m_menu);
    qDebug() << "设置上下文菜单完成";
    
    // 连接信号
    connect(m_trayIcon, &QSystemTrayIcon::activated, 
            this, &SystemTray::onTrayActivated);
    qDebug() << "信号连接完成";
    
    // 初始化对话框
    m_settingsDialog = new SettingsDialog(m_connectionKey);
    qDebug() << "SettingsDialog 创建完成";
    
    m_aboutDialog = new AboutDialog();
    qDebug() << "AboutDialog 创建完成";
    
    // 初始化EasyTier进程管理器
    m_etWorker = new ETRunWorkerWin(this);
    qDebug() << "ETRunWorkerWin 创建完成";
    
    // 连接EasyTier进程管理器信号
    connect(m_etWorker, &ETRunWorkerWin::started, 
            this, &SystemTray::onEasyTierStarted);
    connect(m_etWorker, &ETRunWorkerWin::startFailed, 
            this, &SystemTray::onEasyTierStartFailed);
    connect(m_etWorker, &ETRunWorkerWin::stopped, 
            this, &SystemTray::onEasyTierStopped);
    
    // 连接应用退出信号，确保退出前停止进程
    connect(qApp, &QApplication::aboutToQuit, 
            this, &SystemTray::onApplicationQuit);

    // 连接设置对话框的密钥更新信号，实现修改后自动连接
    connect(m_settingsDialog, &SettingsDialog::connectionKeyChanged,
            this, &SystemTray::onConnectionKeyChanged);
    
    // 检查是否需要自动回连
    if (m_autoReconnect && !m_connectionKey.isEmpty()) {
        // 延迟启动以确保UI完全初始化
        QTimer::singleShot(1000, [this]() {
            showStartProgressDialog();
            updateStatus(ConnectionState::Connecting);
            m_etWorker->start(m_connectionKey);
        });
    }
    
    qDebug() << "SystemTray::SystemTray() 构造完成";
    }
    catch (const std::exception& e) {
        qDebug() << "SystemTray 构造函数中发生异常:" << e.what();
    }
    catch (...) {
        qDebug() << "SystemTray 构造函数中发生未知异常";
    }
}

SystemTray::~SystemTray()
{
    qDebug() << "SystemTray::~SystemTray() 开始析构";
    
    try {
        m_configManager->saveConfig();
        saveSettings();

        delete m_etWorker;
        delete m_settingsDialog;
        delete m_aboutDialog;
        delete m_progressDialog;
        delete m_stopProgressDialog;

    }
    catch (const std::exception& e) {
        qDebug() << "SystemTray 析构函数中发生异常:" << e.what();
    }
    catch (...) {
        qDebug() << "SystemTray 析构函数中发生未知异常";
    }
    
    qDebug() << "SystemTray::~SystemTray() 析构完成";
}

void SystemTray::show() const
{
    // 延迟显示系统托盘图标，确保应用程序事件循环已启动
    QTimer::singleShot(100, [this]() {
        m_trayIcon->show();
        // 只有在非开机自启模式下才显示启动消息
        if (!m_isAutoStartMode) {
            m_trayIcon->showMessage("EasyTier 控制台连接器", "EasyTier 控制台连接器已启动", QSystemTrayIcon::Information, 3000);
        }
    });
}

void SystemTray::setupMenu()
{
    // 标题
    m_titleAction = new QAction(QIcon(":/assets/favicon.svg"), "ET控制台连接器", this);
    QFont titleFont = m_titleAction->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    m_titleAction->setFont(titleFont);
    m_menu->addAction(m_titleAction);
    
    // 状态
    m_statusAction = new QAction(QIcon(":/assets/status-red.svg"), "状态：未启动", this);
    m_menu->addAction(m_statusAction);
    
    // 分隔符
    m_separator1 = m_menu->addSeparator();
    
    // 启动/停止连接
    m_toggleConnectionAction = new QAction(QIcon(":/assets/connect.svg"), "启动连接", this);
    connect(m_toggleConnectionAction, &QAction::triggered, 
            this, &SystemTray::onToggleConnection);
    m_menu->addAction(m_toggleConnectionAction);
    
    // 分隔符
    m_separator2 = m_menu->addSeparator();
    
    // 打开Web控制台
    m_openWebConsoleAction = new QAction(QIcon(":/assets/webconsole.svg"), "打开Web控制台", this);
    connect(m_openWebConsoleAction, &QAction::triggered, 
            this, &SystemTray::onOpenWebConsole);
    m_menu->addAction(m_openWebConsoleAction);
    
    // 设置连接地址密钥
    m_settingsAction = new QAction(QIcon(":/assets/settings.svg"), "设置连接地址密钥", this);
    connect(m_settingsAction, &QAction::triggered, 
            this, &SystemTray::onSettings);
    m_menu->addAction(m_settingsAction);
    
    // 分隔符
    m_separator3 = m_menu->addSeparator();
    
    m_autoStartAction = new QAction("开机自启", this);
    m_autoStartAction->setIcon(QIcon(":/assets/startup.svg"));
    m_autoStartAction->setCheckable(true);
    m_autoStartAction->setChecked(m_autoStart);
    connect(m_autoStartAction, &QAction::toggled, 
            this, &SystemTray::onAutoStart);
    m_menu->addAction(m_autoStartAction);
    
    // 自动回连
    m_autoReconnectAction = new QAction("自动回连", this);
    m_autoReconnectAction->setIcon(QIcon(":/assets/autoconnect.svg"));
    m_autoReconnectAction->setCheckable(true);
    m_autoReconnectAction->setChecked(m_autoReconnect);
    connect(m_autoReconnectAction, &QAction::toggled, 
            this, &SystemTray::onAutoReconnect);
    m_menu->addAction(m_autoReconnectAction);
    
    // 分隔符
    m_separator4 = m_menu->addSeparator();
    
    // 关于软件
    m_aboutAction = new QAction(QIcon(":/assets/about.svg"), "关于软件", this);
    connect(m_aboutAction, &QAction::triggered, 
            this, &SystemTray::onAbout);
    m_menu->addAction(m_aboutAction);
    
    // 退出软件
    m_quitAction = new QAction(QIcon(":/assets/quit.svg"), "退出软件", this);
    connect(m_quitAction, &QAction::triggered, 
            this, &SystemTray::onQuit);
    m_menu->addAction(m_quitAction);
}

void SystemTray::updateStatus(ConnectionState state)
{
    m_connectionState = state;
    
    QString statusText;
    QString tooltipText;
    QString statusIcon;
    
    switch (state) {
    case ConnectionState::NotStarted:
        statusText = "状态：未启动";
        tooltipText = "EasyTier 控制台连接器 - 未启动";
        statusIcon = ":/assets/status-red.svg";
        break;
    case ConnectionState::Connecting:
        statusText = "状态：连接中";
        tooltipText = "EasyTier 控制台连接器 - 连接中";
        statusIcon = ":/assets/status-yellow.svg";
        break;
    case ConnectionState::Connected:
        statusText = "状态：已连接";
        tooltipText = "EasyTier 控制台连接器 - 已连接";
        statusIcon = ":/assets/status-green.svg";
        break;
    }
    
    m_statusAction->setText(statusText);
    m_statusAction->setIcon(QIcon(statusIcon));
    m_trayIcon->setToolTip(tooltipText);
    updateConnectionActions();
}

void SystemTray::updateConnectionActions() const
{
    switch (m_connectionState) {
    case ConnectionState::NotStarted:
        m_toggleConnectionAction->setText("启动连接");
        m_toggleConnectionAction->setIcon(QIcon(":/assets/connect.svg"));
        m_toggleConnectionAction->setEnabled(true);
        break;
    case ConnectionState::Connecting:
        m_toggleConnectionAction->setText("停止连接");
        m_toggleConnectionAction->setIcon(QIcon(":/assets/disconnect.svg"));
        m_toggleConnectionAction->setEnabled(true);
        break;
    case ConnectionState::Connected:
        m_toggleConnectionAction->setText("停止连接");
        m_toggleConnectionAction->setIcon(QIcon(":/assets/disconnect.svg"));
        m_toggleConnectionAction->setEnabled(true);
        break;
    }
}

void SystemTray::loadSettings()
{
    m_autoStart = m_configManager->getAutoStart();
    m_autoReconnect = m_configManager->getAutoReconnect();
    m_connectionKey = m_configManager->getConnectionKey();
}

void SystemTray::saveSettings() const
{
    m_configManager->setAutoStart(m_autoStart);
    m_configManager->setAutoReconnect(m_autoReconnect);
    m_configManager->setConnectionKey(m_connectionKey);
    m_configManager->saveConfig();
}

void SystemTray::onToggleConnection()
{
    if (m_connectionState == ConnectionState::NotStarted) {
        // 启动EasyTier
        showStartProgressDialog();
        updateStatus(ConnectionState::Connecting);
        m_etWorker->start(m_connectionKey);
    } else {
        // 停止EasyTier
        showStopProgressDialog();
        updateStatus(ConnectionState::Connecting);
        m_etWorker->stop();
    }
}

void SystemTray::onOpenWebConsole()
{
    // 打开Web控制台
#ifndef IS_NOT_ET_PRO
    QDesktopServices::openUrl(QUrl("https://console.easytier.net/"));
#else
    QMessageBox::information(nullptr,"提示","暂不支持");
#endif
}

void SystemTray::onSettings()
{
    m_settingsDialog->setConnectionKey(m_connectionKey);
    if (m_settingsDialog->exec() == QDialog::Accepted) {
        m_connectionKey = m_settingsDialog->getConnectionKey();
        m_configManager->setConnectionKey(m_connectionKey);
        m_configManager->saveConfig();
    }
}

void SystemTray::onAutoStart(bool checked)
{
    m_autoStart = checked;
    m_configManager->setAutoStart(m_autoStart);
    m_configManager->saveConfig();
    
    // 实现开机自启功能 - 使用计划任务
    if (checked) {
        // 创建计划任务
        createScheduledTask();
    } else {
        // 删除计划任务
        deleteScheduledTask();
    }
}

void SystemTray::createScheduledTask()
{
    // 获取程序路径
    const QString &appPath = QApplication::applicationFilePath();
    const QString &appName = "EasyTierConnector";
    
    // 构建参数列表
    QStringList args;
    args << "/create"
         << "/tn" << appName
         << "/tr" << appPath
//         << "/sd" << QCoreApplication::applicationDirPath()
         << "/sc" << "onlogon"
         << "/rl" << "highest"
         << "/f";
    
    // 执行命令
    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();
    
    qDebug() << "创建开机自启任务";
    qDebug() << "参数:" << args;
    qDebug() << "输出:" << process.readAllStandardOutput();
    qDebug() << "错误:" << process.readAllStandardError();
}

void SystemTray::deleteScheduledTask()
{
    QString appName = "EasyTierConnector";
    
    // 构建参数列表
    QStringList args;
    args << "/delete"
         << "/tn" << appName
         << "/f";
    
    // 执行命令
    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();
    
    qDebug() << "删除开机自启任务";
    qDebug() << "参数:" << args;
    qDebug() << "输出:" << process.readAllStandardOutput();
    qDebug() << "错误:" << process.readAllStandardError();
}

void SystemTray::onAutoReconnect(bool checked)
{
    m_autoReconnect = checked;
    m_configManager->setAutoReconnect(m_autoReconnect);
    m_configManager->saveConfig();
}

void SystemTray::onAbout() const
{
    m_aboutDialog->exec();
}

void SystemTray::onQuit() const
{
    saveSettings();
    QApplication::quit();
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    // 双击托盘图标打开Web控制台
    if (reason == QSystemTrayIcon::DoubleClick) {
        onOpenWebConsole();
    }
}

void SystemTray::onEasyTierStarted()
{
    closeStopProgressDialog();
    closeProgressDialog();
    updateStatus(ConnectionState::Connected);
    
    // 显示成功消息
    m_trayIcon->showMessage("EasyTier", "EasyTier启动成功", QSystemTrayIcon::Information, 3000);
}

void SystemTray::onEasyTierStartFailed(const QString &error)
{
    closeStopProgressDialog();
    closeProgressDialog();
    updateStatus(ConnectionState::NotStarted);
    
    // 重置重连标志，避免后续错误的重连尝试
    m_reconnectAfterStop = false;
    
    // 检查是否是异常终止消息
    if (error.contains("EasyTier Core 程序异常终止")) {
        m_trayIcon->showMessage("EasyTier", error, QSystemTrayIcon::Critical, 10000);
    } else {
        // 显示普通错误消息
        m_trayIcon->showMessage("Error", "EasyTier Core启动失败: " + error, QSystemTrayIcon::Warning, 5000);
    }
}

void SystemTray::onEasyTierStopped()
{
    closeStopProgressDialog();
    closeProgressDialog();
    updateStatus(ConnectionState::NotStarted);
    
    // 检查是否需要重新连接
    if (m_reconnectAfterStop) {
        m_reconnectAfterStop = false;
        // 延迟500ms后再重新连接，确保进程资源完全释放
        QTimer::singleShot(500, this, &SystemTray::onToggleConnection);
        return;
    }
    
    // 显示成功消息
    m_trayIcon->showMessage("通知", "EasyTier Core已停止", QSystemTrayIcon::Information, 3000);
}

void SystemTray::onEasyTierCrashed(const QString &error)
{
    closeStopProgressDialog();
    closeProgressDialog();
    updateStatus(ConnectionState::NotStarted);
    
    // 检查是否是异常终止消息
    if (error.contains("EasyTier Core 程序异常终止")) {
        // 显示异常终止消息
        m_trayIcon->showMessage("EasyTier", error, QSystemTrayIcon::Critical, 10000);
    }
}

void SystemTray::showStartProgressDialog()
{
    if (m_progressDialog == nullptr) {
        m_progressDialog = new QProgressDialog("正在启动EasyTier...", "取消", 0, 0, nullptr);
        m_progressDialog->setWindowTitle("启动EasyTier");
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->show();
    }
}

void SystemTray::closeProgressDialog()
{
    if (m_progressDialog != nullptr) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void SystemTray::showStopProgressDialog()
{
    if (m_stopProgressDialog == nullptr) {
        m_stopProgressDialog = new QProgressDialog("正在停止EasyTier...", "取消", 0, 0, nullptr);
        m_stopProgressDialog->setWindowTitle("停止EasyTier");
        m_stopProgressDialog->setWindowModality(Qt::WindowModal);
        m_stopProgressDialog->setCancelButton(nullptr);
        m_stopProgressDialog->show();
    }
}

void SystemTray::closeStopProgressDialog()
{
    if (m_stopProgressDialog != nullptr) {
        m_stopProgressDialog->close();
        delete m_stopProgressDialog;
        m_stopProgressDialog = nullptr;
    }
}

void SystemTray::onApplicationQuit()
{
    qDebug() << "应用程序即将退出，停止 EasyTier 进程...";
    
    // 保存设置
    saveSettings();
    
    // 检查是否有进程在运行
    if (m_etWorker->isRunning()) {
        // 显示停止进度对话框
        showStopProgressDialog();
        QApplication::processEvents();

        // 强制停止并等待进程结束
        m_etWorker->stopAndWait();
        
        // 关闭进度对话框
        closeStopProgressDialog();
    }
    
    qDebug() << "EasyTier 进程已停止，应用程序可以退出";
}

void SystemTray::onConnectionKeyChanged()
{
    // 更新连接密钥
    m_connectionKey = m_settingsDialog->getConnectionKey();
    m_configManager->setConnectionKey(m_connectionKey);
    m_configManager->saveConfig();
    
    // 获取进程实际状态
    ETRunWorkerWin::State workerState = m_etWorker->state();
    
    switch (workerState) {
    case ETRunWorkerWin::State::NotStarted:
        // 进程未运行，直接启动连接
        onToggleConnection();
        break;
        
    case ETRunWorkerWin::State::Stopping:
        // 进程正在停止中，设置标志等待停止完成后重新连接
        m_reconnectAfterStop = true;
        break;
        
    case ETRunWorkerWin::State::Starting:
    case ETRunWorkerWin::State::Running:
        // 进程正在启动或运行中，先停止再重新连接
        m_reconnectAfterStop = true;
        showStopProgressDialog();
        updateStatus(ConnectionState::Connecting);
        m_etWorker->stop();
        break;
    }
}


