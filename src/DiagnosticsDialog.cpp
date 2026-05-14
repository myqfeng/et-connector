/**
 * @file DiagnosticsDialog.cpp
 * @brief EasyTier Pro 诊断窗口实现
 */

#include "DiagnosticsDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

DiagnosticsDialog::DiagnosticsDialog(const QString &lastError, QWidget *parent)
    : QDialog(parent), m_lastError(lastError)
{
    setupUI();
    refreshDiagnostics();
}

void DiagnosticsDialog::setupUI()
{
    setWindowTitle("诊断");
    setWindowIcon(QIcon(FAVICON_SVG));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(820, 600);

    auto *mainLayout = new QVBoxLayout(this);
    m_outputEdit = new QTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setLineWrapMode(QTextEdit::NoWrap);
    mainLayout->addWidget(m_outputEdit, 1);

    auto *buttonLayout = new QHBoxLayout();
    auto *refreshButton = new QPushButton("刷新", this);
    auto *copyButton = new QPushButton("复制诊断信息", this);
    auto *closeButton = new QPushButton("关闭", this);

    connect(refreshButton, &QPushButton::clicked, this, &DiagnosticsDialog::refreshDiagnostics);
    connect(copyButton, &QPushButton::clicked, this, &DiagnosticsDialog::copyDiagnostics);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(copyButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
}

void DiagnosticsDialog::refreshDiagnostics()
{
    QStringList lines;
    lines << QString("生成时间: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QString("客户端版本: %1").arg(qApp->applicationVersion());
    lines << QString("EasyTier CLI 路径: %1").arg(ETRunService::getCliPath());
    lines << QString("EasyTier CLI 存在: %1").arg(QFile::exists(ETRunService::getCliPath()) ? "是" : "否");
    lines << QString("EasyTier Core 路径: %1").arg(ETRunService::getCorePath());
    lines << QString("EasyTier Core 存在: %1").arg(QFile::exists(ETRunService::getCorePath()) ? "是" : "否");
    lines << QString("服务已安装: %1").arg(ETRunService::isServiceInstalled() ? "是" : "否");
    lines << QString("服务正在运行: %1").arg(ETRunService::isRunning() ? "是" : "否");
    lines << QString("最近错误: %1").arg(m_lastError.isEmpty() ? "-" : m_lastError);
    lines << "";

    lines << formatCommandResult("EasyTier Core 版本", ETRunService::queryCoreVersion());
    lines << formatCommandResult("node info", ETRunService::queryNodeInfoJson());
    lines << formatCommandResult("peer list", ETRunService::queryPeerListJson());
    lines << formatCommandResult("route list", ETRunService::queryRouteListJson());

    m_outputEdit->setPlainText(redactSecrets(lines.join('\n')));
}

void DiagnosticsDialog::copyDiagnostics() const
{
    QApplication::clipboard()->setText(m_outputEdit->toPlainText());
}

QString DiagnosticsDialog::formatCommandResult(const QString &title, const CommandResult &result)
{
    QStringList lines;
    lines << QString("## %1").arg(title);
    lines << QString("exitCode: %1").arg(result.exitCode);
    lines << QString("success: %1").arg(result.success ? "true" : "false");
    if (!result.errorString.isEmpty()) {
        lines << QString("error: %1").arg(result.errorString);
    }
    lines << "output:";
    lines << (result.output.trimmed().isEmpty() ? "-" : result.output.trimmed());
    lines << "";
    return lines.join('\n');
}

QString DiagnosticsDialog::redactSecrets(QString text)
{
    text.replace(QRegularExpression("(tcp://et-web\\.console\\.easytier\\.net:22020/)[^\\s\"']+"), "\\1<redacted>");
    text.replace(QRegularExpression("(connectionKey\\s*[=:]\\s*)[^\\s\"']+", QRegularExpression::CaseInsensitiveOption), "\\1<redacted>");
    text.replace(QRegularExpression("(oauthDeviceKey\\s*[=:]\\s*)[^\\s\"']+", QRegularExpression::CaseInsensitiveOption), "\\1<redacted>");
    text.replace(QRegularExpression("(access_token\\s*[=:]\\s*)[^\\s\"']+", QRegularExpression::CaseInsensitiveOption), "\\1<redacted>");
    text.replace(QRegularExpression("(token\\s*[=:]\\s*)[^\\s\"']+", QRegularExpression::CaseInsensitiveOption), "\\1<redacted>");
    text.replace(QRegularExpression("etk_[A-Za-z0-9._-]+"), "etk_<redacted>");
    return text;
}
