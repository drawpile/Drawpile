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

#pragma once

#ifndef LayerData_INCLUDED
#define LayerData_INCLUDED

#include "types.h"
#include "../shared/protocol.tools.h" // tool_mode::*
#include "../shared/protocol.defaults.h" // null_layer

//! Layer information
class LayerData
{
public:
	//! Constructor
	LayerData(uint _id=protocol::null_layer, uint _mode=protocol::tool_mode::None,
		uint _opacity=255, bool _locked=false) NOTHROW;
	
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

LayerData::LayerData(uint _id, uint _mode, uint _opacity, bool _locked)
	: id(_id),
	mode(_mode),
	opacity(_opacity),
	locked(_locked)
{
	// nothing
}

#endif // LayerData_INCLUDED
