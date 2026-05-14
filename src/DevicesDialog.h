/**
 * @file DevicesDialog.h
 * @brief EasyTier Pro 我的设备窗口
 */

#ifndef DEVICESDIALOG_H
#define DEVICESDIALOG_H

#include <QDialog>
#include <QJsonDocument>

class ConfigManager;
class QLabel;
class QPushButton;
class QTableWidget;
class QTextEdit;

class DevicesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DevicesDialog(ConfigManager *configManager, QWidget *parent = nullptr);
    ~DevicesDialog() override = default;

private slots:
    void refreshData();
    void copyCurrentAddress() const;
    static void openConsole();

private:
    void setupUI();
    void updateSummary(const QJsonDocument &nodeDoc);
    void updatePeers(const QJsonDocument &peerDoc);
    void showUnavailable(const QString &message);

    static QJsonDocument parseJsonOutput(const QString &output, QString *errorMessage);
    static QString findStringValue(const QJsonValue &value, const QStringList &keys);
    static QJsonArray findArrayValue(const QJsonValue &value);
    static QString formatPeerStatus(const QJsonObject &peer);
    static QString firstNonEmpty(const QStringList &values);

private:
    ConfigManager *m_configManager = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_userLabel = nullptr;
    QLabel *m_tenantLabel = nullptr;
    QLabel *m_hostnameLabel = nullptr;
    QLabel *m_addressLabel = nullptr;
    QTableWidget *m_peerTable = nullptr;
    QTextEdit *m_messageEdit = nullptr;
    QPushButton *m_copyAddressButton = nullptr;

    QString m_currentAddress;
};

#endif // DEVICESDIALOG_H
