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

#include "session_data.h"

#include "session.h"

#include "../shared/protocol.flags.h" // protocol::user::~
#include "../shared/protocol.h" // protocol::ToolInfo
#include "../shared/templates.h" // fIsSet() and friends

SessionData::SessionData(Session &s, const octet super_mode)
	: session(&s),
	layer(protocol::null_layer),
	layer_lock(protocol::null_layer),
	syncWait(false),
	cachedToolInfo(0)
{
	setMode(session->mode|super_mode);
	// nothing
}

SessionData::~SessionData()
{
	delete cachedToolInfo;
}

octet SessionData::getMode() const
{
	return (locked ? protocol::user::Locked : 0)
		+ (muted ? protocol::user::Mute : 0)
		+ (deaf ? protocol::user::Deaf : 0);
}

void SessionData::setMode(const octet flags)
{
	locked = fIsSet(flags, static_cast<octet>(protocol::user::Locked));
	muted = fIsSet(flags, static_cast<octet>(protocol::user::Mute));
	deaf = fIsSet(flags, static_cast<octet>(protocol::user::Deaf));
}
