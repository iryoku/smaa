//--------------------------------------------------------------------------------------
// File: DXUTEnum.cpp
//
// Enumerates D3D adapters, devices, modes, etc.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#undef min // use __min instead
#undef max // use __max instead

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
extern void DXUTGetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE* ppCallbackIsDeviceAcceptable,
                                                 void** ppUserContext );
extern void DXUTGetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE* ppCallbackIsDeviceAcceptable,
                                                  void** ppUserContext );

HRESULT DXUTFindValidD3D9DeviceSettings( DXUTD3D9DeviceSettings* pOut, DXUTD3D9DeviceSettings* pIn,
                                         DXUTMatchOptions* pMatchOptions, DXUTD3D9DeviceSettings* pOptimal );
void DXUTBuildOptimalD3D9DeviceSettings( DXUTD3D9DeviceSettings* pOptimalDeviceSettings,
                                         DXUTD3D9DeviceSettings* pDeviceSettingsIn, DXUTMatchOptions* pMatchOptions );
bool DXUTDoesD3D9DeviceComboMatchPreserveOptions( CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                                  DXUTD3D9DeviceSettings* pDeviceSettingsIn,
                                                  DXUTMatchOptions* pMatchOptions );
float DXUTRankD3D9DeviceCombo( CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                               DXUTD3D9DeviceSettings* pOptimalDeviceSettings,
                               D3DDISPLAYMODE* pAdapterDesktopDisplayMode );
void DXUTBuildValidD3D9DeviceSettings( DXUTD3D9DeviceSettings* pValidDeviceSettings,
                                       CD3D9EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                       DXUTD3D9DeviceSettings* pDeviceSettingsIn, DXUTMatchOptions* pMatchOptions );
HRESULT DXUTFindValidD3D9Resolution( CD3D9EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                     D3DDISPLAYMODE displayModeIn, D3DDISPLAYMODE* pBestDisplayMode );

HRESULT DXUTFindValidD3D10DeviceSettings( DXUTD3D10DeviceSettings* pOut, DXUTD3D10DeviceSettings* pIn,
                                          DXUTMatchOptions* pMatchOptions, DXUTD3D10DeviceSettings* pOptimal );
void DXUTBuildOptimalD3D10DeviceSettings( DXUTD3D10DeviceSettings* pOptimalDeviceSettings,
                                          DXUTD3D10DeviceSettings* pDeviceSettingsIn,
                                          DXUTMatchOptions* pMatchOptions );
bool DXUTDoesD3D10DeviceComboMatchPreserveOptions( CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                                   DXUTD3D10DeviceSettings* pDeviceSettingsIn,
                                                   DXUTMatchOptions* pMatchOptions );
float DXUTRankD3D10DeviceCombo( CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                DXUTD3D10DeviceSettings* pOptimalDeviceSettings, DXGI_MODE_DESC* pAdapterDisplayMode );
void DXUTBuildValidD3D10DeviceSettings( DXUTD3D10DeviceSettings* pValidDeviceSettings,
                                        CD3D10EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                        DXUTD3D10DeviceSettings* pDeviceSettingsIn, DXUTMatchOptions* pMatchOptions );
HRESULT DXUTFindValidD3D10Resolution( CD3D10EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                      DXGI_MODE_DESC displayModeIn, DXGI_MODE_DESC* pBestDisplayMode );

static int __cdecl SortModesCallback( const void* arg1, const void* arg2 );


//======================================================================================
//======================================================================================
// General Direct3D section
//======================================================================================
//======================================================================================


//--------------------------------------------------------------------------------------
// This function tries to find valid device settings based upon the input device settings 
// struct and the match options.  For each device setting a match option in the 
// DXUTMatchOptions struct specifies how the function makes decisions.  For example, if 
// the caller wants a HAL device with a back buffer format of D3DFMT_A2B10G10R10 but the 
// HAL device on the system does not support D3DFMT_A2B10G10R10 however a REF device is 
// installed that does, then the function has a choice to either use REF or to change to 
// a back buffer format to compatible with the HAL device.  The match options lets the 
// caller control how these choices are made.
//
// Each match option must be one of the following types: 
//      DXUTMT_IGNORE_INPUT: Uses the closest valid value to a default 
//      DXUTMT_PRESERVE_INPUT: Uses the input without change, but may cause no valid device to be found
//      DXUTMT_CLOSEST_TO_INPUT: Uses the closest valid value to the input 
//
// If pMatchOptions is NULL then, all of the match options are assumed to be DXUTMT_IGNORE_INPUT.  
// The function returns failure if no valid device settings can be found otherwise 
// the function returns success and the valid device settings are written to pOut.
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTFindValidDeviceSettings( DXUTDeviceSettings* pOut, DXUTDeviceSettings* pIn,
                                            DXUTMatchOptions* pMatchOptions )
{
    HRESULT hr = S_OK;

    if( pOut == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTFindValidDeviceSettings", E_INVALIDARG );

    // Default to DXUTMT_IGNORE_INPUT for everything unless pMatchOptions isn't NULL
    DXUTMatchOptions defaultMatchOptions;
    if( NULL == pMatchOptions )
    {
        ZeroMemory( &defaultMatchOptions, sizeof( DXUTMatchOptions ) );
        pMatchOptions = &defaultMatchOptions;
    }

    bool bAppSupportsD3D9 = DXUTDoesAppSupportD3D9();
    bool bAppSupportsD3D10 = DXUTDoesAppSupportD3D10();

    if( !bAppSupportsD3D9 && !bAppSupportsD3D10 )
    {
        bAppSupportsD3D9 = true;
        bAppSupportsD3D10 = true;
    }

    bool bAvoidD3D9 = false;
    bool bAvoidD3D10 = false;
    if( pMatchOptions->eAPIVersion == DXUTMT_PRESERVE_INPUT && pIn && pIn->ver == DXUT_D3D10_DEVICE )
        bAvoidD3D9 = true;
    if( pMatchOptions->eAPIVersion == DXUTMT_PRESERVE_INPUT && pIn && pIn->ver == DXUT_D3D9_DEVICE )
        bAvoidD3D10 = true;

    bool bPreferD3D9 = false;
    if( pMatchOptions->eAPIVersion != DXUTMT_IGNORE_INPUT && pIn && pIn->ver == DXUT_D3D9_DEVICE )
        bPreferD3D9 = true;

    // Build an optimal device settings structure based upon the match 
    // options.  If the match option is set to ignore, then a optimal default value is used.
    // The default value may not exist on the system, but later this will be taken 
    // into account.
    bool bFoundValidD3D10 = false;
    bool bFoundValidD3D9 = false;

    DXUTDeviceSettings validDeviceSettings;
    if (pIn)
        CopyMemory( &validDeviceSettings, pIn, sizeof( DXUTDeviceSettings ) );
    else
        memset( &validDeviceSettings, 0, sizeof(DXUTDeviceSettings) );

    DXUTDeviceSettings optimalDeviceSettings;

    if( bAppSupportsD3D10 && !bPreferD3D9 && !bAvoidD3D10 )
    {
        bool bD3D10Available = DXUTIsD3D10Available();
        if( bD3D10Available )
        {
            // Force an enumeration with the IsDeviceAcceptable callback
            DXUTGetD3D10Enumeration( false );

            DXUTD3D10DeviceSettings d3d10In;
            ZeroMemory( &d3d10In, sizeof( DXUTD3D10DeviceSettings ) );
            if( pIn )
            {
                if( pIn->ver == DXUT_D3D9_DEVICE )
                    DXUTConvertDeviceSettings9to10( &pIn->d3d9, &d3d10In );
                else
                    d3d10In = pIn->d3d10;
            }

            optimalDeviceSettings.ver = DXUT_D3D10_DEVICE;
            DXUTBuildOptimalD3D10DeviceSettings( &optimalDeviceSettings.d3d10, &d3d10In, pMatchOptions );

            validDeviceSettings.ver = DXUT_D3D10_DEVICE;
            hr = DXUTFindValidD3D10DeviceSettings( &validDeviceSettings.d3d10, &d3d10In,
                                                   pMatchOptions, &optimalDeviceSettings.d3d10 );
            if( SUCCEEDED( hr ) )
                bFoundValidD3D10 = true;
        }
        else
        {
            if( bAvoidD3D9 )
                hr = DXUTERR_NOCOMPATIBLEDEVICES;
            else
                hr = DXUTERR_NODIRECT3D;
        }
    }

    if( bAppSupportsD3D9 && !bFoundValidD3D10 && !bAvoidD3D9 )
    {
        // Force an enumeration with the IsDeviceAcceptable callback
        DXUTGetD3D9Enumeration( false );

        DXUTD3D9DeviceSettings d3d9In;
        ZeroMemory( &d3d9In, sizeof( DXUTD3D9DeviceSettings ) );
        if( pIn )
        {
            if( pIn->ver == DXUT_D3D10_DEVICE )
                DXUTConvertDeviceSettings10to9( &pIn->d3d10, &d3d9In );
            else
                d3d9In = pIn->d3d9;
        }

        optimalDeviceSettings.ver = DXUT_D3D9_DEVICE;
        DXUTBuildOptimalD3D9DeviceSettings( &optimalDeviceSettings.d3d9, &d3d9In, pMatchOptions );

        validDeviceSettings.ver = DXUT_D3D9_DEVICE;
        hr = DXUTFindValidD3D9DeviceSettings( &validDeviceSettings.d3d9, &d3d9In,
                                              pMatchOptions, &optimalDeviceSettings.d3d9 );
        if( SUCCEEDED( hr ) )
            bFoundValidD3D9 = true;
    }

    if( bFoundValidD3D10 || bFoundValidD3D9 )
    {
        *pOut = validDeviceSettings;
        return S_OK;
    }
    else
    {
        return DXUT_ERR( L"DXUTFindValidDeviceSettings", hr );
    }
}


//======================================================================================
//======================================================================================
// Direct3D 9 section
//======================================================================================
//======================================================================================
CD3D9Enumeration*   g_pDXUTD3D9Enumeration = NULL;

HRESULT WINAPI DXUTCreateD3D9Enumeration()
{
    if( g_pDXUTD3D9Enumeration == NULL )
    {
        g_pDXUTD3D9Enumeration = new CD3D9Enumeration();
        if( NULL == g_pDXUTD3D9Enumeration )
            return E_OUTOFMEMORY;
    }
    return S_OK;
}

void WINAPI DXUTDestroyD3D9Enumeration()
{
    SAFE_DELETE( g_pDXUTD3D9Enumeration );
}

class DXUTMemoryHelperD3D9Enum
{
public:
DXUTMemoryHelperD3D9Enum()
{
    DXUTCreateD3D9Enumeration();
}
~DXUTMemoryHelperD3D9Enum()
{
    DXUTDestroyD3D9Enumeration();
}
};

//--------------------------------------------------------------------------------------
CD3D9Enumeration* WINAPI DXUTGetD3D9Enumeration( bool bForceEnumerate )
{
    // Using an static class with accessor function to allow control of the construction order
    static DXUTMemoryHelperD3D9Enum d3d9enumMemory;

    if( g_pDXUTD3D9Enumeration && ( !g_pDXUTD3D9Enumeration->HasEnumerated() || bForceEnumerate ) )
    {
        LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE pCallbackIsDeviceAcceptable;
        void* pUserContext;
        DXUTGetCallbackD3D9DeviceAcceptable( &pCallbackIsDeviceAcceptable, &pUserContext );
        g_pDXUTD3D9Enumeration->Enumerate( pCallbackIsDeviceAcceptable, pUserContext );
    }

    return g_pDXUTD3D9Enumeration;
}


//--------------------------------------------------------------------------------------
CD3D9Enumeration::CD3D9Enumeration()
{
    m_bHasEnumerated = false;
    m_pD3D = NULL;
    m_IsD3D9DeviceAcceptableFunc = NULL;
    m_pIsD3D9DeviceAcceptableFuncUserContext = NULL;
    m_bRequirePostPixelShaderBlending = true;

    m_nMinWidth = 640;
    m_nMinHeight = 480;
    m_nMaxWidth = UINT_MAX;
    m_nMaxHeight = UINT_MAX;

    m_nRefreshMin = 0;
    m_nRefreshMax = UINT_MAX;

    m_nMultisampleQualityMax = 0xFFFF;

    ResetPossibleDepthStencilFormats();
    ResetPossibleMultisampleTypeList();
    ResetPossiblePresentIntervalList();
    SetPossibleVertexProcessingList( true, true, true, false );
}


//--------------------------------------------------------------------------------------
CD3D9Enumeration::~CD3D9Enumeration()
{
    ClearAdapterInfoList();
}



//--------------------------------------------------------------------------------------
// Enumerate for each adapter all of the supported display modes, 
// device types, adapter formats, back buffer formats, window/full screen support, 
// depth stencil formats, multisampling types/qualities, and presentations intervals.
//
// For each combination of device type (HAL/REF), adapter format, back buffer format, and
// IsWindowed it will call the app's ConfirmDevice callback.  This allows the app
// to reject or allow that combination based on its caps/etc.  It also allows the 
// app to change the BehaviorFlags.  The BehaviorFlags defaults non-pure HWVP 
// if supported otherwise it will default to SWVP, however the app can change this 
// through the ConfirmDevice callback.
//--------------------------------------------------------------------------------------
HRESULT CD3D9Enumeration::Enumerate( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE IsD3D9DeviceAcceptableFunc,
                                     void* pIsD3D9DeviceAcceptableFuncUserContext )
{
    CDXUTPerfEventGenerator eventGenerator( DXUT_PERFEVENTCOLOR, L"DXUT D3D9 Enumeration" );
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( pD3D == NULL )
    {
        pD3D = DXUTGetD3D9Object();
        if( pD3D == NULL )
            return DXUTERR_NODIRECT3D;
    }

    m_bHasEnumerated = true;
    m_pD3D = pD3D;
    m_IsD3D9DeviceAcceptableFunc = IsD3D9DeviceAcceptableFunc;
    m_pIsD3D9DeviceAcceptableFuncUserContext = pIsD3D9DeviceAcceptableFuncUserContext;

    HRESULT hr;
    ClearAdapterInfoList();
    CGrowableArray <D3DFORMAT> adapterFormatList;

    const D3DFORMAT allowedAdapterFormatArray[] =
        {
            D3DFMT_X8R8G8B8,
            D3DFMT_X1R5G5B5,
            D3DFMT_R5G6B5,
            D3DFMT_A2R10G10B10
        };
    const UINT allowedAdapterFormatArrayCount = sizeof( allowedAdapterFormatArray ) / sizeof
        ( allowedAdapterFormatArray[0] );

    UINT numAdapters = pD3D->GetAdapterCount();
    for( UINT adapterOrdinal = 0; adapterOrdinal < numAdapters; adapterOrdinal++ )
    {
        CD3D9EnumAdapterInfo* pAdapterInfo = new CD3D9EnumAdapterInfo;
        if( pAdapterInfo == NULL )
            return E_OUTOFMEMORY;

        pAdapterInfo->AdapterOrdinal = adapterOrdinal;
        pD3D->GetAdapterIdentifier( adapterOrdinal, 0, &pAdapterInfo->AdapterIdentifier );

        // Get list of all display modes on this adapter.  
        // Also build a temporary list of all display adapter formats.
        adapterFormatList.RemoveAll();

        for( UINT iFormatList = 0; iFormatList < allowedAdapterFormatArrayCount; iFormatList++ )
        {
            D3DFORMAT allowedAdapterFormat = allowedAdapterFormatArray[iFormatList];
            UINT numAdapterModes = pD3D->GetAdapterModeCount( adapterOrdinal, allowedAdapterFormat );
            for( UINT mode = 0; mode < numAdapterModes; mode++ )
            {
                D3DDISPLAYMODE displayMode;
                pD3D->EnumAdapterModes( adapterOrdinal, allowedAdapterFormat, mode, &displayMode );

                if( displayMode.Width < m_nMinWidth ||
                    displayMode.Height < m_nMinHeight ||
                    displayMode.Width > m_nMaxWidth ||
                    displayMode.Height > m_nMaxHeight ||
                    displayMode.RefreshRate < m_nRefreshMin ||
                    displayMode.RefreshRate > m_nRefreshMax )
                {
                    continue;
                }

                pAdapterInfo->displayModeList.Add( displayMode );

                if( !adapterFormatList.Contains( displayMode.Format ) )
                    adapterFormatList.Add( displayMode.Format );
            }

        }

        D3DDISPLAYMODE displayMode;
        pD3D->GetAdapterDisplayMode( adapterOrdinal, &displayMode );
        if( !adapterFormatList.Contains( displayMode.Format ) )
            adapterFormatList.Add( displayMode.Format );

        // Sort displaymode list
        qsort( pAdapterInfo->displayModeList.GetData(),
               pAdapterInfo->displayModeList.GetSize(), sizeof( D3DDISPLAYMODE ),
               SortModesCallback );

        // Get info for each device on this adapter
        if( FAILED( EnumerateDevices( pAdapterInfo, &adapterFormatList ) ) )
        {
            delete pAdapterInfo;
            continue;
        }

        // If at least one device on this adapter is available and compatible
        // with the app, add the adapterInfo to the list
        if( pAdapterInfo->deviceInfoList.GetSize() > 0 )
        {
            hr = m_AdapterInfoList.Add( pAdapterInfo );
            if( FAILED( hr ) )
                return hr;
        }
        else
            delete pAdapterInfo;
    }

    //
    // Check for 2 or more adapters with the same name. Append the name
    // with some instance number if that's the case to help distinguish
    // them.
    //
    bool bUniqueDesc = true;
    CD3D9EnumAdapterInfo* pAdapterInfo;
    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        CD3D9EnumAdapterInfo* pAdapterInfo1 = m_AdapterInfoList.GetAt( i );

        for( int j = i + 1; j < m_AdapterInfoList.GetSize(); j++ )
        {
            CD3D9EnumAdapterInfo* pAdapterInfo2 = m_AdapterInfoList.GetAt( j );
            if( _stricmp( pAdapterInfo1->AdapterIdentifier.Description,
                          pAdapterInfo2->AdapterIdentifier.Description ) == 0 )
            {
                bUniqueDesc = false;
                break;
            }
        }

        if( !bUniqueDesc )
            break;
    }

    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        pAdapterInfo = m_AdapterInfoList.GetAt( i );

        MultiByteToWideChar( CP_ACP, 0,
                             pAdapterInfo->AdapterIdentifier.Description, -1,
                             pAdapterInfo->szUniqueDescription, 100 );
        pAdapterInfo->szUniqueDescription[100] = 0;

        if( !bUniqueDesc )
        {
            WCHAR sz[100];
            swprintf_s( sz, 100, L" (#%d)", pAdapterInfo->AdapterOrdinal );
            wcscat_s( pAdapterInfo->szUniqueDescription, 256, sz );

        }
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Enumerates D3D devices for a particular adapter.
//--------------------------------------------------------------------------------------
HRESULT CD3D9Enumeration::EnumerateDevices( CD3D9EnumAdapterInfo* pAdapterInfo,
                                            CGrowableArray <D3DFORMAT>* pAdapterFormatList )
{
    HRESULT hr;

    const D3DDEVTYPE devTypeArray[] =
        {
            D3DDEVTYPE_HAL,
            D3DDEVTYPE_SW,
            D3DDEVTYPE_REF
        };
    const UINT devTypeArrayCount = sizeof( devTypeArray ) / sizeof( devTypeArray[0] );

    // Enumerate each Direct3D device type
    for( UINT iDeviceType = 0; iDeviceType < devTypeArrayCount; iDeviceType++ )
    {
        CD3D9EnumDeviceInfo* pDeviceInfo = new CD3D9EnumDeviceInfo;
        if( pDeviceInfo == NULL )
            return E_OUTOFMEMORY;

        // Fill struct w/ AdapterOrdinal and D3DDEVTYPE
        pDeviceInfo->AdapterOrdinal = pAdapterInfo->AdapterOrdinal;
        pDeviceInfo->DeviceType = devTypeArray[iDeviceType];

        // Store device caps
        if( FAILED( hr = m_pD3D->GetDeviceCaps( pAdapterInfo->AdapterOrdinal, pDeviceInfo->DeviceType,
                                                &pDeviceInfo->Caps ) ) )
        {
            delete pDeviceInfo;
            continue;
        }

        if( pDeviceInfo->DeviceType != D3DDEVTYPE_HAL )
        {
            // Create a temp device to verify that it is really possible to create a REF device 
            // [the developer DirectX redist has to be installed]
            D3DDISPLAYMODE Mode;
            m_pD3D->GetAdapterDisplayMode( 0, &Mode );
            D3DPRESENT_PARAMETERS pp;
            ZeroMemory( &pp, sizeof( D3DPRESENT_PARAMETERS ) );
            pp.BackBufferWidth = 1;
            pp.BackBufferHeight = 1;
            pp.BackBufferFormat = Mode.Format;
            pp.BackBufferCount = 1;
            pp.SwapEffect = D3DSWAPEFFECT_COPY;
            pp.Windowed = TRUE;
            pp.hDeviceWindow = DXUTGetHWNDFocus();
            IDirect3DDevice9* pDevice = NULL;
            if( FAILED( hr = m_pD3D->CreateDevice( pAdapterInfo->AdapterOrdinal, pDeviceInfo->DeviceType,
                                                   DXUTGetHWNDFocus(),
                                                   D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp,
                                                   &pDevice ) ) )
            {
                delete pDeviceInfo;
                continue;
            }
            SAFE_RELEASE( pDevice );
        }

        // Get info for each devicecombo on this device
        if( FAILED( hr = EnumerateDeviceCombos( pAdapterInfo, pDeviceInfo, pAdapterFormatList ) ) )
        {
            delete pDeviceInfo;
            continue;
        }

        // If at least one devicecombo for this device is found, 
        // add the deviceInfo to the list
        if( pDeviceInfo->deviceSettingsComboList.GetSize() > 0 )
            pAdapterInfo->deviceInfoList.Add( pDeviceInfo );
        else
            delete pDeviceInfo;
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Enumerates DeviceCombos for a particular device.
//--------------------------------------------------------------------------------------
HRESULT CD3D9Enumeration::EnumerateDeviceCombos( CD3D9EnumAdapterInfo* pAdapterInfo, CD3D9EnumDeviceInfo* pDeviceInfo,
                                                 CGrowableArray <D3DFORMAT>* pAdapterFormatList )
{
    const D3DFORMAT backBufferFormatArray[] =
        {
            D3DFMT_A8R8G8B8,
            D3DFMT_X8R8G8B8,
            D3DFMT_A2R10G10B10,
            D3DFMT_R5G6B5,
            D3DFMT_A1R5G5B5,
            D3DFMT_X1R5G5B5
        };
    const UINT backBufferFormatArrayCount = sizeof( backBufferFormatArray ) / sizeof( backBufferFormatArray[0] );

    // See which adapter formats are supported by this device
    for( int iFormat = 0; iFormat < pAdapterFormatList->GetSize(); iFormat++ )
    {
        D3DFORMAT adapterFormat = pAdapterFormatList->GetAt( iFormat );

        for( UINT iBackBufferFormat = 0; iBackBufferFormat < backBufferFormatArrayCount; iBackBufferFormat++ )
        {
            D3DFORMAT backBufferFormat = backBufferFormatArray[iBackBufferFormat];

            for( int nWindowed = 0; nWindowed < 2; nWindowed++ )
            {
                if( !nWindowed && pAdapterInfo->displayModeList.GetSize() == 0 )
                    continue;

                if( FAILED( m_pD3D->CheckDeviceType( pAdapterInfo->AdapterOrdinal, pDeviceInfo->DeviceType,
                                                     adapterFormat, backBufferFormat, nWindowed ) ) )
                {
                    continue;
                }

                if( m_bRequirePostPixelShaderBlending )
                {
                    // If the backbuffer format doesn't support D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING
                    // then alpha test, pixel fog, render-target blending, color write enable, and dithering. 
                    // are not supported.
                    if( FAILED( m_pD3D->CheckDeviceFormat( pAdapterInfo->AdapterOrdinal, pDeviceInfo->DeviceType,
                                                           adapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                                           D3DRTYPE_TEXTURE, backBufferFormat ) ) )
                    {
                        continue;
                    }
                }

                // If an application callback function has been provided, make sure this device
                // is acceptable to the app.
                if( m_IsD3D9DeviceAcceptableFunc != NULL )
                {
                    if( !m_IsD3D9DeviceAcceptableFunc( &pDeviceInfo->Caps, adapterFormat, backBufferFormat,
                                                       FALSE != nWindowed, m_pIsD3D9DeviceAcceptableFuncUserContext ) )
                        continue;
                }

                // At this point, we have an adapter/device/adapterformat/backbufferformat/iswindowed
                // DeviceCombo that is supported by the system and acceptable to the app. We still 
                // need to find one or more suitable depth/stencil buffer format,
                // multisample type, and present interval.
                CD3D9EnumDeviceSettingsCombo* pDeviceCombo = new CD3D9EnumDeviceSettingsCombo;
                if( pDeviceCombo == NULL )
                    return E_OUTOFMEMORY;

                pDeviceCombo->AdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                pDeviceCombo->DeviceType = pDeviceInfo->DeviceType;
                pDeviceCombo->AdapterFormat = adapterFormat;
                pDeviceCombo->BackBufferFormat = backBufferFormat;
                pDeviceCombo->Windowed = ( nWindowed != 0 );

                BuildDepthStencilFormatList( pDeviceCombo );
                BuildMultiSampleTypeList( pDeviceCombo );
                if( pDeviceCombo->multiSampleTypeList.GetSize() == 0 )
                {
                    delete pDeviceCombo;
                    continue;
                }
                BuildDSMSConflictList( pDeviceCombo );
                BuildPresentIntervalList( pDeviceInfo, pDeviceCombo );
                pDeviceCombo->pAdapterInfo = pAdapterInfo;
                pDeviceCombo->pDeviceInfo = pDeviceInfo;

                if( FAILED( pDeviceInfo->deviceSettingsComboList.Add( pDeviceCombo ) ) )
                    delete pDeviceCombo;
            }
        }
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Adds all depth/stencil formats that are compatible with the device 
//       and app to the given D3DDeviceCombo.
//--------------------------------------------------------------------------------------
void CD3D9Enumeration::BuildDepthStencilFormatList( CD3D9EnumDeviceSettingsCombo* pDeviceCombo )
{
    D3DFORMAT depthStencilFmt;
    for( int idsf = 0; idsf < m_DepthStencilPossibleList.GetSize(); idsf++ )
    {
        depthStencilFmt = m_DepthStencilPossibleList.GetAt( idsf );
        if( SUCCEEDED( m_pD3D->CheckDeviceFormat( pDeviceCombo->AdapterOrdinal,
                                                  pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat,
                                                  D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthStencilFmt ) ) )
        {
            if( SUCCEEDED( m_pD3D->CheckDepthStencilMatch( pDeviceCombo->AdapterOrdinal,
                                                           pDeviceCombo->DeviceType, pDeviceCombo->AdapterFormat,
                                                           pDeviceCombo->BackBufferFormat, depthStencilFmt ) ) )
            {
                pDeviceCombo->depthStencilFormatList.Add( depthStencilFmt );
            }
        }
    }
}




//--------------------------------------------------------------------------------------
// Adds all multisample types that are compatible with the device and app to
//       the given D3DDeviceCombo.
//--------------------------------------------------------------------------------------
void CD3D9Enumeration::BuildMultiSampleTypeList( CD3D9EnumDeviceSettingsCombo* pDeviceCombo )
{
    D3DMULTISAMPLE_TYPE msType;
    DWORD msQuality;
    for( int imst = 0; imst < m_MultiSampleTypeList.GetSize(); imst++ )
    {
        msType = m_MultiSampleTypeList.GetAt( imst );
        if( SUCCEEDED( m_pD3D->CheckDeviceMultiSampleType( pDeviceCombo->AdapterOrdinal,
                                                           pDeviceCombo->DeviceType, pDeviceCombo->BackBufferFormat,
                                                           pDeviceCombo->Windowed, msType, &msQuality ) ) )
        {
            pDeviceCombo->multiSampleTypeList.Add( msType );
            if( msQuality > m_nMultisampleQualityMax + 1 )
                msQuality = m_nMultisampleQualityMax + 1;
            pDeviceCombo->multiSampleQualityList.Add( msQuality );
        }
    }
}




//--------------------------------------------------------------------------------------
// Find any conflicts between the available depth/stencil formats and
//       multisample types.
//--------------------------------------------------------------------------------------
void CD3D9Enumeration::BuildDSMSConflictList( CD3D9EnumDeviceSettingsCombo* pDeviceCombo )
{
    CD3D9EnumDSMSConflict DSMSConflict;

    for( int iDS = 0; iDS < pDeviceCombo->depthStencilFormatList.GetSize(); iDS++ )
    {
        D3DFORMAT dsFmt = pDeviceCombo->depthStencilFormatList.GetAt( iDS );

        for( int iMS = 0; iMS < pDeviceCombo->multiSampleTypeList.GetSize(); iMS++ )
        {
            D3DMULTISAMPLE_TYPE msType = pDeviceCombo->multiSampleTypeList.GetAt( iMS );

            if( FAILED( m_pD3D->CheckDeviceMultiSampleType( pDeviceCombo->AdapterOrdinal, pDeviceCombo->DeviceType,
                                                            dsFmt, pDeviceCombo->Windowed, msType, NULL ) ) )
            {
                DSMSConflict.DSFormat = dsFmt;
                DSMSConflict.MSType = msType;
                pDeviceCombo->DSMSConflictList.Add( DSMSConflict );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Adds all present intervals that are compatible with the device and app 
//       to the given D3DDeviceCombo.
//--------------------------------------------------------------------------------------
void CD3D9Enumeration::BuildPresentIntervalList( CD3D9EnumDeviceInfo* pDeviceInfo,
                                                 CD3D9EnumDeviceSettingsCombo* pDeviceCombo )
{
    UINT pi;
    for( int ipi = 0; ipi < m_PresentIntervalList.GetSize(); ipi++ )
    {
        pi = m_PresentIntervalList.GetAt( ipi );
        if( pDeviceCombo->Windowed )
        {
            if( pi == D3DPRESENT_INTERVAL_TWO ||
                pi == D3DPRESENT_INTERVAL_THREE ||
                pi == D3DPRESENT_INTERVAL_FOUR )
            {
                // These intervals are not supported in windowed mode.
                continue;
            }
        }
        // Note that D3DPRESENT_INTERVAL_DEFAULT is zero, so you
        // can't do a caps check for it -- it is always available.
        if( pi == D3DPRESENT_INTERVAL_DEFAULT ||
            ( pDeviceInfo->Caps.PresentationIntervals & pi ) )
        {
            pDeviceCombo->presentIntervalList.Add( pi );
        }
    }
}



//--------------------------------------------------------------------------------------
// Release all the allocated CD3D9EnumAdapterInfo objects and empty the list
//--------------------------------------------------------------------------------------
void CD3D9Enumeration::ClearAdapterInfoList()
{
    CD3D9EnumAdapterInfo* pAdapterInfo;
    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        pAdapterInfo = m_AdapterInfoList.GetAt( i );
        delete pAdapterInfo;
    }

    m_AdapterInfoList.RemoveAll();
}



//--------------------------------------------------------------------------------------
// Call GetAdapterInfoList() after Enumerate() to get a STL vector of 
//       CD3D9EnumAdapterInfo* 
//--------------------------------------------------------------------------------------
CGrowableArray <CD3D9EnumAdapterInfo*>* CD3D9Enumeration::GetAdapterInfoList()
{
    return &m_AdapterInfoList;
}



//--------------------------------------------------------------------------------------
CD3D9EnumAdapterInfo* CD3D9Enumeration::GetAdapterInfo( UINT AdapterOrdinal )
{
    for( int iAdapter = 0; iAdapter < m_AdapterInfoList.GetSize(); iAdapter++ )
    {
        CD3D9EnumAdapterInfo* pAdapterInfo = m_AdapterInfoList.GetAt( iAdapter );
        if( pAdapterInfo->AdapterOrdinal == AdapterOrdinal )
            return pAdapterInfo;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
CD3D9EnumDeviceInfo* CD3D9Enumeration::GetDeviceInfo( UINT AdapterOrdinal, D3DDEVTYPE DeviceType )
{
    CD3D9EnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
    if( pAdapterInfo )
    {
        for( int iDeviceInfo = 0; iDeviceInfo < pAdapterInfo->deviceInfoList.GetSize(); iDeviceInfo++ )
        {
            CD3D9EnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.GetAt( iDeviceInfo );
            if( pDeviceInfo->DeviceType == DeviceType )
                return pDeviceInfo;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
CD3D9EnumDeviceSettingsCombo* CD3D9Enumeration::GetDeviceSettingsCombo( UINT AdapterOrdinal, D3DDEVTYPE DeviceType,
                                                                        D3DFORMAT AdapterFormat,
                                                                        D3DFORMAT BackBufferFormat, BOOL bWindowed )
{
    CD3D9EnumDeviceInfo* pDeviceInfo = GetDeviceInfo( AdapterOrdinal, DeviceType );
    if( pDeviceInfo )
    {
        for( int iDeviceCombo = 0; iDeviceCombo < pDeviceInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pDeviceInfo->deviceSettingsComboList.GetAt(
                iDeviceCombo );
            if( pDeviceSettingsCombo->AdapterFormat == AdapterFormat &&
                pDeviceSettingsCombo->BackBufferFormat == BackBufferFormat &&
                pDeviceSettingsCombo->Windowed == bWindowed )
                return pDeviceSettingsCombo;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
// Returns the number of color channel bits in the specified D3DFORMAT
//--------------------------------------------------------------------------------------
UINT WINAPI DXUTGetD3D9ColorChannelBits( D3DFORMAT fmt )
{
    switch( fmt )
    {
        case D3DFMT_R8G8B8:
            return 8;
        case D3DFMT_A8R8G8B8:
            return 8;
        case D3DFMT_X8R8G8B8:
            return 8;
        case D3DFMT_R5G6B5:
            return 5;
        case D3DFMT_X1R5G5B5:
            return 5;
        case D3DFMT_A1R5G5B5:
            return 5;
        case D3DFMT_A4R4G4B4:
            return 4;
        case D3DFMT_R3G3B2:
            return 2;
        case D3DFMT_A8R3G3B2:
            return 2;
        case D3DFMT_X4R4G4B4:
            return 4;
        case D3DFMT_A2B10G10R10:
            return 10;
        case D3DFMT_A8B8G8R8:
            return 8;
        case D3DFMT_A2R10G10B10:
            return 10;
        case D3DFMT_A16B16G16R16:
            return 16;
        default:
            return 0;
    }
}


//--------------------------------------------------------------------------------------
// Returns the number of alpha channel bits in the specified D3DFORMAT
//--------------------------------------------------------------------------------------
UINT WINAPI DXUTGetAlphaChannelBits( D3DFORMAT fmt )
{
    switch( fmt )
    {
        case D3DFMT_R8G8B8:
            return 0;
        case D3DFMT_A8R8G8B8:
            return 8;
        case D3DFMT_X8R8G8B8:
            return 0;
        case D3DFMT_R5G6B5:
            return 0;
        case D3DFMT_X1R5G5B5:
            return 0;
        case D3DFMT_A1R5G5B5:
            return 1;
        case D3DFMT_A4R4G4B4:
            return 4;
        case D3DFMT_R3G3B2:
            return 0;
        case D3DFMT_A8R3G3B2:
            return 8;
        case D3DFMT_X4R4G4B4:
            return 0;
        case D3DFMT_A2B10G10R10:
            return 2;
        case D3DFMT_A8B8G8R8:
            return 8;
        case D3DFMT_A2R10G10B10:
            return 2;
        case D3DFMT_A16B16G16R16:
            return 16;
        default:
            return 0;
    }
}


//--------------------------------------------------------------------------------------
// Returns the number of depth bits in the specified D3DFORMAT
//--------------------------------------------------------------------------------------
UINT WINAPI DXUTGetDepthBits( D3DFORMAT fmt )
{
    switch( fmt )
    {
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D32:
            return 32;

        case D3DFMT_D24X8:
        case D3DFMT_D24S8:
        case D3DFMT_D24X4S4:
        case D3DFMT_D24FS8:
            return 24;

        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D16:
            return 16;

        case D3DFMT_D15S1:
            return 15;

        default:
            return 0;
    }
}




//--------------------------------------------------------------------------------------
// Returns the number of stencil bits in the specified D3DFORMAT
//--------------------------------------------------------------------------------------
UINT WINAPI DXUTGetStencilBits( D3DFORMAT fmt )
{
    switch( fmt )
    {
        case D3DFMT_D16_LOCKABLE:
        case D3DFMT_D16:
        case D3DFMT_D32F_LOCKABLE:
        case D3DFMT_D32:
        case D3DFMT_D24X8:
            return 0;

        case D3DFMT_D15S1:
            return 1;

        case D3DFMT_D24X4S4:
            return 4;

        case D3DFMT_D24S8:
        case D3DFMT_D24FS8:
            return 8;

        default:
            return 0;
    }
}



//--------------------------------------------------------------------------------------
// Used to sort D3DDISPLAYMODEs
//--------------------------------------------------------------------------------------
static int __cdecl SortModesCallback( const void* arg1, const void* arg2 )
{
    D3DDISPLAYMODE* pdm1 = ( D3DDISPLAYMODE* )arg1;
    D3DDISPLAYMODE* pdm2 = ( D3DDISPLAYMODE* )arg2;

    if( pdm1->Width > pdm2->Width )
        return 1;
    if( pdm1->Width < pdm2->Width )
        return -1;
    if( pdm1->Height > pdm2->Height )
        return 1;
    if( pdm1->Height < pdm2->Height )
        return -1;
    if( pdm1->Format > pdm2->Format )
        return 1;
    if( pdm1->Format < pdm2->Format )
        return -1;
    if( pdm1->RefreshRate > pdm2->RefreshRate )
        return 1;
    if( pdm1->RefreshRate < pdm2->RefreshRate )
        return -1;
    return 0;
}



//--------------------------------------------------------------------------------------
CD3D9EnumAdapterInfo::~CD3D9EnumAdapterInfo( void )
{
    CD3D9EnumDeviceInfo* pDeviceInfo;
    for( int i = 0; i < deviceInfoList.GetSize(); i++ )
    {
        pDeviceInfo = deviceInfoList.GetAt( i );
        delete pDeviceInfo;
    }
    deviceInfoList.RemoveAll();
}




//--------------------------------------------------------------------------------------
CD3D9EnumDeviceInfo::~CD3D9EnumDeviceInfo( void )
{
    CD3D9EnumDeviceSettingsCombo* pDeviceCombo;
    for( int i = 0; i < deviceSettingsComboList.GetSize(); i++ )
    {
        pDeviceCombo = deviceSettingsComboList.GetAt( i );
        delete pDeviceCombo;
    }
    deviceSettingsComboList.RemoveAll();
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::ResetPossibleDepthStencilFormats()
{
    m_DepthStencilPossibleList.RemoveAll();
    m_DepthStencilPossibleList.Add( D3DFMT_D16 );
    m_DepthStencilPossibleList.Add( D3DFMT_D15S1 );
    m_DepthStencilPossibleList.Add( D3DFMT_D24X8 );
    m_DepthStencilPossibleList.Add( D3DFMT_D24S8 );
    m_DepthStencilPossibleList.Add( D3DFMT_D24X4S4 );
    m_DepthStencilPossibleList.Add( D3DFMT_D32 );
}


//--------------------------------------------------------------------------------------
CGrowableArray <D3DFORMAT>* CD3D9Enumeration::GetPossibleDepthStencilFormatList()
{
    return &m_DepthStencilPossibleList;
}


//--------------------------------------------------------------------------------------
CGrowableArray <D3DMULTISAMPLE_TYPE>* CD3D9Enumeration::GetPossibleMultisampleTypeList()
{
    return &m_MultiSampleTypeList;
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::ResetPossibleMultisampleTypeList()
{
    m_MultiSampleTypeList.RemoveAll();
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_NONE );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_NONMASKABLE );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_2_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_3_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_4_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_5_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_6_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_7_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_8_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_9_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_10_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_11_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_12_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_13_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_14_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_15_SAMPLES );
    m_MultiSampleTypeList.Add( D3DMULTISAMPLE_16_SAMPLES );
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::GetPossibleVertexProcessingList( bool* pbSoftwareVP, bool* pbHardwareVP, bool* pbPureHarewareVP,
                                                        bool* pbMixedVP )
{
    *pbSoftwareVP = m_bSoftwareVP;
    *pbHardwareVP = m_bHardwareVP;
    *pbPureHarewareVP = m_bPureHarewareVP;
    *pbMixedVP = m_bMixedVP;
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::SetPossibleVertexProcessingList( bool bSoftwareVP, bool bHardwareVP, bool bPureHarewareVP,
                                                        bool bMixedVP )
{
    m_bSoftwareVP = bSoftwareVP;
    m_bHardwareVP = bHardwareVP;
    m_bPureHarewareVP = bPureHarewareVP;
    m_bMixedVP = bMixedVP;
}


//--------------------------------------------------------------------------------------
CGrowableArray <UINT>* CD3D9Enumeration::GetPossiblePresentIntervalList()
{
    return &m_PresentIntervalList;
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::ResetPossiblePresentIntervalList()
{
    m_PresentIntervalList.RemoveAll();
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_IMMEDIATE );
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_DEFAULT );
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_ONE );
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_TWO );
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_THREE );
    m_PresentIntervalList.Add( D3DPRESENT_INTERVAL_FOUR );
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::SetResolutionMinMax( UINT nMinWidth, UINT nMinHeight,
                                            UINT nMaxWidth, UINT nMaxHeight )
{
    m_nMinWidth = nMinWidth;
    m_nMinHeight = nMinHeight;
    m_nMaxWidth = nMaxWidth;
    m_nMaxHeight = nMaxHeight;
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::SetRefreshMinMax( UINT nMin, UINT nMax )
{
    m_nRefreshMin = nMin;
    m_nRefreshMax = nMax;
}


//--------------------------------------------------------------------------------------
void CD3D9Enumeration::SetMultisampleQualityMax( UINT nMax )
{
    if( nMax > 0xFFFF )
        nMax = 0xFFFF;
    m_nMultisampleQualityMax = nMax;
}


//--------------------------------------------------------------------------------------
HRESULT DXUTFindValidD3D9DeviceSettings( DXUTD3D9DeviceSettings* pOut, DXUTD3D9DeviceSettings* pIn,
                                         DXUTMatchOptions* pMatchOptions, DXUTD3D9DeviceSettings* pOptimal )
{
    // Find the best combination of:
    //      Adapter Ordinal
    //      Device Type
    //      Adapter Format
    //      Back Buffer Format
    //      Windowed
    // given what's available on the system and the match options combined with the device settings input.
    // This combination of settings is encapsulated by the CD3D9EnumDeviceSettingsCombo class.
    float fBestRanking = -1.0f;
    CD3D9EnumDeviceSettingsCombo* pBestDeviceSettingsCombo = NULL;
    D3DDISPLAYMODE adapterDesktopDisplayMode;

    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration( false );
    CGrowableArray <CD3D9EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
    for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); iAdapter++ )
    {
        CD3D9EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );

        // Get the desktop display mode of adapter 
        pD3D->GetAdapterDisplayMode( pAdapterInfo->AdapterOrdinal, &adapterDesktopDisplayMode );

        // Enum all the device types supported by this adapter to find the best device settings
        for( int iDeviceInfo = 0; iDeviceInfo < pAdapterInfo->deviceInfoList.GetSize(); iDeviceInfo++ )
        {
            CD3D9EnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.GetAt( iDeviceInfo );

            // Enum all the device settings combinations.  A device settings combination is 
            // a unique set of an adapter format, back buffer format, and IsWindowed.
            for( int iDeviceCombo = 0; iDeviceCombo < pDeviceInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
            {
                CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pDeviceInfo->deviceSettingsComboList.GetAt(
                    iDeviceCombo );

                // If windowed mode the adapter format has to be the same as the desktop 
                // display mode format so skip any that don't match
                if( pDeviceSettingsCombo->Windowed &&
                    ( pDeviceSettingsCombo->AdapterFormat != adapterDesktopDisplayMode.Format ) )
                    continue;

                // Skip any combo that doesn't meet the preserve match options
                if( false == DXUTDoesD3D9DeviceComboMatchPreserveOptions( pDeviceSettingsCombo, pIn, pMatchOptions ) )
                    continue;

                // Get a ranking number that describes how closely this device combo matches the optimal combo
                float fCurRanking = DXUTRankD3D9DeviceCombo( pDeviceSettingsCombo,
                                                             pOptimal, &adapterDesktopDisplayMode );

                // If this combo better matches the input device settings then save it
                if( fCurRanking > fBestRanking )
                {
                    pBestDeviceSettingsCombo = pDeviceSettingsCombo;
                    fBestRanking = fCurRanking;
                }
            }
        }
    }

    // If no best device combination was found then fail
    if( pBestDeviceSettingsCombo == NULL )
        return DXUTERR_NOCOMPATIBLEDEVICES;

    // Using the best device settings combo found, build valid device settings taking heed of 
    // the match options and the input device settings
    DXUTD3D9DeviceSettings validDeviceSettings;
    DXUTBuildValidD3D9DeviceSettings( &validDeviceSettings, pBestDeviceSettingsCombo, pIn, pMatchOptions );
    *pOut = validDeviceSettings;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Internal helper function to build a D3D9 device settings structure based upon the match 
// options.  If the match option is set to ignore, then a optimal default value is used.
// The default value may not exist on the system, but later this will be taken 
// into account.
//--------------------------------------------------------------------------------------
void DXUTBuildOptimalD3D9DeviceSettings( DXUTD3D9DeviceSettings* pOptimalDeviceSettings,
                                         DXUTD3D9DeviceSettings* pDeviceSettingsIn,
                                         DXUTMatchOptions* pMatchOptions )
{
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    D3DDISPLAYMODE adapterDesktopDisplayMode;

    ZeroMemory( pOptimalDeviceSettings, sizeof( DXUTD3D9DeviceSettings ) );

    //---------------------
    // Adapter ordinal
    //---------------------    
    if( pMatchOptions->eAdapterOrdinal == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->AdapterOrdinal = D3DADAPTER_DEFAULT;
    else
        pOptimalDeviceSettings->AdapterOrdinal = pDeviceSettingsIn->AdapterOrdinal;

    //---------------------
    // Device type
    //---------------------
    if( pMatchOptions->eDeviceType == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->DeviceType = D3DDEVTYPE_HAL;
    else
        pOptimalDeviceSettings->DeviceType = pDeviceSettingsIn->DeviceType;

    //---------------------
    // Windowed
    //---------------------
    if( pMatchOptions->eWindowed == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.Windowed = TRUE;
    else
        pOptimalDeviceSettings->pp.Windowed = pDeviceSettingsIn->pp.Windowed;

    //---------------------
    // Adapter format
    //---------------------
    if( pMatchOptions->eAdapterFormat == DXUTMT_IGNORE_INPUT )
    {
        // If windowed, default to the desktop display mode
        // If fullscreen, default to the desktop display mode for quick mode change or 
        // default to D3DFMT_X8R8G8B8 if the desktop display mode is < 32bit
        pD3D->GetAdapterDisplayMode( pOptimalDeviceSettings->AdapterOrdinal, &adapterDesktopDisplayMode );
        if( pOptimalDeviceSettings->pp.Windowed || DXUTGetD3D9ColorChannelBits( adapterDesktopDisplayMode.Format ) >=
            8 )
            pOptimalDeviceSettings->AdapterFormat = adapterDesktopDisplayMode.Format;
        else
            pOptimalDeviceSettings->AdapterFormat = D3DFMT_X8R8G8B8;
    }
    else
    {
        pOptimalDeviceSettings->AdapterFormat = pDeviceSettingsIn->AdapterFormat;
    }

    //---------------------
    // Vertex processing
    //---------------------
    if( pMatchOptions->eVertexProcessing == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    else
        pOptimalDeviceSettings->BehaviorFlags = pDeviceSettingsIn->BehaviorFlags;

    //---------------------
    // Resolution
    //---------------------
    if( pMatchOptions->eResolution == DXUTMT_IGNORE_INPUT )
    {
        // If windowed, default to 640x480
        // If fullscreen, default to the desktop res for quick mode change
        if( pOptimalDeviceSettings->pp.Windowed )
        {
            pOptimalDeviceSettings->pp.BackBufferWidth = 640;
            pOptimalDeviceSettings->pp.BackBufferHeight = 480;
        }
        else
        {
            pD3D->GetAdapterDisplayMode( pOptimalDeviceSettings->AdapterOrdinal, &adapterDesktopDisplayMode );
            pOptimalDeviceSettings->pp.BackBufferWidth = adapterDesktopDisplayMode.Width;
            pOptimalDeviceSettings->pp.BackBufferHeight = adapterDesktopDisplayMode.Height;
        }
    }
    else
    {
        pOptimalDeviceSettings->pp.BackBufferWidth = pDeviceSettingsIn->pp.BackBufferWidth;
        pOptimalDeviceSettings->pp.BackBufferHeight = pDeviceSettingsIn->pp.BackBufferHeight;
    }

    //---------------------
    // Back buffer format
    //---------------------
    if( pMatchOptions->eBackBufferFormat == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.BackBufferFormat = pOptimalDeviceSettings->AdapterFormat; // Default to match the adapter format
    else
        pOptimalDeviceSettings->pp.BackBufferFormat = pDeviceSettingsIn->pp.BackBufferFormat;

    //---------------------
    // Back buffer count
    //---------------------
    if( pMatchOptions->eBackBufferCount == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.BackBufferCount = 1; // Default to double buffering.  Causes less latency than triple buffering
    else
        pOptimalDeviceSettings->pp.BackBufferCount = pDeviceSettingsIn->pp.BackBufferCount;

    //---------------------
    // Multisample
    //---------------------
    if( pMatchOptions->eMultiSample == DXUTMT_IGNORE_INPUT )
    {
        // Default to no multisampling 
        pOptimalDeviceSettings->pp.MultiSampleType = D3DMULTISAMPLE_NONE;
        pOptimalDeviceSettings->pp.MultiSampleQuality = 0;
    }
    else
    {
        pOptimalDeviceSettings->pp.MultiSampleType = pDeviceSettingsIn->pp.MultiSampleType;
        pOptimalDeviceSettings->pp.MultiSampleQuality = pDeviceSettingsIn->pp.MultiSampleQuality;
    }

    //---------------------
    // Swap effect
    //---------------------
    if( pMatchOptions->eSwapEffect == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    else
        pOptimalDeviceSettings->pp.SwapEffect = pDeviceSettingsIn->pp.SwapEffect;

    //---------------------
    // Depth stencil 
    //---------------------
    if( pMatchOptions->eDepthFormat == DXUTMT_IGNORE_INPUT &&
        pMatchOptions->eStencilFormat == DXUTMT_IGNORE_INPUT )
    {
        UINT nBackBufferBits = DXUTGetD3D9ColorChannelBits( pOptimalDeviceSettings->pp.BackBufferFormat );
        if( nBackBufferBits >= 8 )
            pOptimalDeviceSettings->pp.AutoDepthStencilFormat = D3DFMT_D32;
        else
            pOptimalDeviceSettings->pp.AutoDepthStencilFormat = D3DFMT_D16;
    }
    else
    {
        pOptimalDeviceSettings->pp.AutoDepthStencilFormat = pDeviceSettingsIn->pp.AutoDepthStencilFormat;
    }

    //---------------------
    // Present flags
    //---------------------
    if( pMatchOptions->ePresentFlags == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    else
        pOptimalDeviceSettings->pp.Flags = pDeviceSettingsIn->pp.Flags;

    //---------------------
    // Refresh rate
    //---------------------
    if( pMatchOptions->eRefreshRate == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->pp.FullScreen_RefreshRateInHz = 0;
    else
        pOptimalDeviceSettings->pp.FullScreen_RefreshRateInHz = pDeviceSettingsIn->pp.FullScreen_RefreshRateInHz;

    //---------------------
    // Present interval
    //---------------------
    if( pMatchOptions->ePresentInterval == DXUTMT_IGNORE_INPUT )
    {
        // For windowed and fullscreen, default to D3DPRESENT_INTERVAL_DEFAULT
        // which will wait for the vertical retrace period to prevent tearing.
        // For benchmarking, use D3DPRESENT_INTERVAL_IMMEDIATE which will
        // will wait not for the vertical retrace period but may introduce tearing.
        pOptimalDeviceSettings->pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    }
    else
    {
        pOptimalDeviceSettings->pp.PresentationInterval = pDeviceSettingsIn->pp.PresentationInterval;
    }
}


//--------------------------------------------------------------------------------------
// Returns false for any CD3D9EnumDeviceSettingsCombo that doesn't meet the preserve 
// match options against the input pDeviceSettingsIn.
//--------------------------------------------------------------------------------------
bool DXUTDoesD3D9DeviceComboMatchPreserveOptions( CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                                  DXUTD3D9DeviceSettings* pDeviceSettingsIn,
                                                  DXUTMatchOptions* pMatchOptions )
{
    //---------------------
    // Adapter ordinal
    //---------------------
    if( pMatchOptions->eAdapterOrdinal == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->AdapterOrdinal != pDeviceSettingsIn->AdapterOrdinal ) )
        return false;

    //---------------------
    // Device type
    //---------------------
    if( pMatchOptions->eDeviceType == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->DeviceType != pDeviceSettingsIn->DeviceType ) )
        return false;

    //---------------------
    // Windowed
    //---------------------
    if( pMatchOptions->eWindowed == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->Windowed != pDeviceSettingsIn->pp.Windowed ) )
        return false;

    //---------------------
    // Adapter format
    //---------------------
    if( pMatchOptions->eAdapterFormat == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->AdapterFormat != pDeviceSettingsIn->AdapterFormat ) )
        return false;

    //---------------------
    // Vertex processing
    //---------------------
    // If keep VP and input has HWVP, then skip if this combo doesn't have HWTL 
    if( pMatchOptions->eVertexProcessing == DXUTMT_PRESERVE_INPUT &&
        ( ( pDeviceSettingsIn->BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) != 0 ) &&
        ( ( pDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ) )
        return false;

    //---------------------
    // Resolution
    //---------------------
    // If keep resolution then check that width and height supported by this combo
    if( pMatchOptions->eResolution == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetSize(); i++ )
        {
            D3DDISPLAYMODE displayMode = pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetAt( i );
            if( displayMode.Format != pDeviceSettingsCombo->AdapterFormat )
                continue; // Skip this display mode if it doesn't match the combo's adapter format

            if( displayMode.Width == pDeviceSettingsIn->pp.BackBufferWidth &&
                displayMode.Height == pDeviceSettingsIn->pp.BackBufferHeight )
            {
                bFound = true;
                break;
            }
        }

        // If the width and height are not supported by this combo, return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Back buffer format
    //---------------------
    if( pMatchOptions->eBackBufferFormat == DXUTMT_PRESERVE_INPUT &&
        pDeviceSettingsCombo->BackBufferFormat != pDeviceSettingsIn->pp.BackBufferFormat )
        return false;

    //---------------------
    // Back buffer count
    //---------------------
    // No caps for the back buffer count

    //---------------------
    // Multisample
    //---------------------
    if( pMatchOptions->eMultiSample == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->multiSampleTypeList.GetSize(); i++ )
        {
            D3DMULTISAMPLE_TYPE msType = pDeviceSettingsCombo->multiSampleTypeList.GetAt( i );
            DWORD msQuality = pDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

            if( msType == pDeviceSettingsIn->pp.MultiSampleType &&
                msQuality > pDeviceSettingsIn->pp.MultiSampleQuality )
            {
                bFound = true;
                break;
            }
        }

        // If multisample type/quality not supported by this combo, then return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Swap effect
    //---------------------
    // No caps for swap effects

    //---------------------
    // Depth stencil 
    //---------------------
    // If keep depth stencil format then check that the depth stencil format is supported by this combo
    if( pMatchOptions->eDepthFormat == DXUTMT_PRESERVE_INPUT &&
        pMatchOptions->eStencilFormat == DXUTMT_PRESERVE_INPUT )
    {
        if( pDeviceSettingsIn->pp.AutoDepthStencilFormat != D3DFMT_UNKNOWN &&
            !pDeviceSettingsCombo->depthStencilFormatList.Contains( pDeviceSettingsIn->pp.AutoDepthStencilFormat ) )
            return false;
    }

    // If keep depth format then check that the depth format is supported by this combo
    if( pMatchOptions->eDepthFormat == DXUTMT_PRESERVE_INPUT &&
        pDeviceSettingsIn->pp.AutoDepthStencilFormat != D3DFMT_UNKNOWN )
    {
        bool bFound = false;
        UINT dwDepthBits = DXUTGetDepthBits( pDeviceSettingsIn->pp.AutoDepthStencilFormat );
        for( int i = 0; i < pDeviceSettingsCombo->depthStencilFormatList.GetSize(); i++ )
        {
            D3DFORMAT depthStencilFmt = pDeviceSettingsCombo->depthStencilFormatList.GetAt( i );
            UINT dwCurDepthBits = DXUTGetDepthBits( depthStencilFmt );
            if( dwCurDepthBits - dwDepthBits == 0 )
                bFound = true;
        }

        if( !bFound )
            return false;
    }

    // If keep depth format then check that the depth format is supported by this combo
    if( pMatchOptions->eStencilFormat == DXUTMT_PRESERVE_INPUT &&
        pDeviceSettingsIn->pp.AutoDepthStencilFormat != D3DFMT_UNKNOWN )
    {
        bool bFound = false;
        UINT dwStencilBits = DXUTGetStencilBits( pDeviceSettingsIn->pp.AutoDepthStencilFormat );
        for( int i = 0; i < pDeviceSettingsCombo->depthStencilFormatList.GetSize(); i++ )
        {
            D3DFORMAT depthStencilFmt = pDeviceSettingsCombo->depthStencilFormatList.GetAt( i );
            UINT dwCurStencilBits = DXUTGetStencilBits( depthStencilFmt );
            if( dwCurStencilBits - dwStencilBits == 0 )
                bFound = true;
        }

        if( !bFound )
            return false;
    }

    //---------------------
    // Present flags
    //---------------------
    // No caps for the present flags

    //---------------------
    // Refresh rate
    //---------------------
    // If keep refresh rate then check that the resolution is supported by this combo
    if( pMatchOptions->eRefreshRate == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetSize(); i++ )
        {
            D3DDISPLAYMODE displayMode = pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetAt( i );
            if( displayMode.Format != pDeviceSettingsCombo->AdapterFormat )
                continue;
            if( displayMode.RefreshRate == pDeviceSettingsIn->pp.FullScreen_RefreshRateInHz )
            {
                bFound = true;
                break;
            }
        }

        // If refresh rate not supported by this combo, then return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Present interval
    //---------------------
    // If keep present interval then check that the present interval is supported by this combo
    if( pMatchOptions->ePresentInterval == DXUTMT_PRESERVE_INPUT &&
        !pDeviceSettingsCombo->presentIntervalList.Contains( pDeviceSettingsIn->pp.PresentationInterval ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Returns a ranking number that describes how closely this device 
// combo matches the optimal combo based on the match options and the optimal device settings
//--------------------------------------------------------------------------------------
float DXUTRankD3D9DeviceCombo( CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                               DXUTD3D9DeviceSettings* pOptimalDeviceSettings,
                               D3DDISPLAYMODE* pAdapterDesktopDisplayMode )
{
    float fCurRanking = 0.0f;

    // Arbitrary weights.  Gives preference to the ordinal, device type, and windowed
    const float fAdapterOrdinalWeight = 1000.0f;
    const float fDeviceTypeWeight = 100.0f;
    const float fWindowWeight = 10.0f;
    const float fAdapterFormatWeight = 1.0f;
    const float fVertexProcessingWeight = 1.0f;
    const float fResolutionWeight = 1.0f;
    const float fBackBufferFormatWeight = 1.0f;
    const float fMultiSampleWeight = 1.0f;
    const float fDepthStencilWeight = 1.0f;
    const float fRefreshRateWeight = 1.0f;
    const float fPresentIntervalWeight = 1.0f;

    //---------------------
    // Adapter ordinal
    //---------------------
    if( pDeviceSettingsCombo->AdapterOrdinal == pOptimalDeviceSettings->AdapterOrdinal )
        fCurRanking += fAdapterOrdinalWeight;

    //---------------------
    // Device type
    //---------------------
    if( pDeviceSettingsCombo->DeviceType == pOptimalDeviceSettings->DeviceType )
        fCurRanking += fDeviceTypeWeight;
    // Slightly prefer HAL 
    if( pDeviceSettingsCombo->DeviceType == D3DDEVTYPE_HAL )
        fCurRanking += 0.1f;

    //---------------------
    // Windowed
    //---------------------
    if( pDeviceSettingsCombo->Windowed == pOptimalDeviceSettings->pp.Windowed )
        fCurRanking += fWindowWeight;

    //---------------------
    // Adapter format
    //---------------------
    if( pDeviceSettingsCombo->AdapterFormat == pOptimalDeviceSettings->AdapterFormat )
    {
        fCurRanking += fAdapterFormatWeight;
    }
    else
    {
        int nBitDepthDelta = abs( ( long )DXUTGetD3D9ColorChannelBits( pDeviceSettingsCombo->AdapterFormat ) -
                                  ( long )DXUTGetD3D9ColorChannelBits( pOptimalDeviceSettings->AdapterFormat ) );
        float fScale = __max( 0.9f - ( float )nBitDepthDelta * 0.2f, 0.0f );
        fCurRanking += fScale * fAdapterFormatWeight;
    }

    if( !pDeviceSettingsCombo->Windowed )
    {
        // Slightly prefer when it matches the desktop format or is D3DFMT_X8R8G8B8
        bool bAdapterOptimalMatch;
        if( DXUTGetD3D9ColorChannelBits( pAdapterDesktopDisplayMode->Format ) >= 8 )
            bAdapterOptimalMatch = ( pDeviceSettingsCombo->AdapterFormat == pAdapterDesktopDisplayMode->Format );
        else
            bAdapterOptimalMatch = ( pDeviceSettingsCombo->AdapterFormat == D3DFMT_X8R8G8B8 );

        if( bAdapterOptimalMatch )
            fCurRanking += 0.1f;
    }

    //---------------------
    // Vertex processing
    //---------------------
    if( ( pOptimalDeviceSettings->BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) != 0 ||
        ( pOptimalDeviceSettings->BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING ) != 0 )
    {
        if( ( pDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) != 0 )
            fCurRanking += fVertexProcessingWeight;
    }
    // Slightly prefer HW T&L
    if( ( pDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) != 0 )
        fCurRanking += 0.1f;

    //---------------------
    // Resolution
    //---------------------
    bool bResolutionFound = false;
    for( int idm = 0; idm < pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetSize(); idm++ )
    {
        D3DDISPLAYMODE displayMode = pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetAt( idm );
        if( displayMode.Format != pDeviceSettingsCombo->AdapterFormat )
            continue;
        if( displayMode.Width == pOptimalDeviceSettings->pp.BackBufferWidth &&
            displayMode.Height == pOptimalDeviceSettings->pp.BackBufferHeight )
            bResolutionFound = true;
    }
    if( bResolutionFound )
        fCurRanking += fResolutionWeight;

    //---------------------
    // Back buffer format
    //---------------------
    if( pDeviceSettingsCombo->BackBufferFormat == pOptimalDeviceSettings->pp.BackBufferFormat )
    {
        fCurRanking += fBackBufferFormatWeight;
    }
    else
    {
        int nBitDepthDelta = abs( ( long )DXUTGetD3D9ColorChannelBits( pDeviceSettingsCombo->BackBufferFormat ) -
                                  ( long )DXUTGetD3D9ColorChannelBits( pOptimalDeviceSettings->pp.BackBufferFormat ) );
        float fScale = __max( 0.9f - ( float )nBitDepthDelta * 0.2f, 0.0f );
        fCurRanking += fScale * fBackBufferFormatWeight;
    }

    // Check if this back buffer format is the same as 
    // the adapter format since this is preferred.
    bool bAdapterMatchesBB = ( pDeviceSettingsCombo->BackBufferFormat == pDeviceSettingsCombo->AdapterFormat );
    if( bAdapterMatchesBB )
        fCurRanking += 0.1f;

    //---------------------
    // Back buffer count
    //---------------------
    // No caps for the back buffer count

    //---------------------
    // Multisample
    //---------------------
    bool bMultiSampleFound = false;
    for( int i = 0; i < pDeviceSettingsCombo->multiSampleTypeList.GetSize(); i++ )
    {
        D3DMULTISAMPLE_TYPE msType = pDeviceSettingsCombo->multiSampleTypeList.GetAt( i );
        DWORD msQuality = pDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

        if( msType == pOptimalDeviceSettings->pp.MultiSampleType &&
            msQuality > pOptimalDeviceSettings->pp.MultiSampleQuality )
        {
            bMultiSampleFound = true;
            break;
        }
    }
    if( bMultiSampleFound )
        fCurRanking += fMultiSampleWeight;

    //---------------------
    // Swap effect
    //---------------------
    // No caps for swap effects

    //---------------------
    // Depth stencil 
    //---------------------
    if( pDeviceSettingsCombo->depthStencilFormatList.Contains( pOptimalDeviceSettings->pp.AutoDepthStencilFormat ) )
        fCurRanking += fDepthStencilWeight;

    //---------------------
    // Present flags
    //---------------------
    // No caps for the present flags

    //---------------------
    // Refresh rate
    //---------------------
    bool bRefreshFound = false;
    for( int idm = 0; idm < pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetSize(); idm++ )
    {
        D3DDISPLAYMODE displayMode = pDeviceSettingsCombo->pAdapterInfo->displayModeList.GetAt( idm );
        if( displayMode.Format != pDeviceSettingsCombo->AdapterFormat )
            continue;
        if( displayMode.RefreshRate == pOptimalDeviceSettings->pp.FullScreen_RefreshRateInHz )
            bRefreshFound = true;
    }
    if( bRefreshFound )
        fCurRanking += fRefreshRateWeight;

    //---------------------
    // Present interval
    //---------------------
    // If keep present interval then check that the present interval is supported by this combo
    if( pDeviceSettingsCombo->presentIntervalList.Contains( pOptimalDeviceSettings->pp.PresentationInterval ) )
        fCurRanking += fPresentIntervalWeight;

    return fCurRanking;
}


//--------------------------------------------------------------------------------------
// Builds valid device settings using the match options, the input device settings, and the 
// best device settings combo found.
//--------------------------------------------------------------------------------------
void DXUTBuildValidD3D9DeviceSettings( DXUTD3D9DeviceSettings* pValidDeviceSettings,
                                       CD3D9EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                       DXUTD3D9DeviceSettings* pDeviceSettingsIn,
                                       DXUTMatchOptions* pMatchOptions )
{
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    D3DDISPLAYMODE adapterDesktopDisplayMode;
    pD3D->GetAdapterDisplayMode( pBestDeviceSettingsCombo->AdapterOrdinal, &adapterDesktopDisplayMode );

    // For each setting pick the best, taking into account the match options and 
    // what's supported by the device

    //---------------------
    // Adapter Ordinal
    //---------------------
    // Just using pBestDeviceSettingsCombo->AdapterOrdinal

    //---------------------
    // Device Type
    //---------------------
    // Just using pBestDeviceSettingsCombo->DeviceType

    //---------------------
    // Windowed 
    //---------------------
    // Just using pBestDeviceSettingsCombo->Windowed

    //---------------------
    // Adapter Format
    //---------------------
    // Just using pBestDeviceSettingsCombo->AdapterFormat

    //---------------------
    // Vertex processing
    //---------------------
    DWORD dwBestBehaviorFlags = 0;
    if( pMatchOptions->eVertexProcessing == DXUTMT_PRESERVE_INPUT )
    {
        dwBestBehaviorFlags = pDeviceSettingsIn->BehaviorFlags;
    }
    else if( pMatchOptions->eVertexProcessing == DXUTMT_IGNORE_INPUT )
    {
        // The framework defaults to HWVP if available otherwise use SWVP
        if( ( pBestDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) != 0 )
            dwBestBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        else
            dwBestBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
    else // if( pMatchOptions->eVertexProcessing == DXUTMT_CLOSEST_TO_INPUT )    
    {
        // Default to input, and fallback to SWVP if HWVP not available 
        dwBestBehaviorFlags = pDeviceSettingsIn->BehaviorFlags;
        if( ( pBestDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 &&
            ( ( dwBestBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) != 0 ||
              ( dwBestBehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING ) != 0 ) )
        {
            dwBestBehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
            dwBestBehaviorFlags &= ~D3DCREATE_MIXED_VERTEXPROCESSING;
            dwBestBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }

        // One of these must be selected
        if( ( dwBestBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING ) == 0 &&
            ( dwBestBehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING ) == 0 &&
            ( dwBestBehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING ) == 0 )
        {
            if( ( pBestDeviceSettingsCombo->pDeviceInfo->Caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) != 0 )
                dwBestBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
            else
                dwBestBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
    }

    //---------------------
    // Resolution
    //---------------------
    D3DDISPLAYMODE bestDisplayMode;
    if( pMatchOptions->eResolution == DXUTMT_PRESERVE_INPUT )
    {
        bestDisplayMode.Width = pDeviceSettingsIn->pp.BackBufferWidth;
        bestDisplayMode.Height = pDeviceSettingsIn->pp.BackBufferHeight;
    }
    else
    {
        D3DDISPLAYMODE displayModeIn;
        if( pMatchOptions->eResolution == DXUTMT_CLOSEST_TO_INPUT &&
            pDeviceSettingsIn )
        {
            displayModeIn.Width = pDeviceSettingsIn->pp.BackBufferWidth;
            displayModeIn.Height = pDeviceSettingsIn->pp.BackBufferHeight;
        }
        else // if( pMatchOptions->eResolution == DXUTMT_IGNORE_INPUT )   
        {
            if( pBestDeviceSettingsCombo->Windowed )
            {
                // The framework defaults to 640x480 for windowed
                displayModeIn.Width = 640;
                displayModeIn.Height = 480;
            }
            else
            {
                // The framework defaults to desktop resolution for fullscreen to try to avoid slow mode change
                displayModeIn.Width = adapterDesktopDisplayMode.Width;
                displayModeIn.Height = adapterDesktopDisplayMode.Height;
            }
        }

        // Call a helper function to find the closest valid display mode to the optimal 
        DXUTFindValidD3D9Resolution( pBestDeviceSettingsCombo, displayModeIn, &bestDisplayMode );
    }

    //---------------------
    // Back Buffer Format
    //---------------------
    // Just using pBestDeviceSettingsCombo->BackBufferFormat

    //---------------------
    // Back buffer count
    //---------------------
    UINT bestBackBufferCount;
    if( pMatchOptions->eBackBufferCount == DXUTMT_PRESERVE_INPUT )
    {
        bestBackBufferCount = pDeviceSettingsIn->pp.BackBufferCount;
    }
    else if( pMatchOptions->eBackBufferCount == DXUTMT_IGNORE_INPUT )
    {
        // Default to double buffering.  Causes less latency than triple buffering
        bestBackBufferCount = 1;
    }
    else // if( pMatchOptions->eBackBufferCount == DXUTMT_CLOSEST_TO_INPUT )   
    {
        bestBackBufferCount = pDeviceSettingsIn->pp.BackBufferCount;
        if( bestBackBufferCount > 3 )
            bestBackBufferCount = 3;
        if( bestBackBufferCount < 1 )
            bestBackBufferCount = 1;
    }

    //---------------------
    // Multisample
    //---------------------
    D3DMULTISAMPLE_TYPE bestMultiSampleType;
    DWORD bestMultiSampleQuality;
    if( pDeviceSettingsIn && pDeviceSettingsIn->pp.SwapEffect != D3DSWAPEFFECT_DISCARD )
    {
        // Swap effect is not set to discard so multisampling has to off
        bestMultiSampleType = D3DMULTISAMPLE_NONE;
        bestMultiSampleQuality = 0;
    }
    else
    {
        if( pMatchOptions->eMultiSample == DXUTMT_PRESERVE_INPUT )
        {
            bestMultiSampleType = pDeviceSettingsIn->pp.MultiSampleType;
            bestMultiSampleQuality = pDeviceSettingsIn->pp.MultiSampleQuality;
        }
        else if( pMatchOptions->eMultiSample == DXUTMT_IGNORE_INPUT )
        {
            // Default to no multisampling (always supported)
            bestMultiSampleType = D3DMULTISAMPLE_NONE;
            bestMultiSampleQuality = 0;
        }
        else if( pMatchOptions->eMultiSample == DXUTMT_CLOSEST_TO_INPUT )
        {
            // Default to no multisampling (always supported)
            bestMultiSampleType = D3DMULTISAMPLE_NONE;
            bestMultiSampleQuality = 0;

            for( int i = 0; i < pBestDeviceSettingsCombo->multiSampleTypeList.GetSize(); i++ )
            {
                D3DMULTISAMPLE_TYPE type = pBestDeviceSettingsCombo->multiSampleTypeList.GetAt( i );
                DWORD qualityLevels = pBestDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

                // Check whether supported type is closer to the input than our current best
                if( abs( type - pDeviceSettingsIn->pp.MultiSampleType ) < abs( bestMultiSampleType -
                                                                               pDeviceSettingsIn->pp.MultiSampleType )
                    )
                {
                    bestMultiSampleType = type;
                    bestMultiSampleQuality = __min( qualityLevels - 1, pDeviceSettingsIn->pp.MultiSampleQuality );
                }
            }
        }
        else
        {
            // Error case
            bestMultiSampleType = D3DMULTISAMPLE_NONE;
            bestMultiSampleQuality = 0;
        }
    }

    //---------------------
    // Swap effect
    //---------------------
    D3DSWAPEFFECT bestSwapEffect;
    if( pMatchOptions->eSwapEffect == DXUTMT_PRESERVE_INPUT )
    {
        bestSwapEffect = pDeviceSettingsIn->pp.SwapEffect;
    }
    else if( pMatchOptions->eSwapEffect == DXUTMT_IGNORE_INPUT )
    {
        bestSwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    else // if( pMatchOptions->eSwapEffect == DXUTMT_CLOSEST_TO_INPUT )   
    {
        bestSwapEffect = pDeviceSettingsIn->pp.SwapEffect;

        // Swap effect has to be one of these 3
        if( bestSwapEffect != D3DSWAPEFFECT_DISCARD &&
            bestSwapEffect != D3DSWAPEFFECT_FLIP &&
            bestSwapEffect != D3DSWAPEFFECT_COPY )
        {
            bestSwapEffect = D3DSWAPEFFECT_DISCARD;
        }
    }

    //---------------------
    // Depth stencil 
    //---------------------
    D3DFORMAT bestDepthStencilFormat;
    bool bestEnableAutoDepthStencil;

    CGrowableArray <int> depthStencilRanking;
    depthStencilRanking.SetSize( pBestDeviceSettingsCombo->depthStencilFormatList.GetSize() );

    UINT dwBackBufferBitDepth = DXUTGetD3D9ColorChannelBits( pBestDeviceSettingsCombo->BackBufferFormat );
    UINT dwInputDepthBitDepth = 0;
    if( pDeviceSettingsIn )
        dwInputDepthBitDepth = DXUTGetDepthBits( pDeviceSettingsIn->pp.AutoDepthStencilFormat );

    for( int i = 0; i < pBestDeviceSettingsCombo->depthStencilFormatList.GetSize(); i++ )
    {
        D3DFORMAT curDepthStencilFmt = pBestDeviceSettingsCombo->depthStencilFormatList.GetAt( i );
        DWORD dwCurDepthBitDepth = DXUTGetDepthBits( curDepthStencilFmt );
        int nRanking;

        if( pMatchOptions->eDepthFormat == DXUTMT_PRESERVE_INPUT )
        {
            // Need to match bit depth of input
            if( dwCurDepthBitDepth == dwInputDepthBitDepth )
                nRanking = 0;
            else
                nRanking = 10000;
        }
        else if( pMatchOptions->eDepthFormat == DXUTMT_IGNORE_INPUT )
        {
            // Prefer match of backbuffer bit depth
            nRanking = abs( ( int )dwCurDepthBitDepth - ( int )dwBackBufferBitDepth * 4 );
        }
        else // if( pMatchOptions->eDepthFormat == DXUTMT_CLOSEST_TO_INPUT )
        {
            // Prefer match of input depth format bit depth
            nRanking = abs( ( int )dwCurDepthBitDepth - ( int )dwInputDepthBitDepth );
        }

        depthStencilRanking.Add( nRanking );
    }

    UINT dwInputStencilBitDepth = 0;
    if( pDeviceSettingsIn )
        dwInputStencilBitDepth = DXUTGetStencilBits( pDeviceSettingsIn->pp.AutoDepthStencilFormat );

    for( int i = 0; i < pBestDeviceSettingsCombo->depthStencilFormatList.GetSize(); i++ )
    {
        D3DFORMAT curDepthStencilFmt = pBestDeviceSettingsCombo->depthStencilFormatList.GetAt( i );
        int nRanking = depthStencilRanking.GetAt( i );
        DWORD dwCurStencilBitDepth = DXUTGetStencilBits( curDepthStencilFmt );

        if( pMatchOptions->eStencilFormat == DXUTMT_PRESERVE_INPUT )
        {
            // Need to match bit depth of input
            if( dwCurStencilBitDepth == dwInputStencilBitDepth )
                nRanking += 0;
            else
                nRanking += 10000;
        }
        else if( pMatchOptions->eStencilFormat == DXUTMT_IGNORE_INPUT )
        {
            // Prefer 0 stencil bit depth
            nRanking += dwCurStencilBitDepth;
        }
        else // if( pMatchOptions->eStencilFormat == DXUTMT_CLOSEST_TO_INPUT )
        {
            // Prefer match of input stencil format bit depth
            nRanking += abs( ( int )dwCurStencilBitDepth - ( int )dwInputStencilBitDepth );
        }

        depthStencilRanking.SetAt( i, nRanking );
    }

    int nBestRanking = 100000;
    int nBestIndex = -1;
    for( int i = 0; i < pBestDeviceSettingsCombo->depthStencilFormatList.GetSize(); i++ )
    {
        int nRanking = depthStencilRanking.GetAt( i );
        if( nRanking < nBestRanking )
        {
            nBestRanking = nRanking;
            nBestIndex = i;
        }
    }

    if( nBestIndex >= 0 )
    {
        bestDepthStencilFormat = pBestDeviceSettingsCombo->depthStencilFormatList.GetAt( nBestIndex );
        bestEnableAutoDepthStencil = true;
    }
    else
    {
        bestDepthStencilFormat = D3DFMT_UNKNOWN;
        bestEnableAutoDepthStencil = false;
    }


    //---------------------
    // Present flags
    //---------------------
    DWORD dwBestFlags;
    if( pMatchOptions->ePresentFlags == DXUTMT_PRESERVE_INPUT )
    {
        dwBestFlags = pDeviceSettingsIn->pp.Flags;
    }
    else if( pMatchOptions->ePresentFlags == DXUTMT_IGNORE_INPUT )
    {
        dwBestFlags = 0;
        if( bestEnableAutoDepthStencil )
            dwBestFlags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    }
    else // if( pMatchOptions->ePresentFlags == DXUTMT_CLOSEST_TO_INPUT )   
    {
        dwBestFlags = pDeviceSettingsIn->pp.Flags;
        if( bestEnableAutoDepthStencil )
            dwBestFlags |= D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
    }

    //---------------------
    // Refresh rate
    //---------------------
    if( pBestDeviceSettingsCombo->Windowed )
    {
        // Must be 0 for windowed
        bestDisplayMode.RefreshRate = 0;
    }
    else
    {
        if( pMatchOptions->eRefreshRate == DXUTMT_PRESERVE_INPUT )
        {
            bestDisplayMode.RefreshRate = pDeviceSettingsIn->pp.FullScreen_RefreshRateInHz;
        }
        else
        {
            UINT refreshRateMatch;
            if( pMatchOptions->eRefreshRate == DXUTMT_CLOSEST_TO_INPUT )
            {
                refreshRateMatch = pDeviceSettingsIn->pp.FullScreen_RefreshRateInHz;
            }
            else // if( pMatchOptions->eRefreshRate == DXUTMT_IGNORE_INPUT )   
            {
                refreshRateMatch = adapterDesktopDisplayMode.RefreshRate;
            }

            bestDisplayMode.RefreshRate = 0;

            if( refreshRateMatch != 0 )
            {
                int nBestRefreshRanking = 100000;
                CGrowableArray <D3DDISPLAYMODE>* pDisplayModeList =
                    &pBestDeviceSettingsCombo->pAdapterInfo->displayModeList;
                for( int iDisplayMode = 0; iDisplayMode < pDisplayModeList->GetSize(); iDisplayMode++ )
                {
                    D3DDISPLAYMODE displayMode = pDisplayModeList->GetAt( iDisplayMode );
                    if( displayMode.Format != pBestDeviceSettingsCombo->AdapterFormat ||
                        displayMode.Height != bestDisplayMode.Height ||
                        displayMode.Width != bestDisplayMode.Width )
                        continue; // Skip display modes that don't match 

                    // Find the delta between the current refresh rate and the optimal refresh rate 
                    int nCurRanking = abs( ( int )displayMode.RefreshRate - ( int )refreshRateMatch );

                    if( nCurRanking < nBestRefreshRanking )
                    {
                        bestDisplayMode.RefreshRate = displayMode.RefreshRate;
                        nBestRefreshRanking = nCurRanking;

                        // Stop if perfect match found
                        if( nBestRefreshRanking == 0 )
                            break;
                    }
                }
            }
        }
    }

    //---------------------
    // Present interval
    //---------------------
    UINT bestPresentInterval;
    if( pMatchOptions->ePresentInterval == DXUTMT_PRESERVE_INPUT )
    {
        bestPresentInterval = pDeviceSettingsIn->pp.PresentationInterval;
    }
    else if( pMatchOptions->ePresentInterval == DXUTMT_IGNORE_INPUT )
    {
        // For windowed and fullscreen, default to D3DPRESENT_INTERVAL_DEFAULT
        // which will wait for the vertical retrace period to prevent tearing.
        // For benchmarking, use D3DPRESENT_INTERVAL_DEFAULT  which will
        // will wait not for the vertical retrace period but may introduce tearing.
        bestPresentInterval = D3DPRESENT_INTERVAL_DEFAULT;
    }
    else // if( pMatchOptions->ePresentInterval == DXUTMT_CLOSEST_TO_INPUT )   
    {
        if( pBestDeviceSettingsCombo->presentIntervalList.Contains( pDeviceSettingsIn->pp.PresentationInterval ) )
        {
            bestPresentInterval = pDeviceSettingsIn->pp.PresentationInterval;
        }
        else
        {
            bestPresentInterval = D3DPRESENT_INTERVAL_DEFAULT;
        }
    }

    // Fill the device settings struct
    ZeroMemory( pValidDeviceSettings, sizeof( DXUTD3D9DeviceSettings ) );
    pValidDeviceSettings->AdapterOrdinal = pBestDeviceSettingsCombo->AdapterOrdinal;
    pValidDeviceSettings->DeviceType = pBestDeviceSettingsCombo->DeviceType;
    pValidDeviceSettings->AdapterFormat = pBestDeviceSettingsCombo->AdapterFormat;
    pValidDeviceSettings->BehaviorFlags = dwBestBehaviorFlags;
    pValidDeviceSettings->pp.BackBufferWidth = bestDisplayMode.Width;
    pValidDeviceSettings->pp.BackBufferHeight = bestDisplayMode.Height;
    pValidDeviceSettings->pp.BackBufferFormat = pBestDeviceSettingsCombo->BackBufferFormat;
    pValidDeviceSettings->pp.BackBufferCount = bestBackBufferCount;
    pValidDeviceSettings->pp.MultiSampleType = bestMultiSampleType;
    pValidDeviceSettings->pp.MultiSampleQuality = bestMultiSampleQuality;
    pValidDeviceSettings->pp.SwapEffect = bestSwapEffect;
    pValidDeviceSettings->pp.hDeviceWindow = pBestDeviceSettingsCombo->Windowed ? DXUTGetHWNDDeviceWindowed() :
        DXUTGetHWNDDeviceFullScreen();
    pValidDeviceSettings->pp.Windowed = pBestDeviceSettingsCombo->Windowed;
    pValidDeviceSettings->pp.EnableAutoDepthStencil = bestEnableAutoDepthStencil;
    pValidDeviceSettings->pp.AutoDepthStencilFormat = bestDepthStencilFormat;
    pValidDeviceSettings->pp.Flags = dwBestFlags;
    pValidDeviceSettings->pp.FullScreen_RefreshRateInHz = bestDisplayMode.RefreshRate;
    pValidDeviceSettings->pp.PresentationInterval = bestPresentInterval;
}


//--------------------------------------------------------------------------------------
// Internal helper function to find the closest allowed display mode to the optimal 
//--------------------------------------------------------------------------------------
HRESULT DXUTFindValidD3D9Resolution( CD3D9EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                     D3DDISPLAYMODE displayModeIn, D3DDISPLAYMODE* pBestDisplayMode )
{
    D3DDISPLAYMODE bestDisplayMode;
    ZeroMemory( &bestDisplayMode, sizeof( D3DDISPLAYMODE ) );

    if( pBestDeviceSettingsCombo->Windowed )
    {
        // In windowed mode, all resolutions are valid but restritions still apply 
        // on the size of the window.  See DXUTChangeD3D9Device() for details
        *pBestDisplayMode = displayModeIn;
    }
    else
    {
        int nBestRanking = 100000;
        int nCurRanking;
        CGrowableArray <D3DDISPLAYMODE>* pDisplayModeList = &pBestDeviceSettingsCombo->pAdapterInfo->displayModeList;
        for( int iDisplayMode = 0; iDisplayMode < pDisplayModeList->GetSize(); iDisplayMode++ )
        {
            D3DDISPLAYMODE displayMode = pDisplayModeList->GetAt( iDisplayMode );

            // Skip display modes that don't match the combo's adapter format
            if( displayMode.Format != pBestDeviceSettingsCombo->AdapterFormat )
                continue;

            // Find the delta between the current width/height and the optimal width/height
            nCurRanking = abs( ( int )displayMode.Width - ( int )displayModeIn.Width ) +
                abs( ( int )displayMode.Height - ( int )displayModeIn.Height );

            if( nCurRanking < nBestRanking )
            {
                bestDisplayMode = displayMode;
                nBestRanking = nCurRanking;

                // Stop if perfect match found
                if( nBestRanking == 0 )
                    break;
            }
        }

        if( bestDisplayMode.Width == 0 )
        {
            *pBestDisplayMode = displayModeIn;
            return E_FAIL; // No valid display modes found
        }

        *pBestDisplayMode = bestDisplayMode;
    }

    return S_OK;
}


//======================================================================================
//======================================================================================
// Direct3D 10 section
//======================================================================================
//======================================================================================
CD3D10Enumeration*  g_pDXUTD3D10Enumeration = NULL;

HRESULT WINAPI DXUTCreateD3D10Enumeration()
{
    if( g_pDXUTD3D10Enumeration == NULL )
    {
        g_pDXUTD3D10Enumeration = new CD3D10Enumeration();
        if( NULL == g_pDXUTD3D10Enumeration )
            return E_OUTOFMEMORY;
    }
    return S_OK;
}

void WINAPI DXUTDestroyD3D10Enumeration()
{
    SAFE_DELETE( g_pDXUTD3D10Enumeration );
}

class DXUTMemoryHelperD3D10Enum
{
public:
DXUTMemoryHelperD3D10Enum()
{
    DXUTCreateD3D10Enumeration();
}
~DXUTMemoryHelperD3D10Enum()
{
    DXUTDestroyD3D10Enumeration();
}
};


//--------------------------------------------------------------------------------------
CD3D10Enumeration* WINAPI DXUTGetD3D10Enumeration( bool bForceEnumerate, bool bEnumerateAllAdapterFormats )
{
    // Using an static class with accessor function to allow control of the construction order
    static DXUTMemoryHelperD3D10Enum d3d10enumMemory;

    if( g_pDXUTD3D10Enumeration && ( !g_pDXUTD3D10Enumeration->HasEnumerated() || bForceEnumerate ) )
    {
        g_pDXUTD3D10Enumeration->SetEnumerateAllAdapterFormats( bEnumerateAllAdapterFormats, false );
        LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE pCallbackIsDeviceAcceptable;
        void* pUserContext;
        DXUTGetCallbackD3D10DeviceAcceptable( &pCallbackIsDeviceAcceptable, &pUserContext );
        g_pDXUTD3D10Enumeration->Enumerate( pCallbackIsDeviceAcceptable, pUserContext );
    }

    return g_pDXUTD3D10Enumeration;
}


//--------------------------------------------------------------------------------------
CD3D10Enumeration::CD3D10Enumeration()
{
    m_bHasEnumerated = false;
    m_IsD3D10DeviceAcceptableFunc = NULL;
    m_pIsD3D10DeviceAcceptableFuncUserContext = NULL;

    m_nMinWidth = 640;
    m_nMinHeight = 480;
    m_nMaxWidth = UINT_MAX;
    m_nMaxHeight = UINT_MAX;
    m_bEnumerateAllAdapterFormats = false;

    m_nRefreshMin = 0;
    m_nRefreshMax = UINT_MAX;

    ResetPossibleDepthStencilFormats();
}


//--------------------------------------------------------------------------------------
CD3D10Enumeration::~CD3D10Enumeration()
{
    ClearAdapterInfoList();
}


//--------------------------------------------------------------------------------------
// Enumerate for each adapter all of the supported display modes, 
// device types, adapter formats, back buffer formats, window/full screen support, 
// depth stencil formats, multisampling types/qualities, and presentations intervals.
//
// For each combination of device type (HAL/REF), adapter format, back buffer format, and
// IsWindowed it will call the app's ConfirmDevice callback.  This allows the app
// to reject or allow that combination based on its caps/etc.  It also allows the 
// app to change the BehaviorFlags.  The BehaviorFlags defaults non-pure HWVP 
// if supported otherwise it will default to SWVP, however the app can change this 
// through the ConfirmDevice callback.
//--------------------------------------------------------------------------------------
HRESULT CD3D10Enumeration::Enumerate( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE IsD3D10DeviceAcceptableFunc,
                                      void* pIsD3D10DeviceAcceptableFuncUserContext )
{
    CDXUTPerfEventGenerator eventGenerator( DXUT_PERFEVENTCOLOR, L"DXUT D3D10 Enumeration" );
    HRESULT hr;
    IDXGIFactory* pFactory = DXUTGetDXGIFactory();
    if( pFactory == NULL )
        return E_FAIL;

    m_bHasEnumerated = true;
    m_IsD3D10DeviceAcceptableFunc = IsD3D10DeviceAcceptableFunc;
    m_pIsD3D10DeviceAcceptableFuncUserContext = pIsD3D10DeviceAcceptableFuncUserContext;

    ClearAdapterInfoList();

    for( int index = 0; ; ++index )
    {
        IDXGIAdapter* pAdapter = NULL;
        hr = pFactory->EnumAdapters( index, &pAdapter );
        if( FAILED( hr ) ) // DXGIERR_NOT_FOUND is expected when the end of the list is hit
            break;

        CD3D10EnumAdapterInfo* pAdapterInfo = new CD3D10EnumAdapterInfo;
        if( !pAdapterInfo )
        {
            SAFE_RELEASE( pAdapter );
            return E_OUTOFMEMORY;
        }
        ZeroMemory( pAdapterInfo, sizeof( CD3D10EnumAdapterInfo ) );
        pAdapterInfo->AdapterOrdinal = index;
        pAdapter->GetDesc( &pAdapterInfo->AdapterDesc );
        pAdapterInfo->m_pAdapter = pAdapter;

        // Enumerate the device driver types on the adapter.
        hr = EnumerateDevices( pAdapterInfo );
        if( FAILED( hr ) )
        {
            delete pAdapterInfo;
            continue;
        }

        hr = EnumerateOutputs( pAdapterInfo );
        if( FAILED( hr ) || pAdapterInfo->outputInfoList.GetSize() <= 0 )
        {
            delete pAdapterInfo;
            continue;
        }

        // Get info for each devicecombo on this device
        if( FAILED( hr = EnumerateDeviceCombos( pFactory, pAdapterInfo ) ) )
        {
            delete pAdapterInfo;
            continue;
        }

        hr = m_AdapterInfoList.Add( pAdapterInfo );
        if( FAILED( hr ) )
        {
            delete pAdapterInfo;
            return hr;
        }
    }
    //  If we did not get an adapter then we should still enumerate WARP and Ref.
    if (m_AdapterInfoList.GetSize() == 0) {


		CD3D10EnumAdapterInfo* pAdapterInfo = new CD3D10EnumAdapterInfo;
        if( !pAdapterInfo )
        {
            return E_OUTOFMEMORY;
        }
        ZeroMemory( pAdapterInfo, sizeof( CD3D10EnumAdapterInfo ) );

        hr = EnumerateDevices( pAdapterInfo );

        // Get info for each devicecombo on this device
        if( FAILED( hr = EnumerateDeviceCombosNoAdapter(  pAdapterInfo ) ) )
        {
            delete pAdapterInfo;
        }

        if (!FAILED(hr)) hr = m_AdapterInfoList.Add( pAdapterInfo );
    }



    //
    // Check for 2 or more adapters with the same name. Append the name
    // with some instance number if that's the case to help distinguish
    // them.
    //
    bool bUniqueDesc = true;
    CD3D10EnumAdapterInfo* pAdapterInfo;
    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        CD3D10EnumAdapterInfo* pAdapterInfo1 = m_AdapterInfoList.GetAt( i );

        for( int j = i + 1; j < m_AdapterInfoList.GetSize(); j++ )
        {
            CD3D10EnumAdapterInfo* pAdapterInfo2 = m_AdapterInfoList.GetAt( j );
            if( wcsncmp( pAdapterInfo1->AdapterDesc.Description,
                         pAdapterInfo2->AdapterDesc.Description, DXGI_MAX_DEVICE_IDENTIFIER_STRING ) == 0 )
            {
                bUniqueDesc = false;
                break;
            }
        }

        if( !bUniqueDesc )
            break;
    }

    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        pAdapterInfo = m_AdapterInfoList.GetAt( i );

        wcscpy_s( pAdapterInfo->szUniqueDescription, 100, pAdapterInfo->AdapterDesc.Description );
        if( !bUniqueDesc )
        {
            WCHAR sz[100];
            swprintf_s( sz, 100, L" (#%d)", pAdapterInfo->AdapterOrdinal );
            wcscat_s( pAdapterInfo->szUniqueDescription, DXGI_MAX_DEVICE_IDENTIFIER_STRING, sz );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CD3D10Enumeration::EnumerateOutputs( CD3D10EnumAdapterInfo* pAdapterInfo )
{
    HRESULT hr;
    IDXGIOutput* pOutput;

    for( int iOutput = 0; ; ++iOutput )
    {
        pOutput = NULL;
        hr = pAdapterInfo->m_pAdapter->EnumOutputs( iOutput, &pOutput );
        if( DXGI_ERROR_NOT_FOUND == hr )
        {
            return S_OK;
        }
        else if( FAILED( hr ) )
        {
            return hr;	//Something bad happened.
        }
        else //Success!
        {
            CD3D10EnumOutputInfo* pOutputInfo = new CD3D10EnumOutputInfo;
            if( !pOutputInfo )
            {
                SAFE_RELEASE( pOutput );
                return E_OUTOFMEMORY;
            }
            ZeroMemory( pOutputInfo, sizeof( CD3D10EnumOutputInfo ) );
            pOutput->GetDesc( &pOutputInfo->Desc );
            pOutputInfo->Output = iOutput;
            pOutputInfo->m_pOutput = pOutput;

            EnumerateDisplayModes( pOutputInfo );
            if( pOutputInfo->displayModeList.GetSize() <= 0 )
            {
                // If this output has no valid display mode, do not save it.
                delete pOutputInfo;
                continue;
            }

            hr = pAdapterInfo->outputInfoList.Add( pOutputInfo );
            if( FAILED( hr ) )
            {
                delete pOutputInfo;
                return hr;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
HRESULT CD3D10Enumeration::EnumerateDisplayModes( CD3D10EnumOutputInfo* pOutputInfo )
{
    HRESULT hr = S_OK;
    DXGI_FORMAT allowedAdapterFormatArray[] =
        {
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,     //This is DXUT's preferred mode

            DXGI_FORMAT_R8G8B8A8_UNORM,			
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R10G10B10A2_UNORM
        };
    int allowedAdapterFormatArrayCount = sizeof( allowedAdapterFormatArray ) / sizeof( allowedAdapterFormatArray[0] );

    // Swap perferred modes for apps running in linear space
    DXGI_FORMAT RemoteMode = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if( !DXUTIsInGammaCorrectMode() )
    {
        allowedAdapterFormatArray[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        allowedAdapterFormatArray[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        RemoteMode = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    // The fast path only enumerates R8G8B8A8_UNORM_SRGB modes
    if( !m_bEnumerateAllAdapterFormats )
        allowedAdapterFormatArrayCount = 1;

    for( int f = 0; f < allowedAdapterFormatArrayCount; ++f )
    {
        // Fast-path: Try to grab at least 512 modes.
        //			  This is to avoid calling GetDisplayModeList more times than necessary.
        //			  GetDisplayModeList is an expensive call.
        UINT NumModes = 512;
        DXGI_MODE_DESC* pDesc = new DXGI_MODE_DESC[ NumModes ];
        assert( pDesc );
        if( !pDesc )
            return E_OUTOFMEMORY;

        hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                         DXGI_ENUM_MODES_SCALING,
                                                         &NumModes,
                                                         pDesc );
        if( DXGI_ERROR_NOT_FOUND == hr )
        {
            SAFE_DELETE_ARRAY( pDesc );
            NumModes = 0;
            break;
        }
        else if( MAKE_DXGI_HRESULT( 34 ) == hr && RemoteMode == allowedAdapterFormatArray[f] )
        {
            // DXGI cannot enumerate display modes over a remote session.  Therefore, create a fake display
            // mode for the current screen resolution for the remote session.
            if( 0 != GetSystemMetrics( 0x1000 ) ) // SM_REMOTESESSION
            {
                DEVMODE DevMode;
                DevMode.dmSize = sizeof( DEVMODE );
                if( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &DevMode ) )
                {
                    NumModes = 1;
                    pDesc[0].Width = DevMode.dmPelsWidth;
                    pDesc[0].Height = DevMode.dmPelsHeight;
                    pDesc[0].Format = RemoteMode;
                    pDesc[0].RefreshRate.Numerator = 60;
                    pDesc[0].RefreshRate.Denominator = 1;
                    pDesc[0].ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
                    pDesc[0].Scaling = DXGI_MODE_SCALING_CENTERED;
                    hr = S_OK;
                }
            }
        }
        else if( DXGI_ERROR_MORE_DATA == hr )
        {
            // Slow path.  There were more than 512 modes.
            SAFE_DELETE_ARRAY( pDesc );
            hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                             DXGI_ENUM_MODES_SCALING,
                                                             &NumModes,
                                                             NULL );
            if( FAILED( hr ) )
            {
                NumModes = 0;
                break;
            }

            pDesc = new DXGI_MODE_DESC[ NumModes ];
            assert( pDesc );
            if( !pDesc )
                return E_OUTOFMEMORY;

            hr = pOutputInfo->m_pOutput->GetDisplayModeList( allowedAdapterFormatArray[f],
                                                             DXGI_ENUM_MODES_SCALING,
                                                             &NumModes,
                                                             pDesc );
            if( FAILED( hr ) )
            {
                SAFE_DELETE_ARRAY( pDesc );
                NumModes = 0;
                break;
            }

        }

        if( 0 == NumModes && 0 == f )
        {
            // No R8G8B8A8_UNORM_SRGB modes!
            // Abort the fast-path if we're on it
            allowedAdapterFormatArrayCount = sizeof( allowedAdapterFormatArray ) / sizeof
                ( allowedAdapterFormatArray[0] );
            SAFE_DELETE_ARRAY( pDesc );
            continue;
        }

        if( SUCCEEDED( hr ) )
        {
            for( UINT m = 0; m < NumModes; m++ )
            {
                pOutputInfo->displayModeList.Add( pDesc[m] );
            }
        }

        SAFE_DELETE_ARRAY( pDesc );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT CD3D10Enumeration::EnumerateDevices( CD3D10EnumAdapterInfo* pAdapterInfo )
{
    HRESULT hr;

    const D3D10_DRIVER_TYPE devTypeArray[] =
        {
           D3D10_DRIVER_TYPE_HARDWARE,
           D3D10_DRIVER_TYPE_SOFTWARE,
           D3D10_DRIVER_TYPE_REFERENCE,
        };
    const UINT devTypeArrayCount = sizeof( devTypeArray ) / sizeof( devTypeArray[0] );

    // Enumerate each Direct3D device type
    for( UINT iDeviceType = 0; iDeviceType < devTypeArrayCount; iDeviceType++ )
    {
        CD3D10EnumDeviceInfo* pDeviceInfo = new CD3D10EnumDeviceInfo;
        if( pDeviceInfo == NULL )
            return E_OUTOFMEMORY;

        // Fill struct w/ AdapterOrdinal and D3DX10_DRIVER_TYPE
        pDeviceInfo->AdapterOrdinal = pAdapterInfo->AdapterOrdinal;
        pDeviceInfo->DeviceType = devTypeArray[iDeviceType];

        // Call D3D10CreateDevice to ensure that this is a D3D10 device.
        ID3D10Device* pd3dDevice = NULL;
        IDXGIAdapter* pAdapter = NULL;
        if( devTypeArray[iDeviceType] == D3D10_DRIVER_TYPE_HARDWARE )
            pAdapter = pAdapterInfo->m_pAdapter;
		
        HMODULE wrp = NULL;
        if (devTypeArray[iDeviceType] == D3D10_DRIVER_TYPE_SOFTWARE) wrp = LoadLibrary(L"D3D10WARP.dll");

        hr = DXUT_Dynamic_D3D10CreateDevice( pAdapter, devTypeArray[iDeviceType], ( HMODULE )wrp, 0, NULL,
                                             D3D10_SDK_VERSION, &pd3dDevice );
        if( FAILED( hr ) )
        {
            delete pDeviceInfo;
            continue;
        }

         if( devTypeArray[iDeviceType] != D3D10_DRIVER_TYPE_HARDWARE )
        {
            IDXGIDevice* pDXGIDev = NULL;
            hr = pd3dDevice->QueryInterface( __uuidof( IDXGIDevice ), ( LPVOID* )&pDXGIDev );
            if( SUCCEEDED( hr ) && pDXGIDev )
            {
                SAFE_RELEASE( pAdapterInfo->m_pAdapter );
                pDXGIDev->GetAdapter( &pAdapterInfo->m_pAdapter );
            }
            SAFE_RELEASE( pDXGIDev );
        }

        SAFE_RELEASE( pd3dDevice );
        pAdapterInfo->deviceInfoList.Add( pDeviceInfo );
    }

    return S_OK;
}

HRESULT CD3D10Enumeration::EnumerateDeviceCombosNoAdapter(  CD3D10EnumAdapterInfo* pAdapterInfo )
{
    // Iterate through each combination of device driver type, output,
    // adapter format, and backbuffer format to build the adapter's device combo list.
    //

        for( int device = 0; device < pAdapterInfo->deviceInfoList.GetSize(); ++device )
        {
            CD3D10EnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.GetAt( device );

            DXGI_FORMAT BufferFormatArray[] =
            {
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,   //This is DXUT's preferred mode

                DXGI_FORMAT_R8G8B8A8_UNORM,		
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R10G10B10A2_UNORM
            };
            const UINT BufferFormatArrayCount = sizeof( BufferFormatArray ) / sizeof
                ( BufferFormatArray[0] );

            // Swap perferred modes for apps running in linear space
            if( !DXUTIsInGammaCorrectMode() )
            {
                BufferFormatArray[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                BufferFormatArray[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }

            for( UINT iBufferFormat = 0; iBufferFormat < BufferFormatArrayCount; iBufferFormat++ )
            {
                DXGI_FORMAT BufferFormat = BufferFormatArray[iBufferFormat];



                    // determine if there are any modes for this particular format


                    // If an application callback function has been provided, make sure this device
                    // is acceptable to the app.

                    if( m_IsD3D10DeviceAcceptableFunc != NULL )
                    {
                        if( !m_IsD3D10DeviceAcceptableFunc( pAdapterInfo->AdapterOrdinal, 0,
                                                            pDeviceInfo->DeviceType, BufferFormat,
                                                            TRUE,
                                                            m_pIsD3D10DeviceAcceptableFuncUserContext ) )
                            continue;
                    }
                    // At this point, we have an adapter/device/backbufferformat/iswindowed
                    // DeviceCombo that is supported by the system. We still 
                    // need to find one or more suitable depth/stencil buffer format,
                    // multisample type, and present interval.
                    CD3D10EnumDeviceSettingsCombo* pDeviceCombo = new CD3D10EnumDeviceSettingsCombo;
                    if( pDeviceCombo == NULL )
                        return E_OUTOFMEMORY;

                    pDeviceCombo->AdapterOrdinal = pDeviceInfo->AdapterOrdinal;
                    pDeviceCombo->DeviceType = pDeviceInfo->DeviceType;
                    pDeviceCombo->BackBufferFormat = BufferFormat;
                    pDeviceCombo->Windowed = TRUE;
                    pDeviceCombo->Output = 0;
                    pDeviceCombo->pAdapterInfo = pAdapterInfo;
                    pDeviceCombo->pDeviceInfo = pDeviceInfo;
                    pDeviceCombo->pOutputInfo = NULL;

                    BuildMultiSampleQualityList( BufferFormat, pDeviceCombo );

                    if( FAILED( pAdapterInfo->deviceSettingsComboList.Add( pDeviceCombo ) ) )
                        delete pDeviceCombo;
                }
                    
        }


    return S_OK;
}
//--------------------------------------------------------------------------------------
HRESULT CD3D10Enumeration::EnumerateDeviceCombos( IDXGIFactory* pFactory, CD3D10EnumAdapterInfo* pAdapterInfo )
{
    // Iterate through each combination of device driver type, output,
    // adapter format, and backbuffer format to build the adapter's device combo list.
    //

    for( int output = 0; output < pAdapterInfo->outputInfoList.GetSize(); ++output )
    {
        CD3D10EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( output );

        for( int device = 0; device < pAdapterInfo->deviceInfoList.GetSize(); ++device )
        {
            CD3D10EnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.GetAt( device );

            DXGI_FORMAT backBufferFormatArray[] =
                {
                    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,   //This is DXUT's preferred mode

                    DXGI_FORMAT_R8G8B8A8_UNORM,		
                    DXGI_FORMAT_R16G16B16A16_FLOAT,
                    DXGI_FORMAT_R10G10B10A2_UNORM
                };
            const UINT backBufferFormatArrayCount = sizeof( backBufferFormatArray ) / sizeof
                ( backBufferFormatArray[0] );

            // Swap perferred modes for apps running in linear space
            if( !DXUTIsInGammaCorrectMode() )
            {
                backBufferFormatArray[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
                backBufferFormatArray[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }

            for( UINT iBackBufferFormat = 0; iBackBufferFormat < backBufferFormatArrayCount; iBackBufferFormat++ )
            {
                DXGI_FORMAT backBufferFormat = backBufferFormatArray[iBackBufferFormat];

                for( int nWindowed = 0; nWindowed < 2; nWindowed++ )
                {
                    if( !nWindowed && pOutputInfo->displayModeList.GetSize() == 0 )
                        continue;

                    // determine if there are any modes for this particular format
                    UINT iModes = 0;
                    for( int i = 0; i < pOutputInfo->displayModeList.GetSize(); i++ )
                    {
                        if( backBufferFormat == pOutputInfo->displayModeList.GetAt( i ).Format )
                            iModes ++;
                    }
                    if( 0 == iModes )
                        continue;

                    // If an application callback function has been provided, make sure this device
                    // is acceptable to the app.
                    if( m_IsD3D10DeviceAcceptableFunc != NULL )
                    {
                        if( !m_IsD3D10DeviceAcceptableFunc( pAdapterInfo->AdapterOrdinal, output,
                                                            pDeviceInfo->DeviceType, backBufferFormat,
                                                            FALSE != nWindowed,
                                                            m_pIsD3D10DeviceAcceptableFuncUserContext ) )
                            continue;
                    }

                    // At this point, we have an adapter/device/backbufferformat/iswindowed
                    // DeviceCombo that is supported by the system. We still 
                    // need to find one or more suitable depth/stencil buffer format,
                    // multisample type, and present interval.
                    CD3D10EnumDeviceSettingsCombo* pDeviceCombo = new CD3D10EnumDeviceSettingsCombo;
                    if( pDeviceCombo == NULL )
                        return E_OUTOFMEMORY;

                    pDeviceCombo->AdapterOrdinal = pDeviceInfo->AdapterOrdinal;
                    pDeviceCombo->DeviceType = pDeviceInfo->DeviceType;
                    pDeviceCombo->BackBufferFormat = backBufferFormat;
                    pDeviceCombo->Windowed = ( nWindowed != 0 );
                    pDeviceCombo->Output = pOutputInfo->Output;
                    pDeviceCombo->pAdapterInfo = pAdapterInfo;
                    pDeviceCombo->pDeviceInfo = pDeviceInfo;
                    pDeviceCombo->pOutputInfo = pOutputInfo;

                    BuildMultiSampleQualityList( backBufferFormat, pDeviceCombo );

                    if( FAILED( pAdapterInfo->deviceSettingsComboList.Add( pDeviceCombo ) ) )
                        delete pDeviceCombo;
                }
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Release all the allocated CD3D10EnumAdapterInfo objects and empty the list
//--------------------------------------------------------------------------------------
void CD3D10Enumeration::ClearAdapterInfoList()
{
    CD3D10EnumAdapterInfo* pAdapterInfo;
    for( int i = 0; i < m_AdapterInfoList.GetSize(); i++ )
    {
        pAdapterInfo = m_AdapterInfoList.GetAt( i );
        delete pAdapterInfo;
    }

    m_AdapterInfoList.RemoveAll();
}


//--------------------------------------------------------------------------------------
void CD3D10Enumeration::ResetPossibleDepthStencilFormats()
{
    m_DepthStencilPossibleList.RemoveAll();
    m_DepthStencilPossibleList.Add( DXGI_FORMAT_D32_FLOAT_S8X24_UINT );
    m_DepthStencilPossibleList.Add( DXGI_FORMAT_D32_FLOAT );
    m_DepthStencilPossibleList.Add( DXGI_FORMAT_D24_UNORM_S8_UINT );
    m_DepthStencilPossibleList.Add( DXGI_FORMAT_D16_UNORM );
}

//--------------------------------------------------------------------------------------
void CD3D10Enumeration::SetEnumerateAllAdapterFormats( bool bEnumerateAllAdapterFormats, bool bEnumerateNow )
{
    m_bEnumerateAllAdapterFormats = bEnumerateAllAdapterFormats;

    if( bEnumerateNow )
    {
        LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE pCallbackIsDeviceAcceptable;
        void* pUserContext;
        DXUTGetCallbackD3D10DeviceAcceptable( &pCallbackIsDeviceAcceptable, &pUserContext );
        g_pDXUTD3D10Enumeration->Enumerate( pCallbackIsDeviceAcceptable, pUserContext );
    }
}


//--------------------------------------------------------------------------------------
void CD3D10Enumeration::BuildMultiSampleQualityList( DXGI_FORMAT fmt, CD3D10EnumDeviceSettingsCombo* pDeviceCombo )
{
    ID3D10Device* pd3dDevice = NULL;
    IDXGIAdapter* pAdapter = NULL;
    if( pDeviceCombo->DeviceType == D3D10_DRIVER_TYPE_HARDWARE )
    {
        IDXGIFactory* factory = DXUTGetDXGIFactory();
        assert( factory != NULL );
        factory->EnumAdapters( pDeviceCombo->pAdapterInfo->AdapterOrdinal, &pAdapter );
    }

    if( FAILED( DXUT_Dynamic_D3D10CreateDevice( pAdapter, pDeviceCombo->DeviceType, ( HMODULE )0, 0, NULL,
                                                D3D10_SDK_VERSION, &pd3dDevice ) ) )
        return;

    for( int i = 1; i <= D3D10_MAX_MULTISAMPLE_SAMPLE_COUNT; ++i )
    {
        UINT Quality;
        if( SUCCEEDED( pd3dDevice->CheckMultisampleQualityLevels( fmt, i, &Quality ) ) && Quality > 0 )
        {
            pDeviceCombo->multiSampleCountList.Add( i );
            pDeviceCombo->multiSampleQualityList.Add( Quality );
        }
    }

    SAFE_RELEASE( pAdapter );
    SAFE_RELEASE( pd3dDevice );
}


//--------------------------------------------------------------------------------------
// Call GetAdapterInfoList() after Enumerate() to get a STL vector of 
//       CD3D10EnumAdapterInfo* 
//--------------------------------------------------------------------------------------
CGrowableArray <CD3D10EnumAdapterInfo*>* CD3D10Enumeration::GetAdapterInfoList()
{
    return &m_AdapterInfoList;
}


//--------------------------------------------------------------------------------------
CD3D10EnumAdapterInfo* CD3D10Enumeration::GetAdapterInfo( UINT AdapterOrdinal )
{
    for( int iAdapter = 0; iAdapter < m_AdapterInfoList.GetSize(); iAdapter++ )
    {
        CD3D10EnumAdapterInfo* pAdapterInfo = m_AdapterInfoList.GetAt( iAdapter );
        if( pAdapterInfo->AdapterOrdinal == AdapterOrdinal )
            return pAdapterInfo;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
CD3D10EnumDeviceInfo* CD3D10Enumeration::GetDeviceInfo( UINT AdapterOrdinal, D3D10_DRIVER_TYPE DeviceType )
{
    CD3D10EnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
    if( pAdapterInfo )
    {
        for( int iDeviceInfo = 0; iDeviceInfo < pAdapterInfo->deviceInfoList.GetSize(); iDeviceInfo++ )
        {
            CD3D10EnumDeviceInfo* pDeviceInfo = pAdapterInfo->deviceInfoList.GetAt( iDeviceInfo );
            if( pDeviceInfo->DeviceType == DeviceType )
                return pDeviceInfo;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
CD3D10EnumOutputInfo* CD3D10Enumeration::GetOutputInfo( UINT AdapterOrdinal, UINT Output )
{
    CD3D10EnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
    if( pAdapterInfo && pAdapterInfo->outputInfoList.GetSize() > int( Output ) )
    {
        return pAdapterInfo->outputInfoList.GetAt( Output );
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
CD3D10EnumDeviceSettingsCombo* CD3D10Enumeration::GetDeviceSettingsCombo( UINT AdapterOrdinal,
                                                                          D3D10_DRIVER_TYPE DeviceType, UINT Output,
                                                                          DXGI_FORMAT BackBufferFormat, BOOL Windowed )
{
    CD3D10EnumAdapterInfo* pAdapterInfo = GetAdapterInfo( AdapterOrdinal );
    if( pAdapterInfo )
    {
        for( int iDeviceCombo = 0; iDeviceCombo < pAdapterInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo = pAdapterInfo->deviceSettingsComboList.GetAt(
                iDeviceCombo );
            if( pDeviceSettingsCombo->BackBufferFormat == BackBufferFormat &&
                pDeviceSettingsCombo->Windowed == Windowed )
                return pDeviceSettingsCombo;
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
CD3D10EnumOutputInfo::~CD3D10EnumOutputInfo( void )
{
    SAFE_RELEASE( m_pOutput );
    displayModeList.RemoveAll();
}


//--------------------------------------------------------------------------------------
CD3D10EnumDeviceInfo::~CD3D10EnumDeviceInfo()
{
}


//--------------------------------------------------------------------------------------
CD3D10EnumAdapterInfo::~CD3D10EnumAdapterInfo( void )
{
    for( int i = 0; i < outputInfoList.GetSize(); i++ )
    {
        CD3D10EnumOutputInfo* pOutputInfo = outputInfoList.GetAt( i );
        delete pOutputInfo;
    }
    outputInfoList.RemoveAll();

    for( int i = 0; i < deviceInfoList.GetSize(); ++i )
    {
        CD3D10EnumDeviceInfo* pDeviceInfo = deviceInfoList.GetAt( i );
        delete pDeviceInfo;
    }
    deviceInfoList.RemoveAll();

    for( int i = 0; i < deviceSettingsComboList.GetSize(); ++i )
    {
        CD3D10EnumDeviceSettingsCombo* pDeviceCombo = deviceSettingsComboList.GetAt( i );
        delete pDeviceCombo;
    }
    deviceSettingsComboList.RemoveAll();

    SAFE_RELEASE( m_pAdapter );
}


//--------------------------------------------------------------------------------------
// Returns the number of color channel bits in the specified DXGI_FORMAT
//--------------------------------------------------------------------------------------
UINT WINAPI DXUTGetDXGIColorChannelBits( DXGI_FORMAT fmt )
{
    switch( fmt )
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 32;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
            return 16;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
            return 10;

        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return 8;

        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return 5;

        default:
            return 0;
    }
}


//--------------------------------------------------------------------------------------
HRESULT DXUTFindValidD3D10DeviceSettings( DXUTD3D10DeviceSettings* pOut, DXUTD3D10DeviceSettings* pIn,
                                          DXUTMatchOptions* pMatchOptions, DXUTD3D10DeviceSettings* pOptimal )
{
    // Find the best combination of:
    //      Adapter Ordinal
    //      Device Type
    //      Back Buffer Format
    //      Windowed
    // given what's available on the system and the match options combined with the device settings input.
    // This combination of settings is encapsulated by the CD3D10EnumDeviceSettingsCombo class.
    float fBestRanking = -1.0f;
    CD3D10EnumDeviceSettingsCombo* pBestDeviceSettingsCombo = NULL;
    DXGI_MODE_DESC adapterDisplayMode;
    memset( &adapterDisplayMode, 0, sizeof(adapterDisplayMode) );

    CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
    CGrowableArray <CD3D10EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
    for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); iAdapter++ )
    {
        CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );

        // Get the desktop display mode of adapter
        DXUTGetD3D10AdapterDisplayMode( pAdapterInfo->AdapterOrdinal, 0, &adapterDisplayMode );

        // Enum all the device settings combinations.  A device settings combination is 
        // a unique set of an adapter format, back buffer format, and IsWindowed.
        for( int iDeviceCombo = 0; iDeviceCombo < pAdapterInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo = pAdapterInfo->deviceSettingsComboList.GetAt(
                iDeviceCombo );

            // Skip any combo that doesn't meet the preserve match options
            if( false == DXUTDoesD3D10DeviceComboMatchPreserveOptions( pDeviceSettingsCombo, pIn, pMatchOptions ) )
                continue;

            // Get a ranking number that describes how closely this device combo matches the optimal combo
            float fCurRanking = DXUTRankD3D10DeviceCombo( pDeviceSettingsCombo, pOptimal, &adapterDisplayMode );

            // If this combo better matches the input device settings then save it
            if( fCurRanking > fBestRanking )
            {
                pBestDeviceSettingsCombo = pDeviceSettingsCombo;
                fBestRanking = fCurRanking;
            }
        }
    }

    // If no best device combination was found then fail
    if( pBestDeviceSettingsCombo == NULL )
        return DXUTERR_NOCOMPATIBLEDEVICES;

    // Using the best device settings combo found, build valid device settings taking heed of 
    // the match options and the input device settings
    DXUTD3D10DeviceSettings validDeviceSettings;
    DXUTBuildValidD3D10DeviceSettings( &validDeviceSettings, pBestDeviceSettingsCombo, pIn, pMatchOptions );
    *pOut = validDeviceSettings;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Internal helper function to build a D3D10 device settings structure based upon the match 
// options.  If the match option is set to ignore, then a optimal default value is used.
// The default value may not exist on the system, but later this will be taken 
// into account.
//--------------------------------------------------------------------------------------
void DXUTBuildOptimalD3D10DeviceSettings( DXUTD3D10DeviceSettings* pOptimalDeviceSettings,
                                          DXUTD3D10DeviceSettings* pDeviceSettingsIn,
                                          DXUTMatchOptions* pMatchOptions )
{
    ZeroMemory( pOptimalDeviceSettings, sizeof( DXUTD3D10DeviceSettings ) );

    // Retrieve the desktop display mode.
    DXGI_MODE_DESC adapterDesktopDisplayMode = { 640, 480, { 0, 0 }, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB };
    DXUTGetD3D10AdapterDisplayMode( pOptimalDeviceSettings->AdapterOrdinal, 0, &adapterDesktopDisplayMode );

    //---------------------
    // Adapter ordinal
    //---------------------
    if( pMatchOptions->eAdapterOrdinal == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->AdapterOrdinal = 0;
    else
        pOptimalDeviceSettings->AdapterOrdinal = pDeviceSettingsIn->AdapterOrdinal;

    //---------------------
    // Device type
    //---------------------
    if( pMatchOptions->eDeviceType == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->DriverType = D3D10_DRIVER_TYPE_HARDWARE;
    else
        pOptimalDeviceSettings->DriverType = pDeviceSettingsIn->DriverType;

    //---------------------
    // Windowed
    //---------------------
    if( pMatchOptions->eWindowed == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->sd.Windowed = TRUE;
    else
        pOptimalDeviceSettings->sd.Windowed = pDeviceSettingsIn->sd.Windowed;

    //---------------------
    // Output #
    //---------------------
    if( pMatchOptions->eOutput == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->Output = 0;
    else
        pOptimalDeviceSettings->Output = pDeviceSettingsIn->Output;

    //---------------------
    // Create flags
    //---------------------
    pOptimalDeviceSettings->CreateFlags = pDeviceSettingsIn->CreateFlags;

    //---------------------
    // Resolution
    //---------------------
    if( pMatchOptions->eResolution == DXUTMT_IGNORE_INPUT )
    {
        // If windowed, default to 640x480
        // If fullscreen, default to the desktop res for quick mode change
        if( pOptimalDeviceSettings->sd.Windowed )
        {
            pOptimalDeviceSettings->sd.BufferDesc.Width = 640;
            pOptimalDeviceSettings->sd.BufferDesc.Height = 480;
        }
        else
        {
            pOptimalDeviceSettings->sd.BufferDesc.Width = adapterDesktopDisplayMode.Width;
            pOptimalDeviceSettings->sd.BufferDesc.Height = adapterDesktopDisplayMode.Height;
        }
    }
    else
    {
        pOptimalDeviceSettings->sd.BufferDesc.Width = pDeviceSettingsIn->sd.BufferDesc.Width;
        pOptimalDeviceSettings->sd.BufferDesc.Height = pDeviceSettingsIn->sd.BufferDesc.Height;
    }

    //---------------------
    // Back buffer format
    //---------------------
    if( pMatchOptions->eBackBufferFormat == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->sd.BufferDesc.Format = adapterDesktopDisplayMode.Format; // Default to match the adapter format
    else
        pOptimalDeviceSettings->sd.BufferDesc.Format = pDeviceSettingsIn->sd.BufferDesc.Format;

    //---------------------
    // Back buffer usage
    //---------------------
    pOptimalDeviceSettings->sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    //---------------------
    // Back buffer count
    //---------------------
    if( pMatchOptions->eBackBufferCount == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->sd.BufferCount = 2; // Default to triple buffering for perf gain
    else
        pOptimalDeviceSettings->sd.BufferCount = pDeviceSettingsIn->sd.BufferCount;

    //---------------------
    // Multisample
    //---------------------
    if( pMatchOptions->eMultiSample == DXUTMT_IGNORE_INPUT )
    {
        // Default to no multisampling 
        pOptimalDeviceSettings->sd.SampleDesc.Count = 0;
        pOptimalDeviceSettings->sd.SampleDesc.Quality = 0;
    }
    else
    {
        pOptimalDeviceSettings->sd.SampleDesc.Count = pDeviceSettingsIn->sd.SampleDesc.Count;
        pOptimalDeviceSettings->sd.SampleDesc.Quality = pDeviceSettingsIn->sd.SampleDesc.Quality;
    }

    //---------------------
    // Swap effect
    //---------------------
    if( pMatchOptions->eSwapEffect == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    else
        pOptimalDeviceSettings->sd.SwapEffect = pDeviceSettingsIn->sd.SwapEffect;

    //---------------------
    // Depth stencil 
    //---------------------
    if( pMatchOptions->eDepthFormat == DXUTMT_IGNORE_INPUT &&
        pMatchOptions->eStencilFormat == DXUTMT_IGNORE_INPUT )
    {
        pOptimalDeviceSettings->AutoCreateDepthStencil = TRUE;
        pOptimalDeviceSettings->AutoDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    }
    else
    {
        pOptimalDeviceSettings->AutoCreateDepthStencil = pDeviceSettingsIn->AutoCreateDepthStencil;
        pOptimalDeviceSettings->AutoDepthStencilFormat = pDeviceSettingsIn->AutoDepthStencilFormat;
    }

    //---------------------
    // Present flags
    //---------------------
    if( pMatchOptions->ePresentFlags == DXUTMT_IGNORE_INPUT )
        pOptimalDeviceSettings->PresentFlags = 0;
    else
        pOptimalDeviceSettings->PresentFlags = pDeviceSettingsIn->PresentFlags;

    //---------------------
    // Refresh rate
    //---------------------
    if( pMatchOptions->eRefreshRate == DXUTMT_IGNORE_INPUT )
    {
        pOptimalDeviceSettings->sd.BufferDesc.RefreshRate.Numerator = 0;
        pOptimalDeviceSettings->sd.BufferDesc.RefreshRate.Denominator = 0;
    }
    else
        pOptimalDeviceSettings->sd.BufferDesc.RefreshRate = pDeviceSettingsIn->sd.BufferDesc.RefreshRate;

    //---------------------
    // Present interval
    //---------------------
    if( pMatchOptions->ePresentInterval == DXUTMT_IGNORE_INPUT )
    {
        // For windowed and fullscreen, default to 1 which will
        // wait for the vertical retrace period to prevent tearing.
        // For benchmarking, use 0 which will not wait for the
        // vertical retrace period but may introduce tearing.
        pOptimalDeviceSettings->SyncInterval = 1;
    }
    else
    {
        pOptimalDeviceSettings->SyncInterval = pDeviceSettingsIn->SyncInterval;
    }
}


//--------------------------------------------------------------------------------------
// Returns false for any CD3D9EnumDeviceSettingsCombo that doesn't meet the preserve 
// match options against the input pDeviceSettingsIn.
//--------------------------------------------------------------------------------------
bool DXUTDoesD3D10DeviceComboMatchPreserveOptions( CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                                   DXUTD3D10DeviceSettings* pDeviceSettingsIn,
                                                   DXUTMatchOptions* pMatchOptions )
{
    //---------------------
    // Adapter ordinal
    //---------------------
    if( pMatchOptions->eAdapterOrdinal == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->AdapterOrdinal != pDeviceSettingsIn->AdapterOrdinal ) )
        return false;

    //---------------------
    // Device type
    //---------------------
    if( pMatchOptions->eDeviceType == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->DeviceType != pDeviceSettingsIn->DriverType ) )
        return false;

    //---------------------
    // Windowed
    //---------------------
    if( pMatchOptions->eWindowed == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->Windowed != pDeviceSettingsIn->sd.Windowed ) )
        return false;

    //---------------------
    // Output
    //---------------------
    if( pMatchOptions->eOutput == DXUTMT_PRESERVE_INPUT &&
        ( pDeviceSettingsCombo->Output != pDeviceSettingsIn->Output ) )
        return false;

    //---------------------
    // Resolution
    //---------------------
    // If keep resolution then check that width and height supported by this combo
    if( pMatchOptions->eResolution == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->pOutputInfo->displayModeList.GetSize(); i++ )
        {
            DXGI_MODE_DESC displayMode = pDeviceSettingsCombo->pOutputInfo->displayModeList.GetAt( i );
            if( displayMode.Width == pDeviceSettingsIn->sd.BufferDesc.Width &&
                displayMode.Height == pDeviceSettingsIn->sd.BufferDesc.Height )
            {
                bFound = true;
                break;
            }
        }

        // If the width and height are not supported by this combo, return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Back buffer format
    //---------------------
    if( pMatchOptions->eBackBufferFormat == DXUTMT_PRESERVE_INPUT &&
        pDeviceSettingsCombo->BackBufferFormat != pDeviceSettingsIn->sd.BufferDesc.Format )
        return false;

    //---------------------
    // Back buffer count
    //---------------------
    // No caps for the back buffer count

    //---------------------
    // Multisample
    //---------------------
    if( pMatchOptions->eMultiSample == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->multiSampleCountList.GetSize(); i++ )
        {
            UINT Count = pDeviceSettingsCombo->multiSampleCountList.GetAt( i );
            UINT Quality = pDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

            if( Count == pDeviceSettingsIn->sd.SampleDesc.Count &&
                Quality > pDeviceSettingsIn->sd.SampleDesc.Quality )
            {
                bFound = true;
                break;
            }
        }

        // If multisample type/quality not supported by this combo, then return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Swap effect
    //---------------------
    // No caps for swap effects

    //---------------------
    // Depth stencil 
    //---------------------
    // No caps for depth stencil

    //---------------------
    // Present flags
    //---------------------
    // No caps for the present flags

    //---------------------
    // Refresh rate
    //---------------------
    // If keep refresh rate then check that the resolution is supported by this combo
    if( pMatchOptions->eRefreshRate == DXUTMT_PRESERVE_INPUT )
    {
        bool bFound = false;
        for( int i = 0; i < pDeviceSettingsCombo->pOutputInfo->displayModeList.GetSize(); i++ )
        {
            DXGI_MODE_DESC displayMode = pDeviceSettingsCombo->pOutputInfo->displayModeList.GetAt( i );
            float fDenom1 = 1;
            float fDenom2 = 1;

            if( displayMode.RefreshRate.Denominator )
                fDenom1 = (float)displayMode.RefreshRate.Denominator;
            if( pDeviceSettingsIn->sd.BufferDesc.RefreshRate.Denominator )
                fDenom2 = (float)pDeviceSettingsIn->sd.BufferDesc.RefreshRate.Denominator;

            if( fabs( float( displayMode.RefreshRate.Numerator ) / fDenom1 -
                      float( pDeviceSettingsIn->sd.BufferDesc.RefreshRate.Numerator ) / fDenom2 ) < 0.1f )
            {
                bFound = true;
                break;
            }
        }

        // If refresh rate not supported by this combo, then return false
        if( !bFound )
            return false;
    }

    //---------------------
    // Present interval
    //---------------------
    // No caps for present interval

    return true;
}


//--------------------------------------------------------------------------------------
// Returns a ranking number that describes how closely this device 
// combo matches the optimal combo based on the match options and the optimal device settings
//--------------------------------------------------------------------------------------
float DXUTRankD3D10DeviceCombo( CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo,
                                DXUTD3D10DeviceSettings* pOptimalDeviceSettings,
                                DXGI_MODE_DESC* pAdapterDisplayMode )
{
    float fCurRanking = 0.0f;

    // Arbitrary weights.  Gives preference to the ordinal, device type, and windowed
    const float fAdapterOrdinalWeight = 1000.0f;
    const float fAdapterOutputWeight = 500.0f;
    const float fDeviceTypeWeight = 100.0f;
    const float fWindowWeight = 10.0f;
    const float fResolutionWeight = 1.0f;
    const float fBackBufferFormatWeight = 1.0f;
    const float fMultiSampleWeight = 1.0f;
    const float fRefreshRateWeight = 1.0f;

    //---------------------
    // Adapter ordinal
    //---------------------
    if( pDeviceSettingsCombo->AdapterOrdinal == pOptimalDeviceSettings->AdapterOrdinal )
        fCurRanking += fAdapterOrdinalWeight;

    //---------------------
    // Adapter ordinal
    //---------------------
    if( pDeviceSettingsCombo->Output == pOptimalDeviceSettings->Output )
        fCurRanking += fAdapterOutputWeight;

    //---------------------
    // Device type
    //---------------------
    if( pDeviceSettingsCombo->DeviceType == pOptimalDeviceSettings->DriverType )
        fCurRanking += fDeviceTypeWeight;
    // Slightly prefer HAL 
    if( pDeviceSettingsCombo->DeviceType == D3DDEVTYPE_HAL )
        fCurRanking += 0.1f;

    //---------------------
    // Windowed
    //---------------------
    if( pDeviceSettingsCombo->Windowed == pOptimalDeviceSettings->sd.Windowed )
        fCurRanking += fWindowWeight;

    //---------------------
    // Resolution
    //---------------------
    bool bResolutionFound = false;
    if (pDeviceSettingsCombo->pOutputInfo != NULL) {
        for( int idm = 0; idm < pDeviceSettingsCombo->pOutputInfo->displayModeList.GetSize(); idm++ )
        {
            DXGI_MODE_DESC displayMode = pDeviceSettingsCombo->pOutputInfo->displayModeList.GetAt( idm );
            if( displayMode.Width == pOptimalDeviceSettings->sd.BufferDesc.Width &&
                displayMode.Height == pOptimalDeviceSettings->sd.BufferDesc.Height )
                bResolutionFound = true;
        }
    }
    if( bResolutionFound )
        fCurRanking += fResolutionWeight;

    //---------------------
    // Back buffer format
    //---------------------
    if( pDeviceSettingsCombo->BackBufferFormat == pOptimalDeviceSettings->sd.BufferDesc.Format )
    {
        fCurRanking += fBackBufferFormatWeight;
    }
    else
    {
        int nBitDepthDelta = abs( ( long )DXUTGetDXGIColorChannelBits( pDeviceSettingsCombo->BackBufferFormat ) -
                                  ( long )DXUTGetDXGIColorChannelBits(
                                  pOptimalDeviceSettings->sd.BufferDesc.Format ) );
        float fScale = __max( 0.9f - ( float )nBitDepthDelta * 0.2f, 0.0f );
        fCurRanking += fScale * fBackBufferFormatWeight;
    }

    //---------------------
    // Back buffer count
    //---------------------
    // No caps for the back buffer count

    //---------------------
    // Multisample
    //---------------------
    bool bMultiSampleFound = false;
    for( int i = 0; i < pDeviceSettingsCombo->multiSampleCountList.GetSize(); i++ )
    {
        UINT Count = pDeviceSettingsCombo->multiSampleCountList.GetAt( i );
        UINT Quality = pDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

        if( Count == pOptimalDeviceSettings->sd.SampleDesc.Count &&
            Quality > pOptimalDeviceSettings->sd.SampleDesc.Quality )
        {
            bMultiSampleFound = true;
            break;
        }
    }
    if( bMultiSampleFound )
        fCurRanking += fMultiSampleWeight;

    //---------------------
    // Swap effect
    //---------------------
    // No caps for swap effects

    //---------------------
    // Depth stencil 
    //---------------------
    // No caps for swap effects

    //---------------------
    // Present flags
    //---------------------
    // No caps for the present flags

    //---------------------
    // Refresh rate
    //---------------------
    bool bRefreshFound = false;
    if ( pDeviceSettingsCombo->pOutputInfo != NULL ) {
        for( int idm = 0; idm < pDeviceSettingsCombo->pOutputInfo->displayModeList.GetSize(); idm++ )
        {
            DXGI_MODE_DESC displayMode = pDeviceSettingsCombo->pOutputInfo->displayModeList.GetAt( idm );
            float fDenom1 = 1;
            float fDenom2 = 1;

            if( displayMode.RefreshRate.Denominator )
                fDenom1 = (float)displayMode.RefreshRate.Denominator;
            
            if( pOptimalDeviceSettings->sd.BufferDesc.RefreshRate.Denominator )
                fDenom2 = (float)pOptimalDeviceSettings->sd.BufferDesc.RefreshRate.Denominator;

            if( fabs( float( displayMode.RefreshRate.Numerator ) / fDenom1 -
                float( pOptimalDeviceSettings->sd.BufferDesc.RefreshRate.Numerator ) / fDenom2 ) < 0.1f )
                bRefreshFound = true;
        }
    }
    if( bRefreshFound )
        fCurRanking += fRefreshRateWeight;

    //---------------------
    // Present interval
    //---------------------
    // No caps for the present flags

    return fCurRanking;
}


//--------------------------------------------------------------------------------------
// Builds valid device settings using the match options, the input device settings, and the 
// best device settings combo found.
//--------------------------------------------------------------------------------------
void DXUTBuildValidD3D10DeviceSettings( DXUTD3D10DeviceSettings* pValidDeviceSettings,
                                        CD3D10EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                        DXUTD3D10DeviceSettings* pDeviceSettingsIn,
                                        DXUTMatchOptions* pMatchOptions )
{
    DXGI_MODE_DESC adapterDisplayMode;
    memset( &adapterDisplayMode, 0, sizeof(adapterDisplayMode) );

    DXUTGetD3D10AdapterDisplayMode( pBestDeviceSettingsCombo->AdapterOrdinal,
                                    pBestDeviceSettingsCombo->Output, &adapterDisplayMode );

    // For each setting pick the best, taking into account the match options and 
    // what's supported by the device

    //---------------------
    // Adapter Ordinal
    //---------------------
    // Just using pBestDeviceSettingsCombo->AdapterOrdinal

    //---------------------
    // Device Type
    //---------------------
    // Just using pBestDeviceSettingsCombo->DeviceType

    //---------------------
    // Windowed 
    //---------------------
    // Just using pBestDeviceSettingsCombo->Windowed

    //---------------------
    // Output
    //---------------------
    // Just using pBestDeviceSettingsCombo->Output

    //---------------------
    // Resolution
    //---------------------
    DXGI_MODE_DESC bestDisplayMode;
    if( pMatchOptions->eResolution == DXUTMT_PRESERVE_INPUT )
    {
        bestDisplayMode.Width = pDeviceSettingsIn->sd.BufferDesc.Width;
        bestDisplayMode.Height = pDeviceSettingsIn->sd.BufferDesc.Height;
    }
    else
    {
        DXGI_MODE_DESC displayModeIn;
        if( pMatchOptions->eResolution == DXUTMT_CLOSEST_TO_INPUT &&
            pDeviceSettingsIn )
        {
            displayModeIn.Width = pDeviceSettingsIn->sd.BufferDesc.Width;
            displayModeIn.Height = pDeviceSettingsIn->sd.BufferDesc.Height;
        }
        else // if( pMatchOptions->eResolution == DXUTMT_IGNORE_INPUT )   
        {
            if( pBestDeviceSettingsCombo->Windowed )
            {
                // The framework defaults to 640x480 for windowed
                displayModeIn.Width = 640;
                displayModeIn.Height = 480;
            }
            else
            {
                // The framework defaults to desktop resolution for fullscreen to try to avoid slow mode change
                displayModeIn.Width = adapterDisplayMode.Width;
                displayModeIn.Height = adapterDisplayMode.Height;
            }
        }

        // Call a helper function to find the closest valid display mode to the optimal
        DXUTFindValidD3D10Resolution( pBestDeviceSettingsCombo, displayModeIn, &bestDisplayMode );
    }

    //---------------------
    // Back Buffer Format
    //---------------------
    // Just using pBestDeviceSettingsCombo->BackBufferFormat

    //---------------------
    // Back Buffer usage
    //---------------------
    // Just using pDeviceSettingsIn->sd.BackBufferUsage | DXGI_USAGE_RENDERTARGETOUTPUT

    //---------------------
    // Back buffer count
    //---------------------
    UINT bestBackBufferCount;
    if( pMatchOptions->eBackBufferCount == DXUTMT_PRESERVE_INPUT )
    {
        bestBackBufferCount = pDeviceSettingsIn->sd.BufferCount;
    }
    else if( pMatchOptions->eBackBufferCount == DXUTMT_IGNORE_INPUT )
    {
        // The framework defaults to triple buffering 
        bestBackBufferCount = 2;
    }
    else // if( pMatchOptions->eBackBufferCount == DXUTMT_CLOSEST_TO_INPUT )   
    {
        bestBackBufferCount = pDeviceSettingsIn->sd.BufferCount;
        if( bestBackBufferCount > 3 )
            bestBackBufferCount = 3;
        if( bestBackBufferCount < 1 )
            bestBackBufferCount = 1;
    }

    //---------------------
    // Multisample
    //---------------------
    UINT bestMultiSampleCount;
    UINT bestMultiSampleQuality;
    if( pDeviceSettingsIn && pDeviceSettingsIn->sd.SwapEffect != DXGI_SWAP_EFFECT_DISCARD )
    {
        // Swap effect is not set to discard so multisampling has to off
        bestMultiSampleCount = 1;
        bestMultiSampleQuality = 0;
    }
    else
    {
        if( pMatchOptions->eMultiSample == DXUTMT_PRESERVE_INPUT )
        {
            bestMultiSampleCount = pDeviceSettingsIn->sd.SampleDesc.Count;
            bestMultiSampleQuality = pDeviceSettingsIn->sd.SampleDesc.Quality;
        }
        else if( pMatchOptions->eMultiSample == DXUTMT_IGNORE_INPUT )
        {
            // Default to no multisampling (always supported)
            bestMultiSampleCount = 1;
            bestMultiSampleQuality = 0;
        }
        else if( pMatchOptions->eMultiSample == DXUTMT_CLOSEST_TO_INPUT )
        {
            // Default to no multisampling (always supported)
            bestMultiSampleCount = 1;
            bestMultiSampleQuality = 0;

            for( int i = 0; i < pBestDeviceSettingsCombo->multiSampleCountList.GetSize(); i++ )
            {
                UINT Count = pBestDeviceSettingsCombo->multiSampleCountList.GetAt( i );
                UINT Quality = pBestDeviceSettingsCombo->multiSampleQualityList.GetAt( i );

                // Check whether supported type is closer to the input than our current best
                if( labs( Count - pDeviceSettingsIn->sd.SampleDesc.Count ) < labs( bestMultiSampleCount -
                                                                                   pDeviceSettingsIn->sd.SampleDesc.Count ) )
                {
                    bestMultiSampleCount = Count;
                    bestMultiSampleQuality = __min( Quality - 1, pDeviceSettingsIn->sd.SampleDesc.Quality );
                }
            }
        }
        else
        {
            // Error case
            bestMultiSampleCount = 1;
            bestMultiSampleQuality = 0;
        }
    }

    //---------------------
    // Swap effect
    //---------------------
    DXGI_SWAP_EFFECT bestSwapEffect;
    if( pMatchOptions->eSwapEffect == DXUTMT_PRESERVE_INPUT )
    {
        bestSwapEffect = pDeviceSettingsIn->sd.SwapEffect;
    }
    else if( pMatchOptions->eSwapEffect == DXUTMT_IGNORE_INPUT )
    {
        bestSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    }
    else // if( pMatchOptions->eSwapEffect == DXUTMT_CLOSEST_TO_INPUT )
    {
        bestSwapEffect = pDeviceSettingsIn->sd.SwapEffect;

        // Swap effect has to be one of these 2
        if( bestSwapEffect != DXGI_SWAP_EFFECT_DISCARD &&
            bestSwapEffect != DXGI_SWAP_EFFECT_SEQUENTIAL )
        {
            bestSwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        }
    }

    //---------------------
    // Depth stencil 
    //---------------------
    DXGI_FORMAT bestDepthStencilFormat;
    bool bestEnableAutoDepthStencil;

    if( pMatchOptions->eDepthFormat == DXUTMT_IGNORE_INPUT &&
        pMatchOptions->eStencilFormat == DXUTMT_IGNORE_INPUT )
    {
        bestEnableAutoDepthStencil = true;
        bestDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    }
    else
    {
        assert( pDeviceSettingsIn != NULL );
        bestEnableAutoDepthStencil = pDeviceSettingsIn->AutoCreateDepthStencil;
        bestDepthStencilFormat = pDeviceSettingsIn->AutoDepthStencilFormat;
    }

    //---------------------
    // Present flags
    //---------------------

    //---------------------
    // Refresh rate
    //---------------------
    if( pBestDeviceSettingsCombo->Windowed )
    {
        // Must be 0 for windowed
        bestDisplayMode.RefreshRate.Numerator = 0;
        bestDisplayMode.RefreshRate.Denominator = 0;
    }
    else
    {
        if( pMatchOptions->eRefreshRate == DXUTMT_PRESERVE_INPUT )
        {
            bestDisplayMode.RefreshRate = pDeviceSettingsIn->sd.BufferDesc.RefreshRate;
        }
        else
        {
            DXGI_RATIONAL refreshRateMatch;
            if( pMatchOptions->eRefreshRate == DXUTMT_CLOSEST_TO_INPUT )
            {
                refreshRateMatch = pDeviceSettingsIn->sd.BufferDesc.RefreshRate;
            }
            else // if( pMatchOptions->eRefreshRate == DXUTMT_IGNORE_INPUT )
            {
                refreshRateMatch = adapterDisplayMode.RefreshRate;
            }

            // Default to 0 in case no matching mode is found.
            // 0, in this case means that we'll let DXGI choose the best one for us
            bestDisplayMode.RefreshRate.Numerator = 0;
            bestDisplayMode.RefreshRate.Denominator = 0;

            //            if( refreshRateMatch != 0 )
            {
                float fBestRefreshRanking = 100000.0f;
                CGrowableArray <DXGI_MODE_DESC>* pDisplayModeList =
                    &pBestDeviceSettingsCombo->pOutputInfo->displayModeList;
                for( int iDisplayMode = 0; iDisplayMode < pDisplayModeList->GetSize(); iDisplayMode++ )
                {
                    DXGI_MODE_DESC displayMode = pDisplayModeList->GetAt( iDisplayMode );
                    if( displayMode.Height != bestDisplayMode.Height ||
                        displayMode.Width != bestDisplayMode.Width )
                        continue; // Skip display modes that don't match

                    // Find the delta between the current refresh rate and the optimal refresh rate
                    float fDenom1 = 1;
                    float fDenom2 = 1;
                    if( displayMode.RefreshRate.Denominator )
                        fDenom1 = (float)displayMode.RefreshRate.Denominator;
                    if( refreshRateMatch.Denominator )
                        fDenom2 = (float)refreshRateMatch.Denominator;

                    float fCurRanking = abs( float( displayMode.RefreshRate.Numerator ) / fDenom1  -
                                             float( refreshRateMatch.Numerator ) / fDenom2 );

                    if( fCurRanking < fBestRefreshRanking )
                    {
                        bestDisplayMode.RefreshRate = displayMode.RefreshRate;
                        fBestRefreshRanking = fCurRanking;

                        // Stop if good-enough match found
                        if( fBestRefreshRanking < 0.1f )
                            break;
                    }
                }
            }
        }
    }

    //---------------------
    // Present interval
    //---------------------
    UINT32 bestPresentInterval;
    if( pMatchOptions->ePresentInterval == DXUTMT_PRESERVE_INPUT )
    {
        bestPresentInterval = pDeviceSettingsIn->SyncInterval;
    }
    else if( pMatchOptions->ePresentInterval == DXUTMT_IGNORE_INPUT )
    {
        // For windowed and fullscreen, default to 1 which will wait for
        // the vertical retrace period to prevent tearing. For benchmarking,
        // use 0 which will will wait not for the vertical retrace period
        // but may introduce tearing.

        // The reference driver does not support v-syncing and will
        // produce _com_error exceptions when the sync interval is
        // anything but 0.
        bestPresentInterval = ( D3D10_DRIVER_TYPE_REFERENCE == pBestDeviceSettingsCombo->DeviceType ) ? 0 : 1;
    }
    else // if( pMatchOptions->ePresentInterval == DXUTMT_CLOSEST_TO_INPUT )
    {
        bestPresentInterval = pDeviceSettingsIn->SyncInterval;
    }

    // Fill the device settings struct
    ZeroMemory( pValidDeviceSettings, sizeof( DXUTD3D10DeviceSettings ) );
    pValidDeviceSettings->AdapterOrdinal = pBestDeviceSettingsCombo->AdapterOrdinal;
    pValidDeviceSettings->Output = pBestDeviceSettingsCombo->Output;
    pValidDeviceSettings->DriverType = pBestDeviceSettingsCombo->DeviceType;
    pValidDeviceSettings->sd.BufferDesc.Width = bestDisplayMode.Width;
    pValidDeviceSettings->sd.BufferDesc.Height = bestDisplayMode.Height;
    pValidDeviceSettings->sd.BufferDesc.Format = pBestDeviceSettingsCombo->BackBufferFormat;
    pValidDeviceSettings->sd.BufferUsage = pDeviceSettingsIn->sd.BufferUsage | DXGI_USAGE_RENDER_TARGET_OUTPUT;
    pValidDeviceSettings->sd.BufferCount = bestBackBufferCount;
    pValidDeviceSettings->sd.SampleDesc.Count = bestMultiSampleCount;
    pValidDeviceSettings->sd.SampleDesc.Quality = bestMultiSampleQuality;
    pValidDeviceSettings->sd.SwapEffect = bestSwapEffect;
    pValidDeviceSettings->sd.OutputWindow = pBestDeviceSettingsCombo->Windowed ? DXUTGetHWNDDeviceWindowed() :
        DXUTGetHWNDDeviceFullScreen();
    pValidDeviceSettings->sd.Windowed = pBestDeviceSettingsCombo->Windowed;
    pValidDeviceSettings->sd.BufferDesc.RefreshRate = bestDisplayMode.RefreshRate;
    pValidDeviceSettings->sd.Flags = 0;
    pValidDeviceSettings->SyncInterval = bestPresentInterval;
    pValidDeviceSettings->AutoCreateDepthStencil = bestEnableAutoDepthStencil;
    pValidDeviceSettings->AutoDepthStencilFormat = bestDepthStencilFormat;
    pValidDeviceSettings->CreateFlags = pDeviceSettingsIn->CreateFlags;
}


//--------------------------------------------------------------------------------------
// Internal helper function to find the closest allowed display mode to the optimal 
//--------------------------------------------------------------------------------------
HRESULT DXUTFindValidD3D10Resolution( CD3D10EnumDeviceSettingsCombo* pBestDeviceSettingsCombo,
                                      DXGI_MODE_DESC displayModeIn, DXGI_MODE_DESC* pBestDisplayMode )
{
    DXGI_MODE_DESC bestDisplayMode;
    ZeroMemory( &bestDisplayMode, sizeof( bestDisplayMode ) );

    if( pBestDeviceSettingsCombo->Windowed )
    {
        *pBestDisplayMode = displayModeIn;

        // If our client rect size is smaller than our backbuffer size, use that size.
        // This would happen when we specify a windowed resolution larger than the screen.
        MONITORINFO Info;
        Info.cbSize = sizeof( MONITORINFO );
    if ( pBestDeviceSettingsCombo->pOutputInfo != NULL ) {
        GetMonitorInfo( pBestDeviceSettingsCombo->pOutputInfo->Desc.Monitor, &Info );

        UINT Width = Info.rcWork.right - Info.rcWork.left;
        UINT Height = Info.rcWork.bottom - Info.rcWork.top;

        RECT rcClient = Info.rcWork;
        AdjustWindowRect( &rcClient, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ), FALSE );
        Width = Width - ( rcClient.right - rcClient.left - Width );
        Height = Height - ( rcClient.bottom - rcClient.top - Height );

        pBestDisplayMode->Width = __min( pBestDisplayMode->Width, Width );
        pBestDisplayMode->Height = __min( pBestDisplayMode->Height, Height );

        } else {

        }
    }
    else
    {
        int nBestRanking = 100000;
        int nCurRanking;
        CGrowableArray <DXGI_MODE_DESC>* pDisplayModeList = &pBestDeviceSettingsCombo->pOutputInfo->displayModeList;
        for( int iDisplayMode = 0; iDisplayMode < pDisplayModeList->GetSize(); iDisplayMode++ )
        {
            DXGI_MODE_DESC displayMode = pDisplayModeList->GetAt( iDisplayMode );

            // Find the delta between the current width/height and the optimal width/height
            nCurRanking = abs( ( int )displayMode.Width - ( int )displayModeIn.Width ) +
                abs( ( int )displayMode.Height - ( int )displayModeIn.Height );

            if( nCurRanking < nBestRanking )
            {
                bestDisplayMode = displayMode;
                nBestRanking = nCurRanking;

                // Stop if perfect match found
                if( nBestRanking == 0 )
                    break;
            }
        }

        if( bestDisplayMode.Width == 0 )
        {
            *pBestDisplayMode = displayModeIn;
            return E_FAIL; // No valid display modes found
        }

        *pBestDisplayMode = bestDisplayMode;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Returns the DXGI_MODE_DESC struct for a given adapter and output 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTGetD3D10AdapterDisplayMode( UINT AdapterOrdinal, UINT nOutput, DXGI_MODE_DESC* pModeDesc )
{
    if( !pModeDesc )
        return E_INVALIDARG;

    CD3D10Enumeration* pD3DEnum = DXUTGetD3D10Enumeration();
    assert( pD3DEnum != NULL );
    CD3D10EnumOutputInfo* pOutputInfo = pD3DEnum->GetOutputInfo( AdapterOrdinal, nOutput );
    if( pOutputInfo )
    {
        pModeDesc->Width = 640;
        pModeDesc->Height = 480;
        pModeDesc->RefreshRate.Numerator = 0;
        pModeDesc->RefreshRate.Denominator = 0;
        pModeDesc->Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        pModeDesc->Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        pModeDesc->ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;

        DXGI_OUTPUT_DESC Desc;
        pOutputInfo->m_pOutput->GetDesc( &Desc );
        pModeDesc->Width = Desc.DesktopCoordinates.right - Desc.DesktopCoordinates.left;
        pModeDesc->Height = Desc.DesktopCoordinates.bottom - Desc.DesktopCoordinates.top;
    }

    if( pModeDesc->Format == DXGI_FORMAT_B8G8R8A8_UNORM )
        pModeDesc->Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    return S_OK;
}
