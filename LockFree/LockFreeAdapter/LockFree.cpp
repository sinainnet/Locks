// LockFree.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ScopedLock.h"
#include "LockFreeStack.h"
using namespace LockFree;

// Example usage
class tMyContainer
{
public:
	tMyContainer()
	{
		temp = new int(10);
	}
	void ChangeValue(int &val)
	{
		tScopedLock MySpinLock(m_LockObj);
		*temp = val;
	}

private:
	int *temp;	
	tSpinLock m_LockObj;
};


int _tmain(int argc, _TCHAR* argv[])
{	
	int p = 100;
	tMyContainer obj;
	obj.ChangeValue(p);
	return 0;
}

