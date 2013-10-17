/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
        time[i] = -1.0f;
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
        time[i] = -1.0f;
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
        } else {
            section->time[windowPos] = -1.0f;
        }
    }
}


wostream &operator<<(wostream &out, Timer &timer) {
    if (timer.enabled) {
        for (auto iter = timer.sections.begin(); iter != timer.sections.end(); iter++) {
            const wstring &name = iter->first;
            Timer::Section *section = iter->second;

            float mean = 0.0f;
            int n = 0;
            for (int i = 0; i < Timer::WindowSize; i++) {
                if (section->time[i] >= 0.0f) {
                    mean += section->time[i];
                    n++;
                }
            }
            mean /= float(n);

            out << setprecision(3) << name << L": ";
            if (n > 0)
                out << mean << L"ms" << endl;
            else
                out << "n/a" << endl;
        }
    }
    return out;
}
