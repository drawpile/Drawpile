Subdirectories:

 * libshared: Code used by both the client and the server
 * libserver: Common server code (uses libshared)
 * libthicksrv: Thick server code shared by the built-in and standalone servers (uses libserver)
 * libclient: GUI-agnostic client code (uses libshared)
 * desktop: Widget based client application (uses libclient and libthicksrv)
 * thinsrv: Standalone thin server (uses libserver)
 * thicksrv: Standalone thick server (uses libthicksrv) (EXPERIMENTAL)
 * tools: Utilities (uses libshared and libclient)
 * mobile: Future QML based client will go here

