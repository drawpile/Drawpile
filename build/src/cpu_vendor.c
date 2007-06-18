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

//! Generic CPU testing
int main()
{
	int _ebx, _edx, _ecx;
	
	asm (
		"movl $0, %%eax;" // eax=0: vendor ID
		"cpuid;"
		: "=b"(_ebx), "=d"(_edx), "=c"(_ecx)
		: // no input
		: "%eax"
	);
	
	char string[13] = {0};
	memcpy(string, &_ebx, 4);
	memcpy(string+4, &_edx, 4);
	memcpy(string+8, &_ecx, 4);
	
	printf("Vendor: %12s\n", string);
	printf("Hex: 0x%08x 0x%08x 0x%08x\n", _ebx, _edx, _ecx);
	
	return 0;
}
