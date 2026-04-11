#include "SettingsDialog.h"
#include <QIcon>

SettingsDialog::SettingsDialog(const QString &connectionKey, QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setConnectionKey(connectionKey);
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUI()
{
    setWindowTitle("设置连接地址与密钥");
    setWindowIcon(QIcon(":/assets/favicon.svg"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(400, 180);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 密钥输入组
    QGroupBox *keyGroup = new QGroupBox("连接地址与密钥", this);
    QHBoxLayout *keyLayout = new QHBoxLayout(keyGroup);
    keyGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QLabel *keyLabel = new QLabel("地址与密钥:", this);
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setMinimumHeight(28); // 设置最小高度为28

    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(m_keyEdit);

    m_keyEdit->setPlaceholderText("例: tcp://et.example.cn:666/etk_xxxxx");
    mainLayout->addWidget(keyGroup);

    // 提示文本
    QLabel *hintLabel = new QLabel("请前往 EasyTier Pro 控制台获取连接地址与密钥", this);
    hintLabel->setStyleSheet("color: #66ccff;");
    hintLabel->setMaximumHeight(30);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);
    
    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("确定", this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    
    m_cancelButton = new QPushButton("取消", this);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

QString SettingsDialog::getConnectionKey() const
{
    return m_keyEdit->text();
}

void SettingsDialog::setConnectionKey(const QString &key)
{
    m_keyEdit->setText(key);
}

void SettingsDialog::accept()
{
    // 验证输入
    QString key = m_keyEdit->text().trimmed();
    if (key.isEmpty()) {
        m_keyEdit->setFocus();
        m_keyEdit->setPlaceholderText("请输入连接地址与密钥");
        return;
    }
    
    emit connectionKeyChanged();
    QDialog::accept();
}