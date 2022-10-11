#pragma once


DLLSHARED HOBJ EventCreate(void);
DLLSHARED void EventDestroy(HOBJ hEvent);
DLLSHARED void EventSignalOnce(HOBJ hEvent);
DLLSHARED void EventSignalAll(HOBJ hEvent);
DLLSHARED void EventWait(HOBJ hEvent);
DLLSHARED void EventWait(HOBJ hEvent, bool(*Predicate)(void* Param), void* Param = nullptr);
DLLSHARED bool EventWaitFor(HOBJ hEvent, uint32 Ms);
DLLSHARED bool EventWaitFor(HOBJ hEvent, uint32 Ms, bool(*Predicate)(void* Param), void* Param = nullptr);