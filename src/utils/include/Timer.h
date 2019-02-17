#pragma once
#include"utils/include/TaskProcesser.h"

typedef void (*TimerCallBack)(void *data);

class DLL_CLASS Timer : public CmiNewMemory
{
public:
	Timer(TaskProcesser* taskProcesser, int durationMS, TimerCallBack timerCB, void* param, bool isRepeat = true)
		:isStop(true)
		, stopCB(nullptr)
		, stopParam(nullptr)
	{
		this->param = param;
		this->timerCB = timerCB;

		this->taskProcesser = taskProcesser;
		taskProcesser->addRef();

		this->durationMS = durationMS;
		this->isRepeat = isRepeat;
	}

	~Timer()
	{
		RELEASE_REF(taskProcesser);
	}

	void SetTimerCB(TimerCallBack timerCB, void* param)
	{
		this->timerCB = timerCB;
		this->param = param;
	}


	void SetStopCB(TimerCallBack stopCB, void* param)
	{
		this->stopCB = stopCB;
		stopParam = param;
	}

	void Start()
	{
		taskProcesser->PostTask(StartTask, nullptr, (void*)this, TMSG_TIMER_START);
	}

	void Stop()
	{	
		taskProcesser->PostTask(StopTask, nullptr, (void*)this, TMSG_TIMER_STOP);
	}

private:
	void PostTask(void* data)
	{
		Timer* timer = (Timer*)data;
		TaskNode* taskNode = new TaskNode(RunTask, nullptr, data, TMSG_TIMER_RUN);
		timer->taskNode = taskNode;
		taskProcesser->PostTask(taskNode, durationMS);
	}

	static void* RunTask(void* data)
	{
		Timer* timer = (Timer*)data;
		if (timer->isStop)
			return 0;

		timer->timerCB(timer->param);

		timer->taskNode = nullptr;

		if(timer->isRepeat)
			timer->PostTask(data);
		else
			timer->isStop = true;

		return 0;
	}

	static void* StartTask(void* data)
	{
		Timer* timer = (Timer*)data;
		if (timer->isStop == false)
			return 0;
		timer->isStop = false;

		timer->PostTask((void*)timer);
		return 0;
	}

	static void* StopTask(void* data)
	{
		Timer* timer = (Timer*)data;
		timer->taskNode = nullptr;
		timer->isStop = false;

		if(timer->stopCB)	
			timer->stopCB(timer->stopParam);

		return 0;
	}

private:
	
	int durationMS;
	TaskProcesser* taskProcesser;
	
	TimerCallBack timerCB;
	void* param;

	TimerCallBack stopCB;
	void* stopParam;


	bool isStop;
	bool isRepeat;
	TaskNode* taskNode;

	friend class TaskPump;
};