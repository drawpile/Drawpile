/*******************************************************************************

   Copyright (C) 2007 M.K.A. <miika.ahonen@gmail.com>

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#define CLEAR(flags, bit) flags &= ~bit;

//! Detect model, family,...
int main()
{
	typedef unsigned int uint;
	
	union {
		struct {
			uint Stepping:4;
			uint Model:4;
			uint Family:4;
			uint Type:2;
			uint __reserved2:2;
			uint ExtModel:4;
			uint ExtFamily:8;
			uint __reserved:4;
		};
		uint x;
	} info;
	
	asm (
		"movl $1, %%eax;" // eax=1: processor info
		"cpuid;"
		: "=a"(info.x)
	);
	
	printf("\nStepping: %2x\n", info.Stepping);
	printf("Model:    %2x (%2x)\n", info.Model, info.Model+(info.ExtModel<<4));
	printf("Family:   %2x (%2x)\n", info.Family, info.Family+info.ExtFamily);
	printf("Type:     %2x\n", info.Type);
	
	printf("xModel:    %x\n", info.ExtModel);
	printf("xFamily:   %x\n", info.ExtFamily);
	
	return 0;
}
