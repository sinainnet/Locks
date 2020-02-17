#ifndef __LOCKFREESTACK_IMPL__

#define __LOCKFREESTACK_IMPL__

#include <iostream>
#include <deque>
#include "ScopedLock.h"

using namespace std;

namespace LockFree
{

	template<class T>
	class tLockFreeStack
	{
	public:
		tLockFreeStack(){}

		void push(T t)
		{
			tScopedLock oSpinLock(m_LockObj);
			m_LockFreeStack.push_back(t);
		}
	
		T pop()
		{
			tScopedLock oSpinLock(m_LockObj);
			if(size() == 0)
				throw std::out_of_range("out of range");

			T LastElement = m_LockFreeStack.back();
			m_LockFreeStack.pop_back(T);
			return LastElement;
		}

		unsigned int size() const
		{
			return m_LockFreeStack.size();
		}

		void eraseAll()
		{
			tScopedLock oSpinLock(m_LockObj);
			m_LockFreeStack.clear();
		}

	public:
		virtual ~tLockFreeStack() 
		{}

	private:
		deque<T> m_LockFreeStack;
		tSpinLock m_LockObj;

	};
};


#endif // __LOCKFREESTACK_IMPL__