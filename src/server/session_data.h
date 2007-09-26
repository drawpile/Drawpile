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

#ifndef User_SessionData_INCLUDED
#define User_SessionData_INCLUDED

#include "types.h"
#include "shared/protocol.fwd.h"

#include "fwd.decl.h"

//! User session data
class SessionData
{
public:
	//! Default constructor
	/**
	 * @param[in] s Session to associate with the user session data
	 * @param[in] super_mode Any mode flags the server superimposes on all sessions.
	 */
	SessionData(Session &s, octet super_mode) NOTHROW;
	
	//! Destructor
	~SessionData() NOTHROW;
	
	//! Session reference
	Session *session;
	
	octet
		//! Active layer
		layer,
		//! Layer to which the user is locked to
		layer_lock;
	
	bool
		//! Locked
		locked,
		//! Muted
		muted,
		//! Deaf
		deaf;
	
	//! Get user mode
	octet getMode() const NOTHROW;
	
	//! Set user mode
	/**
	 * @param[in] flags Flags to set
	 */
	void setMode(octet flags) NOTHROW;
	
	//! User has sent ACK/Sync
	bool syncWait;
	
	/* cached messages */
	
	//! Cached tool info message
	protocol::ToolInfo *cachedToolInfo;
};

#endif // User_SessionData_INCLUDED
