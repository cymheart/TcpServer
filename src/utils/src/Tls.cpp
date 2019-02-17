#include "utils/include/Tls.h"  


BUILD_SHARE(Tls)
DWORD Tls::dwTlsIndex;

Tls::Tls()
{
	dwTlsIndex = TlsAlloc();
}

Tls::~Tls()
{
	LPVOID tlsmapData = TlsGetValue(dwTlsIndex);
	if (tlsmapData == nullptr)
		return;
	
	TlsHashMap* tlsMap = (TlsHashMap*)tlsmapData;
	RELEASE(tlsMap);
	TlsFree(dwTlsIndex);
}


void Tls::SetValue(string key, LPVOID data)
{
	LPVOID tlsmapData = TlsGetValue(dwTlsIndex);
	TlsHashMap* tlsMap;
	if (tlsmapData == nullptr)
	{
		tlsMap = new TlsHashMap();
		TlsSetValue(dwTlsIndex, &tlsMap);
	}
	else
	{
		tlsMap = (TlsHashMap*)tlsmapData;
	}

	(*tlsMap)[key] = data;
}


LPVOID Tls::GetValue(string key)
{
	LPVOID tlsmapData = TlsGetValue(dwTlsIndex);
	TlsHashMap* tlsMap;
	if (tlsmapData == nullptr)
	{
		return nullptr;
	}
	else
	{
		tlsMap = (TlsHashMap*)tlsmapData;
	}

	return (*tlsMap)[key];
}