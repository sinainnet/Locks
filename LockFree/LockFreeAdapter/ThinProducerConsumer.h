#ifndef __THINPRODUCER_CONSUMER_H__

#define __THINPRODUCER_CONSUMER_H__

#include "stdafx.h"
#include <deque>
#include "ScopedLock.h"

namespace LockFree
{
	template<class T>
	class tThinProducerConsumer
	{
	public:
		
		tThinProducerConsumer() 
		{
			EventQueued = CreateEvent(NULL,			
										TRUE,			// Manual Reset
										FALSE,			// Initial State not Signalled
										NULL); 
		}
		
		// producer adds to the queue
		bool enqueue(T t, bool addToFront = false)
		{
			tScopedLock SpinLock(m_LockObj);
			if(addToFront)
				m_PCQueue.push_front (t);
			else
				m_PCQueue.push_back (t);
			if(SetEvent(EventQueued) == 0)
				throw std::exception("failed to signal the event");
			else 
				return true;
		}
		//
		
		// consumers consumes from the queue
		T dequeue() 
		{
			DWORD dwWait = ::WaitForSingleObjectsEx(EventQueued ,1000, true);
			if (dwWait - WAIT_OBJECT_0 == 0)
			{
				tScopedLock SpinLock(m_LockObj);
				T temp = NULL;
				if(!m_PCQueue.empty())
				{
					ResetEvent (EventQueued);
					temp = Event_Queue.front();
					Event_Queue.pop_front();
				}
				return temp;				
			} 
			else if (dwWait == WAIT_TIMEOUT)
			{
				return NULL;
			}
			else if(dwWait == WAIT_ABANDONED)
			{
				throw std::exception("wait abandoned");
			}
			return NULL;
		}
		//
		

	private:
		HANDLE EventQueued
		deque<T> m_PCQueue
		tSpinLock m_LockObj;

	};
};

#endif //__THINPRODUCER_CONSUMER_H__