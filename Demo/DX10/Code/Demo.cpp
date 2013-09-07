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
#include "SDKmesh.h"
#include "SDKmisc.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "Camera.h"
#include "Copy.h"
#include "RenderTarget.h"
#include "SMAA.h"
#include "Timer.h"

using namespace std;


const int HUD_WIDTH = 140;

enum IDC {
    IDC_TOGGLE_FULLSCREEN,
    IDC_CHANGE_DEVICE,
    IDC_LOAD_IMAGE,
    IDC_INPUT,
    IDC_VIEW_MODE,
    IDC_AA_MODE,
    IDC_PRESET,
    IDC_DETECTION_MODE,
    IDC_ANTIALIASING,
    IDC_PREDICATION,
    IDC_REPROJECTION,
    IDC_LOCK_FRAMERATE,
    IDC_PROFILE,
    IDC_SHADING,
    IDC_THRESHOLD_LABEL,
    IDC_THRESHOLD,
    IDC_MAX_SEARCH_STEPS_LABEL,
    IDC_MAX_SEARCH_STEPS,
    IDC_MAX_SEARCH_STEPS_DIAG_LABEL,
    IDC_MAX_SEARCH_STEPS_DIAG,
    IDC_CORNER_ROUNDING_LABEL,
    IDC_CORNER_ROUNDING
};

CDXUTDialogResourceManager dialogResourceManager;
CD3DSettingsDlg settingsDialog;
CDXUTDialog hud;

Timer *timer = nullptr;
ID3DX10Font *font = nullptr;
ID3DX10Sprite *sprite = nullptr;
CDXUTTextHelper *txtHelper = nullptr;

SMAA *smaa = nullptr;

DepthStencil *depthStencil = nullptr;
DepthStencil *depthStencil1x = nullptr;

RenderTarget *tmpRT_SRGB = nullptr;
RenderTarget *tmpRT = nullptr;
RenderTarget *tmp1xRT[2] = { nullptr, nullptr };
RenderTarget *tmp1xRT_SRGB[2] = { nullptr, nullptr };
RenderTarget *depthBufferRT = nullptr;
RenderTarget *velocityRT = nullptr;
RenderTarget *velocity1xRT = nullptr;
RenderTarget *finalRT[2] = { nullptr, nullptr };
BackbufferRenderTarget *backbufferRT = nullptr;

ID3D10ShaderResourceView *inputColorSRV = nullptr;
ID3D10ShaderResourceView *inputDepthSRV = nullptr;

Camera camera;
CDXUTSDKMesh mesh;
ID3D10InputLayout *vertexLayout = nullptr;
ID3D10Effect *simpleEffect = nullptr;
ID3D10ShaderResourceView *envTexSRV = nullptr;

bool showHud = true;

bool benchmark = false;
fstream benchmarkFile;

enum RecPlayState { RECPLAY_IDLE, RECPLAY_RECORDING, RECPLAY_PLAYING } recPlayState;
double recPlayTime = 0.0;
double recPlayDuration = 0.0;

D3DXMATRIX prevViewProj, currViewProj;

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
        V_RETURN(D3DX10CreateShaderResourceViewFromResource(DXUTGetD3D10Device(), GetModuleHandle(nullptr), text, &loadInfo, nullptr, &inputColorSRV, nullptr));

        // Try to load depth
        loadInfo.Format = DXGI_FORMAT_R32_FLOAT;
        loadInfo.Filter = D3DX10_FILTER_POINT;

        wstring path = wstring(text).substr(0, wstring(text).find_last_of('.')) + L".dds";
        if (FindResource(GetModuleHandle(nullptr), path.c_str(), RT_RCDATA) != nullptr)
            V_RETURN(D3DX10CreateShaderResourceViewFromResource(DXUTGetD3D10Device(), GetModuleHandle(nullptr), path.c_str(), &loadInfo, nullptr, &inputDepthSRV, nullptr));
    } else { // ... search for it in the file system
        ID3D10ShaderResourceView *colorSRV= nullptr;
        if (FAILED(D3DX10CreateShaderResourceViewFromFile(DXUTGetD3D10Device(), text, &loadInfo, nullptr, &colorSRV, nullptr))) {
            MessageBox(nullptr, L"Unable to open selected file", L"ERROR", MB_OK | MB_SETFOREGROUND | MB_TOPMOST);
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
            if (SUCCEEDED(D3DX10CreateShaderResourceViewFromFile(DXUTGetD3D10Device(), path.c_str(), &loadInfo, nullptr, &depthSRV, nullptr)))
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
        V(D3DX10GetImageInfoFromResource(GetModuleHandle(nullptr), s.str().c_str(), nullptr, &info, nullptr));

        D3DX10_IMAGE_LOAD_INFO loadInfo = D3DX10_IMAGE_LOAD_INFO();
        loadInfo.pSrcInfo = &info;
        if (srgb) {
            loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
            loadInfo.Format = MAKE_SRGB(info.Format);
        }
        V(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(nullptr), s.str().c_str(), &loadInfo, nullptr, shaderResourceView, nullptr));
    }
}


HRESULT loadMesh(CDXUTSDKMesh &mesh, const wstring &name, const wstring &path) {
    HRESULT hr;

    HRSRC src = FindResource(GetModuleHandle(nullptr), name.c_str(), RT_RCDATA);
    HGLOBAL res = LoadResource(GetModuleHandle(nullptr), src);
    UINT size = SizeofResource(GetModuleHandle(nullptr), src);
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
    hud.GetCheckBox(IDC_PREDICATION)->SetEnabled(!isMsaa && inputDepthSRV != nullptr);
    hud.GetCheckBox(IDC_REPROJECTION)->SetEnabled(!isMsaa && isTemporalMode);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->SetEnabled(!isMsaa);
    hud.GetCheckBox(IDC_PROFILE)->SetEnabled(!isMsaa && hud.GetComboBox(IDC_LOCK_FRAMERATE)->GetSelectedIndex() == 0);
    hud.GetSlider(IDC_THRESHOLD)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG)->SetEnabled(!isMsaa);
    hud.GetSlider(IDC_CORNER_ROUNDING)->SetEnabled(!isMsaa);

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
    if (inputDepthSRV != nullptr)
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

    V(D3DX10CreateEffectFromResource(GetModuleHandle(nullptr), L"Simple.fx", nullptr, nullptr, nullptr, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, device, nullptr, nullptr, &simpleEffect, nullptr, nullptr));

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
    txtHelper = new CDXUTTextHelper(nullptr, nullptr, font, sprite, 15);

    timer = new Timer(device);
    timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetEnabled() && hud.GetCheckBox(IDC_PROFILE)->GetChecked());

    Copy::init(device);
    V_RETURN(initSimpleEffect(device));

    camera.setAngle(D3DXVECTOR2(30.0f, 0.0f));
    camera.setDistance(60.0f);

    V_RETURN(loadInput());

    D3DX10_IMAGE_LOAD_INFO loadInfo = D3DX10_IMAGE_LOAD_INFO();
    loadInfo.Filter = D3DX10_FILTER_POINT | D3DX10_FILTER_SRGB_IN;
    V_RETURN(D3DX10CreateShaderResourceViewFromResource(device, GetModuleHandle(nullptr), L"EnvMap.dds", &loadInfo, nullptr, &envTexSRV, nullptr));

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

    SAFE_DELETE(timer);

    Copy::release();
    SAFE_RELEASE(simpleEffect);
    SAFE_RELEASE(vertexLayout);

    SAFE_RELEASE(inputColorSRV);
    SAFE_RELEASE(inputDepthSRV);
    mesh.Destroy();

    SAFE_RELEASE(envTexSRV);
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


void initSMAA(ID3D10Device *device, const DXGI_SURFACE_DESC *desc) {
    SMAA::Preset preset = SMAA::Preset(int(hud.GetComboBox(IDC_PRESET)->GetSelectedData()));
    bool predication = hud.GetCheckBox(IDC_PREDICATION)->GetEnabled() && hud.GetCheckBox(IDC_PREDICATION)->GetChecked();
    bool reprojection = hud.GetCheckBox(IDC_REPROJECTION)->GetEnabled() && hud.GetCheckBox(IDC_REPROJECTION)->GetChecked();
    DXGI_ADAPTER_DESC adapterDesc;
    DXUTGetDXGIAdapter()->GetDesc(&adapterDesc); // This seems to be the only reliable way to obtain the adapter in laptops with multiple GPUs, with DXUT

    smaa = new SMAA(device, desc->Width, desc->Height, preset, predication, reprojection, &adapterDesc);
    setVisibleCustomControls(preset == SMAA::PRESET_CUSTOM);

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


void renderMesh(ID3D10Device *device) {
    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Fetch the SMAA parameters:
    bool smaaEnabled = hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked() &&
                       hud.GetCheckBox(IDC_ANTIALIASING)->GetEnabled();
    SMAA::Mode mode = SMAA::Mode(int(hud.GetComboBox(IDC_AA_MODE)->GetSelectedData()));

    // Set the render targets:
    ID3D10RenderTargetView *rt[] = { *tmpRT_SRGB, *velocityRT };
    device->OMSetRenderTargets(2, rt, *depthStencil);

    // Calculate current view-projection matrix:
    currViewProj = camera.getViewMatrix() * camera.getProjectionMatrix();

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

            if (smaaEnabled) {
                currWorldViewProj = smaa->JitteredMatrix(currWorldViewProj, mode);
                prevWorldViewProj = smaa->JitteredMatrix(prevWorldViewProj, mode);
            }

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
}


void renderScene(ID3D10Device *device) {
    PerfEventScope perfEventScope(L"Render Scene");

    // Copy the image or render the mesh:
    bool fromImage = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0;
    if (fromImage) {
        D3D10_VIEWPORT viewport = Utils::viewportFromView(inputColorSRV);
        Copy::go(inputColorSRV, *tmpRT_SRGB, &viewport);
        Copy::go(inputDepthSRV, *depthBufferRT, &viewport);
    } else {
        renderMesh(device);
    }

    device->ResolveSubresource(*velocity1xRT, 0, *velocityRT, 0, DXGI_FORMAT_R16G16_FLOAT);
}


void runSMAA(ID3D10Device *device, SMAA::Mode mode) {
    // Calculate next subpixel index:
    int previousIndex = smaa->getFrameIndex();
    int currentIndex = (smaa->getFrameIndex() + 1) % 2;

    // Fetch configuration parameters:
    bool smaaEnabled = hud.GetCheckBox(IDC_ANTIALIASING)->GetChecked() &&
                       hud.GetCheckBox(IDC_ANTIALIASING)->GetEnabled();
    SMAA::Input input = SMAA::Input(int(hud.GetComboBox(IDC_DETECTION_MODE)->GetSelectedData()));

    // Run SMAA:
    if (smaaEnabled) {
        timer->start(L"SMAA");
        switch (mode) {
            case SMAA::MODE_SMAA_1X:
                smaa->go(*tmpRT, *tmpRT_SRGB, *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode);
                break;
            case SMAA::MODE_SMAA_T2X:
                smaa->go(*tmpRT, *tmpRT_SRGB, *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode);
                smaa->reproject(*finalRT[currentIndex], *finalRT[previousIndex], *velocityRT, *backbufferRT);
                break;
            case SMAA::MODE_SMAA_S2X:
                smaa->separate(*tmpRT, *tmp1xRT[0], *tmp1xRT[1]);
                smaa->go(*tmp1xRT[0], *tmp1xRT_SRGB[0], *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode, 0);
                smaa->go(*tmp1xRT[1], *tmp1xRT_SRGB[1], *depthBufferRT, *backbufferRT, *depthStencil1x, input, mode, 1);
                break;
            case SMAA::MODE_SMAA_4X:
                smaa->separate(*tmpRT, *tmp1xRT[0], *tmp1xRT[1]);
                smaa->go(*tmp1xRT[0], *tmp1xRT_SRGB[0], *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode, 0);
                smaa->go(*tmp1xRT[1], *tmp1xRT_SRGB[1], *depthBufferRT, *finalRT[currentIndex], *depthStencil1x, input, mode, 1);
                smaa->reproject(*finalRT[currentIndex], *finalRT[previousIndex], *velocity1xRT, *backbufferRT);
                break;
        }
        timer->end(L"SMAA");
        timer->endFrame();
    } else {
        if (mode == SMAA::MODE_SMAA_1X)
            Copy::go(*tmpRT_SRGB, *backbufferRT);
        else
            device->ResolveSubresource(*backbufferRT, 0, *tmpRT_SRGB, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }

    // Update subpixel index:
    smaa->nextFrame();
}


void saveBackbuffer(ID3D10Device *device) {
    HRESULT hr;
    RenderTarget *renderTarget = new RenderTarget(device, backbufferRT->getWidth(), backbufferRT->getHeight(), DXGI_FORMAT_R8G8B8A8_UNORM, NoMSAA(), false);
    device->CopyResource(*renderTarget, *backbufferRT);
    V(D3DX10SaveTextureToFile(*renderTarget, D3DX10_IFF_PNG, commandlineOptions.dst.c_str()));
    SAFE_DELETE(renderTarget);
}


void runBenchmark() {
    benchmarkFile << timer << endl;

    int next = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() + 1;
    int n = hud.GetComboBox(IDC_INPUT)->GetNumItems();
    hud.GetComboBox(IDC_INPUT)->SetSelectedByIndex(next < n? next : n);
    loadImage();

    timer->reset();

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

    PerfEventScope perfEventScope(L"HUD");

    if (showHud) {
        V(hud.OnRender(elapsedTime));

        txtHelper->Begin();

        txtHelper->SetInsertionPos(2, 0);
        txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
        txtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        txtHelper->DrawTextLine(DXUTGetDeviceStats());
        txtHelper->DrawTextLine(L"Press 'tab' to toogle the HUD, 'a' and 'd' to quickly cycle through the images");

        txtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.5f, 0.0f, 1.0f));
        if (timer->isEnabled()) {
            wstringstream s;
            s << setprecision(2) << std::fixed;
            s << *timer;
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
}


void CALLBACK onFrameRender(ID3D10Device *device, double time, float elapsedTime, void *context) {
    {
        PerfEventScope perfEvent(L"Preamble");

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
    }

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
        case '5':
            hud.GetComboBox(IDC_PRESET)->SetSelectedByIndex(nchar - '1');
            SAFE_DELETE(smaa);
            initSMAA(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            break;
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
            timer->setEnabled(true);
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


bool CALLBACK modifyDeviceSettings(DXUTDeviceSettings *settings, void *context) {
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

    hud.GetComboBox(IDC_INPUT)->AddItem(L"Fence", nullptr);

    for (int i = 0; i < 7; i++) {
        wstringstream s;
        s.fill('0');
        s << L"Unigine" << setw(2) << i + 1 << ".png";
        hud.GetComboBox(IDC_INPUT)->AddItem(s.str().c_str(), nullptr);
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
                onReleasingSwapChain(nullptr);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), nullptr);
            }
        }
        case IDC_INPUT:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                timer->reset();
                V(loadInput());
                resizeWindow();
                onReleasingSwapChain(nullptr);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), nullptr);
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

                onReleasingSwapChain(nullptr);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), nullptr);

                // Refill the temporal buffer:
                onFrameRender(DXUTGetD3D10Device(), DXUTGetTime(), DXUTGetElapsedTime(), nullptr);
            }
            break;
        }
        case IDC_PRESET:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                SAFE_DELETE(smaa);
                initSMAA(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            }
            break;
        case IDC_ANTIALIASING:
            if (event == EVENT_CHECKBOX_CHANGED) {
                timer->reset();
                hud.GetComboBox(IDC_VIEW_MODE)->SetSelectedByIndex(0);

                // Refill the temporal buffer:
                onFrameRender(DXUTGetD3D10Device(), DXUTGetTime(), DXUTGetElapsedTime(), nullptr);
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
                timer->reset();
                timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetEnabled() && hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            }
            break;
        case IDC_LOCK_FRAMERATE:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                DXUTSetSyncInterval(int(hud.GetComboBox(IDC_LOCK_FRAMERATE)->GetSelectedData()));
                setModeControls();
                timer->reset();
                timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetEnabled() && hud.GetCheckBox(IDC_PROFILE)->GetChecked());
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
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Unlock Framerate", (LPVOID) 0);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 60fps", (LPVOID) 1);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 30fps", (LPVOID) 2);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 20fps", (LPVOID) 3);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 15fps", (LPVOID) 4);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->SetSelectedByData((LPVOID) 0);
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
     * See <WINDOW_FIX> in DXUT.h
     */
    if (commandlineOptions.dst == L"")
        ShowWindow(DXUTGetHWND(), SW_SHOW);

    DXUTMainLoop();

    return DXUTGetExitCode();
}
