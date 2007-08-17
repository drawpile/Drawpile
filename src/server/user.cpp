/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "user.h"

#include "session_data.h" // SessionData*

#include "../shared/protocol.flags.h"
#include "../shared/protocol.defaults.h"
#include "../shared/protocol.h"

#include "../shared/templates.h"

#include "../shared/protocol.helper.h" // protocol::getMessage

typedef std::map<octet, SessionData>::iterator usr_session_i;
typedef std::map<octet, SessionData>::const_iterator usr_session_const_i;

typedef std::deque<message_ref>::iterator usr_message_i;
typedef std::deque<message_ref>::const_iterator usr_message_const_i;

User::User(const octet _id, const Socket& nsock)
	: sock(nsock),
	session(0),
	id(_id),
	events(0),
	state(Init),
	layer(protocol::null_layer),
	syncing(protocol::Global),
	isAdmin(false),
	// client caps
	c_acks(false),
	// extensions
	ext_deflate(false),
	ext_chat(false),
	ext_palette(false),
	// other
	inMsg(0),
	level(0),
	touched(0),
	session_data(0),
	strokes(0)
{
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "User::User(" << static_cast<int>(_id)
		<< ", " << sock.fd() << ")" << std::endl;
	#endif
	assert(_id != protocol::null_user);
}

User::~User()
{
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "User::~User()" << std::endl;
	#endif
	
	delete inMsg;
	
	sessions.clear();
}

bool User::makeActive(octet session_id)
{
	session_data = getSession(session_id);
	if (session_data != 0)
	{
		session = session_data->session;
		return true;
	}
	else
	{
		session = 0;
		return false;
	}
}

SessionData* User::getSession(octet session_id)
{
	const usr_session_i usi(sessions.find(session_id));
	return (usi == sessions.end() ? 0 : &usi->second);
}

const SessionData* User::getConstSession(octet session_id) const
{
	const usr_session_const_i usi(sessions.find(session_id));
	return (usi == sessions.end() ? 0 : &usi->second);
}

void User::cacheTool(protocol::ToolInfo* ti)
{
	assert(ti != session_data->cachedToolInfo); // attempted to re-cache same tool
	assert(ti != 0);
	
	#if defined(DEBUG_USER) and !defined(NDEBUG)
	std::cout << "Caching Tool Info for user #" << static_cast<int>(id) << std::endl;
	#endif
	
	delete session_data->cachedToolInfo;
	session_data->cachedToolInfo = new protocol::ToolInfo(*ti); // use copy-ctor
}

octet User::getCapabilities() const
{
	return (c_acks?(protocol::client::AckFeedback):0);
}

void User::setCapabilities(const octet flags)
{
	c_acks = fIsSet(flags, static_cast<octet>(protocol::client::AckFeedback));
}

octet User::getExtensions() const
{
	return (ext_deflate?(protocol::extensions::Deflate):0)
		+ (ext_chat?(protocol::extensions::Chat):0)
		+ (ext_palette?(protocol::extensions::Palette):0);
}

void User::setExtensions(const octet flags)
{
	ext_deflate = fIsSet(flags, static_cast<octet>(protocol::extensions::Deflate));
	ext_chat = fIsSet(flags, static_cast<octet>(protocol::extensions::Chat));
	ext_palette = fIsSet(flags, static_cast<octet>(protocol::extensions::Palette));
}

uint User::flushQueue()
{
	assert(!queue.empty());
	
	
	const usr_message_i f_msg(queue.begin());
	usr_message_i iter(f_msg+1), last(f_msg);
	// create linked list
	size_t links=1;
	for (; iter != queue.end(); ++iter, ++links)
	{
		if (links == std::numeric_limits<octet>::max()
			or ((*iter)->type != (*last)->type)
			or ((*iter)->user_id != (*last)->user_id)
			or ((*iter)->session_id != (*last)->session_id)
		)
			break; // type changed or reached maximum size of linked list
		
		//(*iter)->next = 0;
		(*iter)->prev = &(**last);
		(*last)->next = &(**iter);
		last = iter;
	}
	
	size_t len=0, size=output.canWrite();
	
	// serialize message/s
	char* buf = (*last)->serialize(len, output.wpos, size);
	
	// in case new buffer was allocated
	if (buf != output.wpos)
		output.set(buf, size, len);
	else
		output.write(len);
	
	/**
	 * @bug Without the special case for link count of 1,
	 * the SharedPtr class breaks and causes segmentation error for some obscure reason.
	 */
	if (links == 1)
		queue.pop_front();
	else
		queue.erase(f_msg, iter);
	
	return links;
}

bool User::getMessage()
{
	if (!inMsg)
	{
		inMsg = protocol::getMessage(input.rpos[0]);
		if (!inMsg)
			return false; // invalid type, drop user
	}
	
	getreqlen:
	size_t cread = input.canRead();
	size_t reqlen = inMsg->reqDataLen(input.rpos, cread);
	if (reqlen > input.left)
	{
		if (reqlen > std::numeric_limits<ushort>::max())
		{
			// invalid data
			delete inMsg;
			inMsg = 0;
		}
		return false; // need more data
	}
	else if (reqlen > cread)
	{
		// Required length is greater than we can currently read,
		// but not greater than we have in total.
		// So, we reposition the buffer for maximal reading.
		input.reposition();
		goto getreqlen;
	}
	else
		input.read( inMsg->unserialize(input.rpos, cread) );
	
	return true;
}
