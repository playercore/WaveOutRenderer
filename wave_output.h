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
// #define _USE_GIPS_

struct IMediaSample;
struct THeader
{
#ifdef _DEBUG
    REFERENCE_TIME Start;
    REFERENCE_TIME Stop;
#endif
    boost::scoped_array<char> Buff;
    int BuffLen;
    WAVEHDR Hdr;
};
namespace webrtc
{
    class AudioProcessing;
}
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
    void SetAPM(webrtc::AudioProcessing* p) { m_apm = p; }

private:
    THeader* hdrAlloc();
    void hdrFree(boost::shared_ptr<THeader> h);
    void wavhdrFree(WAVEHDR* h);
    bool init();
    static void CALLBACK callback(HWAVEOUT dev, UINT msg, DWORD instance,
                                  DWORD param1, DWORD param2);

    CCritSec m_hdrLock;
    webrtc::AudioProcessing* m_apm;
    int m_deviceId;
    HWAVEOUT m_waveOut;
    bool m_paused;
    long m_volume;
    DWORD m_systemVolume; // 用于存储系统音量，退出时恢复
    long m_balance;
    std::list<boost::shared_ptr<THeader>> m_hdrs;
    std::list<boost::shared_ptr<THeader>> m_hdrsFree;
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