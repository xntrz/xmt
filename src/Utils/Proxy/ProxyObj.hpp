#pragma once

#include "ProxySettings.hpp"

#include "Shared/Network/NetTypes.hpp"
#include "Shared/Network/NetProxy.hpp"
#include "Shared/Common/Thread.hpp"
#include "Shared/Common/ThreadPool.hpp"


class CProxyObj
{
public:
	static const int32 STATE_MAX = 8;
	static const int32 STATE_STOP = -1;

	class CState    
	{
	public:
		CState() : m_pSubject(nullptr) {};
		virtual ~CState() {};
		virtual void Attach() = 0;
		virtual void Detach() = 0;
		virtual void Observing() = 0;

		inline CProxyObj& Subject() { return *m_pSubject; };

	private:
		friend CProxyObj;
		CProxyObj* m_pSubject;
	};

public:
	CProxyObj();
	virtual ~CProxyObj();
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Service(uint32 svcFlags) = 0;
	void Run();
	void StateRegist(int32 id, CState* pState);
	void StateJump(int32 id);
	
	inline void ServiceRequest(uint32 svcFlags)         { m_svcFlags.fetch_or(svcFlags); };    
	inline void SetProxy(NETPROXY type, uint64 addr)    { m_proxyType = type; m_proxyAddr = addr; };
	inline NETPROXY GetProxyType() const                { return m_proxyType; };
	inline uint64 GetProxyAddr() const                  { return m_proxyAddr; };
	inline void* GetShared() const                      { return m_shared; };
	inline void SetShared(void* shared)                 { m_shared = shared; };
	inline bool IsStopped() const                       { return m_bStopFlag; };
	inline int32 CurrentStateId() const                 { return m_stateCur; };

private:
	void* m_shared;
	NETPROXY m_proxyType;
	uint64 m_proxyAddr;
	std::atomic<uint32> m_svcFlags;
	std::atomic<int32> m_stateCur;
	std::atomic<int32> m_stateReq;
	std::array<CState*, STATE_MAX> m_stateArray;
	bool m_bStopFlag;
};


template<class T>
class CProxyObjSystem
{
public:
	static_assert(std::is_base_of<CProxyObj, T>::value, "T is not base of CProxyObj");
	
	inline CProxyObjSystem(uint32 workpoolSize, std::chrono::milliseconds updateInterval)
	: m_workpool()
	, m_workpoolSize(workpoolSize)
	, m_workpoolRunCnt(workpoolSize)
	, m_startCnt(0)
	, m_stopCnt(0)
	, m_thread()
	, m_bThreadRun(false)
	, m_bMultithread(CProxySettings::IsMultithread())
	{
		m_workpool = new T[m_workpoolSize];
		m_bThreadRun = true;
		m_thread = thread::spawn("ProxyObjSystem", [&]() {
			while (m_bThreadRun)
				OnRun();
		});
	};

	virtual ~CProxyObjSystem()
	{
		m_bThreadRun = false;
		if (m_thread.joinable())
			m_thread.join();

		if (m_workpool)
		{
			for (uint32 i = 0; i < m_workpoolRunCnt; ++i)
				m_workpool[i].Stop();

			delete[] m_workpool;
			m_workpool = nullptr;
		};
	};

	virtual void OnRun()
	{
		if (m_bMultithread)
			RunMultiThread();
		else
			RunSingleThread();
	};

	virtual bool IsStopped() const
	{
		return (m_stopCnt == m_workpoolSize);
	};

	inline uint32 GetRunNum() const
	{
		ASSERT(m_workpoolSize >= m_stopCnt);
		return (m_workpoolSize - m_stopCnt);
	};
	
	inline uint32 GetStoppedNum() const { return m_stopCnt; };
	inline uint32 GetWorkpoolSize() const { return m_workpoolSize; };

private:
	void RunSingleThread()
	{
		if (m_startCnt == 0)
		{
			for (uint32 i = 0; i < m_workpoolRunCnt; ++i)
				m_workpool[i].Start();
			m_startCnt = m_workpoolSize;
		};

		uint32 stopCnt = 0;
		for (uint32 i = 0; i < m_startCnt; ++i)
		{
			if (!m_workpool[i].IsStopped())
				m_workpool[i].Run();
			else
				++stopCnt;
		};
		m_stopCnt = stopCnt;
	};

	void RunMultiThread()
	{
		if (m_startCnt == 0)
		{
			parallel(m_workpool, m_workpool + m_workpoolRunCnt, [](T& obj) {
				obj.Start();
			});
			m_startCnt = m_workpoolRunCnt;
		};

		std::atomic<uint32> stopCnt = 0;
		parallel(m_workpool, m_workpool + m_startCnt, [&stopCnt](T& obj) {
			if (!obj.IsStopped())
				obj.Run();
			else
				++stopCnt;
		});
		m_stopCnt = stopCnt;
	};

	template<class It, class F>
	void parallel(const It first, const It last, F&& f)
	{
		const std::ptrdiff_t workerCountMax = 4; // default max value
		std::future<void> results[workerCountMax];
		std::ptrdiff_t workerCount = 4;

		std::ptrdiff_t objCnt = (last - first);
		if (objCnt <= 0)
			return;

		std::ptrdiff_t objPerWorker = (objCnt / workerCount);
		std::ptrdiff_t objRemains 	= (objCnt % workerCount);
		std::ptrdiff_t objOffset 	= 0;

		for (std::ptrdiff_t i = 0; i < workerCount; ++i)
		{
			std::ptrdiff_t cnt = objPerWorker + (i == 0 ? objRemains : 0); /* we always give remains objs to first worker */
			std::ptrdiff_t offset = objOffset;

			objOffset += cnt;

			if (cnt <= 0)
				continue;

			results[i] = thread::async([ f, offset, cnt, first, last ]() {
				const It beg = first + offset;
				const It end = first + offset + cnt;

				ASSERT(beg >= first);
				ASSERT(beg < last);
				ASSERT(end <= last);
				ASSERT(end > first);

				const It it = beg;
				
				std::for_each(beg, end, [ = ](auto& v) { f(v); });
			});
		};

		for (std::ptrdiff_t i = 0; i < workerCount; ++i)
		{
			if (results[i].valid())
				results[i].wait();
		};
	};

private:
	T* m_workpool;
	uint32 m_workpoolSize;
	uint32 m_workpoolRunCnt;
	uint32 m_startCnt;
	uint32 m_stopCnt;
	std::thread m_thread;
	std::atomic<bool> m_bThreadRun;
	bool m_bMultithread;
};