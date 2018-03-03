﻿#include "stdafx.h"


CProfiler *CProfiler::_pProfiler = NULL;

///////////////////////////////////////////////////////////////////////
// 생성자
///////////////////////////////////////////////////////////////////////
CProfiler::CProfiler()
{
	///////////////////////////////////////////////////////////////////
	// 타이머 값 얻기
	///////////////////////////////////////////////////////////////////
	QueryPerformanceFrequency(&_IFrequency);
	_dMicroFrequency = (double)_IFrequency.QuadPart / 1000000;

	InitializeSRWLock(&_srwProfilerLock);

	for (int iCnt = 0; iCnt < eMAX_THREAD_SAMPlE; iCnt++)
		_stProfileThread[iCnt].lThreadID = -1;
	
	///////////////////////////////////////////////////////////////////
	// TLS Alloc
	///////////////////////////////////////////////////////////////////
	_dwTlsIndex = TlsAlloc();
	if (TLS_OUT_OF_INDEXES == _dwTlsIndex)
		_exit(-1);
}

///////////////////////////////////////////////////////////////////////
// 소멸자
///////////////////////////////////////////////////////////////////////
CProfiler::~CProfiler()
{

}



///////////////////////////////////////////////////////////////////////
// 프로파일링 시작
///////////////////////////////////////////////////////////////////////
bool			CProfiler::ProfileBegin(WCHAR *pwSampleName)
{
	LARGE_INTEGER	liStartTime;
	st_SAMPLE		*pSample = nullptr;

	///////////////////////////////////////////////////////////////////
	// 샘플 얻기
	///////////////////////////////////////////////////////////////////
	GetSample(pwSampleName, &pSample);
	if (nullptr == pSample)
		return false;


	///////////////////////////////////////////////////////////////////
	// 시간 측정 시작
	///////////////////////////////////////////////////////////////////
	QueryPerformanceCounter(&liStartTime);

	///////////////////////////////////////////////////////////////////
	// Begin이 중복으로 열렸을 때
	///////////////////////////////////////////////////////////////////
	if (0 != pSample->liStartTime.QuadPart)
		return false;

	pSample->liStartTime = liStartTime;
}

///////////////////////////////////////////////////////////////////////
// 프로파일링 끝
///////////////////////////////////////////////////////////////////////
bool			CProfiler::ProfileEnd(WCHAR *pwSampleName)
{
	LARGE_INTEGER	liEndTime, liSampleTime;
	st_SAMPLE		*pSample = nullptr;
	DWORD			dwTlsIndex = -1;

	///////////////////////////////////////////////////////////////////
	// 카운터 측정
	///////////////////////////////////////////////////////////////////
	QueryPerformanceCounter(&liEndTime);

	///////////////////////////////////////////////////////////////////
	// 샘플 얻기
	///////////////////////////////////////////////////////////////////
	GetSample(pwSampleName, &pSample);
	if (nullptr == pSample)
		return false;


	///////////////////////////////////////////////////////////////////
	// 시간 구하기
	///////////////////////////////////////////////////////////////////
	liSampleTime.QuadPart =
		liEndTime.QuadPart - pSample->liStartTime.QuadPart;
		

	///////////////////////////////////////////////////////////////////
	// 최대 최소 시간
	///////////////////////////////////////////////////////////////////
	if (pSample->dMaxTime[1] < liSampleTime.QuadPart)
	{
		pSample->dMaxTime[0] = pSample->dMaxTime[1];
		pSample->dMaxTime[1] = (double)liSampleTime.QuadPart / _dMicroFrequency;
	}

	if (pSample->dMinTime[1] > liSampleTime.QuadPart)
	{
		pSample->dMinTime[0] = pSample->dMinTime[1];
		pSample->dMinTime[1] = (double)liSampleTime.QuadPart / _dMicroFrequency;
	}

	///////////////////////////////////////////////////////////////////
	// 누적시간 저장
	///////////////////////////////////////////////////////////////////
	pSample->dTotalSampleTime +=
		(double)liSampleTime.QuadPart / _dMicroFrequency;

	///////////////////////////////////////////////////////////////////
	// 콜 횟수 증가
	///////////////////////////////////////////////////////////////////
	pSample->iCallCount++;

	return true;
}



///////////////////////////////////////////////////////////////////////
// 샘플 얻기
///////////////////////////////////////////////////////////////////////
bool			CProfiler::GetSample(WCHAR *pwSampleName, st_SAMPLE **pOutSample)
{
	st_SAMPLE *pSample;

	///////////////////////////////////////////////////////////////////
	// 해당 쓰레드의 TLS Index를 얻음
	// 셋팅되어 있지 않다면 샘플값을 세팅함
	///////////////////////////////////////////////////////////////////
	pSample = (st_SAMPLE *)TlsGetValue(_dwTlsIndex);
	if (nullptr == pSample)
	{
		pSample = new st_SAMPLE[eMAX_SAMPLE];

		///////////////////////////////////////////////////////////////
		// 샘플 배열 초기화
		///////////////////////////////////////////////////////////////
		for (int iCnt = 0; iCnt < eMAX_SAMPLE; iCnt++)
			memset(&pSample[iCnt], 0, sizeof(st_SAMPLE));

		///////////////////////////////////////////////////////////////
		// TLS에 값 셋팅
		///////////////////////////////////////////////////////////////
		if (false == TlsSetValue(_dwTlsIndex, (LPVOID)pSample))
			return false;

		///////////////////////////////////////////////////////////////
		// 출력을 위해 ThreadSample 배열에도 저장
		///////////////////////////////////////////////////////////////
		_stProfileThread->pSample = pSample;
		_stProfileThread->lThreadID = GetCurrentThreadId();
	}

	///////////////////////////////////////////////////////////////////
	// 샘플 찾기
	///////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < eMAX_SAMPLE; iCnt++)
	{
		if ((pSample[iCnt].bUseFlag) && (0 == wcscmp(pSample[iCnt].wName, pwSampleName)))
		{
			*pOutSample = &pSample[iCnt];
			break;;
		}

		/////////////////////////////////////////////////////////
		// 해당 쓰레드 안에 샘플이 없을 때
		/////////////////////////////////////////////////////////
		if (0 == wcscmp(pSample[iCnt].wName, L""))
		{
			StringCchPrintf(pSample[iCnt].wName,
				wcslen(pwSampleName)+1,
				L"%s",
				pwSampleName);
			
			pSample[iCnt].bUseFlag = true;

			pSample[iCnt].liStartTime.QuadPart = 0;
			pSample[iCnt].dTotalSampleTime = 0;

			pSample[iCnt].dMaxTime[1] = DBL_MIN;
			pSample[iCnt].dMinTime[1] = DBL_MAX;

			pSample[iCnt].iCallCount = 0;

			*pOutSample = &pSample[iCnt];

			break;
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////
// 프로파일링한 데이터 저장
///////////////////////////////////////////////////////////////////////
bool			CProfiler::SaveProfile()
{
	SYSTEMTIME		stNowTime;
	WCHAR			fileName[256];
	DWORD			dwBytesWritten;
	WCHAR			wBuffer[200];
	HANDLE			hFile;

	///////////////////////////////////////////////////////////////////
	// 파일 생성
	///////////////////////////////////////////////////////////////////
	GetLocalTime(&stNowTime);
	StringCchPrintf(fileName,
		sizeof(fileName),
		L"%4d-%02d-%02d %02d.%02d ProFiling Data.txt",
		stNowTime.wYear,
		stNowTime.wMonth,
		stNowTime.wDay,
		stNowTime.wHour,
		stNowTime.wMinute
		);

	///////////////////////////////////////////////////////////////////
	// 파일 만들기
	///////////////////////////////////////////////////////////////////
	hFile = ::CreateFile(fileName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	::WriteFile(hFile,
		L"﻿ ThreadID |                Name  |           Average  |            Min   |            Max   |          Call |\r\n",
		112 * sizeof(WCHAR), &dwBytesWritten, NULL);
	::WriteFile(hFile,
		L"-------------------------------------------------------------------------------------------------------------\r\n",
		112 * sizeof(WCHAR), &dwBytesWritten, NULL);

	for (int iThCnt = 0; eMAX_THREAD_SAMPlE; iThCnt++)
	{
		if (-1 == _stProfileThread[iThCnt].lThreadID)
			break;

		for (int iSampleCnt = 0; iSampleCnt < eMAX_THREAD_SAMPlE; iSampleCnt++)
		{
			if (false == _stProfileThread[iThCnt].pSample[iSampleCnt].bUseFlag)
				break;
			
			StringCchPrintf(wBuffer,
				sizeof(wBuffer),
				L" %7d | %20s | %16.4f㎲ | %14.4f㎲ | %14.4f㎲ | %13d |\r\n",
				_stProfileThread[iThCnt].lThreadID,
				_stProfileThread[iThCnt].pSample[iSampleCnt].wName,
				_stProfileThread[iThCnt].pSample[iSampleCnt].dTotalSampleTime 
				/ _stProfileThread[iThCnt].pSample[iSampleCnt].iCallCount,
				_stProfileThread[iThCnt].pSample[iSampleCnt].dMinTime[1],
				_stProfileThread[iThCnt].pSample[iSampleCnt].dMaxTime[1],
				_stProfileThread[iThCnt].pSample[iSampleCnt].iCallCount
				);
			::WriteFile(hFile, wBuffer, wcslen(wBuffer) * sizeof(WCHAR), &dwBytesWritten, NULL);
		}
		::WriteFile(hFile,
			L"------------------------------------------------------------------------------------------------------------\r\n",
			111 * sizeof(WCHAR), &dwBytesWritten, NULL);
	}

	return true;
}

