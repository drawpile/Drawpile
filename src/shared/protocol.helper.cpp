/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:
   
   Except as contained in this notice, the name(s) of the above copyright
   holders shall not be used in advertising or otherwise to promote the sale,
   use or other dealings in this Software without prior written authorization.
   
   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

   ---

   For more info, see: http://drawpile.sourceforge.net/

*******************************************************************************/

#include "protocol.helper.h"

#ifndef NDEBUG
	#include <iostream>
#endif

#include "protocol.h"

namespace protocol
{

void msgName(const unsigned char type) throw()
{
	#if !defined(NDEBUG) and defined(DEBUG_PROTOCOL)
	std::cout << "type (" << static_cast<int>(type) << "): ";
	switch (type)
	{
	case Message::Identifier:
		std::cout << "Identifier" << std::endl;
		break;
	case Message::StrokeInfo:
		std::cout << "Stroke Info" << std::endl;
		break;
	case Message::StrokeEnd:
		std::cout << "Stroke End" << std::endl;
		break;
	case Message::ToolInfo:
		std::cout << "Tool Info" << std::endl;
		break;
	case Message::PasswordRequest:
		std::cout << "Password Request" << std::endl;
		break;
	case Message::Password:
		std::cout << "Password" << std::endl;
		break;
	case Message::Synchronize:
		std::cout << "Synchronize" << std::endl;
		break;
	case Message::Raster:
		std::cout << "Raster" << std::endl;
		break;
	case Message::SyncWait:
		std::cout << "SyncWait" << std::endl;
		break;
	case Message::Subscribe:
		std::cout << "Subscribe" << std::endl;
		break;
	case Message::Unsubscribe:
		std::cout << "Unsubscribe" << std::endl;
		break;
	case Message::SessionSelect:
		std::cout << "Session Select" << std::endl;
		break;
	case Message::SessionInstruction:
		std::cout << "SessionInstruction" << std::endl;
		break;
	case Message::Shutdown:
		std::cout << "Shutdown" << std::endl;
		break;
	case Message::SetPassword:
		std::cout << "Set Password" << std::endl;
		break;
	case Message::Authenticate:
		std::cout << "Authenticate" << std::endl;
		break;
	case Message::ListSessions:
		std::cout << "List Sessions" << std::endl;
		break;
	case Message::Cancel:
		std::cout << "Cancel" << std::endl;
		break;
	case Message::UserInfo:
		std::cout << "User Info" << std::endl;
		break;
	case Message::HostInfo:
		std::cout << "Host Info" << std::endl;
		break;
	case Message::SessionInfo:
		std::cout << "Session Info" << std::endl;
		break;
	case Message::Acknowledgement:
		std::cout << "Acknowledgement" << std::endl;
		break;
	case Message::Error:
		std::cout << "Error" << std::endl;
		break;
	case Message::Deflate:
		std::cout << "Deflate" << std::endl;
		break;
	case Message::Chat:
		std::cout << "Chat" << std::endl;
		break;
	case Message::Palette:
		std::cout << "Palette" << std::endl;
		break;
	default:
		std::cout << "{unknown}" << std::endl;
		break;
	}
	#endif
}

Message* getMessage(const unsigned char type) throw(std::bad_alloc)
{
	#ifdef DEBUG_PROTOCOL
	#ifndef NDEBUG
	std::cout << "protocol::getMessage - ";
	msgName(type);
	#endif
	#endif
	
	switch (type)
	{
	case Message::Identifier:
		return new Identifier();
	case Message::StrokeInfo:
		return new StrokeInfo();
	case Message::StrokeEnd:
		return new StrokeEnd();
	case Message::ToolInfo:
		return new ToolInfo();
	case Message::PasswordRequest:
		return new PasswordRequest();
	case Message::Password:
		return new Password();
	case Message::Synchronize:
		return new Synchronize();
	case Message::Raster:
		return new Raster();
	case Message::SyncWait:
		return new SyncWait();
	case Message::Subscribe:
		return new Subscribe();
	case Message::Unsubscribe:
		return new Unsubscribe();
	case Message::SessionSelect:
		return new SessionSelect();
	case Message::SessionInstruction:
		return new SessionInstruction();
	case Message::SetPassword:
		return new SetPassword();
	case Message::Shutdown:
		return new Shutdown();
	case Message::Authenticate:
		return new Authenticate();
	case Message::ListSessions:
		return new ListSessions();
	case Message::Cancel:
		return new Cancel();
	case Message::UserInfo:
		return new UserInfo();
	case Message::HostInfo:
		return new HostInfo();
	case Message::SessionInfo:
		return new SessionInfo();
	case Message::Acknowledgement:
		return new Acknowledgement();
	case Message::Error:
		return new Error();
	case Message::SessionEvent:
		return new SessionEvent();
	case Message::LayerEvent:
		return new LayerEvent();
	case Message::LayerSelect:
		return new LayerSelect();
	case Message::Deflate:
		return new Deflate();
	case Message::Chat:
		return new Chat();
	case Message::Palette:
		return new Palette();
	default:
		std::cerr << "Unknown message type: " << static_cast<unsigned int>(type) << std::endl;
		return 0;
	}
}

} // namespace protocol
