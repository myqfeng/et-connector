/**
 * @file DiagnosticsDialog.h
 * @brief EasyTier Pro 诊断窗口
 */

#ifndef DIAGNOSTICSDIALOG_H
#define DIAGNOSTICSDIALOG_H

#include <QDialog>
#include "ETRunService.h"

class QTextEdit;

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(const QString &lastError, QWidget *parent = nullptr);
    ~DiagnosticsDialog() override = default;

private slots:
    void refreshDiagnostics();
    void copyDiagnostics() const;

private:
    void setupUI();
    static QString redactSecrets(QString text);
    static QString formatCommandResult(const QString &title, const CommandResult &result);

private:
    QString m_lastError;
    QTextEdit *m_outputEdit = nullptr;
};

#endif // DIAGNOSTICSDIALOG_H
