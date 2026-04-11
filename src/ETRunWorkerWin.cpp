#include "ETRunWorkerWin.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QSysInfo>

ETRunWorker::ETRunWorker(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_startTimer(new QTimer(this))
    , m_stopTimer(new QTimer(this))
    , m_state(State::NotStarted)
    , m_startupSuccessReceived(false)
{
    // 配置定时器
    m_startTimer->setSingleShot(true);
    m_stopTimer->setSingleShot(true);
    
    // 连接进程信号
    connect(m_process, &QProcess::started,
            this, &ETRunWorker::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ETRunWorker::onProcessReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &ETRunWorker::onProcessReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ETRunWorker::onProcessFinished);
    
    // Qt5 兼容：连接 errorOccurred 信号
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    connect(m_process, &QProcess::errorOccurred,
            this, &ETRunWorker::onProcessError);
#else
    connect(m_process, QOverload<QProcess::ProcessError>::of(&QProcess::error),
            this, &ETRunWorker::onProcessError);
#endif
    
    // 连接超时定时器
    connect(m_startTimer, &QTimer::timeout, this, &ETRunWorker::onStartTimeout);
    connect(m_stopTimer, &QTimer::timeout, this, &ETRunWorker::onStopTimeout);
}

ETRunWorker::~ETRunWorker()
{
    // 强制停止并等待进程结束
    stopAndWait();
}

void ETRunWorker::start(const QString &connectionKey)
{
    // 验证参数
    if (connectionKey.isEmpty()) {
        emit startFailed("连接地址密钥不能为空");
        return;
    }
    
    // 检查当前状态
    if (m_state == State::Starting) {
        emit startFailed("EasyTier 正在启动中");
        return;
    }
    
    if (m_state == State::Running) {
        emit startFailed("EasyTier 已经在运行中");
        return;
    }
    
    if (m_state == State::Stopping) {
        emit startFailed("EasyTier 正在停止中，请稍后再试");
        return;
    }
    
    // 更新状态为启动中
    setState(State::Starting);
    
    // 重置启动成功标志
    m_startupSuccessReceived = false;
    
    // 准备启动
    QString program = getExecutablePath();
    QStringList args = buildArguments(connectionKey);
    QString workDir = getWorkingDirectory();
    
    qDebug() << "启动 EasyTier Core:";
    qDebug() << "  程序路径:" << program;
    qDebug() << "  工作目录:" << workDir;
    qDebug() << "  参数:" << args;
    
    // 检查可执行文件是否存在
    if (!QFile::exists(program)) {
        setState(State::NotStarted);
        emit startFailed(QString("找不到 EasyTier Core 程序: %1").arg(program));
        return;
    }
    
    // 设置工作目录并启动
    m_process->setWorkingDirectory(workDir);
    m_process->start(program, args);
    
    // 启动超时定时器
    m_startTimer->start(START_TIMEOUT_MS);
}

void ETRunWorker::stop()
{
    // 检查当前状态
    if (m_state == State::NotStarted) {
        emit stopped();
        return;
    }
    
    if (m_state == State::Stopping) {
        return; // 已经在停止中，静默忽略
    }
    
    if (m_state == State::Starting) {
        // 启动过程中请求停止，取消启动超时
        m_startTimer->stop();
    }
    
    // 更新状态为停止中
    setState(State::Stopping);
    
    // 停止超时定时器
    m_stopTimer->start(STOP_TIMEOUT_MS);
    
    // 优雅停止进程
    terminateProcess();
}

void ETRunWorker::stopAndWait()
{
    // 停止所有定时器
    m_startTimer->stop();
    m_stopTimer->stop();
    
    if (m_process->state() == QProcess::NotRunning) {
        m_state = State::NotStarted;
        return;
    }
    
    qDebug() << "正在停止 EasyTier 进程并等待结束...";
    
    // 先尝试优雅停止
    m_process->terminate();
    
    // 等待进程结束，最多3秒
    if (!m_process->waitForFinished(3000)) {
        qDebug() << "优雅停止超时，强制终止进程";
        m_process->kill();
        // 再等待2秒
        m_process->waitForFinished(2000);
    }
    
    m_state = State::NotStarted;
    qDebug() << "EasyTier 进程已停止";
}

bool ETRunWorker::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

void ETRunWorker::setState(State newState)
{
    if (m_state == newState) {
        return;
    }
    
    State oldState = m_state;
    m_state = newState;
    
    qDebug() << "状态转换:" << static_cast<int>(oldState) << "->" << static_cast<int>(newState);
    emit stateChanged(newState, oldState);
}

QStringList ETRunWorker::buildArguments(const QString &connectionKey)
{
    // Qt获取设备主机名
    const QString &hostname = QSysInfo::machineHostName();

    QStringList args;
    args << "--config-server" << connectionKey;
    args << "--hostname" << hostname;
    args << "--secure-mode" << "true";
    return args;
}

QString ETRunWorker::getExecutablePath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString exePath = QDir::toNativeSeparators(appDir + "/etcore/easytier-core.exe");
    return exePath;
}

QString ETRunWorker::getWorkingDirectory() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    return QDir::toNativeSeparators(appDir + "/etcore");
}

void ETRunWorker::terminateProcess()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "正在优雅停止 EasyTier 进程...";
        m_process->terminate();
    }
}

void ETRunWorker::killProcess()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        qDebug() << "正在强制终止 EasyTier 进程...";
        m_process->kill();
    }
}

void ETRunWorker::onProcessStarted()
{
    qDebug() << "EasyTier 进程已启动，等待 Successful 标志...";
    
    // 进程已启动，但还需要等待 "Successful" 输出才算真正成功
    // 不停止启动超时定时器，继续等待 Successful 输出
}

void ETRunWorker::onProcessError(QProcess::ProcessError error)
{
    qDebug() << "进程错误:" << error;
    
    // 只在启动过程中处理错误
    if (m_state != State::Starting) {
        return;
    }
    
    // 停止启动超时定时器
    m_startTimer->stop();
    
    // 重置状态
    setState(State::NotStarted);
    
    // 生成错误信息
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "无法启动进程，请检查程序路径和权限";
        break;
    case QProcess::Crashed:
        errorMsg = "进程启动后立即崩溃";
        break;
    case QProcess::Timedout:
        errorMsg = "进程启动超时";
        break;
    case QProcess::WriteError:
        errorMsg = "写入进程失败";
        break;
    case QProcess::ReadError:
        errorMsg = "读取进程输出失败";
        break;
    default:
        errorMsg = QString("未知错误: %1").arg(error);
        break;
    }
    
    emit startFailed(errorMsg);
}

void ETRunWorker::onProcessReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();
    
    if (!output.isEmpty()) {
        qDebug() << "[EasyTier stdout]" << output;
        emit logOutput(output);
        
        // 检查是否包含 "Successful" 标志
        if (m_state == State::Starting && !m_startupSuccessReceived && output.contains("Successful")) {
            m_startupSuccessReceived = true;
            qDebug() << "检测到 Successful 标志，启动成功";
            
            // 停止启动超时定时器
            m_startTimer->stop();
            
            // 更新状态为运行中
            setState(State::Running);
            
            // 发送启动成功信号
            emit started();
        }
    }
}

void ETRunWorker::onProcessReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    QString error = QString::fromUtf8(data).trimmed();
    
    if (!error.isEmpty()) {
        qDebug() << "[EasyTier stderr]" << error;
        emit logError(error);
    }
}

void ETRunWorker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "进程结束: exitCode=" << exitCode << ", exitStatus=" << exitStatus;
    
    // 停止所有定时器
    m_startTimer->stop();
    m_stopTimer->stop();
    
    // 根据当前状态处理
    switch (m_state) {
    case State::Starting:
        // 启动过程中进程结束
        setState(State::NotStarted);
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            // 正常退出但太快，可能是配置问题
            emit startFailed("进程启动后立即退出，请检查配置参数");
        } else {
            emit startFailed(QString("进程启动失败，退出码: %1").arg(exitCode));
        }
        break;
        
    case State::Running:
        // 运行中进程异常结束
        setState(State::NotStarted);
        emit logError(QString("进程异常终止，退出码: %1").arg(exitCode));
        break;
        
    case State::Stopping:
        // 停止过程中进程结束
        setState(State::NotStarted);
        emit stopped();
        break;
        
    default:
        setState(State::NotStarted);
        break;
    }
}

void ETRunWorker::onStartTimeout()
{
    qDebug() << "启动超时";
    
    // 重置状态
    setState(State::NotStarted);
    
    // 杀死进程
    killProcess();
    
    emit startFailed("启动超时，请检查网络连接和配置");
}

void ETRunWorker::onStopTimeout()
{
    qDebug() << "停止超时，强制终止进程";
    
    // 重置状态
    setState(State::NotStarted);
    
    // 强制杀死进程
    killProcess();
    
    emit stopped();
}