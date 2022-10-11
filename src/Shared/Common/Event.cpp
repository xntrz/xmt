#include "Event.hpp"


struct Event_t
{
	std::mutex Mutex;
	std::condition_variable ConditionVariable;
	std::atomic<bool> Flag;
};


HOBJ EventCreate(void)
{
	Event_t* Event = new Event_t();
	if (!Event)
		return 0;

	Event->Flag = false;

	return HOBJ(Event);
};


void EventDestroy(HOBJ hEvent)
{
	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;
	delete Event;
};


void EventSignalOnce(HOBJ hEvent)
{
	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;

	{
		std::unique_lock<std::mutex> lock(Event->Mutex);
		Event->Flag = true;
		Event->ConditionVariable.notify_one();
	}
};


void EventSignalAll(HOBJ hEvent)
{
	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;

	{
		std::unique_lock<std::mutex> lock(Event->Mutex);
		Event->Flag = true;
		Event->ConditionVariable.notify_all();
	}
};


void EventWait(HOBJ hEvent)
{
	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;

	{
		std::unique_lock<std::mutex> Lock(Event->Mutex);
		Event->ConditionVariable.wait(Lock);
		Event->Flag = false;
	}
};


void EventWait(HOBJ hEvent, bool(*Predicate)(void* Param), void* Param)
{
	bool bResult = false;

	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;

	{
		std::unique_lock<std::mutex> Lock(Event->Mutex);
		Event->ConditionVariable.wait(Lock, std::bind(Predicate, Param));
		Event->Flag = false;
	}
};


bool EventWaitFor(HOBJ hEvent, uint32 Ms)
{
	bool(*Predicate)(void* Param) =
		[](void* Param) -> bool
	{
		return ((Event_t*)Param)->Flag.load();
	};

	return EventWaitFor(
		hEvent,
		Ms,
		Predicate,
		(void*)hEvent
	);
};


bool EventWaitFor(HOBJ hEvent, uint32 Ms, bool(*Predicate)(void* Param), void* Param)
{
	bool bResult = false;

	ASSERT(hEvent);
	Event_t* Event = (Event_t*)hEvent;

	{
		std::unique_lock<std::mutex> Lock(Event->Mutex);
		bResult = Event->ConditionVariable.wait_for(
			Lock,
			std::chrono::milliseconds(Ms),
			std::bind(Predicate, Param)
		);

		if (bResult)
			Event->Flag = false;
	}

	return bResult;
};