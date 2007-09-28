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

#ifdef LAYER_DATA
	#include "layer_data.h" // LayerData
#endif

#include "../shared/protocol.flags.h" // protocol::session::~
#ifdef PERSISTENT_SESSIONS
	#include "../shared/protocol.h"
#endif

#ifndef NDEBUG
	#include <iostream>
#endif

Session::Session(uint _id, uint _mode, uint _limit, uint _owner,
	uint _width, uint _height, uint _level, Array<char>& _title)
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
	raster_valid(false),
	#endif
	persist(false)
{
}

#ifdef PERSISTENT_SESSIONS
Session::~Session()
{
	delete raster;
}
#endif

octet Session::getFlags() const
{
	return (persist ? protocol::session::Persist : 0);
}

bool Session::canJoin() const
{
	return ((users.size() + waitingSync.size()) < limit);
}

User* Session::getUser(octet user_id)
{
	std::map<octet,User*>::iterator ui(users.find(user_id));
	return (ui == users.end() ? 0 : ui->second);
}

#ifdef PERSISTENT_SESSIONS
void Session::invalidateRaster()
{
	raster_valid = false;
	if (raster)
		raster->length = 0;
}

bool Session::appendRaster(protocol::Raster *nraster)
{
	assert(nraster != 0);
	
	if (!raster or raster->size != nraster->size)
	{
		delete raster;
		if (nraster->length == nraster->size)
		{
			raster = new protocol::Raster(0, 0, nraster->size, nraster->data);
			nraster->data = 0;
			return true;
		}
		else
			raster = new protocol::Raster(0, 0, nraster->size, new char[nraster->size]);
	}
	
	// nraster offset does not match current raster length
	if (nraster->offset != raster->length)
	{
		#ifndef NDEBUG
		std::cerr << "- Invalid raster offset!" << std::endl;
		#endif
		return false;
	}
	
	if (raster->offset == 0)
		++syncCounter;
	
	memcpy(raster->data+nraster->offset, nraster->data, nraster->length);
	raster->length += nraster->length;
	
	if (raster->length == raster->size)
	{
		--syncCounter;
		raster_valid = true;
	}
	
	return true;
}
#endif

#ifdef LAYER_SUPPORT
LayerData* Session::getLayer(octet layer_id)
{
	session_layer_i sli = layers.find(layer_id);
	if (sli != session->layers.end())
		return &sli->second;
	else
		return 0;
}
#endif
