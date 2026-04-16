/**
 * @brief EasyTier Core 服务管理器
 * 
 * 通过 easytier-cli.exe 管理 EasyTier Core 系统服务。
 * 服务安装后默认开机自启。
 */

#ifndef ETRUNSERVICE_H
#define ETRUNSERVICE_H

#include <QObject>
#include <QProcess>

/**
 * @brief EasyTier Core 系统服务管理器
 */
class ETRunService : public QObject
{
    Q_OBJECT

public:
    explicit ETRunService(QObject *parent = nullptr);
    ~ETRunService() override;
    
    // 启动服务（未安装则自动安装并启动）返回是否成功
    static bool start(const QString &connectionKey);
    
    // 停止服务并卸载，返回是否成功
    static bool stop();
    
    // 查询服务是否正在运行
    bool isRunning();

private:
    // 获取 easytier-cli.exe 路径
    static QString getCliPath() ;
    
    // 获取工作目录
    static QString getWorkingDirectory() ;
    
    // 执行命令并等待完成，返回退出码和输出
    static int executeCommand(const QString &command, const QStringList &args, QString &output);
    
    // 检查服务是否已安装
    static bool isServiceInstalled() ;
};

#endif // ETRUNSERVICE_H