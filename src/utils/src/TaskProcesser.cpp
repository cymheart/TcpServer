#include"utils/include/TaskProcesser.h"


CommonTaskProcesser::CommonTaskProcesser()
	:taskPump(NULL)
{
	taskPump = new TaskPump();
}

CommonTaskProcesser::~CommonTaskProcesser()
{
	RELEASE(taskPump);
}

bool CommonTaskProcesser::Start()
{
	if (taskPump != NULL)
		taskPump->Start();

	return true;
}

void CommonTaskProcesser::Stop()
{
	if (taskPump != NULL)	
		taskPump->Stop();
}

void CommonTaskProcesser::Pause()
{
	if(taskPump != NULL)	
		taskPump->Pause();
}

void CommonTaskProcesser::Continue()
{
	if (taskPump != NULL)
		taskPump->Continue();
}

LPVOID CommonTaskProcesser::GetMainFiberHandle()
{
	if (taskPump != NULL)	
		return taskPump->GetMainFiberHandle();

	return nullptr;
}

void CommonTaskProcesser::SetUseCoroutine(bool isUse)
{
	if (taskPump != NULL)
		taskPump->SetUseCoroutine(isUse);
}

int CommonTaskProcesser::PostTask(TaskNode* taskNode, int delay)
{
	if (taskPump == NULL)
		return -1;

	return taskPump->PostTask(taskNode, delay);
}

int CommonTaskProcesser::PostTask(TaskCallBack processDataCallBack, TaskCallBack releaseDataCallBack, void* taskData, TaskMsg msg, int delay)
{
	if (taskPump == NULL)
		return -1;

	TaskNode* taskNode = new TaskNode(processDataCallBack, releaseDataCallBack, taskData, msg);
	return taskPump->PostTask(taskNode, delay);
}
