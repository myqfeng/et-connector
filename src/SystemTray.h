#ifndef SYSTEMTRAY_H
#define SYSTEMTRAY_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include "SettingsDialog.h"
#include "AboutDialog.h"
#include "ETRunService.h"
#include "ConfigManager.h"

//#define IS_NOT_ET_PRO

// 连接状态枚举
enum class ConnectionState {
    NotStarted,     // 未启动
    Connecting,     // 连接中
    Connected       // 已连接
};

class SystemTray : public QObject
{
    Q_OBJECT

public:
    explicit SystemTray(QObject *parent = nullptr);
    ~SystemTray() override;

    void show() const;
    void setAutoStartMode(bool autoStart) { m_isAutoStartMode = autoStart; }

private slots:
    void onToggleConnection();
    static void onOpenWebConsole();
    void onSettings();
    void onAutoStart(bool checked);
    void onAbout();
    void onQuit() const;
    static void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    
    // 连接密钥变化槽函数
    void onConnectionKeyChanged();
    
    // 计划任务相关函数（管理托盘程序开机自启）
    static void createScheduledTask();
    static void deleteScheduledTask();

private:
    void setupMenu();
    void updateStatus(ConnectionState state);
    void updateConnectionActions() const;
    void loadSettings();
    void saveSettings() const;
    
    // 显示进度对话框
    void showProgressDialog(const QString &text);
    void closeProgressDialog();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    
    // 菜单项
    QAction *m_titleAction;
    QAction *m_statusAction;
    QAction *m_separator1;
    QAction *m_toggleConnectionAction;
    QAction *m_separator2;
    QAction *m_openWebConsoleAction;
    QAction *m_settingsAction;
    QAction *m_separator3;
    QAction *m_autoStartAction;
    QAction *m_separator4;
    QAction *m_aboutAction;
    QAction *m_quitAction;
    
    // 状态
    ConnectionState m_connectionState;
    bool m_autoStart;           // 是否开机启动托盘程序
    bool m_isAutoStartMode;     // 是否从开机自启启动
    QString m_connectionKey;
    
    // 设置
    QSettings *m_settings;
    
    // 对话框 (懒加载)
    SettingsDialog *m_settingsDialog = nullptr;
    AboutDialog *m_aboutDialog = nullptr;
    
    // EasyTier服务管理器
    ETRunService *m_etService;
    
    // 配置管理器
    ConfigManager *m_configManager;
    
    // 进度对话框
    QProgressDialog *m_progressDialog = nullptr;
};

#endif // SYSTEMTRAY_H
