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

#include <iostream>

namespace protocol
{

#ifndef NDEBUG
void msgName(const uint8_t type) throw()
{
	std::cout << "Message type (" << static_cast<int>(type) << "): ";
	switch (type)
	{
	case type::Identifier:
		std::cout << "Identifier" << std::endl;
		break;
	case type::StrokeInfo:
		std::cout << "Stroke Info" << std::endl;
		break;
	case type::StrokeEnd:
		std::cout << "Stroke End" << std::endl;
		break;
	case type::ToolInfo:
		std::cout << "Tool Info" << std::endl;
		break;
	case type::Authentication:
		std::cout << "Authentication" << std::endl;
		break;
	case type::Password:
		std::cout << "Password" << std::endl;
		break;
	case type::Synchronize:
		std::cout << "Synchronize" << std::endl;
		break;
	case type::Raster:
		std::cout << "Raster" << std::endl;
		break;
	case type::SyncWait:
		std::cout << "SyncWait" << std::endl;
		break;
	case type::Subscribe:
		std::cout << "Subscribe" << std::endl;
		break;
	case type::Unsubscribe:
		std::cout << "Unsubscribe" << std::endl;
		break;
	case type::Instruction:
		std::cout << "Instruction" << std::endl;
		break;
	case type::ListSessions:
		std::cout << "List Sessions" << std::endl;
		break;
	case type::Cancel:
		std::cout << "Cancel" << std::endl;
		break;
	case type::UserInfo:
		std::cout << "User Info" << std::endl;
		break;
	case type::HostInfo:
		std::cout << "Host Info" << std::endl;
		break;
	case type::SessionInfo:
		std::cout << "Session Info" << std::endl;
		break;
	case type::Acknowledgement:
		std::cout << "Acknowledgement" << std::endl;
		break;
	case type::Error:
		std::cout << "Error" << std::endl;
		break;
	/*
	case type::Deflate:
	case type::Chat:
	case type::Palette:
	*/
	default:
		std::cout << "{unknown}" << std::endl;
		break;
	}
}
#else
void msgName(const uint8_t type) throw()
{
}
#endif

Message* getMessage(const uint8_t type) throw()
{
	#ifndef NDEBUG
	std::cout << "protocol::getMessage("<< static_cast<int>(type) << ")" << std::endl;
	#endif
	
	switch (type)
	{
	case type::Identifier:
		msgName(type);
		return new Identifier();
		break;
	case type::StrokeInfo:
		msgName(type);
		return new StrokeInfo();
		break;
	case type::StrokeEnd:
		msgName(type);
		return new StrokeEnd();
		break;
	case type::ToolInfo:
		msgName(type);
		return new ToolInfo();
		break;
	case type::Authentication:
		msgName(type);
		return new Authentication();
		break;
	case type::Password:
		msgName(type);
		return new Password();
		break;
	case type::Synchronize:
		msgName(type);
		return new Synchronize();
		break;
	case type::Raster:
		msgName(type);
		return new Raster();
		break;
	case type::SyncWait:
		msgName(type);
		return new SyncWait();
		break;
	case type::Subscribe:
		msgName(type);
		return new Subscribe();
		break;
	case type::Unsubscribe:
		msgName(type);
		return new Unsubscribe();
		break;
	case type::Instruction:
		msgName(type);
		return new Instruction();
		break;
	case type::ListSessions:
		msgName(type);
		return new ListSessions();
		break;
	case type::Cancel:
		msgName(type);
		return new Cancel();
		break;
	case type::UserInfo:
		msgName(type);
		return new UserInfo();
		break;
	case type::HostInfo:
		msgName(type);
		return new HostInfo();
		break;
	case type::SessionInfo:
		msgName(type);
		return new SessionInfo();
		break;
	case type::Acknowledgement:
		msgName(type);
		return new Acknowledgement();
		break;
	case type::Error:
		msgName(type);
		return new Error();
		break;
	/*
	case type::Deflate:
	case type::Chat:
	case type::Palette:
	*/
	default:
		msgName(type);
		break;
	}
	
	return 0;
}

} // namespace protocol
