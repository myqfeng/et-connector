#include "SettingsDialog.h"
#include "SystemTray.h"
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
#ifdef IS_NOT_ET_PRO
    setWindowTitle("设置连接地址与用户名");
#else
    setWindowTitle("设置连接地址与密钥");
#endif

    setWindowIcon(QIcon(":/assets/favicon.svg"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(400, 180);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 密钥输入组
#ifdef IS_NOT_ET_PRO
    QGroupBox *keyGroup = new QGroupBox("连接地址与用户名", this);
#else
    QGroupBox *keyGroup = new QGroupBox("连接地址与密钥", this);
#endif
    QHBoxLayout *keyLayout = new QHBoxLayout(keyGroup);
    keyGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

#ifdef IS_NOT_ET_PRO
    QLabel *keyLabel = new QLabel("地址与用户名:", this);
#else
    QLabel *keyLabel = new QLabel("地址与密钥:", this);
#endif
    m_keyEdit = new QLineEdit(this);
    m_keyEdit->setMinimumHeight(28); // 设置最小高度为28

    keyLayout->addWidget(keyLabel);
    keyLayout->addWidget(m_keyEdit);

    m_keyEdit->setPlaceholderText("例: tcp://et.example.cn:666/etk_xxxxx");
    mainLayout->addWidget(keyGroup);

    // 提示文本
#ifdef IS_NOT_ET_PRO
    QLabel *hintLabel = new QLabel("请输入 Web 控制台连接地址与用户名", this);
#else
    QLabel *hintLabel = new QLabel("请前往 EasyTier Pro 控制台获取连接地址与密钥", this);
#endif
    hintLabel->setStyleSheet("color: #66ccff;");
    hintLabel->setMaximumHeight(30);
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);
    
    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout(this);
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
#ifdef IS_NOT_ET_PRO
        m_keyEdit->setPlaceholderText("请输入连接地址与用户名");
#else
        m_keyEdit->setPlaceholderText("请输入连接地址与密钥");
#endif
        return;
    }
    
    emit connectionKeyChanged();
    QDialog::accept();
}