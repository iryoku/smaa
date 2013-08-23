/**
 * Copyright (C) 2010 Jorge Jimenez (jorge@iryoku.com). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are 
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the copyright holders.
 */

#include <iomanip>
#include <d3dx9.h>
#include <dxerr.h>
#include "Timer.h"
using namespace std;


#pragma region Useful Macros from DXUT (copy-pasted here as we prefer this to be as self-contained as possible)
#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x) { hr = (x); if (FAILED(hr)) { DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if (FAILED(hr)) { return DXTrace(__FILE__, (DWORD)__LINE__, hr, L#x, true); } }
#endif
#else
#ifndef V
#define V(x) { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x) { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = nullptr; } }
#endif
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = nullptr; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif
#pragma endregion


#ifdef TIMER_DIRECTX_9
Timer::Section::Section(IDirect3DDevice9 *device) {
    HRESULT hr;

    for (int i = 0; i < WindowSize; i++) {
        V(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPDISJOINT, &disjointQuery[i]));
        V(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &timestampStartQuery[i]));
        V(device->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &timestampEndQuery[i]));
        V(device->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &timestampFrequencyQuery[i]));

        finished[i] = false;
        time[i] = 0.0f;
    }
}
#else
Timer::Section::Section(ID3D10Device *device) {
    HRESULT hr;

    for (int i = 0; i < WindowSize; i++) {
        D3D10_QUERY_DESC desc;
        desc.Query = D3D10_QUERY_TIMESTAMP_DISJOINT;
        desc.MiscFlags = 0;
        V(device->CreateQuery(&desc, &disjointQuery[i]));

        desc.Query = D3D10_QUERY_TIMESTAMP;
        V(device->CreateQuery(&desc, &timestampStartQuery[i]));
        V(device->CreateQuery(&desc, &timestampEndQuery[i]));

        finished[i] = false;
        time[i] = 0.0f;
    }
}
#endif


Timer::Section::~Section() {
    for (int i = 0; i < WindowSize; i++) {
        SAFE_RELEASE(disjointQuery[i]);
        SAFE_RELEASE(timestampStartQuery[i]);
        SAFE_RELEASE(timestampEndQuery[i]);
        #ifdef TIMER_DIRECTX_9
        SAFE_RELEASE(timestampFrequencyQuery[i]);
        #endif
    }
}


void Timer::start(const wstring &name) {
    if (enabled) {
        if (sections.find(name) == sections.end())
            sections[name] = new Section(device);
        Section *section = sections[name];
        #ifdef TIMER_DIRECTX_9
        section->disjointQuery[windowPos]->Issue(D3DISSUE_BEGIN);
        section->timestampStartQuery[windowPos]->Issue(D3DISSUE_END);
        #else
        section->disjointQuery[windowPos]->Begin();
        section->timestampStartQuery[windowPos]->End();
        #endif
    }
}


void Timer::end(const wstring &name) {
    if (enabled) {
        Section *section = sections[name];
        #ifdef TIMER_DIRECTX_9
        section->timestampEndQuery[windowPos]->Issue(D3DISSUE_END);
        section->timestampFrequencyQuery[windowPos]->Issue(D3DISSUE_END);
        section->disjointQuery[windowPos]->Issue(D3DISSUE_END);
        #else
        section->timestampEndQuery[windowPos]->End();
        section->disjointQuery[windowPos]->End();
        #endif
        section->finished[windowPos] = true;
    }
}


void Timer::endFrame() {
    windowPos = (windowPos + 1) % WindowSize;

    for (auto iter = sections.begin(); iter != sections.end(); iter++) {
        Timer::Section *section = iter->second;

        if (!section->finished[windowPos])
            continue;

        UINT64 startTime;
        while (section->timestampStartQuery[windowPos]->GetData(&startTime, sizeof(startTime), 0) != S_OK);

        UINT64 endTime;
        while (section->timestampEndQuery[windowPos]->GetData(&endTime, sizeof(endTime), 0) != S_OK);

        BOOL disjoint;
        float frequency;
        #ifdef TIMER_DIRECTX_9
        UINT64 temp;
        while (section->timestampFrequencyQuery[windowPos]->GetData(&temp, sizeof(temp), 0) != S_OK);
        frequency = float(temp);

        while (section->disjointQuery[windowPos]->GetData(&disjoint, sizeof(disjoint), 0) != S_OK);
        #else
        D3D10_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while (section->disjointQuery[windowPos]->GetData(&disjointData, sizeof(disjointData), 0) != S_OK);
        disjoint = disjointData.Disjoint;
        frequency = float(disjointData.Frequency);
        #endif

        if (!disjoint) {
            UINT64 delta = endTime - startTime;
            section->time[windowPos] = 1000.0f * (delta / frequency);
        }
    }
}


wostream &operator<<(wostream &out, Timer &timer) {
    if (timer.enabled) {
        for (auto iter = timer.sections.begin(); iter != timer.sections.end(); iter++) {
            const wstring &name = iter->first;
            Timer::Section *section = iter->second;

            float mean = 0.0f;
            for (int i = 0; i < Timer::WindowSize; i++)
                mean += section->time[i];
            mean /= float(Timer::WindowSize);

            out << setprecision(3) << name << L": " << mean << L"ms" << endl;
        }
    }
    return out;
}
