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
     * @param accessToken 访问令牌
     * @param idToken ID 令牌
     */
    void loginSuccess(const QString &accessToken, const QString &idToken);
    
    /**
     * @brief 登录失败信号
     * @param errorMessage 错误信息
     */
    void loginFailed(const QString &errorMessage);

private:
    /**
     * @brief 用授权码交换访问令牌
     * @param code 授权码
     */
    void swapCodeForToken(const QString &code);
    
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

    static constexpr int TIME_OUT = 60000;     ///< 登录超时时间（60秒）
    static constexpr int CALLBACK_PORT = 54321; ///< 回调服务器端口
    static const QString CALLBACK_URL;         ///< 回调 URL
};

#endif // CASDOOR_LOGIN_H
