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

export type Setter<T> = (value: T) => void;
export type Updater<T> = (value: (prev: T) => T) => void;

export interface User {
  contextId: number;
  name: string;
  connected: boolean;
}

export interface ChatMessage {
  type: "CHAT";
  contextId: number;
  action: boolean;
  text: string;
}

export interface JoinMessage {
  type: "JOIN";
  contextId: number;
}

export interface LeaveMessage {
  type: "LEAVE";
  contextId: number;
}

export type ChatEntry = ChatMessage | JoinMessage | LeaveMessage;

export function isBlank(value: string | undefined | null): boolean {
  return typeof value !== "string" || value.trim() === "";
}

export function changeInput(
  setter: (newValue: string) => void
): React.ChangeEventHandler<HTMLInputElement | HTMLTextAreaElement> {
  return (event: React.ChangeEvent<HTMLInputElement | HTMLTextAreaElement>) => {
    setter(event.target.value);
  };
}

export function stopPropagation(e: React.SyntheticEvent): void {
  e.stopPropagation();
}
