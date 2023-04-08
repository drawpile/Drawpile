// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/functionrunnable.h"

namespace utils {

/*
    Code backported from Qt5.15 to be used in Qt5.12.
*/
FunctionRunnable::FunctionRunnable(std::function<void()> functionToRun) : m_functionToRun(std::move(functionToRun)) {
    setAutoDelete(true);
}

void FunctionRunnable::run() {
    m_functionToRun();
}

}
