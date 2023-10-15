// Copyright 2011-2020, Molecular Matters GmbH <office@molecular-matters.com>
// See LICENSE.txt for licensing details (2-clause BSD License: https://opensource.org/licenses/BSD-2-Clause)

#pragma once

#include "PsdEndianConversion.h"
#include "PsdSyncFileReader.h"


PSD_NAMESPACE_BEGIN

/// \ingroup Util
/// \namespace fileUtil
/// \brief Provides file reading utilities.
namespace fileUtil
{
	/// Reads built-in data types from a file.
	template <typename T>
	inline T ReadFromFile(SyncFileReader& reader);

	/// Reads built-in data types from a file, assuming they are stored as big-endian data.
	/// The read value is automatically converted to the native endianness.
	template <typename T>
	inline T ReadFromFileBE(SyncFileReader& reader);
}

#include "PsdSyncFileUtil.inl"

PSD_NAMESPACE_END
