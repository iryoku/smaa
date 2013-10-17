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

#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <map>
#include <string>
#if TIMER_DIRECTX_9
#include <d3d9.h>
#else
#include <d3d10_1.h>
#endif


class Timer {
    public:
        #if TIMER_DIRECTX_9
        Timer(IDirect3DDevice9 *device, bool enabled=true) :
        #else
        Timer(ID3D10Device *device, bool enabled=true) :
        #endif
            device(device),
            enabled(enabled),
            windowPos(0) { }
        ~Timer() { reset(); }

        void reset() {
            for (auto iter = sections.begin(); iter != sections.end(); iter++)
                if (iter->second != nullptr) delete iter->second;
            sections.clear();
        }

        void start(const std::wstring &name=L"");
        void end(const std::wstring &name=L"");
        void endFrame();

        void setEnabled(bool enabled) { this->enabled = enabled; }
        bool isEnabled() const { return enabled; }

        friend std::wostream &operator<<(std::wostream &out, Timer &timer);

    private:
        static const int WindowSize = 120;

        #if TIMER_DIRECTX_9
        IDirect3DDevice9 *device;
        #else
        ID3D10Device *device;
        #endif

        bool enabled;
        int windowPos;

        class Section {
            public:
                #if TIMER_DIRECTX_9
                Section(IDirect3DDevice9 *device);
                #else
                Section(ID3D10Device *device);
                #endif
                ~Section();

                #if TIMER_DIRECTX_9
                IDirect3DQuery9 *disjointQuery[WindowSize];
                IDirect3DQuery9 *timestampStartQuery[WindowSize];
                IDirect3DQuery9 *timestampEndQuery[WindowSize];
                IDirect3DQuery9 *timestampFrequencyQuery[WindowSize];
                #else
                ID3D10Query *disjointQuery[WindowSize];
                ID3D10Query *timestampStartQuery[WindowSize];
                ID3D10Query *timestampEndQuery[WindowSize];
                #endif

                float time[WindowSize];
                bool finished[WindowSize];
        };
        std::map<std::wstring, Section*> sections;
};

#endif
