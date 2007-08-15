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

#include "config.h"

#include "types.h"
#include "array.h" // Array<>

#include <list> // std::list
#include <map> // std::map

#ifdef PERSISTENT_SESSIONS
namespace protocol
{
	struct Raster;
}
#endif

class LayerData;
class User;

//! Session information
class Session
{
public:
	//! Default constructor
	/**
	 * @param[in] _id Session identifier
	 * @param[in] _mode Default user mode
	 * @param[in] _limit User limit
	 * @param[in] _owner Session owner ID
	 * @param[in] _width Canvas' width
	 * @param[in] _height Canvas' height
	 * @param[in] _level Required feature level
	 * @param[in] _title Session title
	 */
	Session(const uint _id, uint _mode, uint _limit, uint _owner,
		uint _width, uint _height, uint _level, Array<char>& _title) __attribute__ ((nothrow));
	
	#ifdef PERSISTENT_SESSIONS
	//! Destructor
	~Session() __attribute__ ((nothrow));
	#endif
	
	//! Session identifier
	uint id;
	
	//! Session title
	Array<char> title;
	
	//! Password string
	Array<char> password;
	
	//! Default user mode
	octet mode;
	
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
	octet getFlags() const __attribute__ ((nothrow,warn_unused_result));
	
	#ifdef LAYER_SUPPORT
	//! Layer identifier to layer data map
	std::map<octet, LayerData> layers;
	#endif
	
	//! Subscribed users
	std::map<octet, User*> users;
	
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
	bool raster_valid;
	#endif
	
	//! Session should persist
	bool persist;
	
	/* *** Functions *** */
	
	//! Get user
	User* getUser(const octet user_id) __attribute__ ((nothrow,warn_unused_result));
	
	//! Test if session can be joined
	bool canJoin() const __attribute__ ((nothrow,warn_unused_result));
	
	#ifdef PERSISTENT_SESSIONS
	//! Invalidate currently cached raster
	void invalidateRaster() __attribute__ ((nothrow));
	
	//! Append raster message to current cached raster
	bool appendRaster(protocol::Raster *raster) __attribute__ ((nothrow));
	#endif
};

#endif // ServerSession_INCLUDED
