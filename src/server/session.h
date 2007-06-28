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

#include "types.h"
#include "array.h" // Array<>

#include <list> // std::list
#include <map> // std::map

namespace protocol
{
	struct Raster;
}

struct LayerData;
struct User;

//! Session information
struct Session
{
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
		uint _width, uint _height, uint _level, Array<char>& _title) throw();
	
	#ifndef NDEBUG
	//! Destructor
	~Session() throw();
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
	octet getFlags() const throw();
	
	//! Layer identifier to layer data map
	std::map<octet, LayerData> layers;
	
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
	bool raster_cached;
	
	//! Raster was invalidated
	bool raster_invalid;
	#endif
	
	//! Session should persist
	bool persist;
	
	/* *** Functions *** */
	
	//! Test if session can be joined
	bool canJoin() const throw();
	
	//! Invalidate currently cached raster
	void invalidateRaster() throw();
	
	//! Append raster message to current cached raster
	bool appendRaster(protocol::Raster *raster) throw();
};

#endif // ServerSession_INCLUDED
