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

//! Test for SSE support
int main()
{
	int x;
	
	asm (
		"movl $1, %%eax;" // eax=1: processor info and feature bits
		"cpuid;"
		"test $0x00a0_0000, %%edx;" // 25th bit
		"jz NoSupport;"
		"movl $1, %%edx;"
		"jmp Return;"
		"NoSupport:"
		"movl $0, %%edx;"
		"Return:"
		: "=d"(x)
		: // no input
		: "%eax"
	);
	
	// x is 1 if found, 0 otherwise
	
	return x;
}
