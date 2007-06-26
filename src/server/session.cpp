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

#include "session.h"

#include "layer_data.h" // LayerData
#include "../shared/protocol.flags.h" // protocol::session::~

#ifndef NDEBUG
	#include <iostream>
#endif

Session::Session(const uint _id, uint _mode, uint _limit, uint _owner,
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

#ifndef NDEBUG
Session::~Session() throw()
{
	std::cout << "Session::~Session(ID: " << static_cast<int>(id) << ")" << std::endl;
}
#endif

octet Session::getFlags() const throw()
{
	return (persist ? protocol::session::Persist : 0);
}

bool Session::canJoin() const throw()
{
	return ((users.size() + waitingSync.size()) < limit);
}
