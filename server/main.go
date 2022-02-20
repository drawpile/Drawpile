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
package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
	"github.com/rs/zerolog"
	"github.com/rs/zerolog/log"
)

type Server struct {
	targetAddress string
	upgrader      websocket.Upgrader
}

type Client struct {
	closed int32
	ws     *websocket.Conn
	tcp    net.Conn
}

var listen = flag.String("listen", "0.0.0.0:27751", "http server address")
var proxy = flag.String("proxy", "127.0.0.1:27750", "address to proxy to")
var dir = flag.String("dir", "", "directory to serve static files from")
var filesRoot = flag.String("files-root", "/", "path to serve static files from")
var wsRoot = flag.String("ws-root", "/ws", "path to serve the WebSocket connection from")
var checkOrigin = flag.Bool("check-origin", false, "toggle cross-origin check")
var logLevel = flag.String("log", "info", "log level (info, debug, warn, error)")
var color = flag.Bool("color", false, "enable colorful terminal logging")
var tls = flag.Bool("tls", false, "enable serving via TLS")
var cert = flag.String("cert", "", "TLS certificate path")
var key = flag.String("key", "", "TLS key path")

func tryClose(close func() error, what string) {
	defer func() {
		if r := recover(); r != nil {
			log.Error().Str("panic", fmt.Sprint(r)).Str("connection", what).
				Msg("Close panicked")
		}
	}()
	err := close()
	if err != nil {
		log.Err(err).Str("connection", what).Msg("Close failed")
	}
}

func closeConnections(client *Client) {
	if atomic.CompareAndSwapInt32(&client.closed, 0, 1) {
		tryClose(client.ws.Close, "WebSocket")
		tryClose(client.tcp.Close, "TCP")
	}
}

func writeExactly(tcp net.Conn, message []byte) error {
	total, length := 0, len(message)
	for {
		written, err := tcp.Write(message[total:])
		if err != nil {
			return err
		}
		total += written
		if total >= length {
			return nil
		}
	}
}

func proxyFomWebSocketToTcp(client *Client) {
	ws, tcp := client.ws, client.tcp
	lenbuf := make([]byte, 2)
	for {
		messageType, message, err := ws.ReadMessage()
		if err != nil {
			log.Err(err).Msg("WebSocket read failed")
			break
		}
		if messageType != websocket.BinaryMessage {
			log.Error().Msg("WebSocket message is not binary")
			break
		}
		binary.BigEndian.PutUint16(lenbuf, uint16(len(message)-2))
		err = writeExactly(tcp, lenbuf)
		if err == nil {
			err = writeExactly(tcp, message)
		}
		if err != nil {
			log.Err(err).Msg("TCP write failed")
			break
		}
	}
	closeConnections(client)
}

func readExactly(conn net.Conn, buffer []byte) error {
	total := 0
	for total < len(buffer) {
		read, err := conn.Read(buffer[total:])
		if err != nil {
			return err
		}
		total += read
	}
	return nil
}

func proxyFromTcpToWebSocket(client *Client) {
	ws, tcp := client.ws, client.tcp
	buffer := make([]byte, 1024)
	for {
		err := readExactly(tcp, buffer[:2])
		if err != nil {
			log.Err(err).Msg("TCP length read failed")
			break
		}

		payloadLength := int(binary.BigEndian.Uint16(buffer))
		totalLength := 4 + payloadLength
		if len(buffer) < totalLength {
			newBuffer := make([]byte, totalLength)
			copy(newBuffer[:2], buffer[:2])
			buffer = newBuffer
		}

		err = readExactly(tcp, buffer[2:totalLength])
		if err != nil {
			log.Err(err).Msg("TCP body read failed")
			break
		}

		err = ws.WriteMessage(websocket.BinaryMessage, buffer[2:totalLength])
		if err != nil {
			log.Err(err).Msg("WebSocket write failed")
			break
		}
	}
	closeConnections(client)
}

func (server *Server) handle(writer http.ResponseWriter, request *http.Request) {
	log.Info().Str("address", request.RemoteAddr).Msg("New client")

	// Emscripten has a race condition where the WebSocket can already be open
	// and receiving data before the code gets a chance to add its listeners
	// to it. Jamming a wait in here is ugly, but it seems to work in practice.
	time.Sleep(100 * time.Millisecond)

	ws, err := server.upgrader.Upgrade(writer, request, nil)
	if err != nil {
		log.Err(err).Msg("WebSocket upgrade failed")
		return
	}

	address := server.targetAddress
	log.Debug().Str("address", address).Msg("Opening proxy connection")
	tcp, err := net.Dial("tcp", address)
	if err != nil {
		log.Err(err).Str("address", address).Msg("TCP connection failed")
		tryClose(ws.Close, "WebSocket")
		return
	}

	client := &Client{0, ws, tcp}
	go proxyFomWebSocketToTcp(client)
	go proxyFromTcpToWebSocket(client)
}

func main() {
	flag.Parse()

	log.Logger = log.Output(zerolog.ConsoleWriter{
		Out:     os.Stdout,
		NoColor: !*color,
	})

	level, err := zerolog.ParseLevel(*logLevel)
	if err == nil {
		zerolog.SetGlobalLevel(level)
		log.Info().Str("levelString", level.String()).Msg("Log level set")
	} else {
		zerolog.SetGlobalLevel(zerolog.InfoLevel)
		log.Err(err).Str("level", *logLevel).Msg("Invalid log level, using info")
	}

	if len(flag.Args()) != 0 {
		excessArgs := zerolog.Arr()
		for _, arg := range flag.Args() {
			excessArgs.Str(arg)
		}
		log.Fatal().Array("excessArgs", excessArgs).Msg(
			"Excess arguments (missing '=' somewhere?)")
	}

	if *dir == "" {
		log.Fatal().Msg("Missing required argument 'dir'")
	}

	server := Server{
		targetAddress: *proxy,
		upgrader:      websocket.Upgrader{},
	}
	if !*checkOrigin {
		server.upgrader.CheckOrigin = func(r *http.Request) bool { return true }
	}
	http.HandleFunc(*wsRoot, server.handle)

	fileServer := http.FileServer(http.Dir(*dir))
	var handler http.HandlerFunc = func(writer http.ResponseWriter, request *http.Request) {
		log.Debug().Str("path", request.URL.Path).Msg("New request")
		header := writer.Header()
		header.Add("Cross-Origin-Resource-Policy", "same-site")
		header.Add("Cross-Origin-Opener-Policy", "same-origin")
		header.Add("Cross-Origin-Embedder-Policy", "require-corp")
		fileServer.ServeHTTP(writer, request)
	}

	root := *filesRoot
	if !strings.HasSuffix(root, "/") {
		root += "/" // The file server really wants a slash suffix.
	}
	http.Handle(root, http.StripPrefix(root, handler))

	log.Info().Str("address", *listen).Str("proxy", *proxy).Msg("Listening")
	if *tls {
		err = http.ListenAndServeTLS(*listen, *cert, *key, nil)
	} else {
		err = http.ListenAndServe(*listen, nil)
	}
	if err != nil {
		log.Fatal().Err(err).Msg("Listen failed")
	}
}
