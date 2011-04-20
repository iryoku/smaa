//--------------------------------------------------------------------------------------
// File: DXUTSettingsDlg.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------
#pragma once
#ifndef DXUT_SETTINGS_H
#define DXUT_SETTINGS_H

//--------------------------------------------------------------------------------------
// Header Includes
//--------------------------------------------------------------------------------------
#include "DXUTgui.h"

//--------------------------------------------------------------------------------------
// Control IDs
//--------------------------------------------------------------------------------------
#define DXUTSETTINGSDLG_STATIC                          -1
#define DXUTSETTINGSDLG_OK                              1
#define DXUTSETTINGSDLG_CANCEL                          2
#define DXUTSETTINGSDLG_ADAPTER                         3
#define DXUTSETTINGSDLG_DEVICE_TYPE                     4
#define DXUTSETTINGSDLG_WINDOWED                        5
#define DXUTSETTINGSDLG_FULLSCREEN                      6
#define DXUTSETTINGSDLG_ADAPTER_FORMAT                  7
#define DXUTSETTINGSDLG_ADAPTER_FORMAT_LABEL            8
#define DXUTSETTINGSDLG_RESOLUTION                      9
#define DXUTSETTINGSDLG_RESOLUTION_LABEL                10
#define DXUTSETTINGSDLG_REFRESH_RATE                    11
#define DXUTSETTINGSDLG_REFRESH_RATE_LABEL              12
#define DXUTSETTINGSDLG_BACK_BUFFER_FORMAT              13
#define DXUTSETTINGSDLG_BACK_BUFFER_FORMAT_LABEL        14
#define DXUTSETTINGSDLG_DEPTH_STENCIL                   15
#define DXUTSETTINGSDLG_DEPTH_STENCIL_LABEL             16
#define DXUTSETTINGSDLG_MULTISAMPLE_TYPE                17
#define DXUTSETTINGSDLG_MULTISAMPLE_TYPE_LABEL          18
#define DXUTSETTINGSDLG_MULTISAMPLE_QUALITY             19
#define DXUTSETTINGSDLG_MULTISAMPLE_QUALITY_LABEL       20
#define DXUTSETTINGSDLG_VERTEX_PROCESSING               21
#define DXUTSETTINGSDLG_VERTEX_PROCESSING_LABEL         22
#define DXUTSETTINGSDLG_PRESENT_INTERVAL                23
#define DXUTSETTINGSDLG_PRESENT_INTERVAL_LABEL          24
#define DXUTSETTINGSDLG_DEVICECLIP                      25
#define DXUTSETTINGSDLG_RESOLUTION_SHOW_ALL             26
#define DXUTSETTINGSDLG_API_VERSION                     27
#define DXUTSETTINGSDLG_D3D10_ADAPTER_OUTPUT            28
#define DXUTSETTINGSDLG_D3D10_ADAPTER_OUTPUT_LABEL      29
#define DXUTSETTINGSDLG_D3D10_RESOLUTION                30
#define DXUTSETTINGSDLG_D3D10_RESOLUTION_LABEL          31
#define DXUTSETTINGSDLG_D3D10_REFRESH_RATE              32
#define DXUTSETTINGSDLG_D3D10_REFRESH_RATE_LABEL        33
#define DXUTSETTINGSDLG_D3D10_BACK_BUFFER_FORMAT        34
#define DXUTSETTINGSDLG_D3D10_BACK_BUFFER_FORMAT_LABEL  35
#define DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT         36
#define DXUTSETTINGSDLG_D3D10_MULTISAMPLE_COUNT_LABEL   37
#define DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY       38
#define DXUTSETTINGSDLG_D3D10_MULTISAMPLE_QUALITY_LABEL 39
#define DXUTSETTINGSDLG_D3D10_PRESENT_INTERVAL          40
#define DXUTSETTINGSDLG_D3D10_PRESENT_INTERVAL_LABEL    41
#define DXUTSETTINGSDLG_D3D10_DEBUG_DEVICE              42
#define DXUTSETTINGSDLG_MODE_CHANGE_ACCEPT              43
#define DXUTSETTINGSDLG_MODE_CHANGE_REVERT              44
#define DXUTSETTINGSDLG_STATIC_MODE_CHANGE_TIMEOUT      45
#define DXUTSETTINGSDLG_WINDOWED_GROUP                  0x0100


//--------------------------------------------------------------------------------------
// Dialog for selection of device settings 
// Use DXUTGetD3DSettingsDialog() to access global instance
// To control the contents of the dialog, use the CD3D9Enumeration class.
//--------------------------------------------------------------------------------------
class CD3DSettingsDlg
{
public:
                        CD3DSettingsDlg();
                        ~CD3DSettingsDlg();

    void                Init( CDXUTDialogResourceManager* pManager );
    void                Init( CDXUTDialogResourceManager* pManager, LPCWSTR szControlTextureFileName );
    void                Init( CDXUTDialogResourceManager* pManager, LPCWSTR pszControlTextureResourcename,
                              HMODULE hModule );

    HRESULT             Refresh();
    void                OnRender( float fElapsedTime );
    void                OnRender9( float fElapsedTime );
    void                OnRender10( float fElapsedTime );

    HRESULT             OnD3D9CreateDevice( IDirect3DDevice9* pd3dDevice );
    HRESULT             OnD3D9ResetDevice();
    void                OnD3D9LostDevice();
    void                OnD3D9DestroyDevice();

    HRESULT             OnD3D10CreateDevice( ID3D10Device* pd3dDevice );
    HRESULT             OnD3D10ResizedSwapChain( ID3D10Device* pd3dDevice,
                                                 const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );
    void                OnD3D10DestroyDevice();

    CDXUTDialog* GetDialogControl()
    {
        return &m_Dialog;
    }
    bool                IsActive()
    {
        return m_bActive;
    }
    void                SetActive( bool bActive )
    {
        m_bActive = bActive; if( bActive ) Refresh();
    }
    void                ShowControlSet( DXUTDeviceVersion ver );

    LRESULT             MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

protected:
    friend CD3DSettingsDlg* WINAPI DXUTGetD3DSettingsDialog();

    void                CreateControls();
    HRESULT             SetDeviceSettingsFromUI();
    void                SetSelectedD3D10RefreshRate( DXGI_RATIONAL RefreshRate );
    HRESULT             UpdateD3D10Resolutions();

    void                OnEvent( UINT nEvent, int nControlID, CDXUTControl* pControl );
    static void WINAPI  StaticOnEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserData );
    static void WINAPI  StaticOnModeChangeTimer( UINT nIDEvent, void* pUserContext );

    CD3D9EnumAdapterInfo* GetCurrentAdapterInfo();
    CD3D9EnumDeviceInfo* GetCurrentDeviceInfo();
    CD3D9EnumDeviceSettingsCombo* GetCurrentDeviceSettingsCombo();
    CD3D10EnumAdapterInfo* GetCurrentD3D10AdapterInfo();
    CD3D10EnumDeviceInfo* GetCurrentD3D10DeviceInfo();
    CD3D10EnumOutputInfo* GetCurrentD3D10OutputInfo();
    CD3D10EnumDeviceSettingsCombo* GetCurrentD3D10DeviceSettingsCombo();

    void                AddAPIVersion( DXUTDeviceVersion version );
    DXUTDeviceVersion   GetSelectedAPIVersion();

    void                AddAdapter( const WCHAR* strDescription, UINT iAdapter );
    UINT                GetSelectedAdapter();

    void                AddDeviceType( D3DDEVTYPE devType );
    D3DDEVTYPE          GetSelectedDeviceType();

    void                SetWindowed( bool bWindowed );
    bool                IsWindowed();

    void                AddAdapterFormat( D3DFORMAT format );
    D3DFORMAT           GetSelectedAdapterFormat();

    void                AddResolution( DWORD dwWidth, DWORD dwHeight );
    void                GetSelectedResolution( DWORD* pdwWidth, DWORD* pdwHeight );

    void                AddRefreshRate( DWORD dwRate );
    DWORD               GetSelectedRefreshRate();

    void                AddBackBufferFormat( D3DFORMAT format );
    D3DFORMAT           GetSelectedBackBufferFormat();

    void                AddDepthStencilBufferFormat( D3DFORMAT format );
    D3DFORMAT           GetSelectedDepthStencilBufferFormat();

    void                AddMultisampleType( D3DMULTISAMPLE_TYPE type );
    D3DMULTISAMPLE_TYPE GetSelectedMultisampleType();

    void                AddMultisampleQuality( DWORD dwQuality );
    DWORD               GetSelectedMultisampleQuality();

    void                AddVertexProcessingType( DWORD dwType );
    DWORD               GetSelectedVertexProcessingType();

    DWORD               GetSelectedPresentInterval();

    void                SetDeviceClip( bool bDeviceClip );
    bool                IsDeviceClip();

    // D3D10
    void                AddD3D10DeviceType( D3D10_DRIVER_TYPE devType );
    D3D10_DRIVER_TYPE   GetSelectedD3D10DeviceType();

    void                AddD3D10AdapterOutput( const WCHAR* strName, UINT nOutput );
    UINT                GetSelectedD3D10AdapterOutput();

    void                AddD3D10Resolution( DWORD dwWidth, DWORD dwHeight );
    void                GetSelectedD3D10Resolution( DWORD* pdwWidth, DWORD* pdwHeight );

    void                AddD3D10RefreshRate( DXGI_RATIONAL RefreshRate );
    DXGI_RATIONAL       GetSelectedD3D10RefreshRate();

    void                AddD3D10BackBufferFormat( DXGI_FORMAT format );
    DXGI_FORMAT         GetSelectedD3D10BackBufferFormat();

    void                AddD3D10MultisampleCount( UINT count );
    UINT                GetSelectedD3D10MultisampleCount();

    void                AddD3D10MultisampleQuality( UINT Quality );
    UINT                GetSelectedD3D10MultisampleQuality();

    DWORD               GetSelectedD3D10PresentInterval();
    bool                GetSelectedDebugDeviceValue();

    HRESULT             OnAPIVersionChanged( bool bRefresh=false );
    HRESULT             OnAdapterChanged();
    HRESULT             OnDeviceTypeChanged();
    HRESULT             OnWindowedFullScreenChanged();
    HRESULT             OnAdapterOutputChanged();
    HRESULT             OnAdapterFormatChanged();
    HRESULT             OnResolutionChanged();
    HRESULT             OnD3D10ResolutionChanged();
    HRESULT             OnRefreshRateChanged();
    HRESULT             OnBackBufferFormatChanged();
    HRESULT             OnDepthStencilBufferFormatChanged();
    HRESULT             OnMultisampleTypeChanged();
    HRESULT             OnMultisampleQualityChanged();
    HRESULT             OnVertexProcessingChanged();
    HRESULT             OnPresentIntervalChanged();
    HRESULT             OnDebugDeviceChanged();
    HRESULT             OnDeviceClipChanged();

    void                UpdateModeChangeTimeoutText( int nSecRemaining );

    IDirect3DStateBlock9* m_pStateBlock;
    ID3D10StateBlock* m_pStateBlock10;
    CDXUTDialog* m_pActiveDialog;
    CDXUTDialog m_Dialog;
    CDXUTDialog m_RevertModeDialog;
    int m_nRevertModeTimeout;
    UINT m_nIDEvent;
    bool m_bActive;
};


CD3DSettingsDlg* WINAPI DXUTGetD3DSettingsDialog();



#endif

