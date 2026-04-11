#ifndef ETRUNWORKERWIN_H
#define ETRUNWORKERWIN_H

#include <QObject>
#include <QProcess>
#include <QTimer>

/**
 * @brief EasyTier Core 进程管理器
 * 
 * 负责启动、停止 EasyTier Core 进程，并管理其生命周期。
 * 
 * 状态转换图：
 * NotStarted -> Starting -> Running -> Stopping -> NotStarted
 *      ^          |          |           ^
 *      |          |           |           |
 *      +----------+          +-----------+
 *         (失败)                (停止)
 */
class ETRunWorkerWin : public QObject
{
    Q_OBJECT

public:
    // 进程状态枚举
    enum class State {
        NotStarted,  // 未启动
        Starting,    // 启动中
        Running,     // 运行中
        Stopping     // 停止中
    };

    explicit ETRunWorkerWin(QObject *parent = nullptr);
    ~ETRunWorkerWin();
    
    // 启动 EasyTier Core
    void start(const QString &connectionKey);
    
    // 停止 EasyTier Core
    void stop();
    
    // 强制停止并等待进程结束
    void stopAndWait();
    
    // 获取当前状态
    State state() const { return m_state; }
    
    // 检查是否正在运行
    bool isRunning() const;

signals:
    // 状态变化信号
    void stateChanged(State newState, State oldState);
    
    // 启动成功
    void started();
    
    // 启动失败
    void startFailed(const QString &error);
    
    // 停止完成
    void stopped();
    
    // 进程输出日志
    void logOutput(const QString &output);
    
    // 进程错误日志
    void logError(const QString &error);

private slots:
    // 进程启动成功
    void onProcessStarted();
    
    // 进程错误
    void onProcessError(QProcess::ProcessError error);
    
    // 进程标准输出
    void onProcessReadyReadStandardOutput();
    
    // 进程错误输出
    void onProcessReadyReadStandardError();
    
    // 进程结束
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
    // 启动超时
    void onStartTimeout();
    
    // 停止超时
    void onStopTimeout();

private:
    // 状态转换
    void setState(State newState);
    
    // 构建启动参数
    QStringList buildArguments(const QString &connectionKey);
    
    // 获取可执行文件路径
    QString getExecutablePath() const;
    
    // 获取工作目录
    QString getWorkingDirectory() const;
    
    // 优雅停止进程
    void terminateProcess();
    
    // 强制杀死进程
    void killProcess();

private:
    // 进程对象
    QProcess *m_process;
    
    // 启动超时定时器
    QTimer *m_startTimer;
    
    // 停止超时定时器
    QTimer *m_stopTimer;
    
    // 当前状态
    State m_state;
    
    // 是否已收到启动成功的标志（标准输出包含 "Successful"）
    bool m_startupSuccessReceived;
    
    // 超时时间常量（毫秒）
    static constexpr int START_TIMEOUT_MS = 30000;  // 30秒
    static constexpr int STOP_TIMEOUT_MS = 10000;   // 10秒
};

#endif // ETRUNWORKERWIN_H