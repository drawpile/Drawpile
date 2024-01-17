/*
 * We need zlib in Emscripten, but emcc refuses to install ports outside of
 * compiling a program, so we need this dummy file to make it happy.
 */
int main(void)
{
    return 0;
}
