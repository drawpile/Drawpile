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
import { FatalError } from "./Drawdance";

interface FatalErrorScreenProps {
  fatalError: FatalError;
}

function FatalErrorScreen({
  fatalError,
}: FatalErrorScreenProps): React.ReactElement {
  const recourse = fatalError.recourse || (
    <>
      You can try to <a href=".">reload the page</a>.
    </>
  );
  return (
    <div className="container h-100">
      <div className="row justify-content-center h-100">
        <div className="col-lg-6 col-md-8 col-sm-10 align-self-center">
          <div className="card text-center">
            <div className="card-header bg-danger text-white">
              <h2 className="card-title">Error</h2>
            </div>
            <div className="card-body">
              <span className="card-text">
                Drawdance has stumbled into an error and can't continue like
                this. It's probably not your fault.
              </span>
            </div>
            <div className="card-body">
              <span className="card-text text-danger">
                {fatalError.message}
              </span>
            </div>
            <div className="card-footer">
              <span className="card-text">{recourse}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

export default FatalErrorScreen;
