#include "SystemTray.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

SystemTray::SystemTray(QObject *parent)
    : QObject(parent)
    , m_connectionState(ConnectionState::NotStarted)
    , m_autoStart(false)
    , m_isAutoStartMode(false)
{
    qDebug() << "SystemTray::SystemTray() 开始构造";
    
    try {
        // 初始化配置管理器
        m_configManager = new ConfigManager(this);
        m_configManager->loadConfig();
    
        // 兼容旧的设置
        m_settings = new QSettings("EasyTier", "Connector", this);
        loadSettings();
        
        // 如果旧设置存在，迁移到ConfigManager
        if (!m_settings->value("connectionKey", "").toString().isEmpty()) {
            m_connectionKey = m_settings->value("connectionKey", "").toString();
            m_autoStart = m_settings->value("autoStart", false).toBool();
            m_configManager->setConnectionKey(m_connectionKey);
            m_configManager->setAutoStart(m_autoStart);
            m_configManager->saveConfig();
        }
        
        // 重新加载配置
        m_configManager->loadConfig();
        loadSettings();
        
        // 创建系统托盘图标
        m_trayIcon = new QSystemTrayIcon(this);
        QIcon icon(":/assets/favicon.svg");
        m_trayIcon->setIcon(icon);
        m_trayIcon->setToolTip("EasyTier 控制台连接器");
        
        // 创建菜单
        m_menu = new QMenu();
        setupMenu();
        m_trayIcon->setContextMenu(m_menu);
        
        // 连接信号
        connect(m_trayIcon, &QSystemTrayIcon::activated, 
                this, &SystemTray::onTrayActivated);

        // 初始化EasyTier服务管理器
        m_etService = new ETRunService(this);
        
        // 检查服务是否已经在运行
        if (m_etService->isRunning()) {
            updateStatus(ConnectionState::Connected);
            qDebug() << "EasyTier 服务已在运行中";
        }
        
        qDebug() << "SystemTray::SystemTray() 构造完成";
    }
    catch (const std::exception& e) {
        qDebug() << "SystemTray 构造函数中发生异常:" << e.what();
    }
}

SystemTray::~SystemTray()
{
    qDebug() << "SystemTray::~SystemTray() 开始析构";
    
    try {
        m_configManager->saveConfig();
        saveSettings();

        if (m_settingsDialog != nullptr) {
            delete m_settingsDialog;
            m_settingsDialog = nullptr;
        }
        if (m_aboutDialog != nullptr) {
            delete m_aboutDialog;
            m_aboutDialog = nullptr;
        }
        if (m_progressDialog != nullptr) {
            delete m_progressDialog;
            m_progressDialog = nullptr;
        }
    }
    catch (const std::exception& e) {
        qDebug() << "SystemTray 析构函数中发生异常:" << e.what();
    }
    
    qDebug() << "SystemTray::~SystemTray() 析构完成";
}

void SystemTray::show() const
{
    QTimer::singleShot(100, [this]() {
        m_trayIcon->show();
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
    
    m_separator1 = m_menu->addSeparator();
    
    // 启动/停止连接
    m_toggleConnectionAction = new QAction(QIcon(":/assets/connect.svg"), "启动连接", this);
    connect(m_toggleConnectionAction, &QAction::triggered, this, &SystemTray::onToggleConnection);
    m_menu->addAction(m_toggleConnectionAction);
    
    m_separator2 = m_menu->addSeparator();
    
    // 打开Web控制台
    m_openWebConsoleAction = new QAction(QIcon(":/assets/webconsole.svg"), "打开Web控制台", this);
    connect(m_openWebConsoleAction, &QAction::triggered, this, &SystemTray::onOpenWebConsole);
    m_menu->addAction(m_openWebConsoleAction);
    
    // 设置
#ifdef IS_NOT_ET_PRO
    const QString settingsText = "设置连接地址与用户名";
#else
    const QString settingsText = "设置连接地址与密钥";
#endif
    m_settingsAction = new QAction(QIcon(":/assets/settings.svg"), settingsText, this);
    connect(m_settingsAction, &QAction::triggered, this, &SystemTray::onSettings);
    m_menu->addAction(m_settingsAction);
    
    m_separator3 = m_menu->addSeparator();
    
    // 开机自启（仅控制托盘程序）
    m_autoStartAction = new QAction("开机自启", this);
    m_autoStartAction->setIcon(QIcon(":/assets/startup.svg"));
    m_autoStartAction->setCheckable(true);
    m_autoStartAction->setChecked(m_autoStart);
    m_autoStartAction->setToolTip("设置托盘程序开机自启");
    connect(m_autoStartAction, &QAction::toggled, this, &SystemTray::onAutoStart);
    m_menu->addAction(m_autoStartAction);
    
    m_separator4 = m_menu->addSeparator();
    
    // 关于
    m_aboutAction = new QAction(QIcon(":/assets/about.svg"), "关于软件", this);
    connect(m_aboutAction, &QAction::triggered, this, &SystemTray::onAbout);
    m_menu->addAction(m_aboutAction);
    
    // 退出
    m_quitAction = new QAction(QIcon(":/assets/quit.svg"), "退出软件", this);
    connect(m_quitAction, &QAction::triggered, this, &SystemTray::onQuit);
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
        statusText = "状态：Waiting";
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
        m_toggleConnectionAction->setText("处理中...");
        m_toggleConnectionAction->setIcon(QIcon(":/assets/disconnect.svg"));
        m_toggleConnectionAction->setEnabled(false);
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
    m_connectionKey = m_configManager->getConnectionKey();
}

void SystemTray::saveSettings() const
{
    m_configManager->setAutoStart(m_autoStart);
    m_configManager->setConnectionKey(m_connectionKey);
    m_configManager->saveConfig();
}

void SystemTray::onToggleConnection()
{
    if (m_connectionKey.isEmpty()) {
        QMessageBox::warning(nullptr, "提示", "请先设置连接地址与密钥");
        return;
    }
    
    if (m_connectionState == ConnectionState::NotStarted) {
        // 启动服务
        showProgressDialog("正在启动EasyTier服务...");
        updateStatus(ConnectionState::Connecting);
        
        bool success = m_etService->start(m_connectionKey);
        closeProgressDialog();
        
        if (success) {
            updateStatus(ConnectionState::Connected);
            m_trayIcon->showMessage("EasyTier", "EasyTier 服务启动成功", QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("错误", "EasyTier 服务启动失败", QSystemTrayIcon::Warning, 5000);
        }
    } else if (m_connectionState == ConnectionState::Connected) {
        // 停止服务
        showProgressDialog("正在停止EasyTier服务...");
        updateStatus(ConnectionState::Connecting);
        
        bool success = m_etService->stop();
        closeProgressDialog();
        
        if (success) {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("通知", "EasyTier 服务已停止", QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::Connected);
            m_trayIcon->showMessage("错误", "EasyTier 服务停止失败", QSystemTrayIcon::Warning, 5000);
        }
    }
}

void SystemTray::onOpenWebConsole()
{
#ifndef IS_NOT_ET_PRO
    QDesktopServices::openUrl(QUrl("https://console.easytier.net/"));
#else
    QMessageBox::information(nullptr,"提示","暂不支持");
#endif
}

void SystemTray::onSettings()
{
    if (m_settingsDialog == nullptr) {
        m_settingsDialog = new SettingsDialog(m_connectionKey);
        connect(m_settingsDialog, &SettingsDialog::connectionKeyChanged,
                this, &SystemTray::onConnectionKeyChanged);
    }
    
    m_settingsDialog->setConnectionKey(m_connectionKey);
    if (m_settingsDialog->exec() == QDialog::Accepted) {
        m_connectionKey = m_settingsDialog->getConnectionKey();
        m_configManager->setConnectionKey(m_connectionKey);
        m_configManager->saveConfig();
    }
    
    m_settingsDialog->deleteLater();
    m_settingsDialog = nullptr;
}

void SystemTray::onAutoStart(bool checked)
{
    m_autoStart = checked;
    m_configManager->setAutoStart(m_autoStart);
    m_configManager->saveConfig();
    
    if (checked) {
        createScheduledTask();
    } else {
        deleteScheduledTask();
    }
}

void SystemTray::createScheduledTask()
{
    const QString appPath = QString("\"%1\"").arg(QApplication::applicationFilePath());
    const QString appName = "EasyTierConnector";
    
    QStringList args;
    args << "/create"
         << "/tn" << appName
         << "/tr" << appPath
         << "/sc" << "onlogon"
         << "/rl" << "highest"
         << "/f";
    
    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();
    
    qDebug() << "创建开机自启任务:" << process.readAllStandardOutput();
}

void SystemTray::deleteScheduledTask()
{
    QStringList args;
    args << "/delete"
         << "/tn" << "EasyTierConnector"
         << "/f";
    
    QProcess process;
    process.start("schtasks.exe", args);
    process.waitForFinished();
    
    qDebug() << "删除开机自启任务:" << process.readAllStandardOutput();
}

void SystemTray::onAbout()
{
    if (m_aboutDialog == nullptr) {
        m_aboutDialog = new AboutDialog();
    }
    m_aboutDialog->exec();
    m_aboutDialog->deleteLater();
    m_aboutDialog = nullptr;
}

void SystemTray::onQuit() const
{
    saveSettings();
    QApplication::quit();
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        onOpenWebConsole();
    }
}

void SystemTray::onConnectionKeyChanged()
{
    if (m_settingsDialog == nullptr) {
        return;
    }
    
    m_connectionKey = m_settingsDialog->getConnectionKey();
    m_configManager->setConnectionKey(m_connectionKey);
    m_configManager->saveConfig();
    
    // 如果服务已运行，先停止再重启
    if (m_etService->isRunning()) {
        showProgressDialog("正在重启EasyTier服务...");
        m_etService->stop();
        m_etService->start(m_connectionKey);
        closeProgressDialog();
        updateStatus(ConnectionState::Connected);
    }
}

void SystemTray::showProgressDialog(const QString &text)
{
    if (m_progressDialog == nullptr) {
        m_progressDialog = new QProgressDialog(text, QString(), 0, 0, nullptr);
        m_progressDialog->setWindowTitle("请稍候");
        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setCancelButton(nullptr);
        m_progressDialog->setMinimumDuration(0);
    }
    m_progressDialog->setLabelText(text);
    m_progressDialog->show();
    QApplication::processEvents();
}

void SystemTray::closeProgressDialog()
{
    if (m_progressDialog != nullptr) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}
