/**
 * @file CasdoorLogin.cpp
 * @brief Casdoor OAuth 登录类实现
 */

#include "CasdoorLogin.h"
#include <QDesktopServices>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>

// 静态成员初始化
const QString CasdoorLogin::CALLBACK_URL = "http://127.0.0.1:54321/callback";

CasdoorLogin::CasdoorLogin(const QString &clientId, QObject *parent)
    : QObject(parent), m_clientId(clientId)
{
    m_server = new QTcpServer(this);
    m_networkManager = new QNetworkAccessManager(this);
    m_timeoutTimer = new QTimer(this);
    
    // 连接超时信号
    connect(m_timeoutTimer, &QTimer::timeout, this, &CasdoorLogin::onLoginTimeout);
    m_timeoutTimer->setSingleShot(true);
}

void CasdoorLogin::startLogin()
{
    // 1. 如果之前有未完成的登录，先清理
    stopLogin();

    // 2. 在本地启动回调服务器
    if (!m_server->listen(QHostAddress::LocalHost, CALLBACK_PORT)) {
        emit loginFailed(QString("无法启动本地服务，端口 %1 可能被占用").arg(CALLBACK_PORT));
        return;
    }

    // 3. 生成防 CSRF 的随机 state
    m_state = makeRandomString(32);
    
    // 4. 生成 PKCE code verifier 和 challenge
    m_codeVerifier = makeRandomString(64);
    QString codeChallenge = makeCodeChallenge(m_codeVerifier);

    // 4. 构造 OAuth 授权 URL
    // Casdoor 地址: https://auth.console.easytier.net/
    // 应用名: EasyTier
    QString authUrl = "https://auth.console.easytier.net/login/oauth/authorize?"
                      "client_id=" + m_clientId +
                      "&redirect_uri=" + CALLBACK_URL +
                      "&response_type=code"
                      "&scope=openid profile"
                      "&state=" + m_state +
                      "&code_challenge=" + codeChallenge +
                      "&code_challenge_method=S256";

    // 5. 打开默认浏览器进行授权
    QDesktopServices::openUrl(QUrl(authUrl));

    // 6. 启动超时定时器
    m_timeoutTimer->start(TIME_OUT);

    // 7. 等待浏览器回调
    connect(m_server, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket *socket = m_server->nextPendingConnection();
        
        // 等待 HTTP 请求
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QString request = QString::fromUtf8(socket->readAll());

            // 从请求中提取授权码
            QRegularExpression re("code=([^&\\s]+)");
            QRegularExpressionMatch match = re.match(request);

            if (match.hasMatch()) {
                QString code = match.captured(1);
                
                // 返回登录成功页面
                QString response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html; charset=utf-8\r\n"
                                   "Connection: close\r\n\r\n"
                                   "<!DOCTYPE html>"
                                   "<html>"
                                   "<head>"
                                   "<meta charset='utf-8'>"
                                   "<title>登录成功</title>"
                                   "<link rel='icon' href='https://console.easytier.net/favicon.svg'>"
                                   "</head>"
                                   "<body style='font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f5f5f5;'>"
                                   "<div style='background-color: white; border-radius: 10px; padding: 40px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); display: inline-block; min-width: 400px;'>"
                                   "<img src='https://console.easytier.net/favicon.svg' width='80' height='80' style='margin-bottom: 20px;'>"
                                   "<h2 style='color: #66ccff; margin: 0 0 20px 0;'>登录成功!</h2>"
                                   "<p style='color: #666; font-size: 16px; margin: 0;'>您可以关闭此标签页并返回应用程序。</p>"
                                   "</div>"
                                   "</body>"
                                   "</html>";
                socket->write(response.toUtf8());
                socket->flush();
                socket->disconnectFromHost();
                
                // 关闭本地服务器
                m_server->close();

                // 停止超时定时器
                m_timeoutTimer->stop();

                // 使用授权码交换访问令牌
                swapCodeForToken(code);
            } else {
                // 提取错误信息
                QRegularExpression errorRe("error=([^&\\s]+)");
                QRegularExpressionMatch errorMatch = errorRe.match(request);
                QString errorMsg = "授权失败";
                if (errorMatch.hasMatch()) {
                    errorMsg += ": " + errorMatch.captured(1);
                }
                
                QString response = "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Type: text/html; charset=utf-8\r\n"
                                   "Connection: close\r\n\r\n"
                                   "<!DOCTYPE html>"
                                   "<html>"
                                   "<head>"
                                   "<meta charset='utf-8'>"
                                   "<title>登录失败</title>"
                                   "<link rel='icon' href='https://console.easytier.net/favicon.svg'>"
                                   "</head>"
                                   "<body style='font-family: Arial, sans-serif; text-align: center; margin-top: 50px; background-color: #f5f5f5;'>"
                                   "<div style='background-color: white; border-radius: 10px; padding: 40px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); display: inline-block; min-width: 400px;'>"
                                   "<img src='https://console.easytier.net/favicon.svg' width='80' height='80' style='margin-bottom: 20px;'>"
                                   "<h2 style='color: #f44336; margin: 0 0 20px 0;'>登录失败</h2>"
                                   "<p style='color: #666; font-size: 16px; margin: 0;'>" + errorMsg + "</p>"
                                   "</div>"
                                   "</body>"
                                   "</html>";
                socket->write(response.toUtf8());
                socket->flush();
                socket->disconnectFromHost();
                
                m_server->close();
                m_timeoutTimer->stop();
                emit loginFailed(errorMsg);
            }
        });
    });
}

void CasdoorLogin::swapCodeForToken(const QString &code)
{
    QUrl url("https://auth.console.easytier.net/api/login/oauth/access_token");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // 构造 POST 请求参数
    QUrlQuery params;
    params.addQueryItem("grant_type", "authorization_code");
    params.addQueryItem("client_id", m_clientId);
    params.addQueryItem("code", code);
    params.addQueryItem("redirect_uri", CALLBACK_URL);
    params.addQueryItem("code_verifier", m_codeVerifier);

    // 发送 POST 请求
    m_networkManager->post(request, params.toString(QUrl::FullyEncoded).toUtf8());

    // 处理响应
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            QString errorMsg = "网络请求失败: " + reply->errorString();
            emit loginFailed(errorMsg);
            return;
        }

        // 解析 JSON 响应
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument json = QJsonDocument::fromJson(responseData, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            emit loginFailed("解析响应失败: " + parseError.errorString());
            return;
        }

        QJsonObject obj = json.object();

        // 检查错误
        if (obj.contains("error")) {
            QString errorMsg = obj["error"].toString();
            if (obj.contains("error_description")) {
                errorMsg += ": " + obj["error_description"].toString();
            }
            emit loginFailed(errorMsg);
            return;
        }

        // 提取令牌
        QString accessToken = obj["access_token"].toString();

        if (!accessToken.isEmpty()) {
            // 保存 access_token 供后续使用
            m_accessToken = accessToken;
            
            // 开始获取租户信息
            fetchTenants(accessToken);
        } else {
            emit loginFailed("未获取到访问令牌");
        }
    });
}

QString CasdoorLogin::makeRandomString(int length) const
{
    // 生成随机字节数组
    QByteArray randomBytes(length, 0);
    for (int i = 0; i < length; ++i) {
        randomBytes[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    // 转换为 Base64 URL 安全编码（去除填充字符）
    return QString::fromLatin1(randomBytes.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals
    ));
}

QString CasdoorLogin::makeCodeChallenge(const QString &verifier) const
{
    // SHA256 哈希
    QByteArray hash = QCryptographicHash::hash(
        verifier.toUtf8(),
        QCryptographicHash::Sha256
    );

    // 转换为 Base64 URL 安全编码（去除填充字符）
    return QString::fromLatin1(hash.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals
    ));
}

void CasdoorLogin::stopLogin()
{
    // 停止超时定时器
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }

    // 关闭服务器
    if (m_server->isListening()) {
        m_server->close();
    }
    // 断开所有信号连接
    m_server->disconnect();
    m_networkManager->disconnect();
}



void CasdoorLogin::onLoginTimeout()
{
    // 超时，停止登录流程
    stopLogin();

    // 发出登录失败信号
    emit loginFailed("登录超时，请在60秒内完成登录");
}

void CasdoorLogin::fetchTenants(const QString &accessToken)
{
    QUrl url("https://api.console.easytier.net/api/v1/auth/me");
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    
    m_networkManager->get(request);
    
    // 断开之前的 finished 连接，避免多次触发
    disconnect(m_networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("获取租户信息失败: " + reply->errorString());
            return;
        }
        
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument json = QJsonDocument::fromJson(responseData, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            emit loginFailed("解析租户信息失败: " + parseError.errorString());
            return;
        }
        
        QJsonObject obj = json.object();
        
        // 提取用户信息
        QJsonObject userObj = obj["user"].toObject();
        m_userId = userObj["id"].toString();
        m_userDisplayName = userObj["display_name"].toString();
        
        QJsonArray tenantsArray = obj["tenants"].toArray();
        
        QList<TenantInfo> tenants;
        for (const QJsonValue &value : tenantsArray) {
            QJsonObject tenantObj = value.toObject();
            TenantInfo info;
            info.id = tenantObj["id"].toString();
            info.name = tenantObj["name"].toString();
            tenants.append(info);
        }
        
        if (tenants.isEmpty()) {
            emit loginFailed("未找到任何组织，请确保您有访问权限");
            return;
        }
        
        // 显示组织选择对话框
        int selectedIndex = showTenantSelectionDialog(tenants);
        if (selectedIndex >= 0 && selectedIndex < tenants.size()) {
            // 用户选择了组织，获取该组织的密钥列表
            fetchDeviceEnrollmentKeys(m_accessToken, tenants[selectedIndex].id);
        } else {
            // 用户取消选择
            emit loginFailed("用户取消了选择");
        }
    });
}

void CasdoorLogin::fetchDeviceEnrollmentKeys(const QString &accessToken, const QString &tenantId)
{
    QUrl url(QString("https://api.console.easytier.net/api/v1/tenants/%1/device-enrollment-keys").arg(tenantId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    
    m_networkManager->get(request);
    
    // 断开之前的 finished 连接
    disconnect(m_networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this, tenantId](QNetworkReply *reply) {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("获取密钥列表失败: " + reply->errorString());
            return;
        }
        
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument json = QJsonDocument::fromJson(responseData, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            emit loginFailed("解析密钥列表失败: " + parseError.errorString());
            return;
        }
        
        QJsonArray keysArray = json.array();
        
        QList<DeviceKeyInfo> keys;
        for (const QJsonValue &value : keysArray) {
            QJsonObject keyObj = value.toObject();
            
            // 跳过已撤销的密钥
            if (keyObj["revoked"].toBool()) {
                continue;
            }
            
            DeviceKeyInfo info;
            info.id = keyObj["id"].toString();
            info.displayName = keyObj["display_name"].toString();
            info.keyCode = keyObj["key_code"].toString();
            keys.append(info);
        }
        
        if (keys.isEmpty()) {
            emit loginFailed("未找到任何可用的设备接入密钥，请先在控制台创建密钥");
            return;
        }
        
        // 显示密钥选择对话框
        int selectedIndex = showKeySelectionDialog(keys);
        if (selectedIndex >= 0 && selectedIndex < keys.size()) {
            // 用户选择了密钥，获取真实密钥
            fetchDeviceKeySecret(m_accessToken, tenantId, keys[selectedIndex].id, keys[selectedIndex].displayName);
        } else {
            // 用户取消选择
            emit loginFailed("用户取消了选择");
        }
    });
}

void CasdoorLogin::fetchDeviceKeySecret(const QString &accessToken, const QString &tenantId, const QString &keyId, const QString &displayName)
{
    QUrl url(QString("https://api.console.easytier.net/api/v1/tenants/%1/device-enrollment-keys/%2/secret").arg(tenantId).arg(keyId));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    
    m_networkManager->get(request);
    
    // 断开之前的 finished 连接
    disconnect(m_networkManager, &QNetworkAccessManager::finished, nullptr, nullptr);
    
    connect(m_networkManager, &QNetworkAccessManager::finished, this, [this, displayName](QNetworkReply *reply) {
        reply->deleteLater();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed("获取密钥详情失败: " + reply->errorString());
            return;
        }
        
        QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        QJsonDocument json = QJsonDocument::fromJson(responseData, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            emit loginFailed("解析密钥详情失败: " + parseError.errorString());
            return;
        }
        
        QJsonObject obj = json.object();
        QString deviceKey = obj["bootstrap_token"].toString();
        
        if (deviceKey.isEmpty()) {
            emit loginFailed("未获取到设备接入密钥");
            return;
        }
        
        // 登录成功，发送设备接入密钥
        emit loginSuccess(deviceKey, displayName, m_userId, m_userDisplayName);
    });
}

int CasdoorLogin::showTenantSelectionDialog(const QList<TenantInfo> &tenants)
{
    QDialog dialog;
    dialog.setWindowTitle("请选择组织");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel("请选择您要使用的组织：", &dialog);
    layout->addWidget(label);
    
    QListWidget *listWidget = new QListWidget(&dialog);
    for (const TenantInfo &tenant : tenants) {
        listWidget->addItem(tenant.name);
    }
    listWidget->setCurrentRow(0);
    layout->addWidget(listWidget);
    
    QPushButton *okButton = new QPushButton("确定", &dialog);
    QPushButton *cancelButton = new QPushButton("取消", &dialog);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        return listWidget->currentRow();
    }
    return -1;
}

int CasdoorLogin::showKeySelectionDialog(const QList<DeviceKeyInfo> &keys)
{
    QDialog dialog;
    dialog.setWindowTitle("请选择设备接入密钥");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *label = new QLabel("请选择您要使用的设备接入密钥：", &dialog);
    layout->addWidget(label);
    
    QListWidget *listWidget = new QListWidget(&dialog);
    for (const DeviceKeyInfo &key : keys) {
        QString itemText = QString("%1 (%2)").arg(key.displayName, key.keyCode);
        listWidget->addItem(itemText);
    }
    listWidget->setCurrentRow(0);
    layout->addWidget(listWidget);
    
    QPushButton *okButton = new QPushButton("确定", &dialog);
    QPushButton *cancelButton = new QPushButton("取消", &dialog);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        return listWidget->currentRow();
    }
    return -1;
}
