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

#include <stdint.h>
#if defined(HAVE_SLIST)
	#include <ext/slist>
#else
	#include <list>
#endif

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

/* iterators */
#if defined(HAVE_HASH_MAP)
#include <ext/hash_map>
typedef __gnu_cxx::hash_map<uint8_t, User*>::iterator session_usr_i;
typedef __gnu_cxx::hash_map<uint8_t, User*>::const_iterator session_usr_const_i;
typedef __gnu_cxx::hash_map<uint8_t, LayerData>::iterator session_layer_i;
typedef __gnu_cxx::hash_map<uint8_t, LayerData>::const_iterator session_layer_const_i;
#else
#include <map>
typedef std::map<uint8_t, User*>::iterator session_usr_i;
typedef std::map<uint8_t, User*>::const_iterator session_usr_const_i;
typedef std::map<uint8_t, LayerData>::iterator session_layer_i;
typedef std::map<uint8_t, LayerData>::const_iterator session_layer_const_i;
#endif

// Session information
struct Session
	//: MemoryStack<Session>
{
	Session(const uint8_t _id=protocol::Global) throw()
		: id(_id),
		title_len(0),
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
	
	Session(const uint8_t _id, uint8_t _mode, uint8_t _limit, uint8_t _owner,
		uint16_t _width, uint16_t _height, uint16_t _level, uint8_t _title_len, char* _title) throw()
		: id(_id),
		title_len(_title_len),
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
		
		waitingSync.clear();
		users.clear();
		
		delete [] title;
	}
	
	// Session identifier
	uint8_t id;
	
	// Title length
	uint8_t title_len;
	
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
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<uint8_t, LayerData> layers;
	#else
	std::map<uint8_t, LayerData> layers;
	#endif
	
	// Subscribed users
	#if defined(HAVE_HASH_MAP)
	__gnu_cxx::hash_map<uint8_t, User*> users;
	#else
	std::map<uint8_t, User*> users;
	#endif
	
	// Users waiting sync.
	#if defined(HAVE_SLIST)
	__gnu_cxx::slist<User*> waitingSync;
	#else // 
	std::list<User*> waitingSync;
	#endif
	
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
