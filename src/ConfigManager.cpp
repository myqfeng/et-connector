#include "ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_connectionKey("")
    , m_autoStart(false)
    , m_autoReconnect(true) // 默认启用自动回连
{
    // 获取配置文件路径
    QString configPath = getBaseConfigPath();
    
    // 创建EasyTier配置目录
    ensureConfigDirectory(configPath);
    
    // 使用标准的配置文件名
    m_configFilePath = configPath + "/EasyTier/conf.json";
}

ConfigManager::~ConfigManager()
{
    saveConfig();
}

void ConfigManager::ensureConfigDirectory(const QString &basePath)
{
    QString configDir = basePath + "/EasyTier";
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(configDir);
    }
}

QString ConfigManager::getConnectionKey() const
{
    return m_connectionKey;
}

bool ConfigManager::getAutoStart() const
{
    return m_autoStart;
}

bool ConfigManager::getAutoReconnect() const
{
    return m_autoReconnect;
}

void ConfigManager::setConnectionKey(const QString &key)
{
    m_connectionKey = key;
}

void ConfigManager::setAutoStart(bool autoStart)
{
    m_autoStart = autoStart;
}

void ConfigManager::setAutoReconnect(bool autoReconnect)
{
    m_autoReconnect = autoReconnect;
}

bool ConfigManager::saveConfig()
{
    // 获取配置路径
    QString configPath = getBaseConfigPath();
    ensureConfigDirectory(configPath);
    
    QJsonObject configObj;
    configObj["connectionKey"] = m_connectionKey;
    configObj["autoStart"] = m_autoStart;
    configObj["autoReconnect"] = m_autoReconnect;
    
    QJsonDocument doc(configObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
    
    QFile configFile(m_configFilePath);
    if (configFile.open(QIODevice::WriteOnly)) {
        configFile.write(jsonData);
        configFile.close();
        return true;
    }
    
    return false;
}

bool ConfigManager::loadConfig()
{
    // 获取配置路径
    QString configPath = getBaseConfigPath();
    ensureConfigDirectory(configPath);
    
    QFile configFile(m_configFilePath);
    if (configFile.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = configFile.readAll();
        configFile.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isObject()) {
            QJsonObject configObj = doc.object();
            m_connectionKey = configObj["connectionKey"].toString("");
            m_autoStart = configObj["autoStart"].toBool(false);
            m_autoReconnect = configObj["autoReconnect"].toBool(true); // 默认启用自动回连
            return true;
        }
    }
    
    return false;
}

QString ConfigManager::getConfigFilePath() const
{
    return m_configFilePath;
}

QString ConfigManager::getBaseConfigPath() const
{
    // 尝试使用标准的配置目录
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (!configPath.isEmpty()) {
        return configPath;
    }
    
    // 备选：AppDataLocation
    configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!configPath.isEmpty()) {
        return configPath;
    }
    
    // 备选：GenericDataLocation
    configPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!configPath.isEmpty()) {
        return configPath;
    }
    
    // 最后备选：当前目录
    return QDir::currentPath();
}