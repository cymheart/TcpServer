#include "utils/include/UniqueID.h"  

#define   sequenceMask  (-1L ^ (-1L << 12L))  
#define EPOCHFILETIME 11644473600000000Ui64

BUILD_SHARE(UniqueID)
UniqueID::UniqueID()
	:workid(0)
	, seqid(0)
	, last_stamp(0)
{
}

UniqueID::~UniqueID()
{
}

void UniqueID::set_workid(int workid)
{
	this->workid = workid;
}


uint64_t UniqueID::get_curr_ms()
{
#ifdef __GUNC__
	struct timeval tv;
	gettimeofday(&tv, NULL);
	uint64 time = tv.tv_usec;
	time /= 1000;
	time += (tv.tv_sec * 1000);
	return time;
#else
	FILETIME filetime;
	uint64_t time = 0;
	GetSystemTimeAsFileTime(&filetime);

	time |= filetime.dwHighDateTime;
	time <<= 32;
	time |= filetime.dwLowDateTime;

	time /= 10;
	time -= EPOCHFILETIME;
	return time / 1000;
#endif
}

uint64_t UniqueID::wait_next_ms(uint64_t lastStamp)
{
	uint64_t cur = 0;
	do {
		cur = get_curr_ms();
	} while (cur <= lastStamp);
	return cur;
}

uint64_t UniqueID::gen()
{
	uint64_t  uniqueId = 0;
	uint64_t nowtime = get_curr_ms();
	uniqueId = nowtime << 22;
	uniqueId |= (workid & 0x3ff) << 12;

	if (nowtime <last_stamp)
	{
		perror("error");
		exit(-1);
	}
	if (nowtime == last_stamp)
	{
		seqid = InterlockedIncrement(&seqid)& sequenceMask;

		if (seqid == 0)
		{
			nowtime = wait_next_ms(last_stamp);
			uniqueId = nowtime << 22;
			uniqueId |= (workid & 0x3ff) << 12;
		}
	}
	else
	{
		seqid = 0;
	}
	last_stamp = nowtime;
	uniqueId |= seqid;
	return uniqueId;
}


uint64_t UniqueID:: gen_multi()
{
	uint64_t  uniqueId = 0;
	threadLock.Lock();
	uniqueId = gen();
	threadLock.UnLock();
	return uniqueId;
}

////64×óÒÆlen Î»  
//U64 move_left64(U64 a, int len)
//{
//	U32 *p = (U32*)&a;
//	if (len <32)
//	{
//
//		*(p + 1) <<= len;
//		U32 tmp = (*p) >> (32 - len);
//		*(p + 1) |= tmp;
//		*p <<= len;
//	}
//	else
//	{
//		*(p + 1) = *p;
//		*p = 0x00000000;
//		*(p + 1) <<= (len - 32);
//	}
//	return a;
//}
////64ÓÒÒÆlen Î»  
//U64 move_right64(U64 a, int len)
//{
//	U32 *p = (U32*)&a;
//	if (len<32)
//	{
//		*p >>= len;
//		U32 tmp = *(p + 1) << (32 - len);
//		*p |= tmp;
//		*(p + 1) >>= len;
//	}
//	else
//	{
//		*p = *(p + 1);
//		*(p + 1) = 0x00000000;
//		*p >>= (len - 32);
//	}
//	return a;
//}