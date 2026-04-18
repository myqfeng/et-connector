/**
 * @file CasdoorLogin.h
 * @brief Casdoor OAuth 登录类
 * 
 * 使用 PKCE (Proof Key for Code Exchange) 流程进行 OAuth 2.0 授权
 * 支持通过本地回调服务器接收授权码并交换访问令牌
 */

#ifndef CASDOOR_LOGIN_H
#define CASDOOR_LOGIN_H

#include <QObject>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>

/**
 * @brief Casdoor OAuth 登录类
 * 
 * 负责处理与 Casdoor 认证服务器的交互，包括：
 * - 生成 PKCE code verifier 和 code challenge
 * - 打开浏览器进行 OAuth 授权
 * - 运行本地回调服务器接收授权码
 * - 使用授权码交换访问令牌
 */
class CasdoorLogin : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param clientId Casdoor 应用的 Client ID
     * @param parent 父对象
     */
    explicit CasdoorLogin(const QString &clientId, QObject *parent = nullptr);

    /**
     * @brief 开始 OAuth 登录流程
     * 
     * 该方法会：
     * 1. 启动本地回调服务器
     * 2. 生成 PKCE 参数
     * 3. 构造授权 URL
     * 4. 打开默认浏览器进行授权
     */
    void startLogin();
    
    /**
     * @brief 停止登录流程（清理资源）
     */
    void stopLogin();

private slots:
    /**
     * @brief 登录超时处理
     */
    void onLoginTimeout();

signals:
    /**
     * @brief 登录成功信号
     * @param deviceKey 设备接入密钥（etk 开头）
     * @param displayName 密钥显示名称
     * @param userId 用户ID
     * @param userDisplayName 用户显示名称
     */
    void loginSuccess(const QString &deviceKey, const QString &displayName, const QString &userId, const QString &userDisplayName);
    
    /**
     * @brief 登录失败信号
     * @param errorMessage 错误信息
     */
    void loginFailed(const QString &errorMessage);

private:
    /**
     * @brief 组织信息结构
     */
    struct TenantInfo {
        QString id;
        QString name;
    };
    
    /**
     * @brief 设备接入密钥信息结构
     */
    struct DeviceKeyInfo {
        QString id;
        QString displayName;
        QString keyCode;
    };

    /**
     * @brief 用授权码交换访问令牌
     * @param code 授权码
     */
    void swapCodeForToken(const QString &code);
    
    /**
     * @brief 获取租户（组织）信息
     * @param accessToken 访问令牌
     */
    void fetchTenants(const QString &accessToken);
    
    /**
     * @brief 获取设备接入密钥列表
     * @param accessToken 访问令牌
     * @param tenantId 组织 ID
     */
    void fetchDeviceEnrollmentKeys(const QString &accessToken, const QString &tenantId);
    
    /**
     * @brief 获取设备接入密钥详情（真实密钥）
     * @param accessToken 访问令牌
     * @param tenantId 组织 ID
     * @param keyId 密钥 ID
     * @param displayName 密钥显示名称
     */
    void fetchDeviceKeySecret(const QString &accessToken, const QString &tenantId, const QString &keyId, const QString &displayName);
    
    /**
     * @brief 显示组织选择对话框
     * @param tenants 组织列表
     * @return 选中的组织索引，-1 表示取消
     */
    int showTenantSelectionDialog(const QList<TenantInfo> &tenants);
    
    /**
     * @brief 显示密钥选择对话框
     * @param keys 密钥列表
     * @return 选中的密钥索引，-1 表示取消
     */
    int showKeySelectionDialog(const QList<DeviceKeyInfo> &keys);
    
    /**
     * @brief 生成随机字符串（用于 state 和 code verifier）
     * @param length 字符串长度
     * @return URL 安全的 Base64 编码随机字符串
     */
    QString makeRandomString(int length) const;
    
    /**
     * @brief 生成 PKCE code challenge
     * @param verifier Code verifier
     * @return SHA256 哈希后的 Base64 URL 编码字符串
     */
    QString makeCodeChallenge(const QString &verifier) const;

private:
    QString m_clientId;                       ///< Client ID
    QTcpServer *m_server = nullptr;          ///< 本地回调服务器
    QNetworkAccessManager *m_networkManager = nullptr; ///< 网络管理器
    QTimer *m_timeoutTimer = nullptr;        ///< 超时定时器
    
    QString m_codeVerifier;                   ///< PKCE code verifier
    QString m_state;                          ///< OAuth state 参数
    QString m_accessToken;                    ///< 访问令牌
    QString m_userId;                         ///< 用户ID
    QString m_userDisplayName;                ///< 用户显示名称

    static constexpr int TIME_OUT = 60000;     ///< 登录超时时间（60秒）
    static constexpr int CALLBACK_PORT = 54321; ///< 回调服务器端口
    static const QString CALLBACK_URL;         ///< 回调 URL
};

#endif // CASDOOR_LOGIN_H
