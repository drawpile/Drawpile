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
import LicensesModal from "./LicensesModal";
import { Modal } from "bootstrap";

interface AboutTabProps {
  active: boolean;
}

function AboutTab(props: AboutTabProps): React.ReactElement {
  const { active } = props;
  const licensesModalRef = React.useRef<HTMLDivElement>(null);

  function showLicensesModal() {
    const { current } = licensesModalRef;
    if (current) {
      new Modal(current, {}).show();
    }
  }

  return (
    <div className={active ? "overflow-auto" : "d-none"}>
      <h2>About</h2>
      <p className="fw-bold">
        Drawdance {process.env.NPM_PACKAGE_VERSION} - A Drawpile Client
      </p>
      <p>Copyright 2022 askmeaboutloom.</p>
      <p>
        Drawdance is free software: you can redistribute it and/or modify it
        under the terms of the GNU General Public License as published by the
        Free Software Foundation, either version 3 of the License, or (at your
        option) any later version.
      </p>
      <p>
        This program is distributed in the hope that it will be useful, but
        WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
        Public License for more details.
      </p>
      <p>
        You should have received a copy of the GNU General Public License along
        with this program. If not, see{" "}
        <a
          href="https://www.gnu.org/licenses/"
          target="_blank"
          rel="noreferrer"
        >
          https://www.gnu.org/licenses/
        </a>
        .
      </p>
      <p>
        The Drawdance logo is Copyright 2022 askmeaboutloom and licensed under{" "}
        <a
          href="https://creativecommons.org/licenses/by-sa/4.0/"
          target="_blank"
          rel="noreferrer"
        >
          CC BY-SA 4.0
        </a>
        .
      </p>
      <p>
        Drawdance is a separate project from Drawpile. Please don't report
        Drawdance issues to the Drawpile maintainers.
      </p>
      <div className="btn-group w-100">
        <button className="btn btn-link" onClick={showLicensesModal}>
          Licenses
        </button>
        <a
          href="https://github.com/drawpile/Drawdance#name"
          target="_blank"
          rel="noreferrer"
          className="btn btn-link"
        >
          Drawdance on GitHub
        </a>
      </div>
      <LicensesModal ref={licensesModalRef} />
    </div>
  );
}

export default AboutTab;
