/*******************************************************************************

   Copyright (C) 2006 M.K.A. <wyrmchild@sourceforge.net>

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

#ifndef PROTOCOL_HELPER_H_INCLUDED
#define PROTOCOL_HELPER_H_INCLUDED

#include <iostream>

#include <stdint.h>
#include "protocol.h"

namespace protocol
{

inline
Message* getMessage(uint8_t type) throw(std::exception)
{
	#ifndef NDEBUG
	std::cout << "protocol::getMessage("<< static_cast<int>(type) << ")" << std::endl;
	#endif
	
	switch (type)
	{
	case type::Identifier:
		#ifndef NDEBUG
		std::cout << "Type: Identifier" << std::endl;
		#endif
		return new Identifier();
		break;
	case type::StrokeInfo:
		#ifndef NDEBUG
		std::cout << "Type: Stroke Info" << std::endl;
		#endif
		return new StrokeInfo();
		break;
	case type::StrokeEnd:
		#ifndef NDEBUG
		std::cout << "Type: Stroke End" << std::endl;
		#endif
		return new StrokeEnd();
		break;
	case type::ToolInfo:
		#ifndef NDEBUG
		std::cout << "Type: Tool Info" << std::endl;
		#endif
		return new ToolInfo();
		break;
	case type::Authentication:
		#ifndef NDEBUG
		std::cout << "Type: Authentication" << std::endl;
		#endif
		return new Authentication();
		break;
	case type::Password:
		#ifndef NDEBUG
		std::cout << "Type: Password" << std::endl;
		#endif
		return new Password();
		break;
	case type::Synchronize:
		#ifndef NDEBUG
		std::cout << "Type: Synchronize" << std::endl;
		#endif
		return new Synchronize();
		break;
	case type::Raster:
		#ifndef NDEBUG
		std::cout << "Type: Raster" << std::endl;
		#endif
		return new Raster();
		break;
	case type::SyncWait:
		#ifndef NDEBUG
		std::cout << "Type: SyncWait" << std::endl;
		#endif
		return new SyncWait();
		break;
	case type::Subscribe:
		#ifndef NDEBUG
		std::cout << "Type: Subscribe" << std::endl;
		#endif
		return new Subscribe();
		break;
	case type::Unsubscribe:
		#ifndef NDEBUG
		std::cout << "Type: Unsubscribe" << std::endl;
		#endif
		return new Unsubscribe();
		break;
		/*
	case type::SessionSelect:
		return new SessionSelect();
		break;
		*/
	case type::Instruction:
		#ifndef NDEBUG
		std::cout << "Type: Instruction" << std::endl;
		#endif
		return new Instruction();
		break;
	case type::ListSessions:
		#ifndef NDEBUG
		std::cout << "Type: List Sessions" << std::endl;
		#endif
		return new ListSessions();
		break;
	case type::Cancel:
		#ifndef NDEBUG
		std::cout << "Type: Cancel" << std::endl;
		#endif
		return new Cancel();
		break;
	case type::UserInfo:
		#ifndef NDEBUG
		std::cout << "Type: User Info" << std::endl;
		#endif
		return new UserInfo();
		break;
	case type::HostInfo:
		#ifndef NDEBUG
		std::cout << "Type: Host Info" << std::endl;
		#endif
		return new HostInfo();
		break;
	case type::SessionInfo:
		#ifndef NDEBUG
		std::cout << "Type: Session Info" << std::endl;
		#endif
		return new SessionInfo();
		break;
	case type::Acknowledgement:
		#ifndef NDEBUG
		std::cout << "Type: Acknowledgement" << std::endl;
		#endif
		return new Acknowledgement();
		break;
	case type::Error:
		#ifndef NDEBUG
		std::cout << "Type: Error" << std::endl;
		#endif
		return new Error();
		break;
	/*
	case Deflate:
	case Chat:
	case Palette:
	*/
	default:
		std::cerr << "Unknown message type: " << static_cast<int>(type) << std::endl;
		throw std::exception();
		break;
	}
	
	return 0;
}

} // namespace protocol

#endif // PROTOCOL_HELPER_H_INCLUDED
