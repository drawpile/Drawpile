/*******************************************************************************

   Copyright (C) 2006, 2007 M.K.A. <wyrmchild@users.sourceforge.net>
   For more info, see: http://drawpile.sourceforge.net/

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef ServerSession_INCLUDED
#define ServerSession_INCLUDED

//#include "../shared/protocol.types.h"
#include "../shared/protocol.flags.h"
#include "../shared/protocol.tools.h"
#include "../shared/protocol.defaults.h"

#include "../shared/memstack.h"

struct LayerData;
struct Session;
#include "user.h"

/* iterators */
typedef std::map<uint8_t, User*>::iterator session_usr_iterator;
typedef std::map<uint8_t, LayerData>::iterator session_layer_iterator;

#include <stdint.h>
#include <list>

struct LayerData
{
	LayerData() throw()
		: id(protocol::null_layer),
		mode(protocol::tool_mode::None),
		opacity(0)
	{
		
	}
	~LayerData() throw()
	{
		
	}
	
	uint8_t
		// identifier
		id,
		// composition mode
		mode,
		// transparency/opacity
		opacity;
	
	bool locked;
};

// Session information
struct Session
	//: MemoryStack<Session>
{
	Session(const uint8_t _id=protocol::Global) throw()
		: id(_id),
		len(0),
		title(0),
		pw_len(0),
		password(0),
		mode(protocol::user_mode::None),
		limit(10),
		owner(protocol::null_user),
		width(0),
		height(0),
		level(0),
		SelfDestruct(true),
		syncCounter(0),
		locked(false)
	{
		#ifndef NDEBUG
		std::cout << "Session::Session()" << std::endl;
		#endif
	}
	
	Session(const uint8_t _id=protocol::Global, uint8_t _mode, uint8_t _limit, uint8_t _owner,
		uint8_t _width, uint8_t _height, uint8_t _level, uint8_t title_len, char* _title) throw()
		: id(_id),
		len(title_len),
		title(_title),
		pw_len(0),
		password(0),
		mode(_mode),
		limit(_limit),
		owner(_owner),
		width(_width),
		height(_height),
		level(_level),
		SelfDestruct(true),
		syncCounter(0),
		locked(false)
	{
		#ifndef NDEBUG
		std::cout << "Session::Session()" << std::endl;
		#endif
	}
	
	~Session() throw()
	{
		#ifndef NDEBUG
		std::cout << "Session::~Session()" << std::endl;
		#endif
		
		// TODO: Clear user and waiting user lists.
		
		waitingSync.clear();
		users.clear();
		
		delete [] title;
	}
	
	// Session identifier
	uint8_t id;
	
	// Title length
	uint8_t len;
	
	// Session title
	char* title;
	
	// Password length
	uint8_t pw_len;
	
	// Password string
	char* password;
	
	// Default user mode
	uint8_t mode;
	
	// User limit
	uint8_t limit;
	
	// Session owner
	uint8_t owner;
	
	// Canvas size
	uint16_t width, height;
	
	// Feature level required
	uint16_t level;
	
	// Will the session be destructed when all users leave..?
	bool SelfDestruct;
	
	uint8_t getFlags() const throw()
	{
		return (SelfDestruct?0:protocol::session::NoSelfDestruct);
	}
	
	// 
	std::map<uint8_t, LayerData> layers;
	
	// Subscribed users
	std::map<uint8_t, User*> users;
	
	// Users waiting sync.
	std::list<User*> waitingSync;
	
	// Session sync in action.
	uint32_t syncCounter;
	
	// Session is locked, preventing any drawing to take place.
	bool locked;
	
	/* *** Functions *** */
	
	// Session can be joined
	bool canJoin()
	{
		return ((users.size() + waitingSync.size()) < limit);
	}
};

#endif // ServerSession_INCLUDED
