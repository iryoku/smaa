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


#include <sstream>
#include <iomanip>

#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"

#include "MLAA.h"
#include "Timer.h"

using namespace std;


CDXUTDialogResourceManager dialogResourceManager;
CDXUTDialog hud;

ID3DXFont *font = NULL;

Timer *timer = NULL;
MLAA *mlaa = NULL;
IDirect3DSurface9 *backbufferSurface = NULL;
IDirect3DTexture9 *finalbufferColorTexture = NULL;
IDirect3DSurface9 *finalbufferColorSurface = NULL;
IDirect3DTexture9 *finalbufferDepthTexture = NULL;
IDirect3DSurface9 *finalbufferDepthSurface = NULL;
IDirect3DTexture9 *colorTexture = NULL;
IDirect3DTexture9 *depthTexture = NULL;
ID3DXSprite *sprite = NULL;
CDXUTTextHelper *txtHelper = NULL;

bool showHud = true;


#define IDC_TOGGLEFULLSCREEN    1
#define IDC_PROFILE             2
#define IDC_DETECTIONMODE       3


bool CALLBACK isDeviceAcceptable(D3DCAPS9 *caps, D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, bool windowed, void *userContext) {
    if (caps->PixelShaderVersion < D3DPS_VERSION(3, 0) || caps->VertexShaderVersion < D3DVS_VERSION(3, 0))
        return false;
    else
        return true;
}


HRESULT CALLBACK onCreateDevice(IDirect3DDevice9 *device, const D3DSURFACE_DESC *desc, void *userContext) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D9CreateDevice(device));

    V_RETURN(D3DXCreateFont(device, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial", &font));

    return S_OK;
}


void CALLBACK onDestroyDevice(void* pUserContext) {
    dialogResourceManager.OnD3D9DestroyDevice();

    SAFE_RELEASE(font);
}


HRESULT CALLBACK onResetDevice(IDirect3DDevice9 *device, const D3DSURFACE_DESC *desc, void *userContext) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D9ResetDevice());

    if (font) V_RETURN(font->OnResetDevice());
    
    timer = new Timer(device);
    timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
    timer->setRepetitionsCount(100);

    mlaa = new MLAA(device, desc->Width, desc->Height, 8);

    V(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbufferSurface));

    V(device->CreateTexture(desc->Width, desc->Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &finalbufferColorTexture, NULL));
    V(finalbufferColorTexture->GetSurfaceLevel(0, &finalbufferColorSurface));

    V(device->CreateTexture(desc->Width, desc->Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &finalbufferDepthTexture, NULL));
    V(finalbufferDepthTexture->GetSurfaceLevel(0, &finalbufferDepthSurface));

    D3DXIMAGE_INFO info;
    V(D3DXGetImageInfoFromResource(NULL, L"Unigine01.png", &info));
    V(D3DXCreateTextureFromResourceEx(device, NULL, L"Unigine01.png", info.Width, info.Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, NULL, &colorTexture));
    V(D3DXGetImageInfoFromResource(NULL, L"Unigine01.dds", &info));
    V(D3DXCreateTextureFromResourceEx(device, NULL, L"Unigine01.dds", info.Width, info.Height, 1, 0, D3DFMT_R32F, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, NULL, &depthTexture));

    V_RETURN(D3DXCreateSprite(device, &sprite));
    txtHelper = new CDXUTTextHelper(font, sprite, NULL, NULL, 15);

    hud.SetLocation(desc->Width - 170, 0);
    hud.SetSize(170, 170);

    return S_OK;
}


void CALLBACK onLostDevice(void *userContext) {
    dialogResourceManager.OnD3D9LostDevice();
    if (font) font->OnLostDevice();
    SAFE_DELETE(timer);
    SAFE_DELETE(mlaa);
    SAFE_RELEASE(backbufferSurface);
    SAFE_RELEASE(finalbufferColorTexture);
    SAFE_RELEASE(finalbufferColorSurface);
    SAFE_RELEASE(finalbufferDepthTexture);
    SAFE_RELEASE(finalbufferDepthSurface);
    SAFE_RELEASE(colorTexture);
    SAFE_RELEASE(depthTexture);
    SAFE_RELEASE(sprite);
    SAFE_DELETE(txtHelper);
}


void drawHud() {
    txtHelper->Begin();

    txtHelper->SetInsertionPos(2, 0);
    txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
    txtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
    txtHelper->DrawTextLine(DXUTGetDeviceStats());

    if (timer->isEnabled()) {
        wstringstream s;
        s << setprecision(5) << std::fixed;
        s << *timer;
        txtHelper->DrawTextLine(s.str().c_str());
    }

    txtHelper->End();
}


void mainPass(IDirect3DDevice9 *device) {
    HRESULT hr;

    // A dummy copy over here.
    IDirect3DSurface9 *colorSurface = NULL;
    V(colorTexture->GetSurfaceLevel(0, &colorSurface));
    D3DSURFACE_DESC desc;
    colorSurface->GetDesc(&desc);
    const D3DSURFACE_DESC *backbufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    RECT rect = {0, 0, min(desc.Width, backbufferDesc->Width), min(desc.Height, backbufferDesc->Height)};
    V(device->StretchRect(colorSurface, &rect, finalbufferColorSurface, &rect, D3DTEXF_POINT));
    SAFE_RELEASE(colorSurface);

    // And another one over here.
    IDirect3DSurface9 *depthSurface = NULL;
    V(depthTexture->GetSurfaceLevel(0, &depthSurface));
    V(device->StretchRect(depthSurface, &rect, finalbufferDepthSurface, &rect, D3DTEXF_POINT));
    SAFE_RELEASE(depthSurface);
}


void CALLBACK onFrameRender(IDirect3DDevice9 *device, double time, float elapsedTime, void *userContext) {
    HRESULT hr;

    // IMPORTANT: stencil must be cleared to zero before executing 'mlaa->go'
    V(device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0));

    V(device->BeginScene());
        // This emulates main pass.
        mainPass(device);

        // Run MLAA
        bool useDepth = int(hud.GetComboBox(IDC_DETECTIONMODE)->GetSelectedData()) == 1;
        int n = hud.GetCheckBox(IDC_PROFILE)->GetChecked()? timer->getRepetitionsCount() : 1;

        timer->start();
        for (int i = 0; i < n; i++) { // This loop is just for profiling.
            if (useDepth)
                mlaa->go(finalbufferColorTexture, backbufferSurface, finalbufferDepthTexture);
            else
                mlaa->go(finalbufferColorTexture, backbufferSurface);
        }
        timer->clock(L"MLAA");

        // Draw the HUD
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats"); // These events are to help PIX identify what the code is doing
        if (showHud) {
            drawHud();
            V(hud.OnRender(elapsedTime));
        }
        DXUT_EndPerfEvent();
    V(device->EndScene());
}


LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, bool *finished, void *userContext) {
    *finished = dialogResourceManager.MsgProc(hwnd, msg, wparam, lparam);
    if (*finished)
        return 0;

    *finished = hud.MsgProc(hwnd, msg, wparam, lparam);
    if (*finished)
        return 0;

    return 0;
}


void CALLBACK onKeyboard(UINT nchar, bool keyDown, bool altDown, void *userContext) {
    switch (nchar) {
        case VK_TAB: {
            if (keyDown)
                showHud = !showHud;
            break;
        }
    }
}


void setAdapter(DXUTDeviceSettings *settings) {
    HRESULT hr;

    // Look for 'NVIDIA PerfHUD' adapter. If it is present, override default settings.
    IDirect3D9 *d3d = DXUTGetD3D9Object();
    for (UINT i = 0; i < d3d->GetAdapterCount(); i++) {
        D3DADAPTER_IDENTIFIER9 id;
        V(d3d->GetAdapterIdentifier(i, 0, &id));
        if (strstr(id.Description, "PerfHUD") != 0) {
            settings->d3d9.AdapterOrdinal = i;
            settings->d3d9.DeviceType = D3DDEVTYPE_REF;
            break;
        }
    }
}


bool CALLBACK modifyDeviceSettings(DXUTDeviceSettings *settings, void *userContext) {
    setAdapter(settings);

    if (settings->ver == DXUT_D3D9_DEVICE) {
        // Debugging vertex shaders requires either REF or software vertex processing 
        // and debugging pixel shaders requires REF.  
        #ifdef DEBUG_VS
        if (settings->d3d9.DeviceType != D3DDEVTYPE_REF) {
            settings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
            settings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
            settings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
        #endif
        #ifdef DEBUG_PS
        settings->d3d9.DeviceType = D3DDEVTYPE_REF;
        #endif
    }

    settings->d3d9.pp.AutoDepthStencilFormat = D3DFMT_D24S8;

    return true;
}


void CALLBACK onGUIEvent(UINT event, int controlId, CDXUTControl* control, void *userContext) {
    switch(controlId) {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_PROFILE:
            if (event == EVENT_CHECKBOX_CHANGED) {
                timer->reset();
                timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            }
            break;
    }
}


void initApp() {
    hud.Init(&dialogResourceManager);

    hud.SetCallback(onGUIEvent); int iY = 10;
    hud.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22);
    hud.AddComboBox(IDC_DETECTIONMODE, 35, iY += 24, 125, 22, 0, false);
    hud.GetComboBox(IDC_DETECTIONMODE)->AddItem(L"Color edge det.", (LPVOID) 0);
    hud.GetComboBox(IDC_DETECTIONMODE)->AddItem(L"Depth edge det.", (LPVOID) 1);
    hud.AddCheckBox(IDC_PROFILE, L"Profile", 35, iY += 24, 125, 22, false);
}


int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // Enable run-time memory check for debug builds.
    #if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    #endif


    DXUTSetCallbackD3D9DeviceAcceptable(isDeviceAcceptable);
    DXUTSetCallbackD3D9DeviceCreated(onCreateDevice);
    DXUTSetCallbackD3D9DeviceReset(onResetDevice);
    DXUTSetCallbackD3D9DeviceLost(onLostDevice);
    DXUTSetCallbackD3D9DeviceDestroyed(onDestroyDevice);
    DXUTSetCallbackD3D9FrameRender(onFrameRender);
    
    DXUTSetCallbackMsgProc(msgProc);
    DXUTSetCallbackKeyboard(onKeyboard);
    DXUTSetCallbackDeviceChanging(modifyDeviceSettings);

    initApp();

    if (FAILED(DXUTInit(true, true, L"-forcevsync:0")))
        return -1;

    DXUTSetCursorSettings( true, true );
    if (FAILED(DXUTCreateWindow(L"Practical Morphological Anti-Aliasing Demo (Jimenez's MLAA)")))
        return -1;

    if (FAILED(DXUTCreateDevice(true, 1280, 720)))
        return -1;

    DXUTMainLoop();

    return DXUTGetExitCode();
}