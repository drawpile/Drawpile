Subdirectories:

 * libshared: Code used by both the client and the server
 * libserver: Common server code (uses libshared) (TO BE REWRITTEN)
 * libthicksrv: Thick server code shared by the built-in and standalone servers (uses libserver) (TO BE REWRITTEN)
 * libclient: GUI-agnostic client code (uses libshared and rustpile)
 * rustpile: A C++ interface to the Rust libraries
 * dpcore: Drawpile's paint engine and protocol
 * desktop: Widget based client application (uses libclient and libthicksrv)
 * thinsrv: Standalone thin server (uses libserver) (TO BE REWRITTEN)
 * thicksrv: Standalone thick server (uses libthicksrv) (TO BE REWRITTEN)
 * tools: Utilities (uses libshared and libclient) (TO BE REMOVED: obsoleted by drawpile-cli)
 * drawpile-cli: A command line tool for converting and rendering recordings
 * mobile: Future QML based client will go here (TO BE REMOVED: outdated)

