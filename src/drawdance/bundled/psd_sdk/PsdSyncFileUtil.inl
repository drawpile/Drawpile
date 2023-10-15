// Copyright 2011-2020, Molecular Matters GmbH <office@molecular-matters.com>
// See LICENSE.txt for licensing details (2-clause BSD License: https://opensource.org/licenses/BSD-2-Clause)

namespace fileUtil
{
	// ---------------------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------
	template <typename T>
	inline T ReadFromFile(SyncFileReader& reader)
	{
		T value = 0;
		reader.Read(&value, sizeof(T));
		return value;
	}


	// ---------------------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------
	template <typename T>
	inline T ReadFromFileBE(SyncFileReader& reader)
	{
		T value = ReadFromFile<T>(reader);
		value = endianUtil::BigEndianToNative(value);
		return value;
	}
}
