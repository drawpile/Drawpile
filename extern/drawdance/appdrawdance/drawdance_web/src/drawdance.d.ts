/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

declare const DRAWDANCE_CONFIG: {
  targetUrl: string;
  initialDialogTitle: string;
  initialDialogPrompt: string;
  welcomeMessage: string;
};

interface EmscriptenCreateModuleParams {
  canvas?: HTMLCanvasElement;
  setStatus?: (string) => void;
  locateFile?: (string) => string;
}

interface DrawdanceInstance {
  ALLOC_NORMAL: number;
  intArrayFromString: (stringy: string) => Array;
  allocate: (slab: Array, allocator: number) => number;
  _DP_send_from_browser: (handle: number, payload: number) => void;
}

declare function createModule(
  params?: EmscriptenCreateModuleParams
): Promise<DrawdanceInstance>;

interface DrawdanceCustomEvent {
  detail: {
    handle: number;
    payload: string;
  };
}

declare function addEventListener(
  type: "dpevents",
  handler: (event: DrawdanceCustomEvent & CustomEvent) => void
): void;

declare function _DP_send_from_browser(handle: number, payload: string): void;
