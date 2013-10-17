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


#include <sstream>
#include <iomanip>

#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"

#include "SMAA.h"
#include "Timer.h"

using namespace std;

const int HUD_WIDTH = 125;

CDXUTDialogResourceManager dialogResourceManager;
CDXUTDialog hud;

ID3DXFont *font = nullptr;

Timer *timer = nullptr;
SMAA *smaa = nullptr;
IDirect3DSurface9 *backbufferSurface = nullptr;
IDirect3DTexture9 *finalbufferColorTex = nullptr;
IDirect3DSurface9 *finalbufferColorSurface = nullptr;
IDirect3DTexture9 *finalbufferDepthTex = nullptr;
IDirect3DSurface9 *finalbufferDepthSurface = nullptr;
IDirect3DTexture9 *colorTex = nullptr;
IDirect3DTexture9 *depthTex = nullptr;
ID3DXSprite *sprite = nullptr;
CDXUTTextHelper *txtHelper = nullptr;

bool showHud = true;

enum IDC {
    IDC_TOGGLE_FULLSCREEN,
    IDC_PRESET,
    IDC_DETECTION_MODE,
    IDC_ANTIALIASING,
    IDC_PROFILE,
    IDC_THRESHOLD_LABEL,
    IDC_THRESHOLD,
    IDC_MAX_SEARCH_STEPS_LABEL,
    IDC_MAX_SEARCH_STEPS,
    IDC_MAX_SEARCH_STEPS_DIAG_LABEL,
    IDC_MAX_SEARCH_STEPS_DIAG,
    IDC_CORNER_ROUNDING_LABEL,
    IDC_CORNER_ROUNDING,
};

struct {
    float threshold;
    int searchSteps;
    int diagSearchSteps;
    float cornerRounding;
    wstring src;
    wstring dst;
} commandlineOptions = {0.1f, 16, 8, 25.0f, L"", L""};

bool CALLBACK isDeviceAcceptable(D3DCAPS9 *caps, D3DFORMAT adapterFormat, D3DFORMAT backBufferFormat, bool windowed, void *userContext) {
    if (caps->PixelShaderVersion < D3DPS_VERSION(3, 0) || caps->VertexShaderVersion < D3DVS_VERSION(3, 0))
        return false;
    else
        return true;
}

float round(float n) {
    return floor(n + 0.5f);
}

HRESULT CALLBACK onCreateDevice(IDirect3DDevice9 *device, const D3DSURFACE_DESC *desc, void *userContext) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D9CreateDevice(device));

    V_RETURN(D3DXCreateFont(device, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial", &font));

    return S_OK;
}


void setVisibleCustomControls(bool visible) {
    hud.GetStatic(IDC_THRESHOLD_LABEL)->SetVisible(visible);
    hud.GetSlider(IDC_THRESHOLD)->SetVisible(visible);
    hud.GetStatic(IDC_MAX_SEARCH_STEPS_LABEL)->SetVisible(visible);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS)->SetVisible(visible);
    hud.GetStatic(IDC_MAX_SEARCH_STEPS_DIAG_LABEL)->SetVisible(visible);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG)->SetVisible(visible);
    hud.GetStatic(IDC_CORNER_ROUNDING_LABEL)->SetVisible(visible);
    hud.GetSlider(IDC_CORNER_ROUNDING)->SetVisible(visible);
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

    SMAA::Preset preset = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
    smaa = new SMAA(device, desc->Width, desc->Height, preset);
    setVisibleCustomControls(preset == SMAA::PRESET_CUSTOM);

    V(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbufferSurface));

    V(device->CreateTexture(desc->Width, desc->Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &finalbufferColorTex, nullptr));
    V(finalbufferColorTex->GetSurfaceLevel(0, &finalbufferColorSurface));

    V(device->CreateTexture(desc->Width, desc->Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &finalbufferDepthTex, nullptr));
    V(finalbufferDepthTex->GetSurfaceLevel(0, &finalbufferDepthSurface));

    D3DXIMAGE_INFO info;
    V(D3DXGetImageInfoFromResource(nullptr, L"Unigine02.png", &info));
    V(D3DXCreateTextureFromResourceEx(device, nullptr, L"Unigine02.png", info.Width, info.Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, nullptr, &colorTex));
    V(D3DXGetImageInfoFromResource(nullptr, L"Unigine02.dds", &info));
    V(D3DXCreateTextureFromResourceEx(device, nullptr, L"Unigine02.dds", info.Width, info.Height, 1, 0, D3DFMT_R32F, D3DPOOL_DEFAULT, D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &info, nullptr, &depthTex));

    V_RETURN(D3DXCreateSprite(device, &sprite));
    txtHelper = new CDXUTTextHelper(font, sprite, nullptr, nullptr, 15);

    hud.SetLocation(desc->Width - 170, 0);
    hud.SetSize(170, 170);

    return S_OK;
}


void CALLBACK onLostDevice(void *userContext) {
    dialogResourceManager.OnD3D9LostDevice();
    if (font) font->OnLostDevice();
    SAFE_DELETE(timer);
    SAFE_DELETE(smaa);
    SAFE_RELEASE(backbufferSurface);
    SAFE_RELEASE(finalbufferColorTex);
    SAFE_RELEASE(finalbufferColorSurface);
    SAFE_RELEASE(finalbufferDepthTex);
    SAFE_RELEASE(finalbufferDepthSurface);
    SAFE_RELEASE(colorTex);
    SAFE_RELEASE(depthTex);
    SAFE_RELEASE(sprite);
    SAFE_DELETE(txtHelper);
}


void drawHud(float elapsedTime) {
    HRESULT hr;

    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"HUD");
    if (showHud) {
        V(hud.OnRender(elapsedTime));

        txtHelper->Begin();

        txtHelper->SetInsertionPos(2, 0);
        txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
        txtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        txtHelper->DrawTextLine(DXUTGetDeviceStats());
    
        txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.5f, 0.0f, 1.0f));
        if (timer->isEnabled()) {
            wstringstream s;
            s << setprecision(2) << std::fixed;
            s << *timer;
            txtHelper->DrawTextLine(s.str().c_str());
        }

        txtHelper->End();
    }
    D3DPERF_EndEvent();
}


void mainPass(IDirect3DDevice9 *device) {
    HRESULT hr;

    // A dummy copy over here.
    IDirect3DSurface9 *colorSurface = nullptr;
    V(colorTex->GetSurfaceLevel(0, &colorSurface));
    D3DSURFACE_DESC desc;
    colorSurface->GetDesc(&desc);
    const D3DSURFACE_DESC *backbufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    RECT rect = {0, 0, min(desc.Width, backbufferDesc->Width), min(desc.Height, backbufferDesc->Height)};
    V(device->StretchRect(colorSurface, &rect, finalbufferColorSurface, &rect, D3DTEXF_POINT));
    SAFE_RELEASE(colorSurface);

    // And another one over here.
    IDirect3DSurface9 *depthSurface = nullptr;
    V(depthTex->GetSurfaceLevel(0, &depthSurface));
    V(device->StretchRect(depthSurface, &rect, finalbufferDepthSurface, &rect, D3DTEXF_POINT));
    SAFE_RELEASE(depthSurface);
}


void copy(IDirect3DDevice9 *device) {
    HRESULT hr;

    // A dummy copy over here.
    IDirect3DSurface9 *colorSurface = nullptr;
    V(colorTex->GetSurfaceLevel(0, &colorSurface));
    D3DSURFACE_DESC desc;
    colorSurface->GetDesc(&desc);
    const D3DSURFACE_DESC *backbufferDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    RECT rect = {0, 0, min(desc.Width, backbufferDesc->Width), min(desc.Height, backbufferDesc->Height)};
    V(device->StretchRect(colorSurface, &rect, backbufferSurface, &rect, D3DTEXF_POINT));
    SAFE_RELEASE(colorSurface);
}


void CALLBACK onFrameRender(IDirect3DDevice9 *device, double time, float elapsedTime, void *userContext) {
    HRESULT hr;

    // IMPORTANT: stencil must be cleared to zero before executing 'smaa->go'
    V(device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0));

    V(device->BeginScene());
        // This emulates main pass.
        mainPass(device);

        // Run SMAA
        if (hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked()) {
            SMAA::Input input = SMAA::Input(int(hud.GetComboBox(IDC_DETECTION_MODE)->GetSelectedData()));

            timer->start(L"SMAA");
            switch (input) {
                case SMAA::INPUT_LUMA:
                case SMAA::INPUT_COLOR:
                    smaa->go(finalbufferColorTex, finalbufferColorTex, backbufferSurface, input);
                    break;
                case SMAA::INPUT_DEPTH:
                    smaa->go(finalbufferDepthTex, finalbufferColorTex, backbufferSurface, input);
                    break;
            }
            timer->end(L"SMAA");
            timer->endFrame();
        } else {
            copy(device);
        }

        // Draw the HUD
        drawHud(elapsedTime);
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
    if (keyDown)
    switch (nchar) {
        case VK_TAB: {
            if (keyDown)
                showHud = !showHud;
            break;
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5': {
            hud.GetComboBox(IDC_PRESET)->SetSelectedByIndex(nchar - '1');
            onLostDevice(nullptr);
            onResetDevice(DXUTGetD3D9Device(), DXUTGetD3D9BackBufferSurfaceDesc(), nullptr);
            break;
        }
        case 'X':
            hud.GetCheckBox(IDC_PROFILE)->SetChecked(!hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            break;
        case 'Z':
            hud.GetCheckBox(IDC_ANTIALIASING)->SetChecked(!hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked());
            break;
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
        case IDC_TOGGLE_FULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_PRESET:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                SMAA::Preset selected;
                selected = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
                onLostDevice(nullptr);
                onResetDevice(DXUTGetD3D9Device(), DXUTGetD3D9BackBufferSurfaceDesc(), nullptr);
            }
            break;
        case IDC_ANTIALIASING:
            if (event == EVENT_CHECKBOX_CHANGED)
                timer->reset();
            break;
        case IDC_PROFILE:
            if (event == EVENT_CHECKBOX_CHANGED) {
                timer->reset();
                timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            }
            break;
        case IDC_THRESHOLD:
            if (event == EVENT_SLIDER_VALUE_CHANGED) {
                CDXUTSlider *slider = (CDXUTSlider *) control;
                int min, max;
                slider->GetRange(min, max);

                float scale = float(slider->GetValue()) / (max - min);
                smaa->setThreshold(scale * 0.5f);
            
                wstringstream s;
                s << L"Threshold: " << scale * 0.5f;
                hud.GetStatic(IDC_THRESHOLD_LABEL)->SetText(s.str().c_str());
            }
            break;
        case IDC_MAX_SEARCH_STEPS:
            if (event == EVENT_SLIDER_VALUE_CHANGED) {
                CDXUTSlider *slider = (CDXUTSlider *) control;
                int min, max;
                slider->GetRange(min, max);

                float scale = float(slider->GetValue()) / (max - min);
                smaa->setMaxSearchSteps(int(round(scale * 112.0f)));

                wstringstream s;
                s << L"Max Search Steps: " << int(round(scale * 112.0f));
                hud.GetStatic(IDC_MAX_SEARCH_STEPS_LABEL)->SetText(s.str().c_str());
            }
            break;
        case IDC_MAX_SEARCH_STEPS_DIAG:
            if (event == EVENT_SLIDER_VALUE_CHANGED) {
                CDXUTSlider *slider = (CDXUTSlider *) control;
                int min, max;
                slider->GetRange(min, max);

                float scale = float(slider->GetValue()) / (max - min);
                smaa->setMaxSearchStepsDiag(int(round(scale * 20.0f)));

                wstringstream s;
                s << L"Max Diag. Search Steps: " << int(round(scale * 20.0f));
                hud.GetStatic(IDC_MAX_SEARCH_STEPS_DIAG_LABEL)->SetText(s.str().c_str());
            }
            break;
        case IDC_CORNER_ROUNDING:
            if (event == EVENT_SLIDER_VALUE_CHANGED) {
                CDXUTSlider *slider = (CDXUTSlider *) control;
                int min, max;
                slider->GetRange(min, max);

                float scale = float(slider->GetValue()) / (max - min);
                smaa->setCornerRounding(scale * 100.0f);

                wstringstream s;
                s << L"Corner Rounding: " << scale * 100.0f;
                hud.GetStatic(IDC_CORNER_ROUNDING_LABEL)->SetText(s.str().c_str());
            }
            break;
    }
}


void initApp() {
    hud.Init(&dialogResourceManager);

    hud.SetCallback(onGUIEvent); int iY = 10;
    hud.AddButton(IDC_TOGGLE_FULLSCREEN, L"Toggle full screen", 35, iY, HUD_WIDTH, 22);

    iY += 24;

    hud.AddComboBox(IDC_PRESET, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Low", (LPVOID) 0);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Medium", (LPVOID) 1);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA High", (LPVOID) 2);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Ultra", (LPVOID) 3);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Custom", (LPVOID) 4);
    hud.GetComboBox(IDC_PRESET)->SetSelectedByData((LPVOID) 2);

    hud.AddComboBox(IDC_DETECTION_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Luma edge det.", (LPVOID) 0);
    hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Color edge det.", (LPVOID) 1);
    hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Depth edge det.", (LPVOID) 2);

    hud.AddCheckBox(IDC_ANTIALIASING, L"SMAA", 35, iY += 24, HUD_WIDTH, 22, true);
    hud.AddCheckBox(IDC_PROFILE, L"Profile", 35, iY += 24, HUD_WIDTH, 22, false);
    wstringstream s;
    s << L"Threshold: " << commandlineOptions.threshold;
    hud.AddStatic(IDC_THRESHOLD_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_THRESHOLD, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.threshold / 0.5f));
    hud.GetStatic(IDC_THRESHOLD_LABEL)->SetVisible(false);
    hud.GetSlider(IDC_THRESHOLD)->SetVisible(false);

    s = wstringstream();
    s << L"Max Search Steps: " << commandlineOptions.searchSteps;
    hud.AddStatic(IDC_MAX_SEARCH_STEPS_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_MAX_SEARCH_STEPS, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.searchSteps / 112.0f));
    hud.GetStatic(IDC_MAX_SEARCH_STEPS_LABEL)->SetVisible(false);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS)->SetVisible(false);

    s = wstringstream();
    s << L"Max Diag. Search Steps: " << commandlineOptions.diagSearchSteps;
    hud.AddStatic(IDC_MAX_SEARCH_STEPS_DIAG_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_MAX_SEARCH_STEPS_DIAG, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.diagSearchSteps / 20.0f));
    hud.GetStatic(IDC_MAX_SEARCH_STEPS_DIAG_LABEL)->SetVisible(false);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG)->SetVisible(false);

    s = wstringstream();
    s << L"Corner Rounding: " << commandlineOptions.cornerRounding;
    hud.AddStatic(IDC_CORNER_ROUNDING_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_CORNER_ROUNDING, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.cornerRounding / 100.0f));
    hud.GetStatic(IDC_CORNER_ROUNDING_LABEL)->SetVisible(false);
    hud.GetSlider(IDC_CORNER_ROUNDING)->SetVisible(false);
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
    if (FAILED(DXUTCreateWindow(L"SMAA: Subpixel Morphological Antialiasing")))
        return -1;

    if (FAILED(DXUTCreateDevice(true, 1280, 720)))
        return -1;

    /**
     * See <WINDOW_FIX> in DXUT.h
     */
    ShowWindow(DXUTGetHWND(), SW_SHOW);

    DXUTMainLoop();

    return DXUTGetExitCode();
}