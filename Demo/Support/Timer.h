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
#include <string>
#include <map>
#include <vector>


#ifdef TIMER_DIRECTX_9
#include <d3d9.h>
#else
#include <d3d10.h>
#endif
#include <dxerr.h>


class Timer {
    public:
        #ifdef TIMER_DIRECTX_9
        Timer(IDirect3DDevice9 *device);
        #else
        Timer(ID3D10Device *device);
        #endif
        ~Timer();

        void reset() { sections.clear(); }
        void start();
        float clock(const std::wstring &msg=L"");
        float accumulated() const { return accum; }

        void sleep(float ms);

        void setEnabled(bool enabled) { this->enabled = enabled; }
        bool isEnabled() const { return enabled; }

        void setFlushEnabled(bool flushEnabled) { this->flushEnabled = flushEnabled; }
        bool isFlushEnabled() const { return flushEnabled; }

        void setWindowSize(int windowSize) { this->windowSize = windowSize; }
        int getWindowSize() const { return windowSize; }

        void setRepetitionsCount(int repetitionCount) { this->repetitionCount = repetitionCount; }
        int getRepetitionsCount() const { return repetitionCount; }

        float getSection(const std::wstring &name) { return 1000.0f * sections[name].mean / repetitionCount; }

        friend std::wostream &operator<<(std::wostream &out, const Timer &timer);

    private:
        float mean(const std::wstring &msg, float t);
        void flush();

        #ifdef TIMER_DIRECTX_9
        IDirect3DQuery9 *event;
        #else
        ID3D10Query *event;
        #endif

        __int64 t0;
        float accum;

        bool enabled;
        bool flushEnabled;
        int windowSize;
        int repetitionCount;

        class Section {
            public:
                Section() : mean(0.0), pos(0), completed(0.0f) {}
                std::vector<std::pair<float, bool> > buffer;
                float mean;
                int pos;
                float completed;
        };
        std::map<std::wstring, Section> sections;
};

#endif
