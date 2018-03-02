// Profiler.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <conio.h>

void test()
{
	int *p = new int;

	*p = 3;

	delete p;
}


int _tmain(int argc, _TCHAR* argv[])
{
	CProfiler *pProfile = new CProfiler();
	char chControlKey;

	for (int iCnt = 0; iCnt < 20000; iCnt++)
	{
		pProfile->ProfileBegin(L"SleepSleepSleepSleep");
		test();
		pProfile->ProfileEnd(L"SleepSleepSleepSleep");


	}

	pProfile->SaveProfile();

	return 0;
}

