/*******************************************************************************

   Copyright (C) 2007, 2008 M.K.A. <wyrmchild@users.sourceforge.net>
   
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#pragma once

#ifndef Event_H
#define Event_H

#include "config.h"

#if defined(EV_EPOLL)
	#include "epoll.h"
#elif defined(EV_SELECT)
	#include "select.h"
#else
	#error No event mechanism defined!
#endif

#endif // Event_H
