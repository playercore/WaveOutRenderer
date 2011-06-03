#include "wave_output.h"

#include <cassert>

#include <ks.h>
#include <ksmedia.h>

#include "../common/debug_util.h"

using boost::shared_ptr;
using boost::scoped_array;
using std::list;

void CALLBACK CWaveOutput::callback(HWAVEOUT dev, UINT msg, DWORD instance,
                                    DWORD param1, DWORD param2)
{
  WAVEHDR *wHdr;
  switch (msg)
  {
  case WOM_OPEN:
      break;
  case WOM_CLOSE:
      break;
  case WOM_DONE:
      {
          CWaveOutput* output = reinterpret_cast<CWaveOutput*>(instance);
          wHdr = reinterpret_cast<WAVEHDR*>(param1);
          output->wavhdrFree(wHdr);
      }
      break;
  default:
      break;
  }
}
const int ms = 200;
CWaveOutput::CWaveOutput(int deviceID)
    : m_hdrLock()
    , m_deviceId(deviceID)
    , m_waveOut(0)
    , m_paused(false)
    , m_volume(100)
    , m_systemVolume(100) // 用于存储系统音量，退出时恢复
    , m_balance(50)
    , m_hdrs(NULL)
    , m_hdrsFree(NULL)
    , m_format()
    , m_event(NULL)
#ifdef REALTIME
    , m_firstTick(-1) 
    , m_sampleStartTime(0)
    , m_bytePerSec(0)
    , m_span(0)
#endif
{
    m_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    SetEvent(m_event);
    waveOutGetVolume((HWAVEOUT)-1, &m_systemVolume);
}

CWaveOutput::~CWaveOutput()
{
    if (m_event)
    {
        CloseHandle(m_event);
        m_event = NULL;
    }

    Close();
    waveOutSetVolume((HWAVEOUT)-1, m_systemVolume);
}

bool CWaveOutput::init()
{
    if (m_waveOut > 0)
        return true;

    if (!m_format)
        return false;

    if (MMSYSERR_NOERROR != waveOutOpen(&m_waveOut, m_deviceId - 1,
                                        (LPCWAVEFORMATEX)m_format.get(),
                                        (DWORD_PTR)&callback,
                                        (DWORD_PTR)this, CALLBACK_FUNCTION))
    {
        m_waveOut = 0;
        return false;
    }

    return true;

}

bool CWaveOutput::Init(WAVEFORMATEX* format)
{
    Close();
    if (!format)
        return false;

    m_format.reset(new BYTE[sizeof(WAVEFORMATEX) + format->cbSize]);
    memcpy(m_format.get(), format, sizeof(WAVEFORMATEX) + format->cbSize);
#ifdef REALTIME
    m_bytePerSec = format->nSamplesPerSec * format->nChannels * (format->wBitsPerSample >> 3);
#endif // REALTIME
    return init();
}

void CWaveOutput::Close()
{
    if (m_waveOut)
    {
        Flush();
        waveOutClose(m_waveOut);
        m_waveOut = NULL;
    }
}

INT64 CWaveOutput::GetPosition()
{
    if (m_waveOut)
    {
        MMTIME time;
        memset(&time, 0, sizeof(time));
        time.wType = TIME_BYTES;
        if (waveOutGetPosition(m_waveOut, &time, sizeof(time)) == 
            MMSYSERR_NOERROR)
            return time.u.cb;
    }
    return 0;
}

HRESULT CWaveOutput::SetVolume(long volume)
{
    m_volume = volume;
    if (m_volume > 100)
        m_volume = 100;
 
    if (m_waveOut > 0)
        return waveOutSetVolume(m_waveOut, MAKELONG(m_volume * 655.35, 
                                m_volume * 655.35));

    return E_FAIL;
}

bool CWaveOutput::Flush()
{
    waveOutReset(m_waveOut);
    return true;
}

bool CWaveOutput::Play(IMediaSample* sample)
{
    assert(sample && m_waveOut > 0);
    
    BYTE* data = NULL;
    if (FAILED(sample->GetPointer(reinterpret_cast<BYTE**>(&data))))
        return false;    

    int dataLen = sample->GetActualDataLength();
#ifdef REALTIME
    REFERENCE_TIME start;
    REFERENCE_TIME stop;
    sample->GetTime(&start, &stop);

    DWORD tick = GetTickCount();
    if (-1 == m_firstTick)
        m_firstTick = tick;

    m_sampleStartTime += dataLen * 1000 / m_bytePerSec;
    if (tick - m_sampleStartTime > m_firstTick + m_span + ms) //延时超过了阀值
    {
        TRACE(L"延时变大了，丢弃\r\n");
        m_span += 20; 
//         m_firstTick = tick - (tick - m_sampleStartTime - m_firstTick ) / 2;
//         m_sampleStartTime = 0;
        return true;
    }
    else if (tick - m_sampleStartTime < m_firstTick + m_span)
    {
        TRACE(L"延时变小了\r\n");
        m_firstTick = tick;
        m_sampleStartTime = 0;
        m_span = 0;
        //return true;
    }
    TRACE(L"span = %dms\r\n", m_span);

#endif

    HEADER* h = hdrAlloc();
    if (h->buffLen < dataLen)
    {
        h->buff.reset(new char[dataLen]);
        h->buffLen = dataLen;
    }

    memcpy(h->buff.get(), data, dataLen);

    h->hdr.lpData = h->buff.get();
    h->hdr.dwBufferLength = dataLen;
    h->hdr.dwBytesRecorded = h->hdr.dwBufferLength;
    waveOutPrepareHeader(m_waveOut, &h->hdr, sizeof(WAVEHDR));
    waveOutWrite(m_waveOut, &h->hdr, sizeof(WAVEHDR));

#ifdef _DEBUG
    sample->GetTime(&h->start, &h->stop);
    TRACE(L"Play Sample At %lld - %lld, Tick = %dms.\r\n", h->start / 10000, 
          h->stop / 10000, GetTickCount());

#endif
    return true;
}

bool CWaveOutput::SetPause(bool pause)
{
    if (m_waveOut <= 0 && !init())
        return false;

    MMRESULT ret = 0; 
    if (pause)
        ret = waveOutPause(m_waveOut) == 0;
    else if (m_paused)
        ret = waveOutRestart(m_waveOut);

    m_paused = pause;
    return ret == 0;
}

HRESULT CWaveOutput::SetBalance(long byBalance)
{
    if (m_waveOut > 0)
    {
        m_balance = byBalance;
        if (m_balance < 0)
            m_balance = 0;
        else if (m_balance > 100)
            m_balance = 100;

        byte left = static_cast<byte>(m_volume);
        byte right = static_cast<byte>(m_volume);

        if (m_balance < 50)
        {
            // 消减右声道音量
            right = (byte)(m_balance / 50.0 * m_volume);
        }
        else if (m_balance > 50)
        {    
            // 消减左声道音量
            left = (byte)((100 - m_balance) / 50.0 * m_volume);
        }

       return waveOutSetVolume(m_waveOut, MAKELONG(left * 655.35, right * 655.35));
    }

    return E_FAIL;
}

void CWaveOutput::wavhdrFree(WAVEHDR* h)
{
    CAutoLock l(&m_hdrLock);
    list<shared_ptr<HEADER>>::iterator i = m_hdrs.begin();
    for(; i != m_hdrs.end(); ++i)
    {
        if (h == &(*i)->hdr)
        {
#ifdef _DEBUG
            TRACE(L"Free Sample At %lld - %lld, Tick = %dms.\r\n", (*i)->start / 10000, 
                (*i)->stop / 10000, GetTickCount());
            (*i)->start = 0;
            (*i)->stop = 0;
#endif // _DEBUG
            hdrFree(*i);
            return;
        }
    }
    assert(false);
}

HEADER* CWaveOutput::hdrAlloc()
{
//     WaitForSingleObject(m_event, -1);
    CAutoLock l(&m_hdrLock);
    boost::shared_ptr<HEADER> hdr;
    if (!m_hdrsFree.empty())
    {
        hdr = m_hdrsFree.front();
        m_hdrsFree.pop_front();
    }
    else
    {
        hdr.reset(new HEADER);
        hdr->buffLen = 0;
    }
    m_hdrs.push_front(hdr);
    if (m_hdrs.size() > 50)
        ResetEvent(m_event);

    memset(&hdr->hdr, 0, sizeof(WAVEHDR));
    return hdr.get();
}

void CWaveOutput::hdrFree(shared_ptr<HEADER> h)
{
    list<shared_ptr<HEADER>>::iterator i = m_hdrs.begin();
    for( ; i != m_hdrs.end(); ++i)
    {
        if ((*i) == h)
            break;
    }
    if (i == m_hdrs.end())
        return;

    m_hdrsFree.push_front(*i);
    m_hdrs.erase(i);
    if (m_hdrs.size() <= 50)
        SetEvent(m_event);
 }