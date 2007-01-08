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

#if !defined(NDEBUG) and defined(DEBUG_PROTOCOL)
void msgName(const uint8_t type) throw()
{
	std::cout << "type (" << static_cast<int>(type) << "): ";
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
	case type::SessionSelect:
		std::cout << "Session Select" << std::endl;
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
	case type::Deflate:
		std::cout << "Deflate" << std::endl;
		break;
	case type::Chat:
		std::cout << "Chat" << std::endl;
		break;
	case type::Palette:
		std::cout << "Palette" << std::endl;
		break;
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

Message* copyMessage(const Message* msg) throw(std::bad_alloc)
{
	#ifdef DEBUG_PROTOCOL
	#ifndef NDEBUG
	std::cout << "protocol::copyMessage - ";
	msgName(msg->type);
	#endif
	#endif
	
	switch (msg->type)
	{
	case type::Identifier:
		return new Identifier(*(static_cast<const Identifier*>(msg)));
	case type::StrokeInfo:
		return new StrokeInfo(*(static_cast<const StrokeInfo*>(msg)));
	case type::StrokeEnd:
		return new StrokeEnd(*(static_cast<const StrokeEnd*>(msg)));
	case type::ToolInfo:
		return new ToolInfo(*(static_cast<const ToolInfo*>(msg)));
	case type::Authentication:
		return new Authentication(*(static_cast<const Authentication*>(msg)));
	case type::Password:
		return new Password(*(static_cast<const Password*>(msg)));
	case type::Synchronize:
		return new Synchronize(*(static_cast<const Synchronize*>(msg)));
	case type::Raster:
		return new Raster(*(static_cast<const Raster*>(msg)));
	case type::SyncWait:
		return new SyncWait(*(static_cast<const SyncWait*>(msg)));
	case type::Subscribe:
		return new Subscribe(*(static_cast<const Subscribe*>(msg)));
	case type::Unsubscribe:
		return new Unsubscribe(*(static_cast<const Unsubscribe*>(msg)));
	case type::SessionSelect:
		return new SessionSelect(*(static_cast<const SessionSelect*>(msg)));
	case type::Instruction:
		return new Instruction(*(static_cast<const Instruction*>(msg)));
	case type::ListSessions:
		return new ListSessions(*(static_cast<const ListSessions*>(msg)));
	case type::Cancel:
		return new Cancel(*(static_cast<const Cancel*>(msg)));
	case type::UserInfo:
		return new UserInfo(*(static_cast<const UserInfo*>(msg)));
	case type::HostInfo:
		return new HostInfo(*(static_cast<const HostInfo*>(msg)));
	case type::SessionInfo:
		return new SessionInfo(*(static_cast<const SessionInfo*>(msg)));
	case type::Acknowledgement:
		return new Acknowledgement(*(static_cast<const Acknowledgement*>(msg)));
	case type::Error:
		return new Error(*(static_cast<const Error*>(msg)));
	case type::Deflate:
		return new Deflate(*(static_cast<const Deflate*>(msg)));
	case type::Chat:
		return new Chat(*(static_cast<const Chat*>(msg)));
	case type::Palette:
		return new Palette(*(static_cast<const Palette*>(msg)));
	default:
		return 0;
	}
}

Message* getMessage(const uint8_t type) throw()
{
	#ifdef DEBUG_PROTOCOL
	#ifndef NDEBUG
	std::cout << "protocol::getMessage - ";
	msgName(type);
	#endif
	#endif
	
	switch (type)
	{
	case type::Identifier:
		return new Identifier();
	case type::StrokeInfo:
		return new StrokeInfo();
	case type::StrokeEnd:
		return new StrokeEnd();
	case type::ToolInfo:
		return new ToolInfo();
	case type::Authentication:
		return new Authentication();
	case type::Password:
		return new Password();
	case type::Synchronize:
		return new Synchronize();
	case type::Raster:
		return new Raster();
	case type::SyncWait:
		return new SyncWait();
	case type::Subscribe:
		return new Subscribe();
	case type::Unsubscribe:
		return new Unsubscribe();
	case type::SessionSelect:
		return new SessionSelect();
	case type::Instruction:
		return new Instruction();
	case type::ListSessions:
		return new ListSessions();
	case type::Cancel:
		return new Cancel();
	case type::UserInfo:
		return new UserInfo();
	case type::HostInfo:
		return new HostInfo();
	case type::SessionInfo:
		return new SessionInfo();
	case type::Acknowledgement:
		return new Acknowledgement();
	case type::Error:
		return new Error();
	case type::Deflate:
		return new Deflate();
	case type::Chat:
		return new Chat();
	case type::Palette:
		return new Palette();
	default:
		return 0;
	}
}

} // namespace protocol
