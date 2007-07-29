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
	using std::cout;
	using std::cerr;
	using std::endl;
#endif

#include "protocol.h"

namespace protocol
{

void msgName(const unsigned char type)
{
	#if !defined(NDEBUG) and defined(DEBUG_PROTOCOL)
	cout << "type (" << static_cast<int>(type) << "): ";
	switch (type)
	{
	case Message::Identifier:
		cout << "Identifier" << endl;
		break;
	case Message::StrokeInfo:
		cout << "Stroke Info" << endl;
		break;
	case Message::StrokeEnd:
		cout << "Stroke End" << endl;
		break;
	case Message::ToolInfo:
		cout << "Tool Info" << endl;
		break;
	case Message::PasswordRequest:
		cout << "Password Request" << endl;
		break;
	case Message::Password:
		cout << "Password" << endl;
		break;
	case Message::Synchronize:
		cout << "Synchronize" << endl;
		break;
	case Message::Raster:
		cout << "Raster" << endl;
		break;
	case Message::SyncWait:
		cout << "SyncWait" << endl;
		break;
	case Message::Subscribe:
		cout << "Subscribe" << endl;
		break;
	case Message::Unsubscribe:
		cout << "Unsubscribe" << endl;
		break;
	case Message::SessionSelect:
		cout << "Session Select" << endl;
		break;
	case Message::SessionInstruction:
		cout << "SessionInstruction" << endl;
		break;
	case Message::Shutdown:
		cout << "Shutdown" << endl;
		break;
	case Message::SetPassword:
		cout << "Set Password" << endl;
		break;
	case Message::Authenticate:
		cout << "Authenticate" << endl;
		break;
	case Message::ListSessions:
		cout << "List Sessions" << endl;
		break;
	case Message::Cancel:
		cout << "Cancel" << endl;
		break;
	case Message::UserInfo:
		cout << "User Info" << endl;
		break;
	case Message::HostInfo:
		cout << "Host Info" << endl;
		break;
	case Message::SessionInfo:
		cout << "Session Info" << endl;
		break;
	case Message::Acknowledgement:
		cout << "Acknowledgement" << endl;
		break;
	case Message::Error:
		cout << "Error" << endl;
		break;
	case Message::Deflate:
		cout << "Deflate" << endl;
		break;
	case Message::Chat:
		cout << "Chat" << endl;
		break;
	case Message::Palette:
		cout << "Palette" << endl;
		break;
	default:
		cout << "{unknown}" << endl;
		break;
	}
	#endif
}

Message* getMessage(const unsigned char type)
{
	#ifdef DEBUG_PROTOCOL
	#ifndef NDEBUG
	cout << "protocol::getMessage - ";
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
		#ifndef NDEBUG
		cerr << "Unknown message type: " << static_cast<unsigned int>(type) << endl;
		#endif
		return 0;
	}
}

} // namespace protocol
