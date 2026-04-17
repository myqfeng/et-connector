/**
 * @file SystemTray.cpp
 * @brief 系统托盘主类实现
 */

#include "SystemTray.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <iostream>

SystemTray::SystemTray(QObject *parent)
    : QObject(parent)
{
    std::clog << "SystemTray: 初始化开始" << std::endl;
    
    // 1. 初始化配置管理器
    m_configManager = new ConfigManager(this);
    if (!m_configManager->loadConfig()) {
        std::cerr << "SystemTray: 配置加载失败，使用默认配置" << std::endl;
    }
    
    // 1.5. 初始化 Casdoor 登录器
    m_casdoorLogin = new CasdoorLogin("d3ff87a9cd6317695066", this);
    connect(m_casdoorLogin, &CasdoorLogin::loginSuccess, this, [this](const QString &accessToken, const QString &idToken) {
        // 登录成功，显示对话框显示 token
        QString message = "Access Token:\n" + accessToken + "\n\nID Token:\n" + idToken;
        QMessageBox::information(nullptr, "登录成功", message);
    });
    connect(m_casdoorLogin, &CasdoorLogin::loginFailed, this, [this](const QString &errorMessage) {
        QMessageBox::warning(nullptr, "登录失败", errorMessage);
    });
    
    // 2. 加载配置
    loadSettings();
    
    // 3. 创建系统托盘图标
    m_trayIcon = new QSystemTrayIcon(this);
    QIcon icon(":/assets/favicon.svg");
    if (icon.isNull()) {
        std::cerr << "SystemTray: 无法加载托盘图标" << std::endl;
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("EasyTier 控制台连接器");
    
    // 4. 创建菜单
    m_menu = new QMenu();
    setupMenu();
    m_trayIcon->setContextMenu(m_menu);
    
    // 5. 连接托盘激活信号
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &SystemTray::onTrayActivated);

    // 6. 检查服务是否已在运行
    if (ETRunService::isRunning()) {
        updateStatus(ConnectionState::Connected);
        std::clog << "SystemTray: EasyTier 服务已在运行中" << std::endl;
    }
    
    // 7. 启动心跳定时器，每2秒检测 easytier-core 进程状态
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &SystemTray::onHeartbeat);
    m_heartbeatTimer->start(2000);
    
    std::clog << "SystemTray: 初始化完成" << std::endl;
}

SystemTray::~SystemTray()
{
    std::clog << "SystemTray: 析构开始" << std::endl;
    
    // 保存配置
    if (m_configManager) {
        m_configManager->saveConfig();
    }
    
    // 对话框由 QPointer 自动管理，无需手动删除
    
    std::clog << "SystemTray: 析构完成" << std::endl;
}

void SystemTray::show() const
{
    // 延迟显示，确保 UI 完全初始化
    QTimer::singleShot(100, [this]() {
        m_trayIcon->show();
        if (!m_isAutoStartMode) {
            m_trayIcon->showMessage(
                "EasyTier 控制台连接器", 
                "EasyTier 控制台连接器已启动", 
                QSystemTrayIcon::Information, 
                3000
            );
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
    
    // 打开 Web 控制台
    m_openWebConsoleAction = new QAction(QIcon(":/assets/webconsole.svg"), "打开Web控制台", this);
    connect(m_openWebConsoleAction, &QAction::triggered, this, &SystemTray::onOpenWebConsole);
    m_menu->addAction(m_openWebConsoleAction);
    
    // 登录 EasyTier Pro
    m_loginEasyTierProAction = new QAction("登录 EasyTier Pro", this);
    connect(m_loginEasyTierProAction, &QAction::triggered, this, &SystemTray::onLoginEasyTierPro);
    m_menu->addAction(m_loginEasyTierProAction);
    
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
    
    // 开机自启
    m_autoStartAction = new QAction("开机启动托盘", this);
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
    
    // 同步注册表实际状态到 UI
    bool registered = isAutoStartRegistered();
    if (m_autoStart != registered) {
        m_autoStart = registered;
        m_configManager->setAutoStart(m_autoStart);
        m_configManager->saveConfig();
    }
}

void SystemTray::saveSettings()
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
        showProgressDialog("正在启动 EasyTier 服务...");
        updateStatus(ConnectionState::Connecting);
        
        bool success = ETRunService::start(m_connectionKey);
        closeProgressDialog();
        
        if (success) {
            updateStatus(ConnectionState::Connected);
            m_trayIcon->showMessage("EasyTier", "EasyTier 服务启动成功", 
                                    QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("错误", "EasyTier 服务启动失败", 
                                    QSystemTrayIcon::Warning, 5000);
        }
    } else if (m_connectionState == ConnectionState::Connected) {
        // 停止服务
        showProgressDialog("正在停止 EasyTier 服务...");
        updateStatus(ConnectionState::Connecting);
        
        bool success = ETRunService::stop();
        closeProgressDialog();
        
        if (success) {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("通知", "EasyTier 服务已停止", 
                                    QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::Connected);
            m_trayIcon->showMessage("错误", "EasyTier 服务停止失败", 
                                    QSystemTrayIcon::Warning, 5000);
        }
    }
}

void SystemTray::onOpenWebConsole()
{
#ifndef IS_NOT_ET_PRO
    QDesktopServices::openUrl(QUrl("https://console.easytier.net/"));
#else
    QMessageBox::information(nullptr, "提示", "暂不支持");
#endif
}

void SystemTray::onLoginEasyTierPro()
{
    // 启动 OAuth 登录流程
    m_casdoorLogin->startLogin();
}

void SystemTray::onSettings()
{
    // 创建或复用对话框
    if (m_settingsDialog.isNull()) {
        m_settingsDialog = new SettingsDialog(m_connectionKey);
        connect(m_settingsDialog.data(), &SettingsDialog::connectionKeyChanged,
                this, &SystemTray::onConnectionKeyChanged);
    }
    
    m_settingsDialog->setConnectionKey(m_connectionKey);
    
    if (m_settingsDialog->exec() == QDialog::Accepted) {
        m_connectionKey = m_settingsDialog->getConnectionKey();
        m_configManager->setConnectionKey(m_connectionKey);
        m_configManager->saveConfig();
    }
    
    // QPointer 会在对象删除后自动变为 nullptr
    if (!m_settingsDialog.isNull()) {
        m_settingsDialog->deleteLater();
    }
}

void SystemTray::onAutoStart(bool checked)
{
    m_autoStart = checked;
    m_configManager->setAutoStart(m_autoStart);
    m_configManager->saveConfig();
    
    bool success;
    if (checked) {
        success = registerAutoStart();
    } else {
        success = unregisterAutoStart();
    }
    
    if (!success) {
        // 操作失败，恢复 UI 状态
        m_autoStartAction->blockSignals(true);
        m_autoStartAction->setChecked(!checked);
        m_autoStartAction->blockSignals(false);
        
        QMessageBox::warning(nullptr, "提示", 
            checked ? "设置开机自启失败" : "取消开机自启失败");
    }
}

bool SystemTray::registerAutoStart()
{
    const QString appPath = QApplication::applicationFilePath();
    QString value = QString("\"%1\" --auto-start").arg(appPath);

    // 在Windows下要改成反斜杠路径
#ifdef Q_OS_WIN32
    value.replace('/', '\\');
#endif

    
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    settings.setValue("EasyTierConnector", value);
    settings.sync();
    
    bool success = (settings.status() == QSettings::NoError);
    std::clog << "注册开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
}

bool SystemTray::unregisterAutoStart()
{
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    settings.remove("EasyTierConnector");
    settings.sync();
    
    bool success = (settings.status() == QSettings::NoError);
    std::clog << "取消开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
}

bool SystemTray::isAutoStartRegistered()
{
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    return settings.contains("EasyTierConnector");
}

void SystemTray::onAbout()
{
    if (m_aboutDialog.isNull()) {
        m_aboutDialog = new AboutDialog();
    }
    m_aboutDialog->exec();
    
    if (!m_aboutDialog.isNull()) {
        m_aboutDialog->deleteLater();
    }
}

void SystemTray::onQuit()
{
    saveSettings();
    QApplication::quit();
}

void SystemTray::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    // 双击打开 Web 控制台
    if (reason == QSystemTrayIcon::DoubleClick) {
        onOpenWebConsole();
    }
}

void SystemTray::onHeartbeat()
{
    bool running = ETRunService::isRunning();
    
    // 只在状态发生变化时更新 UI，避免频繁刷新
    if (running && m_connectionState != ConnectionState::Connected) {
        updateStatus(ConnectionState::Connected);
        std::clog << "SystemTray: 心跳检测 - 服务已上线" << std::endl;
    } else if (!running && m_connectionState == ConnectionState::Connected) {
        updateStatus(ConnectionState::NotStarted);
        std::clog << "SystemTray: 心跳检测 - 服务已离线" << std::endl;
    }
}

void SystemTray::onConnectionKeyChanged()
{
    if (m_settingsDialog.isNull()) {
        return;
    }
    
    m_connectionKey = m_settingsDialog->getConnectionKey();
    m_configManager->setConnectionKey(m_connectionKey);
    m_configManager->saveConfig();
    
    // 如果服务已运行，重启服务
    if (ETRunService::isRunning()) {
        showProgressDialog("正在重启 EasyTier 服务...");
        
        bool stopped = ETRunService::stop();
        bool started = false;
        
        if (stopped) {
            // 等待资源释放
            QThread::msleep(500);
            started = ETRunService::start(m_connectionKey);
        }
        
        closeProgressDialog();
        
        if (started) {
            updateStatus(ConnectionState::Connected);
            m_trayIcon->showMessage("EasyTier", "服务已重启", 
                                    QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("错误", "服务重启失败", 
                                    QSystemTrayIcon::Warning, 5000);
        }
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
        m_progressDialog->deleteLater();
        m_progressDialog = nullptr;
    }
}