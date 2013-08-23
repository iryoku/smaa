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
        static const int WindowSize = 100;

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
