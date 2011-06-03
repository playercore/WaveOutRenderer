#include <vld.h>
#include "wave_out_renderer.h"

#include <ks.h>
#include <ksmedia.h>
#include <boost/math/special_functions/round.hpp>
#include <InitGuid.h>

#include "..\common\debug_util.h"

// {590CD56B-B49A-44a7-960C-3A4006DCDA5F}
DEFINE_GUID(CLSID_WaveOutRenderer,  0x590cd56b, 0xb49a, 0x44a7, 0x96, 0xc, 0x3a,
            0x40, 0x6, 0xdc, 0xda, 0x5f);

static int VolumeToBasicAudio(int volume)
{
    return static_cast<int>(
        boost::math::round((log10((volume + 1) / 10000.0) * 2500)));
}

static int BasicAudioToVolume(int volume)
{
    return static_cast<int>(
        boost::math::round(pow(10, volume / 2500.0) * 10001 - 1));
}

CWaveOutRenderer::CWaveOutRenderer(IUnknown* unk, HRESULT* hr)
    : CBaseRenderer(CLSID_WaveOutRenderer, NAME("WaveOutRenderer"), unk, hr)
    , m_outPut()
{
}

CWaveOutRenderer::~CWaveOutRenderer()
{
}

HRESULT CWaveOutRenderer::CheckInputType(const CMediaType* mtIn)
{
    return CheckMediaType(mtIn);
}

HRESULT CWaveOutRenderer::CheckMediaType(const CMediaType* mt)
{
	if (mt == NULL)
		return E_INVALIDARG;

	WAVEFORMATEX* format = reinterpret_cast<WAVEFORMATEX*>(mt->Format());

	if (format == NULL) 
		return VFW_E_TYPE_NOT_ACCEPTED;

	if ((mt->majortype != MEDIATYPE_Audio) || 
        (mt->formattype != FORMAT_WaveFormatEx)) 
		return VFW_E_TYPE_NOT_ACCEPTED;

    if (format->wFormatTag != WAVE_FORMAT_PCM && 
        format->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
    {
        if  (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE || format->cbSize < 22)
		    return VFW_E_TYPE_NOT_ACCEPTED;

        WAVEFORMATEXTENSIBLE* formatEx = 
            reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format);
        if (formatEx->SubFormat != KSDATAFORMAT_SUBTYPE_PCM)
		    return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}

HRESULT CWaveOutRenderer::DoRenderSample(IMediaSample* mediaSample)
{
    return m_outPut->Play(mediaSample) ? S_OK : E_FAIL;
}

HRESULT CWaveOutRenderer::CompleteConnect(IPin* pin)
{
    if (!pin)
        return E_POINTER;

    HRESULT r = CBaseRenderer::CompleteConnect(pin);
    if (FAILED(r))
        return r;

    if (!m_outPut)
        m_outPut.reset(new CWaveOutput(0));

    CMediaType mt;
    pin->ConnectionMediaType(&mt);
    WAVEFORMATEX* format = reinterpret_cast<WAVEFORMATEX*>(mt.Format());

    return m_outPut->Init(format) ? S_OK : E_FAIL;
}

HRESULT CWaveOutRenderer::EndOfStream()
{
    if (m_outPut)
        m_outPut->Close();

    return CBaseRenderer::EndOfStream();
}

HRESULT CWaveOutRenderer::BeginFlush()
{
    if (m_outPut)
        m_outPut->Flush();

    return CBaseRenderer::BeginFlush();  
}

STDMETHODIMP CWaveOutRenderer::Run(REFERENCE_TIME tStart)
{
	if (m_State == State_Running)
		return NOERROR;
    
    if (!m_outPut)
        return E_FAIL;

    m_outPut->SetPause(false);

	return  CBaseRenderer::Run(tStart);
}

STDMETHODIMP CWaveOutRenderer::Pause()
{
	if (m_State == State_Paused)
		return NOERROR;

    if (!m_outPut)
        return E_FAIL;

    HRESULT hr = CBaseRenderer::Pause();
    if (SUCCEEDED(hr))
        m_outPut->SetPause(true);

	return hr;
};

STDMETHODIMP CWaveOutRenderer::Stop()
{
    if (m_outPut)
        m_outPut->Close();

	return CBaseRenderer::Stop();
};


STDMETHODIMP CWaveOutRenderer::NonDelegatingQueryInterface(REFIID riid, void** o)
{
    if (!o)
        return E_POINTER;

    *o = NULL;
    if (IID_IDispatch == riid)
    {
        IDispatch* d = this;
        d->AddRef();
        *o = d;
        return S_OK;
    }

    if (IID_IBasicAudio == riid)
    {
        IBasicAudio* b = this;
        b->AddRef();
        *o = b;
        return S_OK;
    }

	return CBaseRenderer::NonDelegatingQueryInterface (riid, o);
}

// === IBasicAudio
STDMETHODIMP CWaveOutRenderer::put_Volume(long lVolume)
{
    if (!m_outPut)
        return E_FAIL;
    
	return m_outPut->SetVolume((BasicAudioToVolume(lVolume) + 1) / 100);
}

STDMETHODIMP CWaveOutRenderer::get_Volume(long *plVolume)
{
    if (!m_outPut || !plVolume)
        return E_FAIL;

	*plVolume = VolumeToBasicAudio(m_outPut->GetVolume() * 100) ;
    
	return S_OK; 
}

STDMETHODIMP CWaveOutRenderer::put_Balance(long lBalance)
{
    if (!m_outPut)
        return E_FAIL;

	return m_outPut->SetBalance((lBalance + 10000) / 200);
}

STDMETHODIMP CWaveOutRenderer::get_Balance(long *plBalance)
{
    if (!m_outPut || !plBalance)
        return E_FAIL;

    long b = m_outPut->GetBalance();
	*plBalance = (b - 50) * 200;
	return S_OK;
}

CUnknown* CWaveOutRenderer::CreateInstance(LPUNKNOWN unk, HRESULT* hr)
{
    *hr = S_OK;
    CUnknown* punk = new CWaveOutRenderer(unk, hr);
    if(punk == NULL) 
        *hr = E_OUTOFMEMORY;

    return punk;
}

HRESULT WINAPI CreateWaveOutRenderer(LPUNKNOWN unk, IBaseFilter** out)
{
    if (!out)
        return E_POINTER;

    HRESULT hr = S_OK;
    IBaseFilter* punk = new CWaveOutRenderer(unk, &hr);
    if(punk == NULL) 
        return E_OUTOFMEMORY;

    punk->AddRef();
    *out = punk;
    return hr;
}

//BaseClass 已经帮我们做好了一些导出函数的工作，我们只需要实现CreateInstance和
//定义g_Templates，g_cTemplates就可以导出函数DllCanUnloadNow和DllGetClassObject



const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] =
{
    {&GUID_NULL},
};

const AMOVIESETUP_PIN sudpPins[] =
{
    {
        L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, NULL,
            sizeof(sudPinTypesIn) / sizeof(sudPinTypesIn[0]), sudPinTypesIn
    },
};

const AMOVIESETUP_FILTER sudFilter[] =
{
    {
        &CLSID_WaveOutRenderer, L"WaveOutRenderer", 0x40000002,
            sizeof(sudpPins) / sizeof(sudpPins[0]), sudpPins
    },
};

CFactoryTemplate g_Templates[] =
{
    {
        sudFilter[0].strName, sudFilter[0].clsID, CWaveOutRenderer::CreateInstance, NULL,
            &sudFilter[0]
    }
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);
REGFILTERPINS regfilterPin =
{ 
    L"WaveOut Renderer InputPin", TRUE, FALSE, FALSE, TRUE,
    &CLSID_WaveOutRenderer, L"WaveOut Renderer InputPin", 0, 0

};
REGFILTER2 regfilter2 = {1, 0x40000002, 1, & regfilterPin}; 
STDAPI DllRegisterServer() 
{
    //return AMovieDllRegisterServer2(TRUE);
    HRESULT hr;  
    IFilterMapper2 *pFM2 = NULL;  
    hr = AMovieDllRegisterServer2(TRUE);  
    if (FAILED(hr))  
        return hr;  

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER, 
        IID_IFilterMapper2, (void**)&pFM2);  
    if (FAILED(hr))  
        return hr;  

    hr = pFM2->RegisterFilter(CLSID_WaveOutRenderer, L"WaveOutRenderer", NULL, 
        &CLSID_AudioRendererCategory, L"WaveOutRenderer", &regfilter2);  
    pFM2->Release();  
    return hr;  
}

STDAPI DllUnregisterServer()
{
    //return AMovieDllRegisterServer2(FALSE);
    HRESULT hr;  
    IFilterMapper2 *pFM2 = NULL;  
    hr = AMovieDllRegisterServer2(FALSE);  
    if (FAILED(hr))  
        return hr;  

    hr = CoCreateInstance(CLSID_FilterMapper2, NULL, CLSCTX_INPROC_SERVER,
        IID_IFilterMapper2, (void **)&pFM2);  
    if (FAILED(hr))  
        return hr;  

    hr = pFM2->UnregisterFilter(&CLSID_VideoInputDeviceCategory, 
        L"WaveOutRenderer", CLSID_WaveOutRenderer);  
    pFM2->Release();  
    return hr;  

}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved)

{
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason,
                         lpReserved);
}