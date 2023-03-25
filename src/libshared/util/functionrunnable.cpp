/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2023-2023

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/


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
