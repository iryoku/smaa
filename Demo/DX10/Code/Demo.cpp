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


#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#include <sstream>
#include <fstream>
#include <iomanip>
#include <cmath>

#include "Timer.h"
#include "RenderTarget.h"
#include "Camera.h"
#include "Copy.h"
#include "SMAA.h"

using namespace std;


const int HUD_WIDTH = 140;

CDXUTDialogResourceManager dialogResourceManager;
CD3DSettingsDlg settingsDialog;
CDXUTDialog hud;

Timer *profileTimer = NULL;
Timer *framerateLockTimer = NULL;
ID3DX10Font *font = NULL;
ID3DX10Sprite *sprite = NULL;
CDXUTTextHelper *txtHelper = NULL;

SMAA *smaa = NULL;

DepthStencil *depthStencil = NULL;
DepthStencil *depthStencil1x = NULL;

RenderTarget *tmpRT_SRGB = NULL;
RenderTarget *tmpRT = NULL;
RenderTarget *tmp1xRT[2] = { NULL, NULL };
RenderTarget *tmp1xRT_SRGB[2] = { NULL, NULL };
RenderTarget *depthBufferRT = NULL;
RenderTarget *velocityRT = NULL;
RenderTarget *velocity1xRT = NULL;
RenderTarget *finalRT[2] = { NULL, NULL };
BackbufferRenderTarget *backbufferRT = NULL;

ID3D10ShaderResourceView *inputColorSRV = NULL;
ID3D10ShaderResourceView *inputDepthSRV = NULL;

Camera camera;
CDXUTSDKMesh mesh;
ID3D10InputLayout *vertexLayout = NULL;
ID3D10Effect *simpleEffect = NULL;
ID3D10ShaderResourceView *envTexSRV = NULL;

bool showHud = true;

bool benchmark = false;
fstream benchmarkFile;

enum RecPlayState { RECPLAY_IDLE, RECPLAY_RECORDING, RECPLAY_PLAYING } recPlayState;
double recPlayTime = 0.0;
double recPlayDuration = 0.0;

D3DXMATRIX prevViewProj, currViewProj;
int subpixelIndex = 0;

struct MSAAMode {
    wstring name;
    DXGI_SAMPLE_DESC desc;
};
MSAAMode msaaModes[] = {
    {L"MSAA 1x",   {1,  0}},
    {L"MSAA 2x",   {2,  0}},
    {L"MSAA 4x",   {4,  0}},
    {L"CSAA 8x",   {4,  8}},
    {L"CSAA 8xQ",  {8,  8}},
    {L"CSAA 16x",  {4, 16}},
    {L"CSAA 16xQ", {8, 16}}
};
vector<MSAAMode> supportedMsaaModes;

struct {
    float threshold;
    int searchSteps;
    int diagSearchSteps;
    float cornerRounding;
    wstring src;
    wstring dst;
} commandlineOptions = {0.1f, 16, 8, 25.0f, L"", L""};


enum FramerateLock { FPS_UNLOCK, FPS_LOCK_TO_15=66, FPS_LOCK_TO_30=33, FPS_LOCK_TO_60=16 };


#define IDC_TOGGLE_FULLSCREEN            1
#define IDC_CHANGE_DEVICE                2
#define IDC_LOAD_IMAGE                   3
#define IDC_INPUT                        4
#define IDC_VIEW_MODE                    5
#define IDC_AA_MODE                      6
#define IDC_PRESET                       7
#define IDC_DETECTION_MODE               8
#define IDC_ANTIALIASING                 9
#define IDC_PREDICATION                 10
#define IDC_REPROJECTION                11
#define IDC_LOCK_FRAMERATE              12
#define IDC_PROFILE                     13
#define IDC_SHADING                     14
#define IDC_THRESHOLD_LABEL             15
#define IDC_THRESHOLD                   16
#define IDC_MAX_SEARCH_STEPS_LABEL      17
#define IDC_MAX_SEARCH_STEPS            18
#define IDC_MAX_SEARCH_STEPS_DIAG_LABEL 19
#define IDC_MAX_SEARCH_STEPS_DIAG       20
#define IDC_CORNER_ROUNDING_LABEL       21
#define IDC_CORNER_ROUNDING             22


float round(float n) {
    return floor(n + 0.5f);
}


bool CALLBACK isDeviceAcceptable(UINT adapter, UINT output, D3D10_DRIVER_TYPE deviceType, DXGI_FORMAT format, bool windowed, void *context) {
    return true;
}


HRESULT loadImage() {
    HRESULT hr;

    D3DX10_IMAGE_LOAD_INFO loadInfo = D3DX10_IMAGE_LOAD_INFO();
    loadInfo.MipLevels = 1;
    loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;

    WCHAR *text = hud.GetComboBox(IDC_INPUT)->GetSelectedItem()->strText;
    if (int(hud.GetComboBox(IDC_INPUT)->GetSelectedData()) != -1) { // ... then, we have to search for it in the executable resources
        SAFE_RELEASE(inputColorSRV);
        SAFE_RELEASE(inputDepthSRV);

        // Load color
        V_RETURN(D3DX10CreateShaderResourceViewFromResource(DXUTGetD3D10Device(), GetModuleHandle(NULL), text, &loadInfo, NULL, &inputColorSRV, NULL));

        // Try to load depth
        loadInfo.Format = DXGI_FORMAT_R32_FLOAT;
        loadInfo.Filter = D3DX10_FILTER_POINT;

        wstring path = wstring(text).substr(0, wstring(text).find_last_of('.')) + L".dds";
        if (FindResource(GetModuleHandle(NULL), path.c_str(), RT_RCDATA) != NULL)
            V_RETURN(D3DX10CreateShaderResourceViewFromResource(DXUTGetD3D10Device(), GetModuleHandle(NULL), path.c_str(), &loadInfo, NULL, &inputDepthSRV, NULL));
    } else { // ... search for it in the file system
        ID3D10ShaderResourceView *colorSRV= NULL;
        if (FAILED(D3DX10CreateShaderResourceViewFromFile(DXUTGetD3D10Device(), text, &loadInfo, NULL, &colorSRV, NULL))) {
            MessageBox(NULL, L"Unable to open selected file", L"ERROR", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
            hud.GetComboBox(IDC_INPUT)->RemoveItem(hud.GetComboBox(IDC_INPUT)->FindItem(text));
            if (commandlineOptions.src != L"") {
                exit(1);
            } else {
                V_RETURN(loadImage()); // Fallback to previous image
            }
        } else {
            SAFE_RELEASE(inputColorSRV);
            SAFE_RELEASE(inputDepthSRV);

            inputColorSRV = colorSRV;

            ID3D10ShaderResourceView *depthSRV;
            wstring path = wstring(text).substr(0, wstring(text).find_last_of('.')) + L".dds";
            if (SUCCEEDED(D3DX10CreateShaderResourceViewFromFile(DXUTGetD3D10Device(), path.c_str(), &loadInfo, NULL, &depthSRV, NULL)))
                inputDepthSRV = depthSRV;
        }
    }
    return S_OK;
}


void CALLBACK createTextureFromFile(ID3D10Device* device, char *filename, ID3D10ShaderResourceView **shaderResourceView, void *context, bool srgb) {
    HRESULT hr;

    *shaderResourceView = (ID3D10ShaderResourceView *) ERROR_RESOURCE_VALUE;
    if (string(filename) != "default.dds" && string(filename) != "default-normalmap.dds") {
        wstring path = wstring((wchar_t *) context);
        wstringstream s;
        s << path;
            if (path != L"") s << "\\";
        s << filename;

        D3DX10_IMAGE_INFO info;
        V(D3DX10GetImageInfoFromResource(GetModuleHandle(NULL), s.str().c_str(), NULL, &info, NULL));

        D3DX10_IMAGE_LOAD_INFO loadInfo = D3DX10_IMAGE_LOAD_INFO();
        loadInfo.pSrcInfo = &info;
        if (srgb) {
            loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
            loadInfo.Format = MAKE_SRGB(info.Format);
        }
        V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), s.str().c_str(), &loadInfo, NULL, shaderResourceView, NULL));
    }
}


HRESULT loadMesh(CDXUTSDKMesh &mesh, const wstring &name, const wstring &path) {
    HRESULT hr;

    HRSRC src = FindResource(GetModuleHandle(NULL), name.c_str(), RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(NULL), src);
    UINT size = SizeofResource(GetModuleHandle(NULL), src);
    LPBYTE data = (LPBYTE) LockResource(res);

    SDKMESH_CALLBACKS10 callbacks;
    ZeroMemory(&callbacks, sizeof(SDKMESH_CALLBACKS10));  
    callbacks.pCreateTextureFromFile = &createTextureFromFile;
    callbacks.pContext = (void *) path.c_str();

    V_RETURN(mesh.Create(DXUTGetD3D10Device(), data, size, true, true, &callbacks));

    return S_OK;
}


void setModeControls() {
    SMAA::Mode mode = SMAA::Mode(int(hud.GetComboBox(IDC_AA_MODE)->GetSelectedData()));
    bool isMsaa = mode >= 10;
    bool isTemporalMode = !isMsaa && mode != SMAA::MODE_SMAA_1X;

    hud.GetComboBox(IDC_VIEW_MODE)->SetEnabled(!isMsaa);
    hud.GetComboBox(IDC_PRESET)->SetEnabled(!isMsaa);
    hud.GetComboBox(IDC_DETECTION_MODE)->SetEnabled(!isMsaa);
    hud.GetCheckBox(IDC_ANTIALIASING)->SetEnabled(!isMsaa);
    hud.GetCheckBox(IDC_PREDICATION)->SetEnabled(!isMsaa && inputDepthSRV != NULL);
    hud.GetCheckBox(IDC_REPROJECTION)->SetEnabled(!isMsaa && isTemporalMode);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->SetEnabled(!isMsaa);
    hud.GetCheckBox(IDC_PROFILE)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_THRESHOLD)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_CORNER_ROUNDING)->SetEnabled(!isMsaa);

    hud.GetCheckBox(IDC_PROFILE)->SetChecked(hud.GetCheckBox(IDC_PROFILE)->GetChecked() && !isMsaa);

    hud.GetCheckBox(IDC_SHADING)->SetEnabled(hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() == 0);
}


HRESULT loadInput() {
    HRESULT hr;

    if (hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0) {
        V_RETURN(loadImage());
        hud.GetComboBox(IDC_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_1X);
        hud.GetComboBox(IDC_AA_MODE)->SetEnabled(false);
    } else {
        SAFE_RELEASE(inputColorSRV);
        SAFE_RELEASE(inputDepthSRV);
        mesh.Destroy();

        V_RETURN(loadMesh(mesh, L"Fence.sdkmesh", L""));
        hud.GetComboBox(IDC_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_T2X);
        hud.GetComboBox(IDC_AA_MODE)->SetEnabled(true);
    }

    int selectedIndex = hud.GetComboBox(IDC_DETECTION_MODE)->GetSelectedIndex();
    hud.GetComboBox(IDC_DETECTION_MODE)->RemoveAllItems();
    hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Luma edge det.", (LPVOID) SMAA::INPUT_LUMA);
    hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Color edge det.", (LPVOID) SMAA::INPUT_COLOR);
    if (inputDepthSRV != NULL)
        hud.GetComboBox(IDC_DETECTION_MODE)->AddItem(L"Depth edge det.", (LPVOID) SMAA::INPUT_DEPTH);
    hud.GetComboBox(IDC_DETECTION_MODE)->SetSelectedByIndex(selectedIndex);

    setModeControls();

    return S_OK;
}


void resizeWindow() {
    if (hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0) {
        ID3D10Texture2D *texture2D;
        inputColorSRV->GetResource(reinterpret_cast<ID3D10Resource **>(&texture2D));
        D3D10_TEXTURE2D_DESC desc;
        texture2D->GetDesc(&desc);
        texture2D->Release();

        RECT rect = {0, 0, desc.Width, desc.Height};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(DXUTGetHWNDDeviceWindowed(), 0, 0, 0,  (rect.right - rect.left), (rect.bottom - rect.top), SWP_NOZORDER | SWP_NOMOVE);
    }
}


HRESULT initSimpleEffect(ID3D10Device *device) {
    HRESULT hr;

    V(D3DX10CreateEffectFromResource(GetModuleHandle(NULL), L"Simple.fx", NULL, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, NULL, NULL, &simpleEffect, NULL, NULL));

    const D3D10_INPUT_ELEMENT_DESC inputDesc[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D10_APPEND_ALIGNED_ELEMENT, D3D10_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = sizeof(inputDesc) / sizeof(D3D10_INPUT_ELEMENT_DESC);

    D3D10_PASS_DESC passDesc;
    V_RETURN(simpleEffect->GetTechniqueByName("Simple")->GetPassByIndex(0)->GetDesc(&passDesc));
    V_RETURN(device->CreateInputLayout(inputDesc, numElements, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &vertexLayout));

    return S_OK;
}


void buildModesComboBox() {
    hud.GetComboBox(IDC_AA_MODE)->RemoveAllItems();

    hud.GetComboBox(IDC_AA_MODE)->AddItem(L"SMAA 1x", (LPVOID) SMAA::MODE_SMAA_1X);
    hud.GetComboBox(IDC_AA_MODE)->AddItem(L"SMAA T2x", (LPVOID) SMAA::MODE_SMAA_T2X);
    hud.GetComboBox(IDC_AA_MODE)->AddItem(L"SMAA S2x", (LPVOID) SMAA::MODE_SMAA_S2X);
    hud.GetComboBox(IDC_AA_MODE)->AddItem(L"SMAA 4x", (LPVOID) SMAA::MODE_SMAA_4X);

    supportedMsaaModes.clear();
    for(int i = 0; i < sizeof(msaaModes) / sizeof(MSAAMode); i++){
        ID3D10Device *device = DXUTGetD3D10Device();
        UINT quality;
        device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, msaaModes[i].desc.Count, &quality);
        if (quality > msaaModes[i].desc.Quality) {
            hud.GetComboBox(IDC_AA_MODE)->AddItem(msaaModes[i].name.c_str(), (void *) (10 + i));
            supportedMsaaModes.push_back(msaaModes[i]);
        }
    }

    hud.GetComboBox(IDC_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_T2X);
    setModeControls();
}


HRESULT CALLBACK onCreateDevice(ID3D10Device *device, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D10CreateDevice(device));
    V_RETURN(settingsDialog.OnD3D10CreateDevice(device));

    V_RETURN(D3DX10CreateFont(device, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &font));
    V_RETURN(D3DX10CreateSprite(device, 512, &sprite));
    txtHelper = new CDXUTTextHelper(NULL, NULL, font, sprite, 15);

    profileTimer = new Timer(device);
    profileTimer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
    profileTimer->setRepetitionsCount(100);

    framerateLockTimer = new Timer(device);

    Copy::init(device);
    V_RETURN(initSimpleEffect(device));

    camera.setAngle(D3DXVECTOR2(0.0f, 0.0f));
    camera.setDistance(30.0f);

    V_RETURN(loadInput());

    D3DX10_IMAGE_LOAD_INFO loadInfo = D3DX10_IMAGE_LOAD_INFO();
    loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(NULL), L"EnvMap.dds", &loadInfo, NULL, &envTexSRV, NULL));

    buildModesComboBox();

    return S_OK;
}


void CALLBACK onDestroyDevice(void *context) {
    dialogResourceManager.OnD3D10DestroyDevice();
    settingsDialog.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_RELEASE(font);
    SAFE_RELEASE(sprite);
    SAFE_DELETE(txtHelper);

    SAFE_DELETE(profileTimer);
    SAFE_DELETE(framerateLockTimer);

    Copy::release();
    SAFE_RELEASE(simpleEffect);
    SAFE_RELEASE(vertexLayout);

    SAFE_RELEASE(inputColorSRV);
    SAFE_RELEASE(inputDepthSRV);
    mesh.Destroy();

    SAFE_RELEASE(envTexSRV);
}


void initSMAA(ID3D10Device *device, const DXGI_SURFACE_DESC *desc) {
    SMAA::Preset preset = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
    bool predication = hud.GetCheckBox(IDC_PREDICATION)->GetEnabled() && hud.GetCheckBox(IDC_PREDICATION)->GetChecked();
    bool reprojection = hud.GetCheckBox(IDC_REPROJECTION)->GetEnabled() && hud.GetCheckBox(IDC_REPROJECTION)->GetChecked();
    smaa = new SMAA(device, desc->Width, desc->Height, preset, predication, reprojection);

    int min, max;
    float scale;

    CDXUTSlider *slider = hud.GetSlider(IDC_THRESHOLD);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setThreshold(0.5f * scale);

    slider = hud.GetSlider(IDC_MAX_SEARCH_STEPS);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setMaxSearchSteps(int(round(98.0f * scale)));

    slider = hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setMaxSearchStepsDiag(int(round(20.0f * scale)));

    slider = hud.GetSlider(IDC_CORNER_ROUNDING);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setCornerRounding(100.0f * scale);
}


HRESULT CALLBACK onResizedSwapChain(ID3D10Device *device, IDXGISwapChain *swapChain, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;
    V_RETURN(dialogResourceManager.OnD3D10ResizedSwapChain(device, desc));
    V_RETURN(settingsDialog.OnD3D10ResizedSwapChain(device, desc));

    hud.SetLocation(desc->Width - (45 + HUD_WIDTH), 0);

    float aspect = (float) desc->Width / desc->Height;
    camera.setProjection(18.0f * D3DX_PI / 180.f, aspect, 0.01f, 100.0f);
    camera.setViewportSize(D3DXVECTOR2(float(desc->Width), float(desc->Height)));

    initSMAA(device, desc);

    DXGI_SAMPLE_DESC sampleDesc;
    int mode = int(hud.GetComboBox(IDC_AA_MODE)->GetSelectedData());
    if (mode > 10) // Then, it's a MSAA mode:
        sampleDesc = supportedMsaaModes[mode - 10].desc;
    else if (mode == SMAA::MODE_SMAA_S2X || mode == SMAA::MODE_SMAA_4X)
        sampleDesc = msaaModes[1].desc;
    else // Then, we are going to use SMAA 1x or SMAA T2x:
        sampleDesc = NoMSAA();

    depthStencil = new DepthStencil(device, desc->Width, desc->Height,  DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, sampleDesc);
    depthStencil1x = new DepthStencil(device, desc->Width, desc->Height,  DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);

    tmpRT_SRGB = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, sampleDesc);
    tmpRT = new RenderTarget(device, *tmpRT_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM);
    for (int i = 0; i < 2; i++) {
        tmp1xRT_SRGB[i] = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, sampleDesc);
        tmp1xRT[i] = new RenderTarget(device, *tmp1xRT_SRGB[i], DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    depthBufferRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R32_FLOAT);
    velocityRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT, sampleDesc);
    velocity1xRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT);
    for (int i = 0; i < 2; i++)
        finalRT[i] = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    backbufferRT = new BackbufferRenderTarget(device, DXUTGetDXGISwapChain());

    return S_OK;
}


void CALLBACK onReleasingSwapChain(void *context) {
    dialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_DELETE(smaa);

    SAFE_DELETE(depthStencil);
    SAFE_DELETE(depthStencil1x);

    SAFE_DELETE(tmpRT);
    SAFE_DELETE(tmpRT_SRGB);
    for (int i = 0; i < 2; i++) {
        SAFE_DELETE(tmp1xRT[i]);
        SAFE_DELETE(tmp1xRT_SRGB[i]);
    }

    SAFE_DELETE(depthBufferRT);
    SAFE_DELETE(velocityRT);
    SAFE_DELETE(velocity1xRT);
    for (int i = 0; i < 2; i++)
        SAFE_DELETE(finalRT[i]);
    SAFE_DELETE(backbufferRT);
}


void renderMesh(ID3D10Device *device, const D3DXVECTOR2 &jitter) {
    D3DPERF_BeginEvent(D3DCOLOR_XRGB(0, 0, 0), L"Render Scene");
    HRESULT hr;

    // Save the state.
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Set the render targets:
    ID3D10RenderTargetView *rt[] = { *tmpRT_SRGB, *velocityRT };
    device->OMSetRenderTargets(2, rt, *depthStencil);

    // Calculate current view-projection matrix:
    currViewProj = camera.getViewMatrix() * camera.getProjectionMatrix();

    // Covert the jitter from screen space to non-homogeneous projection space:
    const DXGI_SURFACE_DESC *desc = DXUTGetDXGIBackBufferSurfaceDesc();
    D3DXVECTOR2 jitterProjectionSpace = 2.0f * jitter;
    jitterProjectionSpace.x /= float(desc->Width); jitterProjectionSpace.y /= float(desc->Height);
    V(simpleEffect->GetVariableByName("jitter")->AsVector()->SetFloatVector((float *) jitterProjectionSpace));

    // Set enviroment map for metal reflections:
    V(simpleEffect->GetVariableByName("envTex")->AsShaderResource()->SetResource(envTexSRV));

    // Enable/Disable shading:
    V(simpleEffect->GetVariableByName("shading")->AsScalar()->SetBool(hud.GetCheckBox(IDC_SHADING)->GetChecked()));

    // Set the vertex layout:
    device->IASetInputLayout(vertexLayout);

    // Render the grid:
    for (float x = -20.0; x <= 20.0; x += 1.0) {
        for (float y = -20.0; y <= 20.0; y += 1.0) {
            D3DXMATRIX world;
            D3DXMatrixTranslation(&world, x, y, 0.0f);

            D3DXMATRIX currWorldViewProj = world * currViewProj;
            D3DXMATRIX prevWorldViewProj = world * prevViewProj;

            V(simpleEffect->GetVariableByName("currWorldViewProj")->AsMatrix()->SetMatrix((float *) currWorldViewProj));
            V(simpleEffect->GetVariableByName("prevWorldViewProj")->AsMatrix()->SetMatrix((float *) prevWorldViewProj));
            V(simpleEffect->GetVariableByName("eyePositionW")->AsVector()->SetFloatVector((float *) &camera.getEyePosition()));
            V(simpleEffect->GetVariableByName("world")->AsMatrix()->SetMatrix((float *) world));
            V(simpleEffect->GetTechniqueByName("Simple")->GetPassByIndex(0)->Apply(0));

            mesh.Render(device, simpleEffect->GetTechniqueByName("Simple"), simpleEffect->GetVariableByName("diffuseTex")->AsShaderResource(), simpleEffect->GetVariableByName("normalTex")->AsShaderResource());
        }
    }

    // Update previous view-projection matrix:
    prevViewProj = currViewProj;

    D3DPERF_EndEvent();
}


void renderScene(ID3D10Device *device) {
    bool fromImage = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0;
    bool smaaEnabled = hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked() &&
                       hud.GetCheckBox(IDC_ANTIALIASING)->GetEnabled();
    SMAA::Mode mode = SMAA::Mode(int(hud.GetComboBox(IDC_AA_MODE)->GetSelectedData()));

    // Copy the image or render the mesh:
    if (fromImage) {
        D3D10_VIEWPORT viewport = Utils::viewportFromView(inputColorSRV);
        Copy::go(inputColorSRV, *tmpRT_SRGB, &viewport);
        Copy::go(inputDepthSRV, *depthBufferRT, &viewport);
    } else {
        D3DXVECTOR2 jitter;
        if (smaaEnabled) {
            switch (mode) {
                case SMAA::MODE_SMAA_1X:
                case SMAA::MODE_SMAA_S2X:
                    jitter = D3DXVECTOR2(0.0f, 0.0f);
                    break;
                case SMAA::MODE_SMAA_T2X: {
                    D3DXVECTOR2 jitters[] = {
                        D3DXVECTOR2( 0.25f, -0.25f),
                        D3DXVECTOR2(-0.25f,  0.25f)
                    };
                    jitter = jitters[subpixelIndex];
                    break;
                }
                case SMAA::MODE_SMAA_4X:
                    D3DXVECTOR2 jitters[] = {
                        D3DXVECTOR2( 0.125f,  0.125f),
                        D3DXVECTOR2(-0.125f, -0.125f)
                    };
                    jitter = jitters[subpixelIndex];
                    break;
            }
        } else {
            jitter = D3DXVECTOR2(0.0f, 0.0f);
        }
        renderMesh(device, jitter);
    }

    device->ResolveSubresource(*velocity1xRT, 0, *velocityRT, 0, DXGI_FORMAT_R16G16_FLOAT);
}


void runSMAA(ID3D10Device *device, SMAA::Mode mode) {
    // Calculate next subpixel index:
    int previousIndex = subpixelIndex;
    int currentIndex = (subpixelIndex + 1) % 2;

    // Fetch configuration parameters:
    bool smaaEnabled = hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked() &&
                       hud.GetCheckBox(IDC_ANTIALIASING)->GetEnabled();
    SMAA::Input input = SMAA::Input(int(hud.GetComboBox(IDC_DETECTION_MODE)->GetSelectedData()));
    int repetitionsCount = hud.GetCheckBox(IDC_PROFILE)->GetChecked()? profileTimer->getRepetitionsCount() : 1;

    // Run SMAA:
    if (smaaEnabled) {
        profileTimer->start();
        switch (mode) {
            case SMAA::MODE_SMAA_1X:
                for (int i = 0; i < repetitionsCount; i++) // This loop is for profiling.
                    smaa->go(*tmpRT, *tmpRT_SRGB, *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode);
                break;
            case SMAA::MODE_SMAA_T2X:
                for (int i = 0; i < repetitionsCount; i++) {
                    smaa->go(*tmpRT, *tmpRT_SRGB, *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode, subpixelIndex);
                    smaa->reproject(*finalRT[currentIndex], *finalRT[previousIndex], *velocityRT, *backbufferRT);
                }
                break;
            case SMAA::MODE_SMAA_S2X:
                for (int i = 0; i < repetitionsCount; i++) {
                    smaa->separate(*tmpRT, *tmp1xRT[0], *tmp1xRT[1]);
                    smaa->go(*tmp1xRT[0], *tmp1xRT_SRGB[0], *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode, smaa->msaaReorder(0), 1.0);
                    smaa->go(*tmp1xRT[1], *tmp1xRT_SRGB[1], *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode, smaa->msaaReorder(1), 0.5);
                }
                break;
            case SMAA::MODE_SMAA_4X:
                for (int i = 0; i < repetitionsCount; i++) {
                    smaa->separate(*tmpRT, *tmp1xRT[0], *tmp1xRT[1]);
                    smaa->go(*tmp1xRT[0], *tmp1xRT_SRGB[0], *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode, 2 * subpixelIndex + smaa->msaaReorder(0), 1.0);
                    smaa->go(*tmp1xRT[1], *tmp1xRT_SRGB[1], *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode, 2 * subpixelIndex + smaa->msaaReorder(1), 0.5);
                    smaa->reproject(*finalRT[currentIndex], *finalRT[previousIndex], *velocity1xRT, *backbufferRT);
                }
                break;
        }
        profileTimer->clock(L"SMAA");
    } else {
        if (mode == SMAA::MODE_SMAA_1X)
            Copy::go(*tmpRT_SRGB, *backbufferRT);
        else
            device->ResolveSubresource(*backbufferRT, 0, *tmpRT_SRGB, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }

    // Update subpixel index:
    subpixelIndex = currentIndex;
}


void saveBackbuffer(ID3D10Device *device) {
    HRESULT hr;
    RenderTarget *renderTarget = new RenderTarget(device, backbufferRT->getWidth(), backbufferRT->getHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, NoMSAA(), false);
    device->CopyResource(*renderTarget, *backbufferRT);
    V(D3DX10SaveTextureToFile(*renderTarget, D3DX10_IFF_PNG, commandlineOptions.dst.c_str()));
    SAFE_DELETE(renderTarget);
}


void runBenchmark() {
    benchmarkFile << profileTimer->getSection(L"SMAA") << endl;

    int next = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() + 1;
    int n = hud.GetComboBox(IDC_INPUT)->GetNumItems();
    hud.GetComboBox(IDC_INPUT)->SetSelectedByIndex(next < n? next : n);
    loadImage();

    profileTimer->reset();

    if (next == n) {
        benchmark = false;
        benchmarkFile.close();
    }
}


void drawTextures(ID3D10Device *device) {
    switch (int(hud.GetComboBox(IDC_VIEW_MODE)->GetSelectedData())) {
        case 1:
            Copy::go(*smaa->getEdgesRenderTarget(), *backbufferRT);
            break;
        case 2:
            Copy::go(*smaa->getBlendRenderTarget(), *backbufferRT);
            break;
        default:
            break;
    }
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
        txtHelper->DrawTextLine(L"Press 'tab' to toogle the HUD, 'a' and 'd' to quickly cycle through the images");

        txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.5f, 0.0f, 1.0f));
        if (profileTimer->isEnabled()) {
            wstringstream s;
            s << setprecision(2) << std::fixed;
            s << *profileTimer;
            txtHelper->DrawTextLine(s.str().c_str());
        }

        wstringstream s;
        if (recPlayState == RECPLAY_RECORDING) {
            s << setprecision(2) << std::fixed << L"Recording: " << (DXUTGetTime() - recPlayTime);
            txtHelper->DrawTextLine(s.str().c_str());
        } else if (recPlayState == RECPLAY_PLAYING) {
            s << setprecision(2) << std::fixed << L"Playing: " << (DXUTGetTime() - recPlayTime);
            txtHelper->DrawTextLine(s.str().c_str());
        }

        txtHelper->End();
    }
    D3DPERF_EndEvent();
}


void CALLBACK onFrameRender(ID3D10Device *device, double time, float elapsedTime, void *context) {
    framerateLockTimer->start();

    // Render the settings dialog:
    if (settingsDialog.IsActive()) {
        settingsDialog.OnRender(elapsedTime);
        return;
    }

    // Clear render targets:
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    device->ClearRenderTargetView(*tmpRT_SRGB, clearColor);
    device->ClearRenderTargetView(*velocityRT, clearColor);
    device->ClearDepthStencilView(*depthStencil1x, D3D10_CLEAR_STENCIL, 1.0, 0);
    device->ClearDepthStencilView(*depthStencil, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0);

    // Render the scene:
    renderScene(device);

    // Run SMAA or MSAA:
    SMAA::Mode mode = SMAA::Mode(int(hud.GetComboBox(IDC_AA_MODE)->GetSelectedData()));
    switch (mode) {
        case SMAA::MODE_SMAA_1X:
        case SMAA::MODE_SMAA_T2X:
        case SMAA::MODE_SMAA_S2X:
        case SMAA::MODE_SMAA_4X:
            runSMAA(device, mode);
            break;
        default: // MSAA mode
            device->ResolveSubresource(*backbufferRT, 0, *tmpRT_SRGB, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            break;
    }

    // Save the image to a file, if required:
    if (commandlineOptions.dst != L"") {
        saveBackbuffer(device);
        exit(0);
    }

    // Run the benchmark, if required:
    if (benchmark)
        runBenchmark();

    // Draw the HUD:
    drawTextures(device);
    drawHud(elapsedTime);

    // Lock the frame rate:
    FramerateLock framerateLock = FramerateLock(int(hud.GetComboBox(IDC_LOCK_FRAMERATE)->GetSelectedData()));
    if (framerateLock != FPS_UNLOCK)
        framerateLockTimer->sleep(int(framerateLock) / 1000.0f);
}


void CALLBACK onFrameMove(double time, float elapsedTime, void *context) {
    camera.frameMove(elapsedTime);

    if (recPlayState == RECPLAY_PLAYING &&
        DXUTGetTime() - recPlayTime > recPlayDuration) {

        recPlayState = RECPLAY_IDLE;
        camera.setAngularVelocity(D3DXVECTOR2(0.0f, 0.0f));
    }
}


LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, bool *finished, void *context) {
    *finished = dialogResourceManager.MsgProc(hwnd, msg, wparam, lparam);
    if (*finished)
        return 0;

    if (settingsDialog.IsActive()) {
        settingsDialog.MsgProc(hwnd, msg, wparam, lparam);
        return 0;
    }

    *finished = hud.MsgProc(hwnd, msg, wparam, lparam);
    if (*finished)
        return 0;

    if (camera.handleMessages(hwnd, msg, wparam, lparam)) {
        switch(msg) {
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
                if (recPlayState == RECPLAY_PLAYING) {
                    recPlayState = RECPLAY_IDLE;
                    camera.setAngularVelocity(D3DXVECTOR2(0.0f, 0.0f));
                }
                break;
         }
         return 0;
    }

    return 0;
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


void CALLBACK keyboardProc(UINT nchar, bool keyDown, bool altDown, void *context) {
    HRESULT hr;

    if (keyDown)
    switch (nchar) {
        case VK_TAB: {
            showHud = !showHud;
            break;
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5': {
            hud.GetComboBox(IDC_PRESET)->SetSelectedByIndex(nchar - '1');
            SMAA::Preset selected = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
            setVisibleCustomControls(selected == SMAA::PRESET_CUSTOM);
            onReleasingSwapChain(NULL);
            onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            break;
        }
        case 'A': {
            int previous = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() - 1;
            hud.GetComboBox(IDC_INPUT)->SetSelectedByIndex(previous > 0? previous : 0);
            V(loadInput());
            break;
        }          
        case 'D': {
            int next = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() + 1;
            int n = hud.GetComboBox(IDC_INPUT)->GetNumItems();
            hud.GetComboBox(IDC_INPUT)->SetSelectedByIndex(next < n? next : n);
            V(loadInput());
            break;
        }
        case 'P':
            hud.GetCheckBox(IDC_PROFILE)->SetChecked(true);
            profileTimer->setEnabled(true);
            benchmark = true;
            benchmarkFile.open("Benchmark.txt",  ios_base::out);
            hud.GetComboBox(IDC_INPUT)->SetSelectedByIndex(1);
            V(loadInput());
            break;
        case 'R':
            if (recPlayState != RECPLAY_RECORDING) {
                recPlayState = RECPLAY_RECORDING;
                recPlayTime = DXUTGetTime();
                fstream f("Movie.txt", fstream::out);
                f << camera;
            } else {
                recPlayState = RECPLAY_IDLE;
                fstream f("Movie.txt", fstream::out | fstream::app);
                f << (DXUTGetTime() - recPlayTime);
            }
            break;
        case 'T':
            if (recPlayState == RECPLAY_RECORDING) {
                fstream f("Movie.txt", fstream::out | fstream::app);
                f << (DXUTGetTime() - recPlayTime);
            }
            recPlayState = RECPLAY_PLAYING;
            recPlayTime = DXUTGetTime();
            fstream f("Movie.txt", fstream::in);
            f >> camera;
            f >> recPlayDuration;
            break;
    }
}


void setAdapter(DXUTDeviceSettings *settings) {
    HRESULT hr;

    // Look for 'NVIDIA PerfHUD' adapter. If it is present, override default settings.
    IDXGIFactory *factory;
    V(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) &factory));

    // Search for a PerfHUD adapter.
    IDXGIAdapter *adapter = NULL;
    int i = 0;
    while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        if (adapter) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                const bool isPerfHUD = wcscmp(desc.Description, L"NVIDIA PerfHUD") == 0;

                if(isPerfHUD) {
                    // IMPORTANT: we modified DXUT for this to work. Search for <PERFHUD_FIX> in DXUT.cpp
                    settings->d3d10.AdapterOrdinal = i;
                    settings->d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
                    break;
                }
            }
        }
        i++;
    }
}


bool CALLBACK modifyDeviceSettings(DXUTDeviceSettings *settings, void *context) {
    setAdapter(settings);

    settingsDialog.GetDialogControl()->GetComboBox(DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT)->SetEnabled(false);
    settingsDialog.GetDialogControl()->GetComboBox(DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY)->SetEnabled(false);
    settingsDialog.GetDialogControl()->GetStatic(DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL)->SetEnabled(false);
    settingsDialog.GetDialogControl()->GetStatic(DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL)->SetEnabled(false);
    settings->d3d10.AutoCreateDepthStencil = false;
    settings->d3d10.sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    return true;
}


void buildInputComboBox() {
    hud.GetComboBox(IDC_INPUT)->RemoveAllItems();

    hud.GetComboBox(IDC_INPUT)->AddItem(L"Fence", NULL);

    for (int i = 0; i < 7; i++) {
        wstringstream s;
        s.fill('0');
        s << L"Unigine" << setw(2) << i + 1 << ".png";
        hud.GetComboBox(IDC_INPUT)->AddItem(s.str().c_str(), NULL);
    }

    WIN32_FIND_DATA data;
    HANDLE handle = FindFirstFile(L"Images\\*.png", &data);
    if (handle != INVALID_HANDLE_VALUE){
        do {
            hud.GetComboBox(IDC_INPUT)->AddItem((L"Images\\" + wstring(data.cFileName)).c_str(), (LPVOID) -1);
        } while(FindNextFile(handle, &data));
        FindClose(handle);
    }
}


void CALLBACK onGUIEvent(UINT event, int id, CDXUTControl *control, void *context) {
    HRESULT hr;

    switch (id) {
        case IDC_TOGGLE_FULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_CHANGE_DEVICE:
            settingsDialog.SetActive(!settingsDialog.IsActive());
            break;
        case IDC_LOAD_IMAGE: {
            if (!DXUTIsWindowed()) 
                DXUTToggleFullScreen();

            OPENFILENAME params;
            ZeroMemory(&params, sizeof(OPENFILENAME));
            
            const int maxLength = 512;
            WCHAR file[maxLength]; 
            file[0] = 0;

            params.lStructSize   = sizeof(OPENFILENAME);
            params.hwndOwner     = DXUTGetHWND();
            params.lpstrFilter   = L"Image Files (*.bmp, *.jpg, *.png)\0*.bmp;*.jpg;*.png\0\0";
            params.nFilterIndex  = 1;
            params.lpstrFile     = file;
            params.nMaxFile      = maxLength;
            params.lpstrTitle    = L"Open File";
            params.lpstrInitialDir = file;
            params.nMaxFileTitle = sizeof(params.lpstrTitle);
            params.Flags         = OFN_FILEMUSTEXIST;
            params.dwReserved    = OFN_EXPLORER;

            if (GetOpenFileName(&params)) {
                buildInputComboBox();
                hud.GetComboBox(IDC_INPUT)->AddItem(file, (LPVOID) -1);
                hud.GetComboBox(IDC_INPUT)->SetSelectedByData((LPVOID) -1);

                V(loadInput());
                resizeWindow();
                onReleasingSwapChain(NULL);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            }
        }
        case IDC_INPUT:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                profileTimer->reset();
                V(loadInput());
                resizeWindow();
                onReleasingSwapChain(NULL);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);
            }
            break;
        case IDC_VIEW_MODE:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                if (int(hud.GetComboBox(IDC_VIEW_MODE)->GetSelectedData()) > 0) {
                    hud.GetCheckBox(IDC_ANTIALIASING)->SetChecked(true);
                }
            }
            break;
        case IDC_AA_MODE: {
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                setModeControls();

                onReleasingSwapChain(NULL);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), NULL);

                // Refill the temporal buffer:
                onFrameRender(DXUTGetD3D10Device(), DXUTGetTime(), DXUTGetElapsedTime(), NULL);
            }
            break;
        }
        case IDC_PRESET:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                SMAA::Preset selected = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
                setVisibleCustomControls(selected == SMAA::PRESET_CUSTOM);

                SAFE_DELETE(smaa);
                initSMAA(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            }
            break;
        case IDC_ANTIALIASING:
            if (event == EVENT_CHECKBOX_CHANGED) {
                profileTimer->reset();
                hud.GetComboBox(IDC_VIEW_MODE)->SetSelectedByIndex(0);

                // Refill the temporal buffer:
                onFrameRender(DXUTGetD3D10Device(), DXUTGetTime(), DXUTGetElapsedTime(), NULL);
            }
            break;
        case IDC_PREDICATION:
        case IDC_REPROJECTION:
            if (event == EVENT_CHECKBOX_CHANGED) {
                SAFE_DELETE(smaa);
                initSMAA(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            }
            break;
        case IDC_PROFILE:
            if (event == EVENT_CHECKBOX_CHANGED) {
                profileTimer->reset();
                profileTimer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetChecked());
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
                smaa->setMaxSearchSteps(int(round(scale * 98.0f)));

                wstringstream s;
                s << L"Max Search Steps: " << int(round(scale * 98.0f));
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
        default:
            break;
    }
}


void initApp() {
    settingsDialog.Init(&dialogResourceManager);
    hud.Init(&dialogResourceManager);
    hud.SetCallback(onGUIEvent);

    int iY = 10;

    hud.AddButton(IDC_TOGGLE_FULLSCREEN, L"Toggle full screen", 35, iY, HUD_WIDTH, 22);
    hud.AddButton(IDC_CHANGE_DEVICE, L"Change device", 35, iY += 24, HUD_WIDTH, 22, VK_F2);
    hud.AddButton(IDC_LOAD_IMAGE, L"Load image", 35, iY += 24, HUD_WIDTH, 22);

    iY += 24;

    hud.AddComboBox(IDC_INPUT, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    buildInputComboBox();
    if (commandlineOptions.src != L"") {
        hud.GetComboBox(IDC_INPUT)->AddItem(commandlineOptions.src.c_str(), (LPVOID) -1);
        hud.GetComboBox(IDC_INPUT)->SetSelectedByData((LPVOID) -1);
    }

    hud.AddComboBox(IDC_VIEW_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_VIEW_MODE)->AddItem(L"View image", (LPVOID) 0);
    hud.GetComboBox(IDC_VIEW_MODE)->AddItem(L"View edges", (LPVOID) 1);
    hud.GetComboBox(IDC_VIEW_MODE)->AddItem(L"View weights", (LPVOID) 2);

    iY += 24;

    hud.AddComboBox(IDC_AA_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_AA_MODE)->SetDropHeight(120);

    hud.AddComboBox(IDC_PRESET, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Low", (LPVOID) SMAA::PRESET_LOW);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Medium", (LPVOID) SMAA::PRESET_MEDIUM);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA High", (LPVOID) SMAA::PRESET_HIGH);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Ultra", (LPVOID) SMAA::PRESET_ULTRA);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Custom", (LPVOID) SMAA::PRESET_CUSTOM);
    hud.GetComboBox(IDC_PRESET)->SetSelectedByData((LPVOID) SMAA::PRESET_HIGH);

    hud.AddComboBox(IDC_DETECTION_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);

    hud.AddCheckBox(IDC_ANTIALIASING, L"Enable SMAA", 35, iY += 24, HUD_WIDTH, 22, true, 'Z');
    hud.AddCheckBox(IDC_PREDICATION, L"Predicated Tresholding", 35, iY += 24, HUD_WIDTH, 22, false);
    hud.AddCheckBox(IDC_REPROJECTION, L"Temporal Reprojection", 35, iY += 24, HUD_WIDTH, 22, true);

    iY += 24;

    hud.AddComboBox(IDC_LOCK_FRAMERATE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Unlock Framerate", (LPVOID) FPS_UNLOCK);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 15fps", (LPVOID) FPS_LOCK_TO_15);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 30fps", (LPVOID) FPS_LOCK_TO_30);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 60fps", (LPVOID) FPS_LOCK_TO_60);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->SetSelectedByData((LPVOID) FPS_LOCK_TO_30);
    hud.AddCheckBox(IDC_PROFILE, L"Profile", 35, iY += 24, HUD_WIDTH, 22, false, 'X');
    hud.AddCheckBox(IDC_SHADING, L"Shading", 35, iY += 24, HUD_WIDTH, 22, false, 'S');

    iY += 24;

    wstringstream s;
    s << L"Threshold: " << commandlineOptions.threshold;
    hud.AddStatic(IDC_THRESHOLD_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_THRESHOLD, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.threshold / 0.5f));
    hud.GetStatic(IDC_THRESHOLD_LABEL)->SetVisible(false);
    hud.GetSlider(IDC_THRESHOLD)->SetVisible(false);

    s = wstringstream();
    s << L"Max Search Steps: " << commandlineOptions.searchSteps;
    hud.AddStatic(IDC_MAX_SEARCH_STEPS_LABEL, s.str().c_str(), 35, iY += 24, HUD_WIDTH, 22);
    hud.AddSlider(IDC_MAX_SEARCH_STEPS, 35, iY += 24, HUD_WIDTH, 22, 0, 100, int(100.0f * commandlineOptions.searchSteps / 98.0f));
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


void parseCommandLine() {
    // Cheap, hackish, but simple command-line parsing:
    //   Demo.exe <threshold:float> <searchSteps:int> <diagSearchSteps:int> <cornerRounding:float> <file.png in:str> <file.png out:str>
    wstringstream s(GetCommandLineW());

    wstring executable;
    s >> executable;

    s >> commandlineOptions.threshold;
    if (!s) return;
    commandlineOptions.threshold = max(min(commandlineOptions.threshold, 0.5f), 0.0f);

    s >> commandlineOptions.searchSteps;
    if (!s) return;
    commandlineOptions.searchSteps = max(min(commandlineOptions.searchSteps, 98), 0);

    s >> commandlineOptions.diagSearchSteps;
    if (!s) return;
    commandlineOptions.diagSearchSteps = max(min(commandlineOptions.diagSearchSteps, 20), 0);

    s >> commandlineOptions.cornerRounding;
    if (!s) return;
    commandlineOptions.cornerRounding = max(min(commandlineOptions.cornerRounding, 100.0f), 0.0f);

    s >> commandlineOptions.src;
    if (!s) return;

    s >> commandlineOptions.dst;
    if (!s) return;
}


INT WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // Enable run-time memory check for debug builds.
    #if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif

    parseCommandLine();

    DXUTSetCallbackD3D10DeviceAcceptable(isDeviceAcceptable);
    DXUTSetCallbackD3D10DeviceCreated(onCreateDevice);
    DXUTSetCallbackD3D10DeviceDestroyed(onDestroyDevice);
    DXUTSetCallbackD3D10SwapChainResized(onResizedSwapChain);
    DXUTSetCallbackD3D10SwapChainReleasing(onReleasingSwapChain);
    DXUTSetCallbackD3D10FrameRender(onFrameRender);
    DXUTSetCallbackFrameMove(onFrameMove);

    DXUTSetCallbackMsgProc(msgProc);
    DXUTSetCallbackKeyboard(keyboardProc);
    DXUTSetCallbackDeviceChanging(modifyDeviceSettings);

    initApp();

    if (FAILED(DXUTInit(true, true, L"-forcevsync:0")))
        return -1;

    DXUTSetCursorSettings(true, true);
    if (FAILED(DXUTCreateWindow(L"SMAA: Subpixel Morphological Antialiasing")))
        return -1;

    if (FAILED(DXUTCreateDevice(true, 1280, 720)))
        return -1;

    resizeWindow();

    /**
     * A note to myself: we hacked DXUT to not show the window by default.
     * See <WINDOW_FIX> in DXUT.h
     */
    if (commandlineOptions.dst == L"")
        ShowWindow(DXUTGetHWND(), SW_SHOW);

    DXUTMainLoop();

    return DXUTGetExitCode();
}
