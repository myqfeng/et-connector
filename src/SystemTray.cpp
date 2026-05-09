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
#include <QDir>
#include <QRegularExpression>

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
    connect(m_casdoorLogin, &CasdoorLogin::loginSuccess, this, [this](const QString &deviceKey, const QString &displayName, const QString &userId, const QString &userDisplayName, const QString &tenantName) {
        // 拼接完整的连接地址密钥字符串
        QString fullConnectionKey = QString("tcp://et-web.console.easytier.net:22020/%1").arg(deviceKey);
        
        // 更新配置
        m_connectionKey = fullConnectionKey;
        m_configManager->setConnectionKey(m_connectionKey);
        m_configManager->setUserId(userId);
        m_configManager->setUserDisplayName(userDisplayName);
        m_configManager->setOAuthDeviceKey(deviceKey);
        m_configManager->setTenantDisplayName(tenantName);
        m_configManager->saveConfig();
        
        // 如果服务正在运行，先停止服务，然后启动服务（使用新密钥）
        showProgressDialog("正在启动 EasyTier 服务...");
        if (ETRunService::isRunning()) {
            
            bool stopped = ETRunService::stop();
            
            if (stopped) {
                QThread::msleep(500);  // 等待资源释放
            } else {
                QMessageBox::warning(nullptr, "错误", "服务停止失败，请重试!");
                return;
            }
        }

        // 启动服务
        bool started = false;
        started = ETRunService::start(m_connectionKey);
        if (started) {
                updateStatus(ConnectionState::Connected);
                m_trayIcon->showMessage("EasyTier", "服务已使用新密钥启动",
                                        QSystemTrayIcon::Information, 3000);
        } else {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("错误", "服务启动失败",
            QSystemTrayIcon::Warning, 5000);
        }
        
        // 更新用户登录状态
        updateUserStatus();

        closeProgressDialog();
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
    
    // 8. 更新用户登录状态显示
    updateUserStatus();
    
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
    
    // 用户登录状态
    m_userStatusAction = new QAction(QIcon(":/assets/user.svg"), "用户：未登录", this);
    m_menu->addAction(m_userStatusAction);
    
    // 组织信息（仅OAuth登录时显示）
    m_tenantStatusAction = new QAction(QIcon(":/assets/tenant.svg"), "", this);
    m_tenantStatusAction->setVisible(false);
    m_menu->addAction(m_tenantStatusAction);
    
    // 连接状态
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
    m_loginEasyTierProAction = new QAction(QIcon(":/assets/login.svg"), "登录 EasyTier Pro", this);
    connect(m_loginEasyTierProAction, &QAction::triggered, this, &SystemTray::onLoginEasyTierPro);
    m_menu->addAction(m_loginEasyTierProAction);
    
    // 设置
    const QString settingsText = "设置连接地址与密钥";
    m_settingsAction = new QAction(QIcon(":/assets/settings.svg"), settingsText, this);
    connect(m_settingsAction, &QAction::triggered, this, &SystemTray::onSettings);
    //m_menu->addAction(m_settingsAction);

    // 清空连接信息
    const QString clearText = "清空连接信息";
    m_clearConnectionAction = new QAction(QIcon(":/assets/clear.svg"), clearText, this);
    connect(m_clearConnectionAction, &QAction::triggered, this, &SystemTray::onClearConnectionInfo);
    m_menu->addAction(m_clearConnectionAction);

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
    m_quitAction = new QAction(QIcon(":/assets/quit.svg"), "退出托盘程序", this);
    connect(m_quitAction, &QAction::triggered, this, &SystemTray::onQuit);
    m_menu->addAction(m_quitAction);
}

void SystemTray::updateUserStatus()
{
    std::clog << "updateUserStatus: m_connectionKey=[" << m_connectionKey.toStdString() << "]" << std::endl;
    
    // 判断是否使用EasyTier Pro官方控制台地址
    QRegularExpression proRe("tcp://et-web\\.console\\.easytier\\.net:22020/(.+)");
    QRegularExpressionMatch proMatch = proRe.match(m_connectionKey);
    
    std::clog << "updateUserStatus: hasProMatch=" << proMatch.hasMatch() << " isEmpty=" << m_connectionKey.isEmpty() << std::endl;
    
    if (proMatch.hasMatch()) {
        // 使用官方控制台，检查OAuth登录状态
        QString currentDeviceKey = proMatch.captured(1).trimmed();
        QString oauthDeviceKey = m_configManager->getOAuthDeviceKey();
        
        if (currentDeviceKey == oauthDeviceKey && !oauthDeviceKey.isEmpty()) {
            // 一致，显示用户名
            QString userDisplayName = m_configManager->getUserDisplayName();
            if (!userDisplayName.isEmpty()) {
                m_userStatusAction->setText(QString("用户：%1").arg(userDisplayName));
                m_userStatusAction->setIcon(QIcon(":/assets/user-loggedin.svg"));
                m_loginEasyTierProAction->setText("重新登录");
                
                // 显示组织信息
                QString tenantName = m_configManager->getTenantDisplayName();
                if (!tenantName.isEmpty()) {
                    m_tenantStatusAction->setText(QString("组织：%1").arg(tenantName));
                    m_tenantStatusAction->setVisible(true);
                } else {
                    m_tenantStatusAction->setVisible(false);
                }
                
                std::clog << "updateUserStatus: 显示已登录用户" << std::endl;
                return;
            }
        }
        
        // 不一致或未保存，显示未登录
        m_tenantStatusAction->setVisible(false);
        m_userStatusAction->setText("用户：未登录");
        m_userStatusAction->setIcon(QIcon(":/assets/user.svg"));
        m_loginEasyTierProAction->setText("登录 EasyTier Pro");
        std::clog << "updateUserStatus: 显示未登录" << std::endl;
    } else if (!m_connectionKey.isEmpty()) {
        // 使用自定义连接信息
        m_tenantStatusAction->setVisible(false);
        m_userStatusAction->setText("自定义连接");
        m_userStatusAction->setIcon(QIcon(":/assets/user-opensource.svg"));
        m_loginEasyTierProAction->setText("登录 EasyTier Pro");
        std::clog << "updateUserStatus: 显示自定义连接" << std::endl;
    } else {
        // 未设置连接地址
        m_tenantStatusAction->setVisible(false);
        m_userStatusAction->setText("用户：未登录");
        m_userStatusAction->setIcon(QIcon(":/assets/user.svg"));
        m_loginEasyTierProAction->setText("登录 EasyTier Pro");
        std::clog << "updateUserStatus: 显示未登录(空密钥)" << std::endl;
    }
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
    QDesktopServices::openUrl(QUrl("https://console.easytier.net/"));
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
        updateUserStatus();
    }
     
    // QPointer 会在对象删除后自动变为 nullptr
    if (!m_settingsDialog.isNull()) {
        m_settingsDialog->deleteLater();
    }
}

void SystemTray::onClearConnectionInfo()
{
    QMessageBox::StandardButton reply = QMessageBox::question(nullptr,
        "确认清空",
        "确定要清空连接地址与密钥吗？\n清空后需要重新设置才能连接。",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    // 清空连接信息
    m_connectionKey.clear();
    m_configManager->setConnectionKey(QString());
    m_configManager->setOAuthDeviceKey(QString());
    m_configManager->setTenantDisplayName(QString());
    m_configManager->saveConfig();
    updateUserStatus();

    // 如果服务正在运行，停止服务
    if (ETRunService::isRunning()) {
        showProgressDialog("正在停止 EasyTier 服务...");
        bool stopped = ETRunService::stop();
        closeProgressDialog();

        if (stopped) {
            updateStatus(ConnectionState::NotStarted);
            m_trayIcon->showMessage("EasyTier", "连接信息已清空，服务已停止",
                                    QSystemTrayIcon::Information, 3000);
        } else {
            m_trayIcon->showMessage("错误", "服务停止失败",
                                    QSystemTrayIcon::Warning, 5000);
        }
    } else {
        m_trayIcon->showMessage("EasyTier", "连接信息已清空",
                                QSystemTrayIcon::Information, 3000);
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

#ifdef Q_OS_WIN32
    QString value = QString("\"%1\" --auto-start").arg(appPath);
    value.replace('/', '\\');

    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    settings.setValue("EasyTierConnector", value);
    settings.sync();

    bool success = (settings.status() == QSettings::NoError);
    std::clog << "注册开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
#elif defined(Q_OS_MACOS)
    // macOS: 使用 ~/Library/LaunchAgents/ 目录下的 plist 文件
    QString launchAgentsDir = QDir::homePath() + "/Library/LaunchAgents";
    QDir dir(launchAgentsDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString plistPath = launchAgentsDir + "/com.easytier.connector.plist";
    QString plistContent = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key>\n"
        "    <string>com.easytier.connector</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>%1</string>\n"
        "        <string>--auto-start</string>\n"
        "    </array>\n"
        "    <key>RunAtLoad</key>\n"
        "    <true/>\n"
        "    <key>KeepAlive</key>\n"
        "    <false/>\n"
        "</dict>\n"
        "</plist>\n"
    ).arg(appPath);

    QFile plistFile(plistPath);
    if (!plistFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "注册开机自启: 失败，无法写入 plist 文件" << std::endl;
        return false;
    }

    plistFile.write(plistContent.toUtf8());
    plistFile.close();

    std::clog << "注册开机自启: 成功" << std::endl;
    return true;
#else
    // Linux: 使用 ~/.config/autostart/ 目录下的 .desktop 文件
    QString desktopFilePath = QDir::homePath() + "/.config/autostart/EasyTierConnector.desktop";
    QDir autostartDir(QDir::homePath() + "/.config/autostart");
    if (!autostartDir.exists()) {
        autostartDir.mkpath(".");
    }

    QString desktopEntry = QString("[Desktop Entry]\n"
                                   "Type=Application\n"
                                   "Name=EasyTierConnector\n"
                                   "Exec=%1 --auto-start\n"
                                   "Hidden=false\n"
                                   "NoDisplay=false\n"
                                   "X-GNOME-Autostart-enabled=true\n")
                               .arg(appPath);

    QFile desktopFile(desktopFilePath);
    if (!desktopFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::cerr << "注册开机自启: 失败，无法写入 .desktop 文件" << std::endl;
        return false;
    }

    desktopFile.write(desktopEntry.toUtf8());
    desktopFile.close();

    std::clog << "注册开机自启: 成功" << std::endl;
    return true;
#endif
}

bool SystemTray::unregisterAutoStart()
{
#ifdef Q_OS_WIN32
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    settings.remove("EasyTierConnector");
    settings.sync();

    bool success = (settings.status() == QSettings::NoError);
    std::clog << "取消开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
#elif defined(Q_OS_MACOS)
    QString plistPath = QDir::homePath() + "/Library/LaunchAgents/com.easytier.connector.plist";
    bool success = QFile::remove(plistPath);
    std::clog << "取消开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
#else
    QString desktopFilePath = QDir::homePath() + "/.config/autostart/EasyTierConnector.desktop";
    bool success = QFile::remove(desktopFilePath);
    std::clog << "取消开机自启: " << (success ? "成功" : "失败") << std::endl;
    return success;
#endif
}

bool SystemTray::isAutoStartRegistered()
{
#ifdef Q_OS_WIN32
    QSettings settings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                       QSettings::NativeFormat);
    return settings.contains("EasyTierConnector");
#elif defined(Q_OS_MACOS)
    QString plistPath = QDir::homePath() + "/Library/LaunchAgents/com.easytier.connector.plist";
    return QFile::exists(plistPath);
#else
    QString desktopFilePath = QDir::homePath() + "/.config/autostart/EasyTierConnector.desktop";
    return QFile::exists(desktopFilePath);
#endif
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
    // 如果服务未运行，直接退出
    if (!ETRunService::isRunning()) {
        saveSettings();
        QApplication::quit();
        return;
    }

    // 服务正在运行，检查是否有记住的选择
    bool rememberQuitChoice = m_configManager->getRememberQuitChoice();
    bool stopOnQuit = m_configManager->getStopOnQuit();

    if (rememberQuitChoice) {
        // 已记住选择，按记忆执行
        if (stopOnQuit) {
            // 记住的选择是"停止并退出"
            showProgressDialog("正在停止 EasyTier 服务...");
            bool stopped = ETRunService::stop();
            closeProgressDialog();

            if (stopped) {
                saveSettings();
                QApplication::quit();
            } else {
                m_trayIcon->showMessage("错误", "EasyTier 服务停止失败，程序保持运行",
                                        QSystemTrayIcon::Warning, 5000);
            }
        } else {
            // 记住的选择是"仅退出前端"
            saveSettings();
            QApplication::quit();
        }
        return;
    }

    // 未记住选择，弹出确认对话框
    if (m_quitConfirmDialog.isNull()) {
        m_quitConfirmDialog = new QuitConfirmDialog();
    }

    if (m_quitConfirmDialog->exec() == QDialog::Accepted) {
        bool choseStopAndQuit = m_quitConfirmDialog->choseStopAndQuit();
        bool rememberChoice = m_quitConfirmDialog->isRememberChoice();

        // 如果用户勾选了"记住我的选择"，保存到配置
        if (rememberChoice) {
            m_configManager->setRememberQuitChoice(true);
            m_configManager->setStopOnQuit(choseStopAndQuit);
            m_configManager->saveConfig();
        }

        if (choseStopAndQuit) {
            // 用户选择"是"：停止服务后退出
            showProgressDialog("正在停止 EasyTier 服务...");
            bool stopped = ETRunService::stop();
            closeProgressDialog();

            if (stopped) {
                saveSettings();
                QApplication::quit();
            } else {
                m_trayIcon->showMessage("错误", "EasyTier 服务停止失败，程序保持运行",
                                        QSystemTrayIcon::Warning, 5000);
            }
        } else {
            // 用户选择"否"：仅退出前端
            saveSettings();
            QApplication::quit();
        }
    }

    // 清理对话框
    if (!m_quitConfirmDialog.isNull()) {
        m_quitConfirmDialog->deleteLater();
    }
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
    
    // 更新用户登录状态
    updateUserStatus();
    
    // 如果服务已运行
    if (ETRunService::isRunning()) {
        if (m_connectionKey.isEmpty()) {
            // 密钥被清空，停止服务
            showProgressDialog("正在停止 EasyTier 服务...");
            bool stopped = ETRunService::stop();
            closeProgressDialog();

            if (stopped) {
                updateStatus(ConnectionState::NotStarted);
                m_trayIcon->showMessage("EasyTier", "连接密钥已清空，服务已停止",
                                        QSystemTrayIcon::Information, 3000);
            } else {
                m_trayIcon->showMessage("错误", "服务停止失败",
                                        QSystemTrayIcon::Warning, 5000);
            }
        } else {
            // 密钥变更，重启服务
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