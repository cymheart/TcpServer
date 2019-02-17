#ifndef _TASKPROCESSER_H_
#define _TASKPROCESSER_H_

#include"utils/include/TaskPump.h"

class DLL_CLASS TaskProcesser : public CmiNewMemoryRef
{
public:
	virtual bool Start() = 0;
	virtual void Stop() = 0;
	virtual void Pause() = 0;
	virtual void Continue() = 0;	

	virtual LPVOID GetMainFiberHandle() = 0;
	virtual void SetUseCoroutine(bool isUse) = 0;

	virtual int PostTask(TaskNode* taskNode, int delay = 0) = 0;
	virtual int PostTask(TaskCallBack processDataCallBack,TaskCallBack releaseDataCallBack, void* taskData, TaskMsg msg = TMSG_DATA, int delay = 0) = 0;

};

class DLL_CLASS CommonTaskProcesser:public TaskProcesser
{
public:
	CommonTaskProcesser();
	virtual ~CommonTaskProcesser();


	bool Start();
	void Stop();
	void Pause();
	void Continue();

	LPVOID GetMainFiberHandle();
	void SetUseCoroutine(bool isUse);

	int PostTask(TaskNode* taskNode, int delay = 0);
	int PostTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, TaskMsg msg = TMSG_DATA, int delay = 0);
	
private:
	TaskPump*  taskPump;

};

#endif