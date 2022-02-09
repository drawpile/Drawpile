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
import { isBlank } from "../Common";
import { ConnectDialogMode } from "./ConnectCommon";

interface StatusContentProps {
  mode: ConnectDialogMode;
  status: string | undefined;
  children: any;
}

function getTitle(mode: ConnectDialogMode): string {
  if (mode === "CONNECTING") {
    return "Connecting";
  } else if (mode === "LOGGING_IN") {
    return "Logging In";
  } else if (mode === "JOINING") {
    return "Joining";
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function StatusContent(props: StatusContentProps): React.ReactElement {
  const { mode, status, children } = props;
  return (
    <div className="card">
      <div className="card-header">
        <h2 className="card-title">{getTitle(mode)}</h2>
      </div>
      <div className="card-body">
        <span className="card-text">
          {isBlank(status) ? "Starting..." : status}
        </span>
      </div>
      <div className="card-footer">{children}</div>
    </div>
  );
}

export default StatusContent;
