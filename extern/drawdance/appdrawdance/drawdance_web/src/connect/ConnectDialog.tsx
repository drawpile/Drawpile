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
import {
  ClientCreateEvent,
  ClientEventEvent,
  Drawdance,
  LoginErrorEvent,
  LoginRequireCredentialsEvent,
  LoginRequireRoomEvent,
  LoginRequireRoomPasswordEvent,
  Room,
} from "../Drawdance";
import { isBlank, Setter } from "../Common";
import CredentialsContent from "./CredentialsContent";
import { CLIENT_EVENT_DESCRIPTIONS, ConnectDialogMode } from "./ConnectCommon";
import StatusContent from "./StatusContent";
import { DrawdanceClientEvent } from "../DrawdanceEvents";
import RoomsContent from "./RoomsContent";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import { faTimes } from "@fortawesome/free-solid-svg-icons";

interface ConnectDialogProps {
  drawdance: Drawdance;
  wasConnected: boolean;
  disconnectReason: string | undefined;
  username: string;
  setUsername: Setter<string>;
}

function getContent(
  mode: ConnectDialogMode
): typeof CredentialsContent | typeof StatusContent | typeof RoomsContent {
  if (
    mode === "INITIAL" ||
    mode === "DISCONNECTED" ||
    mode === "AUTH" ||
    mode === "EXT_AUTH" ||
    mode === "ROOM_PASSWORD"
  ) {
    return CredentialsContent;
  } else if (
    mode === "CONNECTING" ||
    mode === "LOGGING_IN" ||
    mode === "JOINING"
  ) {
    return StatusContent;
  } else if (mode === "ROOM") {
    return RoomsContent;
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function buildDialogStyle(
  drawdance: Drawdance,
  wasConnected: boolean
): React.CSSProperties {
  return {
    // To avoid flickering behind the dialog, we don't actually use a background
    // with alpha until the WASM application is running and we've established a
    // connection. At that point the canvas content should be stable. If we get
    // disconnected, the previous session will be visible behind the dialog.
    background: drawdance.initialized && wasConnected ? "#0000007f" : "#3f3f3f",
  };
}

function ConnectDialog(props: ConnectDialogProps): React.ReactElement {
  const { drawdance, wasConnected, username } = props;

  const [mode, setMode] = React.useState<ConnectDialogMode>(
    wasConnected ? "DISCONNECTED" : "INITIAL"
  );
  const [status, setStatus] = React.useState<string>();
  const [loginError, setLoginError] = React.useState<LoginErrorEvent>();
  const [clientIdAttempted, setClientIdAttempted] = React.useState<number>();
  const [clientIdConnecting, setClientIdConnecting] = React.useState<number>();
  const [cancelRequested, setCancelRequested] = React.useState(false);
  const [extAuthUrl, setExtAuthUrl] = React.useState<string>();
  const [sessionTitle, setSessionTitle] = React.useState<string>();
  const [rooms, setRooms] = React.useState<Room[]>([]);
  const [roomTitle, setRoomTitle] = React.useState<string>();

  React.useEffect(() => {
    return Drawdance.subscribe({
      MODULE_LOAD_STATUS: (text: string): void => {
        setStatus(
          `Loading the application - ${isBlank(text) ? "Booting up..." : text}`
        );
      },
      CLIENT_CREATE: ({ client_id: clientId }: ClientCreateEvent): void => {
        setClientIdAttempted(clientId);
        setClientIdConnecting(clientId);
        setCancelRequested(false);
        setLoginError(undefined);
        setStatus("Connecting...");
      },
    });
  }, []);

  React.useEffect(() => {
    return Drawdance.subscribe({
      LOGIN_ERROR: (event: LoginErrorEvent): void => {
        const { client_id: clientId } = event;
        if (clientId === clientIdAttempted) {
          setMode("DISCONNECTED");
          setLoginError(event);
        }
      },
    });
  }, [clientIdAttempted]);

  React.useEffect(() => {
    return Drawdance.subscribe({
      LOGIN_REQUIRE_ROOM: ({
        client_id: clientId,
        title,
        rooms,
      }: LoginRequireRoomEvent): void => {
        if (clientId === clientIdConnecting) {
          setMode("ROOM");
          setSessionTitle(title);
          setRooms(rooms);
        }
      },
      LOGIN_REQUIRE_ROOM_PASSWORD: ({
        client_id: clientId,
        title,
      }: LoginRequireRoomPasswordEvent): void => {
        if (clientId === clientIdConnecting) {
          setMode("ROOM_PASSWORD");
          setRoomTitle(title);
        }
      },
    });
  }, [clientIdConnecting]);

  React.useEffect(() => {
    return Drawdance.subscribe({
      CLIENT_EVENT: ({
        client_id: clientId,
        type,
        message,
      }: ClientEventEvent): void => {
        if (clientId === clientIdConnecting) {
          if (
            type === DrawdanceClientEvent.CONNECTION_CLOSED ||
            type === DrawdanceClientEvent.FREE
          ) {
            setMode("DISCONNECTED");
            setClientIdConnecting(undefined);
          }
          const description = CLIENT_EVENT_DESCRIPTIONS[type];
          if (description) {
            setStatus(description(message));
          } else {
            const fallback = DrawdanceClientEvent[type] || `${type}`;
            if (message === undefined) {
              setStatus(fallback);
            } else {
              setStatus(`${fallback}: ${message}`);
            }
          }
        }
      },
    });
  }, [clientIdConnecting, cancelRequested]);

  React.useEffect(() => {
    return Drawdance.subscribe({
      LOGIN_REQUIRE_CREDENTIALS: ({
        client_id: clientId,
        password_required: passwordRequired,
        ext_auth_url: extAuthUrl,
      }: LoginRequireCredentialsEvent): void => {
        if (passwordRequired) {
          if (extAuthUrl) {
            setExtAuthUrl(extAuthUrl);
            setMode("EXT_AUTH");
          } else {
            setMode("AUTH");
          }
        } else {
          drawdance.publish("LOGIN_PROVIDE_CREDENTIALS", {
            client_id: clientId,
            username: username,
          });
        }
      },
    });
  }, [drawdance, username]);

  const cancel = (): void => {
    if (clientIdConnecting !== undefined) {
      drawdance.publish("REQUEST_SESSION_CANCEL", {
        client_id: clientIdConnecting,
      });
      setCancelRequested(true);
    }
  };
  const cancelButton = (
    <button
      type="submit"
      className="btn btn-danger"
      onClick={cancel}
      disabled={clientIdConnecting === undefined}
    >
      <FontAwesomeIcon icon={faTimes} />
      &nbsp;Cancel
    </button>
  );

  const Content = getContent(mode);
  const contentProps = {
    ...props,
    mode,
    setMode,
    status,
    setStatus,
    loginError,
    setLoginError,
    clientIdConnecting,
    setClientIdConnecting,
    extAuthUrl,
    setExtAuthUrl,
    sessionTitle,
    setSessionTitle,
    rooms,
    setRooms,
    roomTitle,
    setRoomTitle,
  };

  return (
    <div
      id="canvas-dialog"
      style={buildDialogStyle(drawdance, wasConnected)}
      className="row justify-content-center w-100 h-100 position-absolute"
    >
      <div className="col-lg-6 align-self-center">
        <Content {...contentProps}>{cancelButton}</Content>
      </div>
    </div>
  );
}

export default ConnectDialog;
