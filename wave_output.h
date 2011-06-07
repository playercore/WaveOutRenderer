#ifndef _WAVE_OUTPUT_H_      
#define _WAVE_OUTPUT_H_

#include <list>

#include <Windows.h>
#include <MMReg.h>
#include <MMSystem.h>
#include <streams.h>
#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>

// #define REALTIME

struct IMediaSample;
struct HEADER
{
#ifdef _DEBUG
    REFERENCE_TIME start;
    REFERENCE_TIME stop;
#endif
    boost::scoped_array<char> buff;
    int buffLen;
    WAVEHDR hdr;
};

class CWaveOutput
{
public:
    CWaveOutput(int deviceID);
    ~CWaveOutput();

    bool Init(WAVEFORMATEX* format);
    void Close();
    INT64 GetPosition();
    HRESULT SetVolume(long volume);
    bool Flush();
    bool Play(IMediaSample* sample);
    bool SetPause(bool pause);
    HRESULT SetBalance(long byBalance);
    long GetVolume() { return m_volume; }
    long GetBalance() { return m_balance; }

private:

    HEADER* hdrAlloc();
    void hdrFree(boost::shared_ptr<HEADER> h);
    void wavhdrFree(WAVEHDR* h);
    bool init();
    static void CALLBACK callback(HWAVEOUT dev, UINT msg, DWORD instance,
                                  DWORD param1, DWORD param2);

    CCritSec m_hdrLock;
    int m_deviceId;
    HWAVEOUT m_waveOut;
    bool m_paused;
    long m_volume;
    DWORD m_systemVolume; // ���ڴ洢ϵͳ�������˳�ʱ�ָ�
    long m_balance;
    std::list<boost::shared_ptr<HEADER>> m_hdrs;
    std::list<boost::shared_ptr<HEADER>> m_hdrsFree;
    boost::scoped_array<BYTE> m_format;
    HANDLE m_event;
#ifdef REALTIME
    DWORD m_firstTick; 
    int   m_sampleStartTime;
    int   m_bytePerSec;
    int   m_span;

#endif
};

#endif