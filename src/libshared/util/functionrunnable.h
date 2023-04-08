// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FUNCTIONRUNNABLE_H
#define FUNCTIONRUNNABLE_H

#include <QRunnable>
#include <functional>

namespace utils {

/*
    Code backported from Qt5.15 to be used in Qt5.12.
*/
class FunctionRunnable : public QRunnable
{
    std::function<void()> m_functionToRun;
public:
    FunctionRunnable(std::function<void()> functionToRun);
    void run() override;
};

}

#endif
