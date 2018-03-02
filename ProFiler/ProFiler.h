#ifndef __PROFILER__H__
#define __PROFILER__H__

/*-----------------------------------------------------------------------*/
// ��Ƽ ������ �������Ϸ�
/*-----------------------------------------------------------------------*/
class CProfiler
{
private :
	/*-------------------------------------------------------------------*/
	// �������Ϸ� �⺻ ����
	//
	// eMAX_THREAD_NAME		: ������ �̸� �ִ� ����
	// eMAX_SAMPLE			: �� �������� �ִ� ���� ����
	// eMAX_SAMPLE_THREAD	: �������� �ִ� ����
	/*-------------------------------------------------------------------*/
	enum e_CONFIG_PROFILER
	{
		eMAX_SAMPLE_NAME		= 64,
		eMAX_SAMPLE				= 100,
		eMAX_THREAD_SAMPlE		= 100
	};

	///////////////////////////////////////////////////////////////////////
	// ���� ������ ���� ����
	///////////////////////////////////////////////////////////////////////
	struct st_SAMPLE
	{
		WCHAR			wName[eMAX_SAMPLE_NAME];

		LARGE_INTEGER	liStartTime;

		double			dTotalSampleTime;

		double			dMaxTime[2];
		double			dMinTime[2];

		int				iCallCount;
	};

	///////////////////////////////////////////////////////////////////////
	// ������ ������ ������ ����
	//
	// lThreadID : ������ ID
	// stSample	 : ���� �迭
	///////////////////////////////////////////////////////////////////////
	struct st_THREAD_SAMPLE
	{
		DWORD			lThreadID;
		st_SAMPLE		stSample[eMAX_SAMPLE];
	};



public :
	///////////////////////////////////////////////////////////////////////
	// ������
	///////////////////////////////////////////////////////////////////////
					CProfiler();

	///////////////////////////////////////////////////////////////////////
	// �Ҹ���
	///////////////////////////////////////////////////////////////////////
	virtual			~CProfiler();

	

	///////////////////////////////////////////////////////////////////////
	// �������ϸ� ����
	///////////////////////////////////////////////////////////////////////
	bool			ProfileBegin(WCHAR *pwSampleName);

	///////////////////////////////////////////////////////////////////////
	// �������ϸ� ��
	///////////////////////////////////////////////////////////////////////
	bool			ProfileEnd(WCHAR *pwSampleName);



	///////////////////////////////////////////////////////////////////////
	// �������ϸ��� ������ ����
	///////////////////////////////////////////////////////////////////////
	bool			SaveProfile();



private :
	///////////////////////////////////////////////////////////////////////
	// ���� ���
	///////////////////////////////////////////////////////////////////////
	bool			GetSample(WCHAR *pwSampleName, st_SAMPLE **pOutSample);


	///////////////////////////////////////////////////////////////////////
	// ������ ������ �ε��� ��ȯ
	///////////////////////////////////////////////////////////////////////
	int				GetThreadSampleIndex();



private :
	///////////////////////////////////////////////////////////////////////
	// �������ϸ��� ��� ����
	///////////////////////////////////////////////////////////////////////
	st_THREAD_SAMPLE		_stProfileThread[eMAX_THREAD_SAMPlE];


	///////////////////////////////////////////////////////////////////////
	// ���ػ� Ÿ�̸� ��(CPU�� Ŭ�� ��)
	///////////////////////////////////////////////////////////////////////
	LARGE_INTEGER			_IFrequency;
	double					_dMicroFrequency;

	///////////////////////////////////////////////////////////////////////
	// �������Ϸ� ����ȭ ��ü
	///////////////////////////////////////////////////////////////////////
	SRWLOCK					_srwProfilerLock;
};

#endif