#ifndef _TASKPUMP_H_
#define _TASKPUMP_H_

#include"utils/include/utils.h"
#include"utils/include/CmiThreadLock.h"
#include"utils/include/CmiWaitSem.h"
#include"utils/include/CmiNewMemory.h"
#include"utils/include/CmiAlloc.h"
#include<list>
using namespace std;

#define MAX_DELAY_TIME  0x7FFFFFFF

class Timer;
class TaskNode;
typedef void* (*TaskCallBack)(void *data);

typedef list<TaskNode*, CmiAlloc<TaskNode*>> TaskNodeList;
static void ReleaseTaskNodeList(TaskNodeList* nodeList);

enum TaskMsg
{
	TMSG_DATA,
	TMSG_QUIT,
	TMSG_TIMER_START,
	TMSG_TIMER_RUN,
	TMSG_TIMER_STOP,
	TMSG_PAUSE
};

class TaskNode : public CmiNewMemory
{
public:
	TaskNode(TaskMsg taskMsg);

	TaskNode(
		TaskCallBack processDataCallBack,
		TaskCallBack releaseDataCallBack,
		void* taskData, TaskMsg msg = TMSG_DATA);

public:
	uint64_t                startTime;
	int                     delay;                //延迟执行时间
	TaskCallBack            processDataCallBack;  //处理数据回调函数
	TaskCallBack            releaseDataCallBack;  //释放数据回调函数
	void                   *data;                 //任务数据
	TaskMsg                 msg;                  //任务标志

};

class TaskQueue : public CmiNewMemory
{
public:
	TaskQueue();
	~TaskQueue();

	void Clear();

public:
	TaskNodeList*   readList;
	TaskNodeList*   writeList;

	TaskNodeList*   timerReadList;
	TaskNodeList*   timerWriteList;

	CmiThreadLock*  lock;
};


class TaskPump : public CmiNewMemory
{
public:
	TaskPump(int _type = 0);
	~TaskPump();

	int Start();
	int Pause();
	int Continue();
	int Stop();


	int ProcessTaskNodeData(TaskNode* node);

	int PostTask(TaskNode* taskNode, int delay = 0);
	int PostCommonTask(TaskNode* taskNode, int delay);
	int PostTimerTask(TaskNode* taskNode, int delay);

	void SetUseCoroutine(bool isUse)
	{		
		if(handle == nullptr)	
			isUseCoroutine = isUse;
	}

	LPVOID GetMainFiberHandle()
	{
		return mainFiberHandle;
	}

	static DWORD WINAPI Run(LPVOID param);
	

private:
	class TimerCompare :public CmiNewMemory
	{
	public:
		TimerCompare(uint64_t _time = 0) :curTime(_time) {}
		bool operator()(TaskNode* left, TaskNode* right)
		{
			int delayA = curTime - left->startTime - left->delay;
			int delayB = curTime - right->startTime - right->delay;
			return delayA > delayB;
		}

	private:
		uint64_t curTime;

		friend class TaskPump;
	};

private:
	int type;
	HANDLE handle;
	LPVOID mainFiberHandle;
	TaskQueue* taskQue;
	TimerCompare* timerCompare;
	CmiWaitSem*      readSem;
	CmiWaitSem*      pauseSem;
	CmiWaitSem*      quitSem;

	bool isUseCoroutine;
};


#endif
