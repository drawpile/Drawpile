/*******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef ServerStatistics_INCLUDED
#define ServerStatistics_INCLUDED

#include "types.h"

//! Server statistics
struct Statistics
{
	ulong 
		//! Stroke info messages
		strokeInfo,
		//! Stroke end messages
		strokeEnd,
		//! Tool info messages
		toolInfo;
	
	ulong
		//! Cumulative total of connections to server
		connections,
		//! Cumulative total of disconnections from server
		disconnects;
	
	ulong
		//! Number of messages received
		inMsgCount,
		//! Number of messages sent
		outMsgCount,
		//! Number of bytes sent
		dataSent,
		//! Number of bytes received
		dataRecv;
	
	ulong
		//! Number of bytes saved by bundling
		bundledAway,
		//! Number of bytes saved by compression
		compressedAway,
		//! Data processed for compression with bad results
		compressionWasted;
};

#endif // ServerStatistics_INCLUDED
