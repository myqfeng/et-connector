#include "AboutDialog.h"
#include <QIcon>
#include <QPixmap>
#include <QDate>
#include <QApplication>


AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::setupUI()
{
    setWindowTitle("关于 EasyTier 控制台连接器");
    setWindowIcon(QIcon(":/assets/favicon.svg"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setFixedSize(400, 250);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    
    // 图标
    m_iconLabel = new QLabel(this);
    QPixmap iconPixmap(":/assets/favicon.svg");
    m_iconLabel->setPixmap(iconPixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_iconLabel);
    
    // 标题
    m_titleLabel = new QLabel("EasyTier 控制台连接器", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);
    
    // 版本
    m_versionLabel = new QLabel(QString("版本: %1").arg(qApp->applicationVersion()), this);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_versionLabel);
    
    // 描述
    m_descriptionLabel = new QLabel(
        "极简设计，简单易用的 EasyTier Web 控制台连接器",
        this
    );
    m_descriptionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_descriptionLabel);

    // 版权
    m_copyrightLabel = new QLabel(QString("Copyright © %1 明月清风. All rights reserved.").arg(QDate::currentDate().year()), this);
    QFont copyrightFont = m_copyrightLabel->font();
    copyrightFont.setPointSize(8);
    m_copyrightLabel->setFont(copyrightFont);
    m_copyrightLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_copyrightLabel);
    
    // 按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout(this);
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("确定", this);
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
}
