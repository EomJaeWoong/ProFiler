#include "stdafx.h"

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

	///////////////////////////////////////////////////////////////////
	// 샘플 쓰레드 초기화
	///////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < eMAX_THREAD_SAMPlE; iCnt++)
		_stProfileThread[iCnt].lThreadID = -1;
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
	// 해당 샘플 얻고 카운터 측정
	///////////////////////////////////////////////////////////////////
	GetSample(pwSampleName, &pSample);
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

	///////////////////////////////////////////////////////////////////
	// 카운터 측정
	///////////////////////////////////////////////////////////////////
	QueryPerformanceCounter(&liEndTime);

	GetSample(pwSampleName, &pSample);

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
	///////////////////////////////////////////////////////////////////
	// 쓰레드 ID 얻기
	///////////////////////////////////////////////////////////////////
	int iIndex = GetThreadSampleIndex();

	///////////////////////////////////////////////////////////////////
	// 등록된 쓰레드가 아닐 때
	///////////////////////////////////////////////////////////////////
	if (-1 == iIndex)
	{
		///////////////////////////////////////////////////////////////
		// 쓰레드 등록이 안되어있을 경우 등록하기
		///////////////////////////////////////////////////////////////
		AcquireSRWLockExclusive(&_srwProfilerLock);
		for (iIndex = 0; iIndex < eMAX_THREAD_SAMPlE; iIndex++)
		{
			if (-1 == _stProfileThread[iIndex].lThreadID)
			{
				_stProfileThread[iIndex].lThreadID = GetCurrentThreadId();
				break;
			}
		}
		ReleaseSRWLockExclusive(&_srwProfilerLock);
	}

	///////////////////////////////////////////////////////////////////
	// 자리가 없을 경우 리턴
	///////////////////////////////////////////////////////////////////
	if (eMAX_THREAD_SAMPlE <= iIndex)
		return false;


	///////////////////////////////////////////////////////////////////
	// 샘플 찾기
	///////////////////////////////////////////////////////////////////
	for (int iCnt = 0; iCnt < eMAX_SAMPLE; iCnt++)
	{
		if (0 == wcscmp(_stProfileThread[iIndex].stSample[iCnt].wName, pwSampleName))
		{
			*pOutSample = &_stProfileThread[iIndex].stSample[iCnt];
			break;;
		}
			
		/////////////////////////////////////////////////////////
		// 해당 쓰레드 안에 샘플이 없을 때
		/////////////////////////////////////////////////////////
		if (0 == wcscmp(_stProfileThread[iIndex].stSample[iCnt].wName, L""))
		{
			StringCchPrintf(_stProfileThread[iIndex].stSample[iCnt].wName,
				wcslen(pwSampleName)+1,
				L"%s",
				pwSampleName);
			
			_stProfileThread[iIndex].stSample[iCnt].liStartTime.QuadPart = 0;
			_stProfileThread[iIndex].stSample[iCnt].dTotalSampleTime = 0;

			_stProfileThread[iIndex].stSample[iCnt].dMaxTime[1] = DBL_MIN;
			_stProfileThread[iIndex].stSample[iCnt].dMinTime[1] = DBL_MAX;

			_stProfileThread[iIndex].stSample[iCnt].iCallCount = 0;

			*pOutSample = &_stProfileThread[iIndex].stSample[iCnt];

			break;
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////
// 쓰레드 샘플의 인덱스 반환
///////////////////////////////////////////////////////////////////////
int				CProfiler::GetThreadSampleIndex()
{
	int iIndex;

	for (iIndex = 0; iIndex < eMAX_THREAD_SAMPlE; iIndex++)
	{
		///////////////////////////////////////////////////////////////
		// 해당 스레드 찾음
		///////////////////////////////////////////////////////////////
		if (GetCurrentThreadId() == _stProfileThread[iIndex].lThreadID)

			return iIndex;
	}

	return -1;
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
		L"﻿  ThreadID |           Name  |     Average  |        Min   |        Max   |      Call |\r\n",
		90 * sizeof(WCHAR), &dwBytesWritten, NULL);
	::WriteFile(hFile,
		L"-------------------------------------------------------------------------------------- \r\n",
		90 * sizeof(WCHAR), &dwBytesWritten, NULL);

	for (int iThCnt = 0; eMAX_THREAD_SAMPlE; iThCnt++)
	{
		if (-1 == _stProfileThread[iThCnt].lThreadID)
			break;

		for (int iSampleCnt = 0; iSampleCnt < eMAX_THREAD_SAMPlE; iSampleCnt++)
		{
			if (0 == wcscmp(_stProfileThread[iThCnt].stSample[iSampleCnt].wName, L""))
				break;
			
			StringCchPrintf(wBuffer,
				sizeof(wBuffer),
				L" %8d | %15s | %10.4f㎲ | %10.4f㎲ | %10.4f㎲ | %9d |\r\n",
				_stProfileThread[iThCnt].lThreadID,
				_stProfileThread[iThCnt].stSample[iSampleCnt].wName,
				_stProfileThread[iThCnt].stSample[iSampleCnt].dTotalSampleTime 
				/ _stProfileThread[iThCnt].stSample[iSampleCnt].iCallCount,
				_stProfileThread[iThCnt].stSample[iSampleCnt].dMinTime[1],
				_stProfileThread[iThCnt].stSample[iSampleCnt].dMaxTime[1],
				_stProfileThread[iThCnt].stSample[iSampleCnt].iCallCount
				);
			::WriteFile(hFile, wBuffer, wcslen(wBuffer) * sizeof(WCHAR), &dwBytesWritten, NULL);
		}
		::WriteFile(hFile,
			L"-------------------------------------------------------------------------------------- ",
			87 * sizeof(WCHAR), &dwBytesWritten, NULL);
	}

	return true;
}

