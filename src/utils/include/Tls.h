#pragma once

#include"utils/include/utils.h"
#include <time.h>
#include "utils/include/CmiThreadLock.h"
#include"utils/include/Singleton.h"
#include <unordered_map>  
using namespace std;

typedef unordered_map<string, LPVOID> TlsHashMap;

class DLL_CLASS Tls
{
	SINGLETON(Tls)

public:

	void SetValue(string key, LPVOID data);
	LPVOID GetValue(string key);

private:
	static DWORD dwTlsIndex;  
};