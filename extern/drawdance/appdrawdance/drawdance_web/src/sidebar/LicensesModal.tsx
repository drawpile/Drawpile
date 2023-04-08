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
import { Setter } from "../Common";
import { DRAWDANCE_LICENSES } from "./Licenses";

interface LicensesModalProps {}

const ATTRIBUTION_TXT_URL: string =
  "https://raw.githubusercontent.com/drawpile/Drawdance/main" +
  "/appdrawdance/drawdance_web/attribution.txt";

const LICENSE_DESCRIPTIONS: { [key: string]: string | undefined } =
  Object.freeze({
    "BSD-3-Clause": 'the BSD 3-Clause "New" or "Revised" License',
    curl: "the curl License",
    "GPL-2.0-or-later":
      "the GNU General Public License (GPL), version 2 or later; Drawdance is using it under version 3",
    "GPL-3.0": "the GNU General Public License (GPL), version 3",
    "GPL-3.0-or-later":
      "the GNU General Public License (GPL), version 3 or later",
    MIT: "the MIT License",
    Unlicense: "the Unlicense, therefore it is in the Public Domain",
    Zlib: "the zlib License",
  });

function getLicenseTabs(
  activeLicense: string,
  setActiveLicense: Setter<string>
): React.ReactElement[] {
  return DRAWDANCE_LICENSES.map(({ name }) => (
    <li key={name} className="nav-item" onClick={() => setActiveLicense(name)}>
      <button
        className={activeLicense === name ? "nav-link active" : "nav-link"}
      >
        {name}
      </button>
    </li>
  ));
}

function getLicenseBody(activeLicense: string): React.ReactElement {
  const license = DRAWDANCE_LICENSES.find(({ name }) => name === activeLicense);
  if (license) {
    const { name, url, ident, text } = license;
    return (
      <>
        <h3>{name}</h3>
        <p>
          <a href={url} target="_blank" rel="noreferrer">
            {url}
          </a>
        </p>
        <p>
          {name} is licensed under {LICENSE_DESCRIPTIONS[ident]}. The license
          text follows below.
        </p>
        <hr />
        <pre>{text}</pre>
      </>
    );
  } else if (activeLicense === "frontend") {
    return (
      <>
        <h3>Web Frontend Licenses</h3>
        <p>
          The licenses for this web frontend are too numerous to list here. The
          attributions have been compiled separately here:{" "}
          <a href={ATTRIBUTION_TXT_URL} target="_blank" rel="noreferrer">
            {ATTRIBUTION_TXT_URL}
          </a>
          .
        </p>
      </>
    );
  } else {
    return <p>Selected license doesn't exist.</p>;
  }
}

const LicensesModal = React.forwardRef(
  (
    _props: LicensesModalProps,
    ref: React.ForwardedRef<HTMLDivElement>
  ): React.ReactElement => {
    const [activeLicense, setActiveLicense] = React.useState(
      DRAWDANCE_LICENSES[0].name
    );
    return (
      <div ref={ref} className="modal fade" tabIndex={-1}>
        <div className="modal-dialog modal-xl modal-dialog-scrollable">
          <div className="modal-content">
            <div className="modal-header">
              <h5 className="modal-title" id="exampleModalLabel">
                Licenses
              </h5>
              <button
                type="button"
                className="btn-close"
                data-bs-dismiss="modal"
              ></button>
            </div>
            <div className="modal-header">
              <ul className="nav nav-pills">
                {getLicenseTabs(activeLicense, setActiveLicense)}
                <li
                  key="frontend"
                  className="nav-item"
                  onClick={() => setActiveLicense("frontend")}
                >
                  <button
                    className={
                      activeLicense === "frontend"
                        ? "nav-link active"
                        : "nav-link"
                    }
                  >
                    Web Frontend
                  </button>
                </li>
              </ul>
            </div>
            <div className="modal-body">{getLicenseBody(activeLicense)}</div>
            <div className="modal-footer">
              <button
                type="button"
                className="btn btn-secondary"
                data-bs-dismiss="modal"
              >
                Close
              </button>
            </div>
          </div>
        </div>
      </div>
    );
  }
);

export default LicensesModal;
