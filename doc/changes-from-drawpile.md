# Drawpile on Drawdance

This document describes the changes made to Drawpile since version 2.2.0-beta3.

You can find periodic development releases for Windows in the [Releases section](https://github.com/askmeaboutlo0m/Drawpile/releases).

On Linux, you should be able to just build the branch `dancepile-2.2` from source. See [the build instructions in README.md](../README.md#building-on-linux).


## Changes since 2.2.0-beta3

This is probably a non-exhaustive list, but it should hit the most important points I hope.

* Switched out Rustpile engine with [Drawdance](https://github.com/drawpile/Drawdance/tree/dev-2.2). This has several advantages:

    * Drawdance also runs in the browser and allows for multithreading there. Developing a single engine for all platforms instead of having one on desktop and one in the browser is of course way less work.

    * It uses a frame-based approach similar to games and how Krita does it nowadays. The canvas gets updated every 60th of a second (or less, if the user desires.) This leads to less unnecessary repainting at a rate faster than is shown on screen anyway.

    * It eschews tracking changed canvas areas entirely, instead using dirty checking between each frame. This removes an entire class of errors resulting in visual update bugs like [this](https://github.com/drawpile/Drawpile/pull/1046) or [this](https://github.com/drawpile/Drawpile/issues/940).

    * It multithreads more stuff, like rendering of tiles or loading of ORA files, leading to a significant speedup.

    * Drawdance implements the regionmove command, so it doesn't have to punt to putimage calls (but this is currently not enabled for protocol compatibility with Rustpile.) Since it's our own implementation, we could implement [other interpolation modes](https://github.com/drawpile/Drawpile/issues/803) into it.

    * It has better onion skin support, allowing for customizing the color and opacity of each skin as well as properly re-rendering layers that are used on multiple frames.

    * It wholly separates the local view from the session state, allowing for local canvas backgrounds, hidden layers and any other local-only modification without modifying the underlying state.

    * It has several optimizations that speed it up a good amount. It combines multiple draw dabs calls into a single "multidab" operation and executes it all in one go. Since these are so common, this speeds up painting and undo/redo by a bunch. It avoids allocations in hot code, instead re-using persistent buffers if possible. There's also efforts underway to make blending use SIMD, which seems to add a good bit of speed as well.

    * It supports recording of debug dumps with full local and remote history tracking, as well as the playback thereof. This means debugging issues with undo and conflict handling is much easier, since it can just be recorded in the wild and then played back afterwards, looking at what's going on with the history. This helped solve those double undo bugs that Rustpile had.

    * It's less cool because it's written in C instead of Rust, but it's much easier to integrate with C++ and actually works in the browser because Emscripten support actually works (Rust also has an emscripten target, but it doesn't work properly, doesn't multithread and is unmaintained.) It also allows for link-time optimization, which adds speed, and the use of address sanitzer, which helps find memory leaks and similar (Rust purportedly supports them too, but they don't actually work when liking with C or C++ because of linker bugs.) Once Rust matures enough that those sharp edges get smoothed out, I could see it replacing pieces of Drawdance again, which isn't too hard because Rust can talk to C just fine.

* Reworked the CMake scripts for CMake 3, allowing e.g. link-time optimization to be enabled now. There's also address sanitizer and clang-tidy support now, which are great for finding bugs.

* Added Qt 6 support. I don't see it replacing Qt 5 anytime soon due to Qt 6 not supporting Windows 7 and 8 anymore, but having the upgrade path clear is good.

* Added a proper, native Windows build using vcpkg and MSVC, with bonus support for building these through Github Actions. It's a lot less fragile than the cross-compile setup, lets us update our libraries and allows for cool stuff like producing statically-linked executables (which doesn't work for Qt 5.12 with KIS_TABLET patches, but Qt 5.15 and Qt 6 do.)

* Added several helpful gadgets in Tools > Developer Tools: tablet event logging, performance profile recording, artificial lag, artificial disconnect, debug dump recording and debug dump playback. Debug dumps record the local and remote commands entirely, allowing for the history debugging mentioned above.

* Switched pretty much all spinners in the application to use Krita's tablet-friendly spinner sliders.

* Changed the UI style to be Krita Dark on Windows and Linux (but not OSX because I currently can't test it.) This looks a lot better than the strange Vista style on Windows.

* Added an onion skins docker, similar to the one Krita has, where you can set the tinting of the frames above and below, as well as the opacity of every skin.

* Made the default layout work better for small screens and added a View > Layouts dialog where you can save and restore layouts. It comes with several layouts that look similar to other programs, which should be nice for people coming from other software.

* Fixed the docks jiggering all around when hiding and unhiding them.

* Made most of the docks scale better, allowing them to get smaller than they did before.

* Reworked canvas shortcuts entirely. Instead of being fixed to modifier keys, spacebar and middle click, they are now freely assignable.

* Feathering support for the flood fill tool.

* Pen tilt and rotation support.

* Force NSFM option for the server, for servers that mandate all sessions to be marked NSFM.

* Many other smaller improvements and bugfixes, but I think explaining them all would just lead to regurgitating the [git](https://github.com/askmeaboutlo0m/Drawpile/commits/dancepile-2.2) [logs](https://github.com/Drawpile/Drawdance/commits/dev-2.2).
