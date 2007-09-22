#ifndef ProtocolFwdDecl_INCLUDED
#define ProtocolFwdDecl_INCLUDED
#ifndef Protocol_INCLUDED // don't process if real protocol.h has already been included

namespace protocol
{
	class Message;
	
	class Identifier;
	
	class StrokeInfo;
	class StrokeEnd;
	class ToolInfo;
	
	class Acknowledgement;
	class Error;
	
	class Synchronize;
	class SyncWait;
	class Raster;
	class Cancel;
	
	class Deflate;
	class Chat;
	class Palette;
	
	class PasswordRequest;
	class Password;
	class SetPassword;
	
	class Authenticate;
	
	class ListSessions;
	class Subscribe;
	class Unsubscribe;
	
	class UserInfo;
	class HostInfo;
	class SessionInfo;
	
	class SessionInstruction;
	class SessionEvent;
	class SessionSelect;
	
	class LayerEvent;
	class LayerSelect;
	
	class Shutdown;
};

#endif // Protocol_INCLUDED
#endif // ProtocolFwdDecl_INCLUDED
