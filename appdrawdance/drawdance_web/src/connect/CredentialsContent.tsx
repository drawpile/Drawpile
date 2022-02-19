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
  faCheck,
  faTriangleExclamation,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import React from "react";
import { changeInput, isBlank, Setter } from "../Common";
import { Drawdance, LoginErrorEvent } from "../Drawdance";
import {
  ConnectDialogMode,
  DISCONNECT_REASONS,
  LOGIN_ERRORS,
} from "./ConnectCommon";

interface CredentialsContentProps {
  drawdance: Drawdance;
  disconnectReason?: string;
  mode: ConnectDialogMode;
  setMode: Setter<ConnectDialogMode>;
  loginError?: LoginErrorEvent;
  extAuthUrl?: string;
  username: string;
  setUsername: Setter<string>;
  clientIdConnecting?: number;
  roomTitle?: string;
  children: any;
}

function isSubmitDisabled(
  mode: ConnectDialogMode,
  usernameInput: string,
  passwordInput: string
): boolean {
  if (mode === "INITIAL" || mode === "DISCONNECTED") {
    return isBlank(usernameInput);
  } else if (mode === "AUTH" || mode === "EXT_AUTH") {
    return isBlank(usernameInput) || isBlank(passwordInput);
  } else if (mode === "ROOM_PASSWORD") {
    return isBlank(passwordInput);
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function getUsernameInput(
  mode: ConnectDialogMode,
  usernameInput: string,
  setUsernameInput: Setter<string>
): React.ReactElement | undefined {
  if (mode === "ROOM_PASSWORD") {
    return undefined;
  } else {
    const focus = mode !== "AUTH" && mode !== "EXT_AUTH";
    return (
      <div className="form-group">
        <input
          type="text"
          className="form-control"
          required
          autoFocus={focus}
          name="username"
          placeholder="Username"
          value={usernameInput}
          onChange={changeInput(setUsernameInput)}
          maxLength={22}
        />
      </div>
    );
  }
}

function getPasswordInput(
  mode: ConnectDialogMode,
  passwordInput: string,
  setPasswordInput: Setter<string>
): React.ReactElement | undefined {
  if (mode === "INITIAL" || mode === "DISCONNECTED") {
    return undefined;
  } else {
    const placeholder = mode === "ROOM_PASSWORD" ? "Room Password" : "Password";
    return (
      <div className="form-group">
        <input
          type="password"
          className="form-control"
          required
          autoFocus
          name="password"
          placeholder={placeholder}
          value={passwordInput}
          onChange={changeInput(setPasswordInput)}
        />
      </div>
    );
  }
}

function getTitleText(mode: ConnectDialogMode): string {
  if (mode === "INITIAL") {
    return DRAWDANCE_CONFIG.initialDialogTitle;
  } else if (mode === "DISCONNECTED") {
    return "Disconnected";
  } else if (mode === "AUTH") {
    return "Login";
  } else if (mode === "EXT_AUTH") {
    return "External Login";
  } else if (mode === "ROOM_PASSWORD") {
    return "Room Password";
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function getDisconnectReasonElement(
  disconnectReason: string
): React.ReactElement {
  const disconnectReasonText =
    DISCONNECT_REASONS[disconnectReason] || `Disconnected: ${disconnectReason}`;
  return (
    <p key="disconnectReasonElement" className="text-danger">
      <FontAwesomeIcon icon={faTriangleExclamation} />
      &nbsp;{disconnectReasonText}
    </p>
  );
}

function getLoginErrorElement(loginError: LoginErrorEvent): React.ReactElement {
  const getLoginErrorText = LOGIN_ERRORS[loginError.type];
  const loginErrorText = getLoginErrorText
    ? getLoginErrorText(loginError)
    : `Error: ${loginError.type}`;
  return (
    <p key="loginErrorElement" className="text-danger">
      <FontAwesomeIcon icon={faTriangleExclamation} />
      &nbsp;{loginErrorText}
    </p>
  );
}

function getBodyElement({
  mode,
  disconnectReason,
  loginError,
  username,
  extAuthUrl,
  roomTitle,
}: CredentialsContentProps): React.ReactElement | React.ReactElement[] {
  if (mode === "INITIAL") {
    return <p>{DRAWDANCE_CONFIG.initialDialogPrompt}</p>;
  } else if (mode === "DISCONNECTED") {
    const elements: React.ReactElement[] = [];
    if (disconnectReason && !isBlank(disconnectReason)) {
      elements.push(getDisconnectReasonElement(disconnectReason));
    }
    if (loginError?.type && !isBlank(loginError.type)) {
      elements.push(getLoginErrorElement(loginError));
    }
    if (elements.length === 0) {
      elements.push(
        <p key="noDisconnectReasonElement">You disconnected from the server.</p>
      );
    }
    return elements;
  } else if (mode === "AUTH") {
    return (
      <p>
        The username "{username}" is registered on this server. Either enter its
        password (not the room password!) or pick a different username.
      </p>
    );
  } else if (mode === "EXT_AUTH") {
    return (
      <p>
        The username "{username}" is registered on <code>{extAuthUrl}</code>.
        Either enter its password (not the room password!) or pick a different
        username.
      </p>
    );
  } else if (mode === "ROOM_PASSWORD") {
    return <p>Please enter the password for the room "{roomTitle}".</p>;
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function getConnectButtonText(mode: ConnectDialogMode): string {
  if (mode === "INITIAL") {
    return "Connect";
  } else if (mode === "DISCONNECTED") {
    return "Reconnect";
  } else if (mode === "AUTH" || mode === "EXT_AUTH") {
    return "Log In";
  } else if (mode === "ROOM_PASSWORD") {
    return "Submit";
  } else {
    throw Error(`Unhandled mode '${mode}`);
  }
}

function wantCancelButton(mode: ConnectDialogMode): boolean {
  return mode !== "INITIAL" && mode !== "DISCONNECTED";
}

function submit(
  {
    drawdance,
    clientIdConnecting: clientId,
    setUsername,
    mode,
    setMode,
  }: CredentialsContentProps,
  usernameInput: string,
  passwordInput: string
): void {
  if (mode === "INITIAL" || mode === "DISCONNECTED") {
    setMode("CONNECTING");
    setUsername(usernameInput);
    drawdance.publish("REQUEST_SESSION_JOIN", {
      url: DRAWDANCE_CONFIG.targetUrl,
    });
  } else if (mode === "AUTH" || mode === "EXT_AUTH") {
    setMode("LOGGING_IN");
    setUsername(usernameInput);
    drawdance.publish("LOGIN_PROVIDE_CREDENTIALS", {
      client_id: clientId,
      username: usernameInput,
      password: passwordInput,
    });
  } else if (mode === "ROOM_PASSWORD") {
    setMode("JOINING");
    drawdance.publish("LOGIN_PROVIDE_ROOM_PASSWORD", {
      client_id: clientId,
      room_password: passwordInput,
    });
  }
}

function CredentialsContent(
  props: CredentialsContentProps
): React.ReactElement {
  const { mode, username, children } = props;

  const [usernameInput, setUsernameInput] = React.useState(username);
  const [passwordInput, setPasswordInput] = React.useState("");

  const submitDisabled = isSubmitDisabled(mode, usernameInput, passwordInput);
  const onSubmit = (event: React.FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    if (!submitDisabled) {
      submit(props, usernameInput, passwordInput);
    }
  };

  return (
    <form className="card" onSubmit={onSubmit}>
      <div className="card-header">
        <h2 className="card-title">{getTitleText(mode)}</h2>
      </div>
      <div className="card-body">
        {getBodyElement(props)}
        {getUsernameInput(mode, usernameInput, setUsernameInput)}
        {getPasswordInput(mode, passwordInput, setPasswordInput)}
      </div>
      <div className="card-footer">
        <div className="btn-group">
          <button
            type="submit"
            className="btn btn-primary"
            disabled={submitDisabled}
          >
            <FontAwesomeIcon icon={faCheck} />
            &nbsp;
            {getConnectButtonText(mode)}
          </button>
          {wantCancelButton(mode) ? children : undefined}
        </div>
      </div>
    </form>
  );
}

export default CredentialsContent;
