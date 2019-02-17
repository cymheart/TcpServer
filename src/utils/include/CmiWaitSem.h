#ifndef _CMIWAITSEM_H_
#define _CMIWAITSEM_H_

#include"utils/include/utils.h"
#include"utils/include/CmiNewMemory.h"

#define TIMEDOUT 1
#define WAITSEM  0


class CmiWaitSem : public CmiNewMemory
{
public:
	CmiWaitSem(void){
		Init();
	}
	~CmiWaitSem(){
		Close();
	}

	void Reset()
	{
		ResetEvent(handle);
	}

	void SetSem()
	{
		SetEvent(handle);
	}


int WaitSem(long  delayMillisecond)
{
	//uint64_t start = _GetTickCount64();
	int ret = WaitForSingleObject(handle, delayMillisecond);
	//uint64_t endsf = _GetTickCount64();

	//if (endsf - start > 0)
	//	printf("wait函数超时时间:%d\n", endsf - start);

	ResetEvent(handle);

	if(WAIT_OBJECT_0 != ret)
		return  TIMEDOUT;
	else
		return  WAITSEM;

}


private:

	HANDLE  handle;

	void Init(){	
		handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	void Close(){
		CloseHandle(handle);
	}
};

#endif 
