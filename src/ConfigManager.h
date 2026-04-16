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
    
    // 设置配置
    void setConnectionKey(const QString &key);
    void setAutoStart(bool autoStart);
    
    // 保存配置到文件
    bool saveConfig();
    
    // 从文件加载配置
    bool loadConfig();
    
    // 获取配置文件路径
    QString getConfigFilePath() const;

private:
    // 配置数据
    QString m_connectionKey;
    bool m_autoStart;  // 是否开机启动托盘程序
    
    // 配置文件路径
    QString m_configFilePath;
    
    // 确保配置目录存在
    void ensureConfigDirectory(const QString &basePath);
    
    // 获取基础配置路径
    QString getBaseConfigPath() const;
};

#endif // CONFIGMANAGER_H
