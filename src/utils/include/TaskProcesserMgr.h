#pragma once
#include"utils/include/ByteStream.h"
#include"utils/include/TaskProcesser.h"

class DLL_CLASS TaskProcesserMgr : public CmiNewMemory
{
public:
	TaskProcesserMgr(int _processerAmount, TaskProcesser* _statisticsProcesser = nullptr);
	~TaskProcesserMgr();


	int  PostTask(TaskCallBack processDataCallBack, void* taskData, int delay = 0)
	{
		uint32_t idx = AssignProcessserIdx();
		processingTaskCount[idx]++;
		TaskProcesserMgr* mgr = this;

		ByteStream dataStream;
		dataStream.SetWriteHostToNet(false);
		dataStream.Write((void*)mgr);
		dataStream.Write(idx);
		dataStream.Write((void*)processDataCallBack);
		dataStream.Write(taskData);
		return processers[idx]->PostTask(ProcesserTask, NULL, dataStream.TakeBuf(), TMSG_DATA, delay);
	}

private:
	int AssignProcessserIdx();

private:
	static void* ProcesserTask(void* data);
	static void* DecProcessingAmountTask(void* data);

private:
	TaskProcesser* statisticsProcesser;
	int processerAmount;
	vector<TaskProcesser*> processers;
	vector<int> processingTaskCount;

};