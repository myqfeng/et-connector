#include <QApplication>
#include <QSystemTrayIcon>
#include <QFont>
#include <QThread>
#include <QCommandLineParser>
#include "SystemTray.h"

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    
    // 设置全局默认字体大小为10px
    QFont defaultFont = app.font();
    defaultFont.setPointSize(10);
    app.setFont(defaultFont);
    
    app.setApplicationName("EasyTier Connector");
    app.setApplicationVersion("0.0.1");
    app.setQuitOnLastWindowClosed(false); // 关闭窗口时不退出应用
    
    // 解析命令行参数
    QCommandLineParser parser;
    parser.setApplicationDescription("EasyTier Connector - EasyTier 控制台连接器");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // 添加 --auto-start 选项
    QCommandLineOption autoStartOption(
        QStringList() << "auto-start",
        "开机自启模式启动"
    );
    parser.addOption(autoStartOption);
    
    parser.process(app);
    
    bool isAutoStart = parser.isSet(autoStartOption);
    
    // 创建系统托盘
    SystemTray *tray = new SystemTray();
    tray->setAutoStartMode(isAutoStart);
    tray->show();

    int result = app.exec();

    return result;
}
