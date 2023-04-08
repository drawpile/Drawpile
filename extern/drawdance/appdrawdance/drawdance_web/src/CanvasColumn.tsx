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
import React from "react";
import { Setter, Updater } from "./Common";
import ConnectDialog from "./connect/ConnectDialog";
import { Drawdance } from "./Drawdance";
import FoldButton from "./FoldButton";

interface CanvasColumnProps {
  drawdance: Drawdance | undefined;
  connected: boolean;
  wasConnected: boolean;
  disconnectReason: string | undefined;
  username: string;
  setUsername: Setter<string>;
  folded: boolean;
  setFolded: Updater<boolean>;
}

function getConnectDialog(
  props: CanvasColumnProps
): React.ReactElement | undefined {
  const { drawdance, connected } = props;
  if (drawdance === undefined || connected) {
    return undefined;
  } else {
    const connectDialogProps = { ...props, drawdance };
    return <ConnectDialog {...connectDialogProps}></ConnectDialog>;
  }
}

function preventContextMenu(event: React.MouseEvent<HTMLCanvasElement>): void {
  event.preventDefault();
}

function CanvasColumn(props: CanvasColumnProps): React.ReactElement {
  return (
    <div id="canvas-column" className="col w-100 h-100 position-sticky">
      <FoldButton {...props} />
      {getConnectDialog(props)}
      <canvas
        id="canvas"
        className="w-100 h-100"
        tabIndex={-1}
        onContextMenu={preventContextMenu}
      ></canvas>
    </div>
  );
}

export default CanvasColumn;
