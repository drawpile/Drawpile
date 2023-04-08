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
import {
  faComment,
  faInfoCircle,
  faSplotch,
  faUsers,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React from "react";
import AboutTab from "./sidebar/AboutTab";
import ChatTab from "./sidebar/ChatTab";
import { ChatEntry, User } from "./Common";
import { Drawdance } from "./Drawdance";
import headerLogoNarrow from "./header-logo-narrow.png";
import headerLogoWide from "./header-logo-wide.png";
import UsersTab from "./sidebar/UsersTab";
import WelcomeTab from "./sidebar/WelcomeTab";

type SidebarTab = "WELCOME" | "CHAT" | "USERS" | "ABOUT";

interface SidebarColumnProps {
  drawdance: Drawdance | undefined;
  connected: boolean;
  username: string;
  currentClientId: number | undefined;
  users: Map<number, User>;
  chatEntries: ChatEntry[];
  folded: boolean;
}

function getNavLinkClass(active: boolean): string {
  return active ? "nav-link ps-0 pe-0 active" : "nav-link ps-0 pe-0";
}

function SidebarColumn(props: SidebarColumnProps): React.ReactElement {
  const { connected, folded } = props;

  const [activeTab, setActiveTab] = React.useState<SidebarTab>("WELCOME");
  const [wantChat, setWantChat] = React.useState(false);

  if (wantChat && connected) {
    setActiveTab("CHAT");
    setWantChat(false);
  } else if (!wantChat && !connected) {
    setWantChat(true);
  }

  const welcomeTabProps = {
    ...props,
    active: activeTab === "WELCOME",
  };
  const chatTabProps = {
    ...props,
    active: activeTab === "CHAT",
  };
  const usersTabProps = {
    ...props,
    active: activeTab === "USERS",
  };
  const aboutTabProps = {
    ...props,
    active: activeTab === "ABOUT",
  };
  return (
    <div
      id="sidebar-column"
      className="h-100"
      style={folded ? { display: "none" } : {}}
    >
      <div className="card h-100 d-flex flex-column border-0">
        <div className="card-header pt-0">
          <a
            href="https://github.com/drawpile/Drawdance#name"
            target="_blank"
            rel="noreferrer"
          >
            <img
              className="img-fluid header-logo d-block d-lg-none"
              src={headerLogoNarrow}
              alt="Drawdance - A Drawpile Client"
              title="Show Drawdance on GitHub"
            />
            <img
              className="img-fluid header-logo d-none d-lg-block"
              src={headerLogoWide}
              alt="Drawdance - A Drawpile Client"
              title="Show Drawdance on GitHub"
            />
          </a>
          <ul className="nav nav-tabs nav-fill card-header-tabs">
            <li className="nav-item">
              <button
                className={getNavLinkClass(activeTab === "WELCOME")}
                title="Welcome"
                onClick={() => setActiveTab("WELCOME")}
              >
                <FontAwesomeIcon icon={faSplotch} />
                <span className="d-none d-xxl-inline">&nbsp;Welcome</span>
              </button>
            </li>
            <li className="nav-item">
              <button
                className={getNavLinkClass(activeTab === "CHAT")}
                title="Chat"
                onClick={() => setActiveTab("CHAT")}
              >
                <FontAwesomeIcon icon={faComment} />
                <span className="d-none d-xxl-inline">&nbsp;Chat</span>
              </button>
            </li>
            <li className="nav-item">
              <button
                className={getNavLinkClass(activeTab === "USERS")}
                title="Users"
                onClick={() => setActiveTab("USERS")}
              >
                <FontAwesomeIcon icon={faUsers} />
                <span className="d-none d-xxl-inline">&nbsp;Users</span>
              </button>
            </li>
            <li className="nav-item">
              <button
                className={getNavLinkClass(activeTab === "ABOUT")}
                title="About"
                onClick={() => setActiveTab("ABOUT")}
              >
                <FontAwesomeIcon icon={faInfoCircle} />
              </button>
            </li>
          </ul>
        </div>
        <div className="card-body d-flex flex-column overflow-hidden">
          <WelcomeTab {...welcomeTabProps} />
          <ChatTab {...chatTabProps} />
          <UsersTab {...usersTabProps} />
          <AboutTab {...aboutTabProps} />
        </div>
      </div>
    </div>
  );
}

export default SidebarColumn;
