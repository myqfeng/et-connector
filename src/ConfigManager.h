#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    // 获取配置
    QString getConnectionKey() const;
    bool getAutoStart() const;
    bool getAutoReconnect() const;
    
    // 设置配置
    void setConnectionKey(const QString &key);
    void setAutoStart(bool autoStart);
    void setAutoReconnect(bool autoReconnect);
    
    // 保存配置到文件
    bool saveConfig();
    
    // 从文件加载配置
    bool loadConfig();
    
    // 获取配置文件路径
    QString getConfigFilePath() const;

private:
    // 配置数据
    QString m_connectionKey;
    bool m_autoStart;
    bool m_autoReconnect;
    
    // 配置文件路径
    QString m_configFilePath;
    
    // 确保配置目录存在
    void ensureConfigDirectory(const QString &basePath = QString());
};

#endif // CONFIGMANAGER_H