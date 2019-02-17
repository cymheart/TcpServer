#pragma once

#include"utils/include/CmiAlloc.h"
#include"utils/include/ByteStream.h"
#include <unordered_map>  

typedef unordered_map<uint64_t, ByteStream*, hash<uint64_t>, equal_to<uint64_t>, CmiAlloc<pair<uint64_t, ByteStream*>>> DataCacheHashMap;

class DataCache :public CmiNewMemory
{
public:
	~DataCache()
	{
		for each (auto var in dataCacheHashMap)
		{
			RELEASE(var.second);
		}
	}


	ByteStream* CreateDataCache(uint64_t key)
	{
		ByteStream* dataStream = new ByteStream();
		dataStream->SetWriteHostToNet(false);
		dataStream->SetReadNetToHost(false);
		dataCacheHashMap[key] = dataStream;
		return dataStream;
	}

	ByteStream* GetDataCache(uint64_t key)
	{
		auto it = dataCacheHashMap.find(key);
		if (it != dataCacheHashMap.end()) {
			it->second->Clear();
			return it->second;
		}
		return nullptr;
	}

	void RemoveDataCache(uint64_t key)
	{
		auto it = dataCacheHashMap.find(key);
		if (it != dataCacheHashMap.end())
		{
			RELEASE(it->second);
			dataCacheHashMap.erase(key);
		}
	}

private:
	DataCacheHashMap dataCacheHashMap;
};