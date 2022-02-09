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
  faPaperPlane,
  faSignInAlt,
  faSignOutAlt,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React from "react";
import {
  changeInput,
  ChatEntry,
  ChatMessage,
  isBlank,
  stopPropagation,
  User,
} from "../Common";
import { Drawdance } from "../Drawdance";

interface ChatTabProps {
  drawdance: Drawdance | undefined;
  connected: boolean;
  username: string;
  currentClientId: number | undefined;
  users: Map<number, User>;
  chatEntries: ChatEntry[];
  active: boolean;
}

function getChatMessageClass({ username }: ChatTabProps, name: string): string {
  if (username === name) {
    // Mark own messages with a different background color to make them easier
    // to spot when scrolling. Can be handy when returning after some time away
    // and wanting to figure out what happened in the meantime, for example.
    return "list-group-item list-group-item-secondary";
  } else {
    return "list-group-item";
  }
}

function getChatMessageContent(
  name: string,
  { action, text }: ChatMessage
): React.ReactElement {
  if (action) {
    return (
      <em>
        <strong>{name}</strong> {text}
      </em>
    );
  } else {
    return (
      <>
        <strong>{name}:</strong> {text}
      </>
    );
  }
}

function getChatListItems(
  props: ChatTabProps
): React.ReactElement | React.ReactElement[] {
  const { connected, users, chatEntries } = props;
  if (!connected) {
    return (
      <li className="list-group-item list-group-item-secondary">
        Not connected.
      </li>
    );
  } else if (users.size === 0) {
    return (
      <li className="list-group-item list-group-item-secondary">
        No messages.
      </li>
    );
  } else {
    return chatEntries.map(
      (chatEntry: ChatEntry, index: number): React.ReactElement => {
        const { type, contextId } = chatEntry;
        const name = users.get(contextId)?.name || `#${contextId}`;
        if (type === "CHAT") {
          return (
            <li key={index} className={getChatMessageClass(props, name)}>
              {getChatMessageContent(name, chatEntry)}
            </li>
          );
        } else if (type === "JOIN") {
          return (
            <li key={index} className="list-group-item list-group-item-primary">
              <FontAwesomeIcon icon={faSignInAlt} />
              &nbsp;<strong>{name}</strong> joined.
            </li>
          );
        } else if (type === "LEAVE") {
          return (
            <li key={index} className="list-group-item list-group-item-danger">
              <FontAwesomeIcon icon={faSignOutAlt} />
              &nbsp;<strong>{name}</strong> left.
            </li>
          );
        } else {
          throw new Error(`Unknown chat entry type '${type}`);
        }
      }
    );
  }
}

function ChatTab(props: ChatTabProps): React.ReactElement {
  const { drawdance, currentClientId, connected, chatEntries, active } = props;

  const [entryCount, setEntryCount] = React.useState(-1);
  const [chatInput, setChatInput] = React.useState("");
  const mounted = React.useRef(false);
  const ul = React.useRef<HTMLUListElement>(null);

  React.useEffect(() => {
    mounted.current = true;
    return () => {
      mounted.current = false;
    };
  });

  React.useEffect(() => {
    if (mounted.current && ul.current && entryCount !== chatEntries.length) {
      setEntryCount(chatEntries.length);
      const lastEntry = ul.current.lastChild as HTMLLIElement;
      lastEntry.scrollIntoView({ behavior: "smooth" });
    }
  }, [chatEntries, entryCount, mounted, ul]);

  const sendDisabled = !connected || isBlank(chatInput);

  function submit(event: React.FormEvent): void {
    event.preventDefault();
    if (!sendDisabled) {
      drawdance!.publish("REQUEST_CHAT_MESSAGE_SEND", {
        client_id: currentClientId,
        text: chatInput.trim(),
      });
      setChatInput("");
    }
  }

  return (
    <div className={active ? "d-flex flex-column h-100" : "d-none"}>
      <ul className="list-group flex-grow-1 overflow-y-scroll" ref={ul}>
        {getChatListItems(props)}
      </ul>
      <form onSubmit={submit}>
        <div className="input-group">
          <input
            type="text"
            className="form-control"
            placeholder="Chat Message"
            disabled={!connected}
            autoFocus={connected}
            value={chatInput}
            onChange={changeInput(setChatInput)}
            onKeyPress={stopPropagation}
            onKeyDown={stopPropagation}
            onKeyUp={stopPropagation}
          ></input>
          <button
            type="submit"
            className="btn btn-primary"
            title="Send"
            disabled={sendDisabled}
          >
            <FontAwesomeIcon icon={faPaperPlane} />
            <span className="d-none d-xxl-inline">&nbsp;Send</span>
          </button>
        </div>
      </form>
    </div>
  );
}

export default ChatTab;
