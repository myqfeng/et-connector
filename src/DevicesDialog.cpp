/**
 * @file DevicesDialog.cpp
 * @brief EasyTier Pro 我的设备窗口实现
 */

#include "DevicesDialog.h"
#include "ConfigManager.h"
#include "ETRunService.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>

DevicesDialog::DevicesDialog(ConfigManager *configManager, QWidget *parent)
    : QDialog(parent), m_configManager(configManager)
{
    setupUI();
    refreshData();
}

void DevicesDialog::setupUI()
{
    setWindowTitle("我的设备");
    setWindowIcon(QIcon(FAVICON_SVG));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(760, 520);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    auto *summaryLayout = new QGridLayout();
    summaryLayout->setHorizontalSpacing(18);
    summaryLayout->setVerticalSpacing(8);

    m_statusLabel = new QLabel(this);
    m_userLabel = new QLabel(this);
    m_tenantLabel = new QLabel(this);
    m_hostnameLabel = new QLabel(this);
    m_addressLabel = new QLabel(this);

    summaryLayout->addWidget(new QLabel("连接状态", this), 0, 0);
    summaryLayout->addWidget(m_statusLabel, 0, 1);
    summaryLayout->addWidget(new QLabel("当前账号", this), 1, 0);
    summaryLayout->addWidget(m_userLabel, 1, 1);
    summaryLayout->addWidget(new QLabel("当前组织", this), 2, 0);
    summaryLayout->addWidget(m_tenantLabel, 2, 1);
    summaryLayout->addWidget(new QLabel("本机名称", this), 0, 2);
    summaryLayout->addWidget(m_hostnameLabel, 0, 3);
    summaryLayout->addWidget(new QLabel("本机地址", this), 1, 2);
    summaryLayout->addWidget(m_addressLabel, 1, 3);
    mainLayout->addLayout(summaryLayout);

    m_peerTable = new QTableWidget(this);
    m_peerTable->setColumnCount(5);
    m_peerTable->setHorizontalHeaderLabels(QStringList() << "设备名" << "虚拟 IP" << "在线状态" << "延迟" << "连接方式");
    m_peerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_peerTable->verticalHeader()->setVisible(false);
    m_peerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_peerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_peerTable, 1);

    m_messageEdit = new QTextEdit(this);
    m_messageEdit->setReadOnly(true);
    m_messageEdit->setFixedHeight(70);
    m_messageEdit->setVisible(false);
    mainLayout->addWidget(m_messageEdit);

    auto *buttonLayout = new QHBoxLayout();
    auto *refreshButton = new QPushButton("刷新", this);
    m_copyAddressButton = new QPushButton("复制地址", this);
    auto *consoleButton = new QPushButton("打开控制台", this);
    auto *closeButton = new QPushButton("关闭", this);

    connect(refreshButton, &QPushButton::clicked, this, &DevicesDialog::refreshData);
    connect(m_copyAddressButton, &QPushButton::clicked, this, &DevicesDialog::copyCurrentAddress);
    connect(consoleButton, &QPushButton::clicked, this, &DevicesDialog::openConsole);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(m_copyAddressButton);
    buttonLayout->addWidget(consoleButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void DevicesDialog::refreshData()
{
    m_messageEdit->setVisible(false);
    m_peerTable->setRowCount(0);
    m_currentAddress.clear();

    m_userLabel->setText(m_configManager ? firstNonEmpty({m_configManager->getUserDisplayName(), "未登录"}) : "未登录");
    m_tenantLabel->setText(m_configManager ? firstNonEmpty({m_configManager->getTenantDisplayName(), "-"}) : "-");
    m_hostnameLabel->setText(QSysInfo::machineHostName());
    m_addressLabel->setText("-");
    m_copyAddressButton->setEnabled(false);

    CommandResult nodeResult = ETRunService::queryNodeInfoJson();
    if (!nodeResult.success) {
        showUnavailable(nodeResult.errorString.isEmpty()
                            ? ETRunService::serviceErrorToString(nodeResult.error)
                            : nodeResult.errorString);
        return;
    }

    QString parseError;
    QJsonDocument nodeDoc = parseJsonOutput(nodeResult.output, &parseError);
    if (nodeDoc.isNull()) {
        showUnavailable("节点信息解析失败: " + parseError);
        return;
    }

    updateSummary(nodeDoc);

    CommandResult peerResult = ETRunService::queryPeerListJson();
    if (!peerResult.success) {
        showUnavailable(peerResult.errorString.isEmpty()
                            ? ETRunService::serviceErrorToString(peerResult.error)
                            : peerResult.errorString);
        return;
    }

    QJsonDocument peerDoc = parseJsonOutput(peerResult.output, &parseError);
    if (peerDoc.isNull()) {
        showUnavailable("设备列表解析失败: " + parseError);
        return;
    }

    updatePeers(peerDoc);
}

void DevicesDialog::updateSummary(const QJsonDocument &nodeDoc)
{
    QJsonValue root = nodeDoc.isObject() ? QJsonValue(nodeDoc.object()) : QJsonValue(nodeDoc.array());
    const QString hostname = firstNonEmpty({
        findStringValue(root, {"hostname", "host_name", "name", "device_name"}),
        QSysInfo::machineHostName()
    });
    const QString address = findStringValue(root, {"ipv4", "virtual_ip", "virtual_ipv4", "ip", "address", "peer_id"});

    m_statusLabel->setText("已连接");
    m_hostnameLabel->setText(hostname);
    m_currentAddress = address;
    m_addressLabel->setText(address.isEmpty() ? "-" : address);
    m_copyAddressButton->setEnabled(!m_currentAddress.isEmpty());
}

void DevicesDialog::updatePeers(const QJsonDocument &peerDoc)
{
    QJsonValue root = peerDoc.isObject() ? QJsonValue(peerDoc.object()) : QJsonValue(peerDoc.array());
    QJsonArray peers = findArrayValue(root);

    m_peerTable->setRowCount(peers.size());
    for (int row = 0; row < peers.size(); ++row) {
        QJsonObject peer = peers.at(row).toObject();
        QString name = firstNonEmpty({
            findStringValue(peer, {"hostname", "host_name", "name", "device_name", "peer_name"}),
            "-"
        });
        QString address = firstNonEmpty({
            findStringValue(peer, {"ipv4", "virtual_ip", "virtual_ipv4", "ip", "address", "peer_id"}),
            "-"
        });
        QString latency = firstNonEmpty({
            findStringValue(peer, {"latency", "lat_ms", "cost", "rtt"}),
            "-"
        });
        QString connectionType = firstNonEmpty({
            findStringValue(peer, {"conn_type", "connection_type", "transport", "path", "protocol"}),
            "-"
        });

        m_peerTable->setItem(row, 0, new QTableWidgetItem(name));
        m_peerTable->setItem(row, 1, new QTableWidgetItem(address));
        m_peerTable->setItem(row, 2, new QTableWidgetItem(formatPeerStatus(peer)));
        m_peerTable->setItem(row, 3, new QTableWidgetItem(latency));
        m_peerTable->setItem(row, 4, new QTableWidgetItem(connectionType));
    }

    if (peers.isEmpty()) {
        m_messageEdit->setPlainText("已连接，但当前没有其他在线设备。");
        m_messageEdit->setVisible(true);
    }
}

void DevicesDialog::showUnavailable(const QString &message)
{
    m_statusLabel->setText("当前未连接或 Core 不可用");
    m_messageEdit->setPlainText(message.isEmpty() ? "当前未连接或 Core 不可用，请连接后刷新。" : message);
    m_messageEdit->setVisible(true);
}

void DevicesDialog::copyCurrentAddress() const
{
    if (m_currentAddress.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(m_currentAddress);
}

void DevicesDialog::openConsole()
{
    QDesktopServices::openUrl(QUrl("https://console.easytier.net/"));
}

QJsonDocument DevicesDialog::parseJsonOutput(const QString &output, QString *errorMessage)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
        return doc;
    }

    int objectStart = output.indexOf('{');
    int arrayStart = output.indexOf('[');
    int start = -1;
    if (objectStart >= 0 && arrayStart >= 0) {
        start = qMin(objectStart, arrayStart);
    } else {
        start = qMax(objectStart, arrayStart);
    }

    if (start >= 0) {
        doc = QJsonDocument::fromJson(output.mid(start).toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            return doc;
        }
    }

    if (errorMessage) {
        *errorMessage = parseError.errorString();
    }
    return QJsonDocument();
}

QString DevicesDialog::findStringValue(const QJsonValue &value, const QStringList &keys)
{
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        for (const QString &key : keys) {
            if (obj.contains(key)) {
                QJsonValue child = obj.value(key);
                if (child.isString()) {
                    return child.toString();
                }
                if (child.isDouble() || child.isBool()) {
                    return child.toVariant().toString();
                }
            }
        }

        for (const QJsonValue &child : obj) {
            QString found = findStringValue(child, keys);
            if (!found.isEmpty()) {
                return found;
            }
        }
    } else if (value.isArray()) {
        for (const QJsonValue &child : value.toArray()) {
            QString found = findStringValue(child, keys);
            if (!found.isEmpty()) {
                return found;
            }
        }
    }

    return QString();
}

QJsonArray DevicesDialog::findArrayValue(const QJsonValue &value)
{
    if (value.isArray()) {
        return value.toArray();
    }

    if (!value.isObject()) {
        return QJsonArray();
    }

    QJsonObject obj = value.toObject();
    for (const QString &key : {"peers", "peer_list", "items", "data", "routes"}) {
        QJsonValue child = obj.value(key);
        if (child.isArray()) {
            return child.toArray();
        }
        if (child.isObject()) {
            QJsonArray nested = findArrayValue(child);
            if (!nested.isEmpty()) {
                return nested;
            }
        }
    }

    for (const QJsonValue &child : obj) {
        QJsonArray nested = findArrayValue(child);
        if (!nested.isEmpty()) {
            return nested;
        }
    }

    return QJsonArray();
}

QString DevicesDialog::formatPeerStatus(const QJsonObject &peer)
{
    QString rawStatus = firstNonEmpty({
        findStringValue(peer, {"status", "state", "online", "connected", "is_online"}),
        "-"
    });

    if (rawStatus.compare("true", Qt::CaseInsensitive) == 0 || rawStatus.compare("connected", Qt::CaseInsensitive) == 0) {
        return "在线";
    }
    if (rawStatus.compare("false", Qt::CaseInsensitive) == 0 || rawStatus.compare("offline", Qt::CaseInsensitive) == 0) {
        return "离线";
    }

    return rawStatus;
}

QString DevicesDialog::firstNonEmpty(const QStringList &values)
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty()) {
            return value;
        }
    }
    return QString();
}
