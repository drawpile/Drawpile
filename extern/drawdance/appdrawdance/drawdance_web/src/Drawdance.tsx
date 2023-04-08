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
import { DrawdanceEvent, DrawdanceEventType } from "./DrawdanceEvents";
import { Setter } from "./Common";

const SHARED_ARRAY_BUFFER_MDN_URL =
  "https://developer.mozilla.org/en-US/" +
  "docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer";

const SHARED_ARRAY_BUFFER_MDN_COMPATIBILITY_URL =
  SHARED_ARRAY_BUFFER_MDN_URL + "#browser_compatibility";

const SHARED_ARRAY_BUFFER_MDN_SECURITY_URL =
  SHARED_ARRAY_BUFFER_MDN_URL + "#security_requirements";

export interface LoginRequireCredentialsEvent {
  client_id: number;
  password_required: boolean;
  allow_guests: boolean;
  ext_auth_url?: string;
}

export interface ClientCreateEvent {
  client_id: number;
  url: string;
}

export interface ClientEventEvent {
  client_id: number;
  type: number;
  message?: string;
}

export interface SessionCreateEvent {
  client_id: number;
  server_flags: { [key: string]: string };
  user_flags: { [key: string]: string };
  room_flags: { [key: string]: string };
}

export interface SessionShowEvent {
  client_id: number;
}

export interface SessionDetachEvent {
  client_id: number;
  reason?: string;
}

export interface SessionRemoveEvent {
  client_id: number;
}

export interface LoginErrorEvent {
  client_id: number;
  type: string;
  code?: string;
  expected_type?: string;
  actual_type?: string;
  command_type?: string;
  expected_version?: string;
  actual_version?: string;
  url?: string;
  message?: string;
  status?: string;
  state?: string;
  error?: string;
}

export interface LoginRequireRoomPasswordEvent {
  client_id: number;
  id?: string;
  alias?: string;
  title: string;
  founder: string;
}

export interface Room {
  id: string;
  alias?: string;
  title: string;
  founder: string;
  protocol: string;
  users: number;
  password: boolean;
  closed: boolean;
  nsfm: boolean;
}

export interface LoginRequireRoomEvent {
  client_id: number;
  title?: string;
  rooms: Room[];
}

export interface UserJoinEvent {
  client_id: number;
  context_id: number;
  name: string;
}

export interface UserLeaveEvent {
  client_id: number;
  context_id: number;
}

export interface ChatMessageReceiveEvent {
  client_id: number;
  context_id: number;
  shout: boolean;
  action: boolean;
  pin: boolean;
  text: string;
}

interface DrawdanceEventListeners {
  MODULE_LOAD_STATUS?: (text: string) => void;
  DP_APP_INIT?: () => void;
  DP_APP_QUIT?: () => void;
  CLIENT_CREATE?: (event: ClientCreateEvent) => void;
  CLIENT_EVENT?: (event: ClientEventEvent) => void;
  SESSION_CREATE?: (event: SessionCreateEvent) => void;
  SESSION_SHOW?: (event: SessionShowEvent) => void;
  SESSION_DETACH?: (event: SessionDetachEvent) => void;
  SESSION_REMOVE?: (event: SessionRemoveEvent) => void;
  LOGIN_ERROR?: (event: LoginErrorEvent) => void;
  LOGIN_REQUIRE_CREDENTIALS?: (event: LoginRequireCredentialsEvent) => void;
  LOGIN_REQUIRE_ROOM_PASSWORD?: (event: LoginRequireRoomPasswordEvent) => void;
  LOGIN_REQUIRE_ROOM?: (event: LoginRequireRoomEvent) => void;
  USER_JOIN?: (event: UserJoinEvent) => void;
  USER_LEAVE?: (event: UserLeaveEvent) => void;
  CHAT_MESSAGE_RECEIVE?: (event: ChatMessageReceiveEvent) => void;
}

export interface FatalError {
  message: string | React.ReactElement;
  recourse?: React.ReactElement;
}

function checkFeatures(setFatalError: Setter<FatalError>): boolean {
  const crossOriginIsolated = !!window.crossOriginIsolated;
  const haveSharedArrayBuffer = !!window.SharedArrayBuffer;

  if (!haveSharedArrayBuffer) {
    if (crossOriginIsolated) {
      setFatalError({
        message: (
          <>
            SharedArrayBuffer is not available, your browser may not support it.
            Drawdance can't work without it.
          </>
        ),
        recourse: (
          <>
            You can try using a different browser. Take a look at{" "}
            <a
              href={SHARED_ARRAY_BUFFER_MDN_COMPATIBILITY_URL}
              target="_blank"
              rel="noreferrer"
            >
              the browser compatibility list on MDN
            </a>{" "}
            to see what's supported.
          </>
        ),
      });
    } else {
      setFatalError({
        message: (
          <>
            SharedArrayBuffer is not available because this site is set up
            wrong. Drawdance can't work like this.
          </>
        ),
        recourse: (
          <>
            If this is your website, see{" "}
            <a
              href={SHARED_ARRAY_BUFFER_MDN_SECURITY_URL}
              target="_blank"
              rel="noreferrer"
            >
              the requirements for SharedArrayBuffer on MDN
            </a>
            .
          </>
        ),
      });
    }
    return false;
  }

  return true;
}

function sendToDrawdance(
  instance: DrawdanceInstance,
  handle: number,
  events: DrawdanceEvent[]
): void {
  const json = JSON.stringify(events);
  const payload = instance.allocate(
    instance.intArrayFromString(json),
    instance.ALLOC_NORMAL
  );
  instance._DP_send_from_browser(handle, payload);
}

interface DrawdanceState {
  instance?: DrawdanceInstance;
  handle?: number;
  bufferedEvents?: DrawdanceEvent[];
}

export class Drawdance {
  private readonly state: DrawdanceState;

  constructor(state?: DrawdanceState) {
    this.state = state || {};
  }

  private connect(
    setDrawdance: Setter<Drawdance>,
    setFatalError: Setter<FatalError>
  ): void {
    let eventListener: any;
    eventListener = ({
      detail: { handle, payload },
    }: DrawdanceCustomEvent): void => {
      if (this.state.handle === undefined) {
        this.state.handle = handle;
        this._maybePublishBufferedEvents();
        setDrawdance(new Drawdance(this.state));
      }
      if (this.state.handle === handle) {
        const events: DrawdanceEvent[] = JSON.parse(payload);
        for (const event of events) {
          const type = "DRAWDANCE_" + event[0];
          const detail = event[1];
          window.dispatchEvent(new CustomEvent(type, { detail }));
        }
      } else {
        window.removeEventListener("dpevents", eventListener);
        this.publish("REQUEST_APP_QUIT");
      }
    };
    window.addEventListener("dpevents", eventListener);

    const canvas = document.getElementById("canvas") as HTMLCanvasElement;
    const setStatus = (text: string): void => {
      window.dispatchEvent(
        new CustomEvent("DRAWDANCE_MODULE_LOAD_STATUS", { detail: [text] })
      );
    };
    const locateFile = (path: string): string => {
      return `${process.env.PUBLIC_URL}/${path}`;
    };

    createModule({ canvas, setStatus, locateFile })
      .then((instance) => {
        (window as any).dpinstance = instance;
        this.state.instance = instance;
        setDrawdance(new Drawdance(this.state));
      })
      .catch((e) => {
        console.error("createModule", e);
        setFatalError({ message: "" + e });
      });
  }

  tryConnect(
    setDrawdance: Setter<Drawdance>,
    setFatalError: Setter<FatalError>
  ): void {
    if (checkFeatures(setFatalError)) {
      try {
        this.connect(setDrawdance, setFatalError);
        this._maybePublishBufferedEvents();
        setDrawdance(new Drawdance(this.state));
      } catch (e: any) {
        console.error("tryConnect", e);
        setFatalError({ message: "" + e });
      }
    }
  }

  get initialized(): boolean {
    return this.state.instance !== undefined && this.state.handle !== undefined;
  }

  private _maybePublishBufferedEvents(): void {
    const instance = this.state.instance;
    const handle = this.state.handle;
    const bufferedEvents = this.state.bufferedEvents;
    if (
      instance !== undefined &&
      handle !== undefined &&
      bufferedEvents !== undefined &&
      bufferedEvents.length !== 0
    ) {
      sendToDrawdance(instance, handle, bufferedEvents);
      delete this.state.bufferedEvents;
    }
  }

  get handle(): number | undefined {
    return this.state.handle;
  }

  publish(type: DrawdanceEventType, content?: any): void {
    const event: DrawdanceEvent =
      content === undefined ? [type] : [type, content];
    const instance = this.state.instance;
    const handle = this.state.handle;
    if (instance !== undefined && handle !== undefined) {
      sendToDrawdance(instance, handle, [event]);
    } else if (this.state.bufferedEvents) {
      this.state.bufferedEvents.push(event);
    } else {
      this.state.bufferedEvents = [event];
    }
  }

  static subscribe(listeners: DrawdanceEventListeners): () => void {
    const removers: (() => void)[] = [];

    for (const [key, listener] of Object.entries(listeners)) {
      const type = "DRAWDANCE_" + key;
      const handler = (event: Event): void => {
        const detail = (event as CustomEvent).detail;
        (listener as any)(detail);
      };
      window.addEventListener(type, handler);
      removers.push(() => {
        window.removeEventListener(type, handler);
      });
    }

    return () => {
      for (const remover of removers) {
        remover();
      }
    };
  }
}
