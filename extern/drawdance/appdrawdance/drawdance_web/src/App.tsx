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
import CanvasColumn from "./CanvasColumn";
import { ChatEntry, User } from "./Common";
import {
  ChatMessageReceiveEvent,
  Drawdance,
  FatalError,
  SessionCreateEvent,
  SessionDetachEvent,
  SessionRemoveEvent,
  UserJoinEvent,
  UserLeaveEvent,
} from "./Drawdance";
import FatalErrorScreen from "./FatalErrorScreen";
import SidebarColumn from "./SidebarColumn";

function App(): React.ReactElement {
  const [drawdance, setDrawdance] = React.useState<Drawdance>();
  const [fatalError, setFatalError] = React.useState<FatalError>();
  const [connected, setConnected] = React.useState(false);
  const [wasConnected, setWasConnected] = React.useState(false);
  const [disconnectReason, setDisconnectReason] = React.useState<string>();
  const [username, setUsername] = React.useState("");
  const [currentClientId, setCurrentClientId] = React.useState<number>();
  const [users, setUsers] = React.useState(new Map<number, User>());
  const [chatEntries, setChatEntries] = React.useState<ChatEntry[]>([]);
  const [folded, setFolded] = React.useState(false);

  React.useEffect(() => {
    const onError = (e: ErrorEvent): void => {
      setFatalError({ message: `Internal error: ${e.message}` });
    };
    window.addEventListener("error", onError);
    return () => window.removeEventListener("error", onError);
  }, []);

  React.useEffect(() => {
    new Drawdance().tryConnect(setDrawdance, setFatalError);
  }, []);

  React.useEffect(() => {
    return Drawdance.subscribe({
      SESSION_CREATE: ({ client_id: clientId }: SessionCreateEvent): void => {
        const lastClientId = currentClientId;
        setConnected(true);
        setWasConnected(true);
        setCurrentClientId(clientId);
        setDisconnectReason(undefined);
        setUsers(new Map());
        if (lastClientId !== undefined && lastClientId !== clientId) {
          drawdance!.publish("REQUEST_SESSION_CLOSE", {
            client_id: lastClientId,
          });
        }
      },
    });
  }, [drawdance, currentClientId]);

  React.useEffect(() => {
    return Drawdance.subscribe({
      SESSION_DETACH: ({
        client_id: clientId,
        reason,
      }: SessionDetachEvent): void => {
        if (clientId === currentClientId) {
          setConnected(false);
          setDisconnectReason(reason ? reason : undefined);
        }
      },
      SESSION_REMOVE: ({ client_id: clientId }: SessionRemoveEvent): void => {
        if (clientId === currentClientId) {
          setConnected(false);
          setCurrentClientId(undefined);
        }
      },
      USER_JOIN: ({
        client_id: clientId,
        context_id: contextId,
        name,
      }: UserJoinEvent) => {
        if (clientId === currentClientId) {
          setUsers((prev) => {
            const next = new Map<number, User>(prev);
            next.set(contextId, { contextId, name, connected: true });
            return next;
          });
          setChatEntries((prev) => [...prev, { type: "JOIN", contextId }]);
        }
      },
      USER_LEAVE: ({
        client_id: clientId,
        context_id: contextId,
      }: UserLeaveEvent) => {
        if (clientId === currentClientId) {
          setUsers((prev) => {
            const next = new Map<number, User>(prev);
            const prevUser = next.get(contextId);
            next.set(contextId, {
              contextId,
              name: prevUser?.name || `#${contextId}`,
              connected: false,
            });
            return next;
          });
          setChatEntries((prev) => [...prev, { type: "LEAVE", contextId }]);
        }
      },
      CHAT_MESSAGE_RECEIVE: ({
        client_id: clientId,
        context_id: contextId,
        action,
        text,
      }: ChatMessageReceiveEvent) => {
        if (clientId === currentClientId) {
          setChatEntries((prev) => [
            ...prev,
            { type: "CHAT", contextId, action, text },
          ]);
        }
      },
    });
  }, [currentClientId]);

  if (fatalError) {
    return <FatalErrorScreen {...{ fatalError }}></FatalErrorScreen>;
  } else {
    const canvasColumnProps = {
      drawdance,
      connected,
      wasConnected,
      disconnectReason,
      username,
      setUsername,
      folded,
      setFolded,
    };
    const sidebarColumnProps = {
      drawdance,
      connected,
      username,
      currentClientId,
      users,
      chatEntries,
      folded,
    };
    return (
      <div className="container-flex w-100 h-100">
        <div id="main-row" className="row w-100 h-100">
          <CanvasColumn {...canvasColumnProps}></CanvasColumn>
          <SidebarColumn {...sidebarColumnProps}></SidebarColumn>
        </div>
      </div>
    );
  }
}

export default App;
