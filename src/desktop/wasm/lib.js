// SPDX-License-Identifier: GPL-3.0-or-later
// Normally Emscripten hardcodes this value, but Safari has a broken
// WebAssembly.Memory implementation that dies if you give it a too large
// maximum size "hint", so we actually have to deal with a dynamic heap maximum.
LibraryManager.library.$getHeapMax = function () {
  return Module.DRAWPILE_HEAP_MAX || 4294901760;
};
