#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QString &connectionKey = "", QWidget *parent = nullptr);
    ~SettingsDialog();

    QString getConnectionKey() const;
    void setConnectionKey(const QString &key);

private:
    void setupUI();

    QLineEdit *m_keyEdit;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

protected:
    void accept() override;

signals:
    void connectionKeyChanged();
};

#endif // SETTINGSDIALOG_H
