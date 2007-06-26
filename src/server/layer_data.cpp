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

#include "layer_data.h"

#include "../shared/protocol.tools.h"
#include "../shared/protocol.defaults.h"

LayerData::LayerData() throw()
	: id(protocol::null_layer),
	mode(protocol::tool_mode::None),
	opacity(0),
	locked(false)
{
	// nothing
}

LayerData::LayerData(const uint _id, const uint _mode, const uint _opacity, const bool _locked) throw()
	: id(_id),
	mode(_mode),
	opacity(_opacity),
	locked(_locked)
{
	// nothing
}
