/**
 * @file ETRunService.h
 * @brief EasyTier Core 系统服务管理器
 * 
 * 通过 easytier-cli.exe 管理 EasyTier Core 系统服务。
 * 服务安装后默认开机自启。
 * 
 * 设计说明：
 * - 由于所有方法都是静态的，不需要继承 QObject
 * - 使用 CommandResult 结构体封装命令执行结果
 */

#ifndef ETRUNSERVICE_H
#define ETRUNSERVICE_H

#include <QString>
#include <QStringList>

/**
 * @brief 命令执行结果结构体
 */
struct CommandResult {
    int exitCode = -1;          ///< 进程退出码，-1 表示执行失败
    QString output;             ///< 标准输出和标准错误的合并内容
    bool success = false;       ///< 是否执行成功（exitCode == 0）
    QString errorString;        ///< 错误描述（如果有）
    
    [[nodiscard]] bool outputContains(const QString &text) const {
        return output.contains(text, Qt::CaseInsensitive);
    }
};

/**
 * @brief EasyTier Core 系统服务管理器
 * 
 * 提供静态方法管理 Windows 系统服务。
 * 服务名称: "QtET Web Connector"
 */
class ETRunService
{
public:
    ETRunService() = default;
    ~ETRunService() = default;
    
    // 禁止拷贝
    ETRunService(const ETRunService&) = delete;
    ETRunService& operator=(const ETRunService&) = delete;
    
    /**
     * @brief 启动服务
     * @param connectionKey 连接密钥
     * @return 是否启动成功
     * 
     * 如果服务未安装，会自动安装后启动。
     */
    static bool start(const QString &connectionKey);
    
    /**
     * @brief 停止并卸载服务
     * @return 是否停止成功
     */
    static bool stop();
    
    /**
     * @brief 查询 easytier-core 进程是否正在运行
     * @return 进程是否存在
     */
    static bool isRunning();
    
    /**
     * @brief 检查服务是否已安装
     * @return 服务是否已安装
     */
    static bool isServiceInstalled();
    
    /**
     * @brief 获取 easytier-cli.exe 路径
     * @return CLI 工具完整路径
     */
    static QString getCliPath();
    
    /**
     * @brief 获取工作目录
     * @return etcore 目录路径
     */
    static QString getWorkingDirectory();
    
    /**
     * @brief 执行命令并等待完成
     * @param command 命令路径
     * @param args 命令参数
     * @param timeoutMs 超时时间（毫秒），默认 30 秒
     * @return 命令执行结果
     */
    static CommandResult executeCommand(const QString &command, 
                                        const QStringList &args,
                                        int timeoutMs = 30000);

private:
    /// Windows 服务内部名称（不含空格）
    static constexpr const char* SERVICE_NAME = "QtETWebConnector";
    /// 服务显示名称
    static constexpr const char* SERVICE_DISPLAY_NAME = "QtETWebConnector";
};

#endif // ETRUNSERVICE_H