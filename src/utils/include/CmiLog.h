#pragma once
#include"utils/include/utils.h"
#include"tchar.h"
#include "utils/include/CmiThreadLock.h"
#include"utils/include/Singleton.h"

#include "log4cplus/loggingmacros.h"
#include <log4cplus/logger.h>  
#include <log4cplus/layout.h>  
#include <log4cplus/loglevel.h>  
#include <log4cplus/fileappender.h>  
#include <log4cplus/consoleappender.h>  

using namespace log4cplus;
using namespace helpers;

#define DEBUG(format, ...) CmiLog::GetInstance().debug(format,__VA_ARGS__)
#define WARNING(format, ...) CmiLog::GetInstance().warn(format,__VA_ARGS__)
#define ERR(format, ...) CmiLog::GetInstance().err(format,__VA_ARGS__)

#define LOG_TRACE(p) LOG4CPLUS_TRACE(CmiLog::GetInstance().Log(), p)  
#define LOG_DEBUG(p) LOG4CPLUS_DEBUG(CmiLog::GetInstance().Log(), p)  
#define LOG_NOTICE(p) LOG4CPLUS_INFO(CmiLog::GetInstance().Log(), p)  
#define LOG_WARNING(p)  LOG4CPLUS_WARNING(CmiLog::GetInstance().Log(), p)  
#define LOG_ERROR(p)  LOG4CPLUS_ERROR(CmiLog::GetInstance().Log(), p)  


class DLL_CLASS CmiLog
{	
	SINGLETON(CmiLog)

public:	

	void __cdecl debug(const char *format, ...);
	void __cdecl err(const char *format, ...);
	void __cdecl warn(const char *format, ...);

	Logger Log()
	{
		return Logger::getInstance(_T("Log"));
	}

private:
	static int rowNum;
};