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
	ProfileInit();

	char chControlKey;

	for (int iCnt = 0; iCnt < 20000; iCnt++)
	{
		PRO_BEGIN(L"Sleep");
		test();
		PRO_END(L"Sleep");
	}

	SaveProfile();

	return 0;
}

