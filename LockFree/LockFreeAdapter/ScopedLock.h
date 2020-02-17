
#ifndef _LOCKFREESCOPEDLOCK__H
#define _LOCKFREESCOPEDLOCK__H

#include "SpinLock.h"

namespace LockFree
{

	class tScopedLock
	{
	public:

		tScopedLock(tSpinLock &LockObj):m_LockObj(LockObj)	
		{
			m_SpinWaitLock.Lock(m_LockObj);
		}
		~tScopedLock()
		{
			m_SpinWaitLock.Unlock(m_LockObj);
		}

	private:
		tSpinLock &m_LockObj;
		tSpinWait m_SpinWaitLock;
	private:
		tScopedLock(const tScopedLock&);
		tScopedLock& operator=(const tScopedLock&);
	};
};

#endif //_LOCKFREESCOPEDLOCK__H