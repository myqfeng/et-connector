//
// Created by YMHuang on 2026/4/13.
//

#ifndef EASYTIERCONNECTOR_ETRUNSERVICE_H
#define EASYTIERCONNECTOR_ETRUNSERVICE_H

#include <QObject>


class ETRunService : public QObject
{
    Q_OBJECT

public:
    // 进程状态枚举
    enum class State {
        NotStarted,  // 未启动
        Starting,    // 启动中
        Running,     // 运行中
        Stopping     // 停止中
    } m_state ;
};


#endif //EASYTIERCONNECTOR_ETRUNSERVICE_H