#include"utils/include/TaskPump.h"
#include"utils/include/Timer.h"

void ReleaseTaskNodeList(TaskNodeList* nodeList)
{
	TaskNodeList::iterator iter;
	for (iter = nodeList->begin(); iter != nodeList->end();)
	{
		RELEASE(*iter);
		iter = nodeList->erase(iter);
	}
}

//TaskNode
TaskNode::TaskNode(TaskMsg taskMsg)
	: startTime(0)
	,delay(0)            
	,processDataCallBack(nullptr)
	,releaseDataCallBack(nullptr)
	,data(nullptr)
	,msg(taskMsg)
{
}

TaskNode::TaskNode(
		TaskCallBack processDataCallBack,
		TaskCallBack releaseDataCallBack,
		void* taskData, TaskMsg msg)
	:startTime(0)
	, delay(0)
	, processDataCallBack(nullptr)
	, releaseDataCallBack(nullptr)
	, data(nullptr)
{
	this->processDataCallBack = processDataCallBack;
	this->releaseDataCallBack = releaseDataCallBack;
	this->data = taskData;
	this->msg = msg;
}


//TaskQueue
TaskQueue::TaskQueue()
{
	readList = new TaskNodeList();
	writeList = new TaskNodeList();

	timerReadList = new TaskNodeList();
	timerWriteList = new TaskNodeList();

	lock = new CmiThreadLock();
}

TaskQueue::~TaskQueue()
{
	Clear();

	RELEASE(readList);
	RELEASE(writeList);
	RELEASE(timerReadList);
	RELEASE(timerWriteList);
	RELEASE(lock);
}

void TaskQueue::Clear()
{
	ReleaseTaskNodeList(readList);
	ReleaseTaskNodeList(writeList);

	ReleaseTaskNodeList(timerReadList);
	ReleaseTaskNodeList(timerWriteList);
}



//TaskPump
TaskPump::TaskPump(int _type)
	:type(_type)
	, isUseCoroutine(false)
	, mainFiberHandle(nullptr)
	, handle(nullptr)
{
	timerCompare = new TimerCompare(0);

	readSem = new CmiWaitSem();
	quitSem = new CmiWaitSem();
	pauseSem = new CmiWaitSem();

	taskQue = new TaskQueue;
	if (taskQue == NULL)
		return;
}

TaskPump::~TaskPump()
{
	Stop();

	RELEASE(timerCompare);
	RELEASE(readSem);
	RELEASE(quitSem);
	RELEASE(pauseSem);
	RELEASE(taskQue);
}



int TaskPump::PostTask(TaskNode* taskNode, int delay)
{
	int ret = 0;

	taskQue->lock->Lock();

	switch (taskNode->msg)
	{
	case TMSG_TIMER_START:
		ret = PostTimerTask(taskNode, -2000);		
	break;

	case TMSG_TIMER_RUN:
		ret = PostTimerTask(taskNode, delay);
		break;

	case TMSG_TIMER_STOP:
		ret = PostCommonTask(taskNode, -1000);
		break;

	default:
		ret = PostCommonTask(taskNode, delay);
	}

	taskQue->lock->UnLock();

	//设置任务处理可读信号
	readSem->SetSem();

	return ret;
}


int TaskPump::PostCommonTask(TaskNode* taskNode, int delay)
{
	int ret = 0;

	taskNode->delay = delay;
	if (delay != 0)
		taskNode->startTime = CmiGetTickCount64();

	if (delay == 0)
	{	
		taskQue->writeList->push_back(taskNode);
	}
	else
	{
		taskQue->timerWriteList->push_back(taskNode);
	}

	return ret;
}


int TaskPump::PostTimerTask(TaskNode* taskNode, int delay)
{
	taskNode->delay = delay;
	taskNode->startTime = CmiGetTickCount64();
	taskQue->timerWriteList->push_back(taskNode);
	return 0;
}

int TaskPump::Start()
{
	if (type == 0 && handle == nullptr)
	{
		handle = CreateThread(NULL, 0, Run, (void*)this, 0, NULL);
		if (nullptr == handle) {
			RELEASE(taskQue);
			return -1;
		}
	}

	return 0;
}

int TaskPump::Stop()
{
	Continue();

	TaskNode* node = new TaskNode(TMSG_QUIT);
	if (PostTask(node, 0) != 0)
		return -1;

	quitSem->WaitSem(-1);
	
	handle = nullptr;
	mainFiberHandle = nullptr;

	taskQue->Clear();

	return 0;
}

int TaskPump::Pause()
{
	TaskNode* node = new TaskNode(TMSG_PAUSE);
	if (PostTask(node, 0) != 0)
		return -1;

	return 0;
}

int TaskPump::Continue()
{
	pauseSem->SetSem();
	return 0;
}

int TaskPump::ProcessTaskNodeData(TaskNode* node)
{
	if (node->processDataCallBack)
		node->processDataCallBack(node->data);

	if (node->releaseDataCallBack)
		node->releaseDataCallBack(node->data);

	TaskMsg taskMsg = node->msg;

	switch (taskMsg)
	{
	case TMSG_QUIT:
		quitSem->SetSem();
		return 1;

	case TMSG_PAUSE:
		pauseSem->WaitSem(-1);
		break;
	}

	return 0;
}


DWORD WINAPI TaskPump::Run(LPVOID param)
{
	TaskPump* taskPump = (TaskPump*)param;
	TaskQueue* taskQue = taskPump->taskQue;

	TaskNodeList::iterator iter;
	TaskNodeList* readList = taskQue->readList;
	TaskNodeList* writeList = taskQue->writeList;

	TaskNodeList* timerReadList = taskQue->timerReadList;
	TaskNodeList* timerWriteList = taskQue->timerWriteList;

	TaskNode* cur;
	Timer* timer;
	uint64_t curTime;
	int32_t minDelay = MAX_DELAY_TIME;
	int32_t curdelay;


	if (taskPump->isUseCoroutine) {
		taskPump->mainFiberHandle = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	}

	for (;;)
	{	
		curTime = CmiGetTickCount64();

		for (iter = timerReadList->begin(); iter != timerReadList->end();)
		{
			cur = (*iter);

			if (cur->delay > 0 && curTime - cur->startTime < cur->delay)
			{
				curdelay = (int32_t)(cur->delay - (curTime - cur->startTime));
				if (curdelay < minDelay)
					minDelay = curdelay;
				break;
			}

			if (cur->msg == TMSG_TIMER_STOP) {
				timer = (Timer*)cur->data;
				if (timer->taskNode != nullptr) {
					timerReadList->remove(timer->taskNode);
					RELEASE(timer->taskNode);
				}
			}

			if (taskPump->ProcessTaskNodeData(cur) == 1)
				goto end;

		
			RELEASE(cur);
			iter = timerReadList->erase(iter);
		}

		for (iter = readList->begin(); iter != readList->end();)
		{
			cur = (*iter);

			if (taskPump->ProcessTaskNodeData(cur) == 1)
				goto end;

			RELEASE(cur);
			iter = readList->erase(iter);
		}

		if (taskPump->type == 1 &&
			writeList->empty() &&
			timerWriteList->empty())
		{
			goto end;
		}
	
		taskQue->lock->Lock();

		while (taskPump->type == 0 &&
			writeList->empty() && 
			timerWriteList->empty())
		{
			taskQue->lock->UnLock();
			if (minDelay == MAX_DELAY_TIME)
			{
				taskPump->readSem->WaitSem(-1);
			}
			else 
			{
				taskPump->readSem->WaitSem(minDelay);
				taskQue->lock->Lock();
				minDelay = MAX_DELAY_TIME;
				break;
			}

			taskQue->lock->Lock();
		}

		
		//common
		if (!writeList->empty()) {
			taskQue->writeList = readList;
			taskQue->readList = writeList;

			readList = taskQue->readList;
			writeList = taskQue->writeList;
		}

		//timer
		if (!timerWriteList->empty())
		{			
			minDelay = MAX_DELAY_TIME;

			if (!timerReadList->empty())
			{
				timerReadList->splice(timerReadList->end(), *timerWriteList);
			}
			else
			{
				taskQue->timerWriteList = timerReadList;
				taskQue->timerReadList = timerWriteList;
				timerReadList = taskQue->timerReadList;
				timerWriteList = taskQue->timerWriteList;
			}

			taskPump->timerCompare->curTime = curTime;
			timerReadList->sort(*taskPump->timerCompare);
		}		


		taskQue->lock->UnLock();	

		if (taskPump->type != 0)
			break;
	}

end:
	if (taskPump->type == 0)
		taskPump->quitSem->SetSem();

	if (taskPump->mainFiberHandle != nullptr) {
		DeleteFiber(taskPump->mainFiberHandle);
		taskPump->mainFiberHandle = nullptr;
	}

	return 0;
}




