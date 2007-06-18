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

//! Test for Intel CPU
int main()
{
	int x;
	
	asm (
		"movl $0, %%eax;" // eax=0: vendor ID
		"cpuid;"
		"cmp $0x756e6547, %%ebx;" // Genu
		"jne Mismatch;"
		"cmp $0x49656e69, %%edx;" // ineI
		"jne Mismatch:"
		"cmp $0x6c65746e, %%ecx;" // ntel
		"jne Mismatch:"
		"movl $1, %%edx;"
		"jmp Return;"
		"Mismatch:"
		"movl $0, %%edx;"
		"Return:"
		: "=d"(x)
		: // no input
		: "%eax"
	);
	
	// x is 1 if found, 0 otherwise
	
	return x;
}
