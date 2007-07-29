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

#ifndef LayerData_INCLUDED
#define LayerData_INCLUDED

#include "types.h"

//! Layer information
struct LayerData
{
	//! Constructor
	LayerData() __attribute__ ((nothrow));
	
	//! Constructor
	LayerData(const uint _id, const uint _mode, const uint _opacity=255, const bool _locked=false) __attribute__ ((nothrow));
	
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

#endif // LayerData_INCLUDED
