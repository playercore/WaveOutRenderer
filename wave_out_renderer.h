#ifndef _WAVE_OUT_RENDERER_H_
#define _WAVE_OUT_RENDERER_H_

#include <streams.h>
#include <boost/shared_ptr.hpp>

#include "wave_output.h"

class CWaveOutRenderer : public CBaseRenderer, public IBasicAudio
{
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN unk, HRESULT* hr);
    CWaveOutRenderer(IUnknown* unk, HRESULT* hr);
    ~CWaveOutRenderer();
    
    HRESULT CheckInputType(const CMediaType* mtIn);
	virtual HRESULT CheckMediaType(const CMediaType* mt);
	virtual HRESULT DoRenderSample(IMediaSample* sample);
    virtual HRESULT BeginFlush();
    virtual HRESULT WaitForRenderTime() { return S_OK; };
    HRESULT CompleteConnect(IPin* pin);

	HRESULT EndOfStream();
	DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) NonDelegatingAddRef()
    {
        ULONG n = CBaseRenderer::NonDelegatingAddRef();
        return n;
    }
	// === IMediaFilter
	STDMETHOD(Run)(REFERENCE_TIME tStart);
	STDMETHOD(Stop)();
	STDMETHOD(Pause)();

	// === IDispatch (pour IBasicAudio)
	STDMETHOD(GetTypeInfoCount)(UINT * pctinfo)
    {
        return E_NOTIMPL;
    }

	STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo ** pptinfo)
    {
        return E_NOTIMPL;
    }

	STDMETHOD(GetIDsOfNames)(REFIID riid, OLECHAR** rgszNames, UINT cNames,
                             LCID lcid, DISPID* rgdispid)
    {
        return E_NOTIMPL;
    }

	STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS* pdispparams, VARIANT* pvarResult,
                      EXCEPINFO* pexcepinfo, UINT* puArgErr)
    {
        return E_NOTIMPL;
    }

	// === IBasicAudio
	STDMETHOD(put_Volume)(long lVolume);
	STDMETHOD(get_Volume)(long *plVolume);
	STDMETHOD(put_Balance)(long lBalance);
	STDMETHOD(get_Balance)(long *plBalance);

private:
    boost::shared_ptr<CWaveOutput> m_outPut;
};
#endif
