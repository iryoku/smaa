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
    IDC_PRIMARY_AA_MODE,
    IDC_SECONDARY_AA_MODE,
    IDC_PRESET,
    IDC_DETECTION_MODE,
    IDC_SMAA_FILTERING,
    IDC_PREDICATION,
    IDC_REPROJECTION,
    IDC_LOCK_FRAMERATE,
    IDC_SHADING,
    IDC_PROFILE,
    IDC_COMPARISON_SPLITSCREEN,
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
ID3DX10Sprite *sprite = nullptr;
ID3DX10Font *font[2] = { nullptr, nullptr };
CDXUTTextHelper *txtHelper[2] = { nullptr, nullptr };

SMAA *smaa = nullptr;

BackbufferRenderTarget *backbufferRT = nullptr;

struct RenderTargetCollection {
    DepthStencil *mainDS;
    DepthStencil *mainDS_MS;

    RenderTarget *sceneRT;
    RenderTarget *sceneRT_SRGB;
    RenderTarget *sceneRT_MS;
    RenderTarget *sceneRT_MS_SRGB;

    RenderTarget *velocityRT;
    RenderTarget *velocityRT_MS;

    RenderTarget *tmpRT[2];
    RenderTarget *tmpRT_SRGB[2];

    RenderTarget *previousRT[2];

    // This render target is to ensure we read from a 32-bit depth buffer (to properly measure):
    RenderTarget *depthRT;
};
RenderTargetCollection renderTargetCollection[2];

ID3D10ShaderResourceView *inputColorSRV = nullptr;
ID3D10ShaderResourceView *inputDepthSRV = nullptr;

Camera camera;
CDXUTSDKMesh mesh;
ID3D10InputLayout *vertexLayout = nullptr;
ID3D10Effect *simpleEffect = nullptr;
ID3D10ShaderResourceView *envTexSRV = nullptr;
ID3D10RasterizerState *rasterizerState = nullptr;

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
    UINT vendor;
};
MSAAMode msaaModes[] = {
    {L"SMAA 1x",    {1,  0}, 0x0000}, // MODE_SMAA_1X
    {L"SMAA T2x",   {1,  0}, 0x0000}, // MODE_SMAA_T2X
    {L"SMAA S2x",   {2,  0}, 0x0000}, // MODE_SMAA_S2X
    {L"SMAA 4x",    {2,  0}, 0x0000}, // MODE_SMAA_4X
    {L"MSAA 1x",    {1,  0}, 0x0000},
    {L"MSAA 2x",    {2,  0}, 0x0000},
    {L"MSAA 4x",    {4,  0}, 0x0000},
    {L"MSAA 8x",    {8,  0}, 0x0000},
    {L"CSAA 8x",    {4,  8}, 0x10DE},
    {L"CSAA 8xQ",   {8,  8}, 0x10DE},
    {L"CSAA 16x",   {4, 16}, 0x10DE},
    {L"CSAA 16xQ",  {8, 16}, 0x10DE},
    {L"EQAA 2f4x",  {2,  4}, 0x1002},
    {L"EQAA 4f8x",  {4,  8}, 0x1002},
    {L"EQAA 4f16x", {4, 16}, 0x1002},
    {L"EQAA 8f16x", {8, 16}, 0x1002}
};

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


bool isSplitScreenEnabled() {
    return hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->GetEnabled() && hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->GetChecked();
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
    SMAA::Mode mode = SMAA::Mode(int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
    bool isSMAA = mode <= SMAA::MODE_SMAA_COUNT;
    bool isTemporalMode = mode == SMAA::MODE_SMAA_T2X || mode == SMAA::MODE_SMAA_4X;
    bool isComparisonMode = hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->GetEnabled() && hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->GetChecked();

    hud.GetComboBox(IDC_VIEW_MODE)->SetEnabled(isSMAA);
    hud.GetComboBox(IDC_PRESET)->SetEnabled(isSMAA);
    hud.GetComboBox(IDC_DETECTION_MODE)->SetEnabled(isSMAA);
    hud.GetCheckBox(IDC_SMAA_FILTERING)->SetEnabled(isSMAA);
    hud.GetCheckBox(IDC_PREDICATION)->SetEnabled(isSMAA && inputDepthSRV != nullptr);
    hud.GetCheckBox(IDC_REPROJECTION)->SetEnabled(isSMAA && isTemporalMode);
    hud.GetCheckBox(IDC_PROFILE)->SetEnabled(isSMAA && hud.GetComboBox(IDC_LOCK_FRAMERATE)->GetSelectedIndex() == 0 && !isComparisonMode);
    hud.GetSlider(IDC_THRESHOLD)->SetEnabled(isSMAA);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS)->SetEnabled(isSMAA);
    hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG)->SetEnabled(isSMAA);
    hud.GetSlider(IDC_CORNER_ROUNDING)->SetEnabled(isSMAA);

    hud.GetCheckBox(IDC_SHADING)->SetEnabled(hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() == 0);

    hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetEnabled(isComparisonMode);
}


HRESULT loadInput() {
    HRESULT hr;

    if (hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0) {
        V_RETURN(loadImage());
        hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_1X);
        hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetEnabled(false);
        hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetEnabled(false);
        hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->SetEnabled(false);
    } else {
        SAFE_RELEASE(inputColorSRV);
        SAFE_RELEASE(inputDepthSRV);
        mesh.Destroy();

        V_RETURN(loadMesh(mesh, L"Fence.sdkmesh", L""));
        hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_T2X);
        hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetEnabled(true);
        hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetEnabled(hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->GetChecked());
        hud.GetCheckBox(IDC_COMPARISON_SPLITSCREEN)->SetEnabled(true);
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
    hud.GetComboBox(IDC_PRIMARY_AA_MODE)->RemoveAllItems();
    hud.GetComboBox(IDC_SECONDARY_AA_MODE)->RemoveAllItems();

    DXGI_ADAPTER_DESC adapterDesc;
    DXUTGetDXGIAdapter()->GetDesc(&adapterDesc);

    for(int i = 0; i < sizeof(msaaModes) / sizeof(MSAAMode); i++){
        ID3D10Device *device = DXUTGetD3D10Device();
        UINT quality;
        device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, msaaModes[i].desc.Count, &quality);
        bool matchesVendor = msaaModes[i].vendor == 0x0 || adapterDesc.VendorId == msaaModes[i].vendor;
        if (matchesVendor && quality > msaaModes[i].desc.Quality) {
            hud.GetComboBox(IDC_PRIMARY_AA_MODE)->AddItem(msaaModes[i].name.c_str(), (void *) i);
            hud.GetComboBox(IDC_SECONDARY_AA_MODE)->AddItem(msaaModes[i].name.c_str(), (void *) i);
        }
    }

    hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetSelectedByData((LPVOID) SMAA::MODE_SMAA_T2X);
    hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetSelectedByData((LPVOID) (SMAA::MODE_SMAA_COUNT + 1));
    setModeControls();
}


HRESULT CALLBACK onCreateDevice(ID3D10Device *device, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;

    V_RETURN(dialogResourceManager.OnD3D10CreateDevice(device));
    V_RETURN(settingsDialog.OnD3D10CreateDevice(device));

    V_RETURN(D3DX10CreateSprite(device, 512, &sprite));
    V_RETURN(D3DX10CreateFont(device, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &font[0]));
    V_RETURN(D3DX10CreateFont(device, 30, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &font[1]));
    txtHelper[0] = new CDXUTTextHelper(nullptr, nullptr, font[0], sprite, 15);
    txtHelper[1] = new CDXUTTextHelper(nullptr, nullptr, font[1], sprite, 30);

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

    D3D10_RASTERIZER_DESC rasterizerDesc = { D3D10_FILL_SOLID, D3D10_CULL_BACK, FALSE, 0, 0.0f, 0.0f, TRUE, TRUE, TRUE, FALSE };
    device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
    device->RSSetState(rasterizerState);

    return S_OK;
}


void CALLBACK onDestroyDevice(void *context) {
    dialogResourceManager.OnD3D10DestroyDevice();
    settingsDialog.OnD3D10DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_RELEASE(sprite);
    for (int i = 0; i < 2; i++) {
        SAFE_RELEASE(font[i]);
        SAFE_DELETE(txtHelper[i]);
    }

    SAFE_DELETE(timer);

    Copy::release();
    SAFE_RELEASE(simpleEffect);
    SAFE_RELEASE(vertexLayout);

    SAFE_RELEASE(inputColorSRV);
    SAFE_RELEASE(inputDepthSRV);
    mesh.Destroy();

    SAFE_RELEASE(envTexSRV);

    SAFE_RELEASE(rasterizerState);
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
    smaa->setMaxSearchSteps(int(round(112.0f * scale)));

    slider = hud.GetSlider(IDC_MAX_SEARCH_STEPS_DIAG);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setMaxSearchStepsDiag(int(round(20.0f * scale)));

    slider = hud.GetSlider(IDC_CORNER_ROUNDING);
    slider->GetRange(min, max);
    scale = float(slider->GetValue()) / (max - min);
    smaa->setCornerRounding(100.0f * scale);
}


void initRenderTargetCollection(ID3D10Device *device, const DXGI_SURFACE_DESC *desc, RenderTargetCollection &rtc, int mode) {
    DXGI_SAMPLE_DESC sampleDesc = msaaModes[mode].desc;

    rtc.mainDS = new DepthStencil(device, desc->Width, desc->Height,  DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
    rtc.sceneRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM);
    rtc.sceneRT_SRGB = new RenderTarget(device, *rtc.sceneRT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    rtc.velocityRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT);

    if (msaaModes[mode].desc.Count > 1) {
        rtc.mainDS_MS = new DepthStencil(device, desc->Width, desc->Height,  DXGI_FORMAT_R24G8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, sampleDesc);
        rtc.sceneRT_MS = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM, sampleDesc);
        rtc.sceneRT_MS_SRGB = new RenderTarget(device, *rtc.sceneRT_MS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        rtc.velocityRT_MS = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R16G16_FLOAT, sampleDesc);
    }

    for (int i = 0; i < 2; i++) {
        rtc.tmpRT[i] = new RenderTarget(device,  desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM);
        rtc.tmpRT_SRGB[i] = new RenderTarget(device, *rtc.tmpRT[i], DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }

    for (int i = 0; i < 2; i++)
        rtc.previousRT[i] = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

    rtc.depthRT = new RenderTarget(device, desc->Width, desc->Height, DXGI_FORMAT_R32_FLOAT);
}


void freeRenderTargetCollection(RenderTargetCollection &rtc) {
    SAFE_DELETE(rtc.mainDS);
    SAFE_DELETE(rtc.mainDS_MS);

    SAFE_DELETE(rtc.sceneRT);
    SAFE_DELETE(rtc.sceneRT_SRGB);
    SAFE_DELETE(rtc.sceneRT_MS);
    SAFE_DELETE(rtc.sceneRT_MS_SRGB);

    SAFE_DELETE(rtc.velocityRT);
    SAFE_DELETE(rtc.velocityRT_MS);

    for (int i = 0; i < 2; i++) {
        SAFE_DELETE(rtc.tmpRT[i]);
        SAFE_DELETE(rtc.tmpRT_SRGB[i]);
    }

    for (int i = 0; i < 2; i++)
        SAFE_DELETE(rtc.previousRT[i]);

    SAFE_DELETE(rtc.depthRT);
}


HRESULT CALLBACK onResizedSwapChain(ID3D10Device *device, IDXGISwapChain *swapChain, const DXGI_SURFACE_DESC *desc, void *context) {
    HRESULT hr;
    V_RETURN(dialogResourceManager.OnD3D10ResizedSwapChain(device, desc));
    V_RETURN(settingsDialog.OnD3D10ResizedSwapChain(device, desc));

    hud.SetLocation(desc->Width - (45 + HUD_WIDTH), 0);

    float aspect = (float) desc->Width / desc->Height;
    camera.setProjection(18.0f * D3DX_PI / 180.f, aspect, 0.01f, 100.0f);
    camera.setViewportSize(D3DXVECTOR2(float(desc->Width), float(desc->Height)));

    backbufferRT = new BackbufferRenderTarget(device, DXUTGetDXGISwapChain());
    initSMAA(device, desc);
    initRenderTargetCollection(device, desc, renderTargetCollection[0], int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
    if (isSplitScreenEnabled())
        initRenderTargetCollection(device, desc, renderTargetCollection[1], int(hud.GetComboBox(IDC_SECONDARY_AA_MODE)->GetSelectedData()));

    return S_OK;
}


void CALLBACK onReleasingSwapChain(void *context) {
    dialogResourceManager.OnD3D10ReleasingSwapChain();

    SAFE_DELETE(backbufferRT);
    SAFE_DELETE(smaa);
    freeRenderTargetCollection(renderTargetCollection[0]);
    freeRenderTargetCollection(renderTargetCollection[1]);
}


void clearRenderTargets(ID3D10Device *device, RenderTargetCollection &rtc, int mode) {
    PerfEventScope perfEvent(L"Clear Rendertargets");

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (msaaModes[mode].desc.Count > 1) {
        device->ClearRenderTargetView(*rtc.sceneRT_MS, clearColor);
        device->ClearRenderTargetView(*rtc.velocityRT_MS, clearColor);
        device->ClearDepthStencilView(*rtc.mainDS_MS, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0);
    } else {
        device->ClearRenderTargetView(*rtc.sceneRT, clearColor);
        device->ClearRenderTargetView(*rtc.velocityRT, clearColor);
    }
    device->ClearDepthStencilView(*rtc.mainDS, D3D10_CLEAR_DEPTH | D3D10_CLEAR_STENCIL, 1.0, 0);
}


void setRenderTargets(ID3D10Device *device, RenderTargetCollection &rtc, int mode) {
    if (msaaModes[mode].desc.Count > 1) {
        ID3D10RenderTargetView *rt[] = { *rtc.sceneRT_MS_SRGB, *rtc.velocityRT_MS };
        device->OMSetRenderTargets(2, rt, *rtc.mainDS_MS);
    } else {
        ID3D10RenderTargetView *rt[] = { *rtc.sceneRT_SRGB, *rtc.velocityRT };
        device->OMSetRenderTargets(2, rt, *rtc.mainDS);
    }
}


void renderScene(ID3D10Device *device, RenderTargetCollection &rtc, int mode) {
    PerfEventScope perfEventScope(L"Render Scene");

    HRESULT hr;

    // Save the state:
    SaveViewportsScope saveViewport(device);
    SaveRenderTargetsScope saveRenderTargets(device);
    SaveInputLayoutScope saveInputLayout(device);

    // Set the render targets:
    setRenderTargets(device, rtc, mode);

    // Calculate current view-projection matrix:
    currViewProj = camera.getViewMatrix() * camera.getProjectionMatrix();

    // Set enviroment map for metal reflections:
    V(simpleEffect->GetVariableByName("envTex")->AsShaderResource()->SetResource(envTexSRV));

    // Enable/Disable shading:
    V(simpleEffect->GetVariableByName("shading")->AsScalar()->SetBool(hud.GetCheckBox(IDC_SHADING)->GetChecked()));

    // Set the vertex layout:
    device->IASetInputLayout(vertexLayout);

    // Fetch SMAA parameters:
    bool smaaEnabled = hud.GetCheckBox(IDC_SMAA_FILTERING)->GetChecked() &&
                       mode <= SMAA::MODE_SMAA_COUNT;

    // Render the grid:
    for (float x = -20.0; x <= 20.0; x += 1.0) {
        for (float y = -20.0; y <= 20.0; y += 1.0) {
            D3DXMATRIX world;
            D3DXMatrixTranslation(&world, x, y, 0.0f);

            D3DXMATRIX currWorldViewProj = world * currViewProj;
            D3DXMATRIX prevWorldViewProj = world * prevViewProj;

            if (smaaEnabled) {
                currWorldViewProj = smaa->JitteredMatrix(currWorldViewProj, SMAA::Mode(mode));
                prevWorldViewProj = smaa->JitteredMatrix(prevWorldViewProj, SMAA::Mode(mode));
            }

            V(simpleEffect->GetVariableByName("currWorldViewProj")->AsMatrix()->SetMatrix((float *) currWorldViewProj));
            V(simpleEffect->GetVariableByName("prevWorldViewProj")->AsMatrix()->SetMatrix((float *) prevWorldViewProj));
            V(simpleEffect->GetVariableByName("eyePositionW")->AsVector()->SetFloatVector((float *) &camera.getEyePosition()));
            V(simpleEffect->GetVariableByName("world")->AsMatrix()->SetMatrix((float *) world));
            V(simpleEffect->GetTechniqueByName("Simple")->GetPassByIndex(0)->Apply(0));

            mesh.Render(device, simpleEffect->GetTechniqueByName("Simple"), simpleEffect->GetVariableByName("diffuseTex")->AsShaderResource(), simpleEffect->GetVariableByName("normalTex")->AsShaderResource());
        }
    }

    if (msaaModes[mode].desc.Count > 1)
        device->ResolveSubresource(*rtc.velocityRT, 0, *rtc.velocityRT_MS, 0, DXGI_FORMAT_R16G16_FLOAT);
}


void runSMAA(ID3D10Device *device, RenderTargetCollection &rtc, SMAA::Mode mode) {
    // Calculate next subpixel index:
    int previousIndex = smaa->getFrameIndex();
    int currentIndex = (smaa->getFrameIndex() + 1) % 2;

    // Fetch configuration parameters:
    SMAA::Input input = SMAA::Input(int(hud.GetComboBox(IDC_DETECTION_MODE)->GetSelectedData()));

    // Run SMAA:
    timer->start(L"SMAA");
    switch (mode) {
        case SMAA::MODE_SMAA_1X:
            smaa->go(*rtc.sceneRT, *rtc.sceneRT_SRGB, *rtc.depthRT, nullptr, *backbufferRT, *rtc.mainDS, input, mode);
            break;
        case SMAA::MODE_SMAA_T2X:
            smaa->go(*rtc.sceneRT, *rtc.sceneRT_SRGB, *rtc.depthRT, *rtc.velocityRT, *rtc.previousRT[currentIndex], *rtc.mainDS, input, mode);
            smaa->reproject(*rtc.previousRT[currentIndex], *rtc.previousRT[previousIndex], *rtc.velocityRT, *backbufferRT);
            break;
        case SMAA::MODE_SMAA_S2X:
            smaa->separate(*rtc.sceneRT_MS, *rtc.tmpRT[0], *rtc.tmpRT[1]);
            smaa->go(*rtc.tmpRT[0], *rtc.tmpRT_SRGB[0], *rtc.depthRT, nullptr, *backbufferRT, *rtc.mainDS, input, mode, 0);
            smaa->go(*rtc.tmpRT[1], *rtc.tmpRT_SRGB[1], *rtc.depthRT, nullptr, *backbufferRT, *rtc.mainDS, input, mode, 1);
            break;
        case SMAA::MODE_SMAA_4X:
            smaa->separate(*rtc.sceneRT_MS, *rtc.tmpRT[0], *rtc.tmpRT[1]);
            smaa->go(*rtc.tmpRT[0], *rtc.tmpRT_SRGB[0], *rtc.depthRT, *rtc.velocityRT, *rtc.previousRT[currentIndex], *rtc.mainDS, input, mode, 0);
            smaa->go(*rtc.tmpRT[1], *rtc.tmpRT_SRGB[1], *rtc.depthRT, *rtc.velocityRT, *rtc.previousRT[currentIndex], *rtc.mainDS, input, mode, 1);
            smaa->reproject(*rtc.previousRT[currentIndex], *rtc.previousRT[previousIndex], *rtc.velocityRT, *backbufferRT);
            break;
    }
    timer->end(L"SMAA");
    timer->endFrame();
}


void runAA(ID3D10Device *device, RenderTargetCollection &rtc, int mode) {
    bool smaaEnabled = hud.GetCheckBox(IDC_SMAA_FILTERING)->GetChecked() &&
                       mode <= SMAA::MODE_SMAA_COUNT;

    if (smaaEnabled) {
        switch (mode) {
            case SMAA::MODE_SMAA_1X:
            case SMAA::MODE_SMAA_T2X:
            case SMAA::MODE_SMAA_S2X:
            case SMAA::MODE_SMAA_4X:
                runSMAA(device, rtc, SMAA::Mode(mode));
                break;
            default:
                throw logic_error("unexpected error");
        }
    } else {
        if (msaaModes[mode].desc.Count > 1) {
            device->ResolveSubresource(*rtc.sceneRT_SRGB, 0, *rtc.sceneRT_MS_SRGB, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
            Copy::go(*rtc.sceneRT_SRGB, *backbufferRT);
        } else {
            Copy::go(*rtc.sceneRT_SRGB, *backbufferRT);
        }
    }
}


void render(ID3D10Device *device) {
    PerfEventScope perfEventScope(L"Render");

    // Clear render targets:
    clearRenderTargets(device, renderTargetCollection[0], int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
    if (isSplitScreenEnabled()) {
        clearRenderTargets(device, renderTargetCollection[1], int(hud.GetComboBox(IDC_SECONDARY_AA_MODE)->GetSelectedData()));

        float clearColor[4] = { 1.0f, 0.5f, 0.0f, 0.0f };
        device->ClearRenderTargetView(*backbufferRT, clearColor);
    }

    // Copy the image or render the mesh:
    bool fromImage = hud.GetComboBox(IDC_INPUT)->GetSelectedIndex() > 0;
    if (fromImage) {
        D3D10_VIEWPORT viewport = Utils::viewportFromView(inputColorSRV);
        Copy::go(inputColorSRV, *renderTargetCollection[0].sceneRT_SRGB, &viewport);
        Copy::go(inputDepthSRV, *renderTargetCollection[0].depthRT, &viewport);
        runAA(device, renderTargetCollection[0], int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
    } else {
        const DXGI_SURFACE_DESC *desc = DXUTGetDXGIBackBufferSurfaceDesc();
        D3D10_RECT fullRect = { 0, 0, desc->Width, desc->Height };

        { // First split:
            if (isSplitScreenEnabled()) {
                D3D10_RECT rect = { 0, 0, desc->Width / 2 - 1, desc->Height };
                device->RSSetScissorRects(1, &rect);
            } else {
                device->RSSetScissorRects(1, &fullRect);
            }

            renderScene(device, renderTargetCollection[0], int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
            runAA(device, renderTargetCollection[0], int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData()));
        }

        { // Second split:
            if (isSplitScreenEnabled()) {
                D3D10_RECT splitRect = { desc->Width / 2 + 1, 0, desc->Width, desc->Height };
                device->RSSetScissorRects(1, &splitRect);

                renderScene(device, renderTargetCollection[1], int(hud.GetComboBox(IDC_SECONDARY_AA_MODE)->GetSelectedData()));
                runAA(device, renderTargetCollection[1], int(hud.GetComboBox(IDC_SECONDARY_AA_MODE)->GetSelectedData()));

                device->RSSetScissorRects(1, &fullRect);
            }
        }
    }
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

        txtHelper[0]->Begin();

        txtHelper[0]->SetInsertionPos(2, 0);
        txtHelper[0]->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
        txtHelper[0]->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        txtHelper[0]->DrawTextLine(DXUTGetDeviceStats());
        txtHelper[0]->DrawTextLine(L"Press 'tab' to toogle the HUD, 'a' and 'd' to quickly cycle through the images");

        txtHelper[0]->SetForegroundColor(D3DXCOLOR(1.0f, 0.5f, 0.0f, 1.0f));
        if (timer->isEnabled()) {
            wstringstream s;
            s << setprecision(2) << std::fixed;
            s << *timer;
            txtHelper[0]->DrawTextLine(s.str().c_str());
        }

        wstringstream s;
        if (recPlayState == RECPLAY_RECORDING) {
            s << setprecision(2) << std::fixed << L"Recording: " << (DXUTGetTime() - recPlayTime);
            txtHelper[0]->DrawTextLine(s.str().c_str());
        } else if (recPlayState == RECPLAY_PLAYING) {
            s << setprecision(2) << std::fixed << L"Playing: " << (DXUTGetTime() - recPlayTime);
            txtHelper[0]->DrawTextLine(s.str().c_str());
        }
        txtHelper[0]->End();
    }

    if (isSplitScreenEnabled()) {
        const DXGI_SURFACE_DESC *desc = DXUTGetDXGIBackBufferSurfaceDesc();
        txtHelper[1]->Begin();
        txtHelper[1]->SetInsertionPos(2, desc->Height - 30);
        txtHelper[1]->DrawTextLine(msaaModes[int(hud.GetComboBox(IDC_PRIMARY_AA_MODE)->GetSelectedData())].name.c_str());
        txtHelper[1]->SetInsertionPos(desc->Width - 140, desc->Height - 30);
        txtHelper[1]->DrawTextLine(msaaModes[int(hud.GetComboBox(IDC_SECONDARY_AA_MODE)->GetSelectedData())].name.c_str());
        txtHelper[1]->End();
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
    }

    // Render the scene:
    render(device);

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

    // Update previous view-projection matrix:
    prevViewProj = currViewProj;

    // Update subpixel index:
    smaa->nextFrame();
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
                    hud.GetCheckBox(IDC_SMAA_FILTERING)->SetChecked(true);
                }
            }
            break;
        case IDC_PRIMARY_AA_MODE:
        case IDC_SECONDARY_AA_MODE:
        case IDC_COMPARISON_SPLITSCREEN: {
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED || event == EVENT_CHECKBOX_CHANGED) {
                setModeControls();

                onReleasingSwapChain(nullptr);
                onResizedSwapChain(DXUTGetD3D10Device(), DXUTGetDXGISwapChain(), DXUTGetDXGIBackBufferSurfaceDesc(), nullptr);

                // Refill the temporal buffer:
                onFrameRender(DXUTGetD3D10Device(), DXUTGetTime(), DXUTGetElapsedTime(), nullptr);

                timer->reset();
                timer->setEnabled(hud.GetCheckBox(IDC_PROFILE)->GetEnabled() && hud.GetCheckBox(IDC_PROFILE)->GetChecked());
            }
            break;
        }
        case IDC_PRESET:
            if (event == EVENT_COMBOBOX_SELECTION_CHANGED) {
                SAFE_DELETE(smaa);
                initSMAA(DXUTGetD3D10Device(), DXUTGetDXGIBackBufferSurfaceDesc());
            }
            break;
        case IDC_SMAA_FILTERING:
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

    iY += 20;

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

    iY += 20;

    hud.AddComboBox(IDC_PRIMARY_AA_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_PRIMARY_AA_MODE)->SetDropHeight(120);

    hud.AddComboBox(IDC_PRESET, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Low", (LPVOID) SMAA::PRESET_LOW);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Medium", (LPVOID) SMAA::PRESET_MEDIUM);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA High", (LPVOID) SMAA::PRESET_HIGH);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Ultra", (LPVOID) SMAA::PRESET_ULTRA);
    hud.GetComboBox(IDC_PRESET)->AddItem(L"SMAA Custom", (LPVOID) SMAA::PRESET_CUSTOM);
    hud.GetComboBox(IDC_PRESET)->SetSelectedByData((LPVOID) SMAA::PRESET_HIGH);

    hud.AddComboBox(IDC_DETECTION_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);

    hud.AddCheckBox(IDC_SMAA_FILTERING, L"SMAA Filtering", 35, iY += 24, HUD_WIDTH, 22, true, 'Z');
    hud.AddCheckBox(IDC_PREDICATION, L"Predicated Tresholding", 35, iY += 24, HUD_WIDTH, 22, false);
    hud.AddCheckBox(IDC_REPROJECTION, L"Temporal Reprojection", 35, iY += 24, HUD_WIDTH, 22, true);

    iY += 20;

    hud.AddComboBox(IDC_LOCK_FRAMERATE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Unlock Framerate", (LPVOID) 0);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 60fps", (LPVOID) 1);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 30fps", (LPVOID) 2);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 20fps", (LPVOID) 3);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->AddItem(L"Lock to 15fps", (LPVOID) 4);
    hud.GetComboBox(IDC_LOCK_FRAMERATE)->SetSelectedByData((LPVOID) 0);
    hud.AddCheckBox(IDC_SHADING, L"Shading", 35, iY += 24, HUD_WIDTH, 22, false, 'S');
    hud.AddCheckBox(IDC_PROFILE, L"Profile", 35, iY += 24, HUD_WIDTH, 22, false, 'X');

    iY += 20;

    hud.AddComboBox(IDC_SECONDARY_AA_MODE, 35, iY += 24, HUD_WIDTH, 22, 0, false);
    hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetDropHeight(120);
    hud.GetComboBox(IDC_SECONDARY_AA_MODE)->SetEnabled(false);
    hud.AddCheckBox(IDC_COMPARISON_SPLITSCREEN, L"Comparison Splitscreen", 35, iY += 24, HUD_WIDTH, 22, false);

    iY += 20;

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
    commandlineOptions.searchSteps = max(min(commandlineOptions.searchSteps, 112), 0);

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
