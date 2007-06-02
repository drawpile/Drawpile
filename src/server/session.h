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

#include "common.h"

#include "../shared/protocol.flags.h"
#include "../shared/protocol.tools.h"
#include "../shared/protocol.defaults.h"

#include "array.h"

struct User; // defined elsewhere
#ifndef ServerUser_INCLUDED
	#include "user.h"
#endif

#include <limits> // std::numeric_limits<T>

#include <list>

//! Layer information
struct LayerData
{
	//! ctor
	LayerData() throw()
		: id(protocol::null_layer),
		mode(protocol::tool_mode::None),
		opacity(0),
		locked(false)
	{
	}
	
	//! ctor
	LayerData(const uint _id, const uint _mode, const uint _opacity=std::numeric_limits<uint8_t>::max(), const bool _locked=false) throw()
		: id(_id),
		mode(_mode),
		opacity(_opacity),
		locked(_locked)
	{
	}
	
	//! dtor
	~LayerData() throw()
	{
		
	}
	
	uint
		//! Layer identifier
		id,
		//! Composition mode
		mode,
		//! Opacity
		opacity;
	
	//! Layer lock state
	bool locked;
};

/* iterators */
#include <map>
typedef std::map<uint8_t, User*>::iterator session_usr_i;
typedef std::map<uint8_t, User*>::const_iterator session_usr_const_i;
typedef std::map<uint8_t, LayerData>::iterator session_layer_i;
typedef std::map<uint8_t, LayerData>::const_iterator session_layer_const_i;

//! Session information
struct Session
{
	//! ctor
	Session(const uint _id, uint _mode, uint _limit, uint _owner,
		uint _width, uint _height, uint _level, Array<char>& _title) throw()
		: id(_id),
		title(_title),
		mode(_mode),
		limit(_limit),
		owner(_owner),
		width(_width),
		height(_height),
		level(_level),
		syncCounter(0),
		locked(false),
		#ifdef PERSISTENT_SESSIONS
		raster(0),
		raster_cached(false),
		raster_invalid(false),
		#endif
		persist(false)
	{
		#ifndef NDEBUG
		std::cout << "Session::Session(ID: " << static_cast<int>(id) << ")" << std::endl;
		#endif
	}
	
	//! dtor
	~Session() throw()
	{
		#ifndef NDEBUG
		std::cout << "Session::~Session(ID: " << static_cast<int>(id) << ")" << std::endl;
		#endif
	}
	
	//! Session identifier
	uint id;
	
	//! Session title
	Array<char> title;
	
	//! Password string
	Array<char> password;
	
	//! Default user mode
	uint8_t mode;
	
	//! User limit
	uint limit;
	
	//! Session owner
	uint owner;
	
	uint
		//! Canvas width
		width,
		//! Canvas height
		height;
	
	//! Feature level required
	uint level;
	
	//! Get session flags
	uint8_t getFlags() const throw()
	{
		return (persist?protocol::session::Persist:0);
	}
	
	//! Layer identifier to layer data map
	std::map<uint8_t, LayerData> layers;
	//std::set<LayerData> layers;
	
	//! Subscribed users
	std::map<uint8_t, User*> users;
	
	//! Users waiting sync.
	std::list<User*> waitingSync;
	
	//! Session wait-sync counter
	uint syncCounter;
	
	//! Session lock state
	bool locked;
	
	#ifdef PERSISTENT_SESSIONS
	//! Local raster copy
	protocol::Raster *raster;
	
	//! Raster cached
	bool raster_cached;
	
	//! Raster was invalidated
	bool raster_invalid;
	#endif
	
	//! Session should persist
	bool persist;
	
	/* *** Functions *** */
	
	//! Test if session can be joined
	bool canJoin() const throw()
	{
		return ((users.size() + waitingSync.size()) < limit);
	}
};

#endif // ServerSession_INCLUDED
