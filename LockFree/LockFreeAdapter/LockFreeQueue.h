#ifndef __LOCKFREEQUEUE_IMPL__

#define __LOCKFREEQUEUE_IMPL__

#include <iostream>
#include <deque>
#include "ScopedLock.h"

using namespace std;

namespace LockFree
{

	template<class T>
	class tLockFreeQueue
	{
	public:
		tLockFreeQueue(){}

		void enqueue(T t)
		{
			tScopedLock oSpinLock(m_LockObj);
			m_LockFreeQueue.push_front(t);
		}
	
		T dequeue()
		{
			tScopedLock oSpinLock(m_LockObj);
			if(size() == 0)
				throw std::out_of_range("out of range");

			T LastElement = m_LockFreeQueue.back();
			m_LockFreeQueue.pop_back(T);
			return LastElement; 
		}

		unsigned int size() const
		{
			return m_LockFreeQueue.size();
		}

		void eraseAll()
		{
			tScopedLock oSpinLock(m_LockObj);
			m_LockFreeQueue.clear();
		}

	public:
		virtual ~tLockFreeQueue() 
		{}

	private:
		deque<T> m_LockFreeQueue;
		tSpinLock m_LockObj;

	};
};


#endif // __LOCKFREEQUEUE_IMPL__