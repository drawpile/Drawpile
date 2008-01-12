#pragma once

#ifndef ServerStatistics_INCLUDED
#define ServerStatistics_INCLUDED

#include "types.h"

//! Server statistics
struct Statistics
{
	ulong 
		//! Stroke info messages
		strokeInfo,
		//! Stroke end messages
		strokeEnd,
		//! Tool info messages
		toolInfo;
	
	ulong
		//! Cumulative total of connections to server
		connections,
		//! Cumulative total of disconnections from server
		disconnects;
	
	ulong
		//! Number of messages received
		inMsgCount,
		//! Number of messages sent
		outMsgCount,
		//! Number of bytes sent
		dataSent,
		//! Number of bytes received
		dataRecv;
	
	ulong
		//! Number of bytes saved by bundling
		bundledAway,
		//! Number of bytes saved by compression
		compressedAway,
		//! Data processed for compression with bad results
		compressionWasted;
};

#endif // ServerStatistics_INCLUDED
