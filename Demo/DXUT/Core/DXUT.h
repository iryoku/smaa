//--------------------------------------------------------------------------------------
// File: DXUT.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef DXUT_H
#define DXUT_H

#ifndef UNICODE
#error "DXUT requires a Unicode build."
#endif

#include "dxsdkver.h"
#if ( _DXSDK_PRODUCT_MAJOR < 9 || _DXSDK_BUILD_MAJOR < 1949 )
#error The installed DXSDK is out of date.
#endif

#ifndef STRICT
#define STRICT
#endif

// If app hasn't choosen, set to work with Windows XP and beyond
#ifndef WINVER
#define WINVER         0x0501
#endif
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT   0x0600
#endif

// #define DXUT_AUTOLIB to automatically include the libs needed for DXUT 
#ifdef DXUT_AUTOLIB
#pragma comment( lib, "dxerr.lib" )
#pragma comment( lib, "dxguid.lib" )
#pragma comment( lib, "d3d9.lib" )
#pragma comment( lib, "d3d10.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#pragma comment( lib, "d3dx10d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#pragma comment( lib, "d3dx10.lib" )
#endif
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "winmm.lib" )
#pragma comment( lib, "comctl32.lib" )
#endif

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds

// Enable extra D3D debugging in debug builds if using the debug DirectX runtime.  
// This makes D3D objects work well in the debugger watch window, but slows down 
// performance slightly.
#if defined(DEBUG) || defined(_DEBUG)
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

// Standard Windows includes
#include <windows.h>
#include <initguid.h>
#include <assert.h>
#include <wchar.h>
#include <mmsystem.h>
#include <commctrl.h> // for InitCommonControls() 
#include <shellapi.h> // for ExtractIcon()
#include <new.h>      // for placement new
#include <shlobj.h>
#include <math.h>      
#include <limits.h>      
#include <stdio.h>

// CRT's memory leak detection
#if defined(DEBUG) || defined(_DEBUG)
#include <crtdbg.h>
#endif

// Direct3D9 includes
#include <d3d9.h>
#include <d3dx9.h>

// Direct3D10 includes
#include <d3dcommon.h>
#include <dxgi.h>
#include <d3d10_1.h>
#include <d3d10.h>
#include <d3dcompiler.h>
#include <d3dx10.h>

// XInput includes
#include <xinput.h>

// HRESULT translation for Direct3D and other APIs 
#include <dxerr.h>


#if defined(DEBUG) || defined(_DEBUG)
#ifndef V
#define V(x)           { hr = (x); if( FAILED(hr) ) { DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
#endif
#else
#ifndef V
#define V(x)           { hr = (x); }
#endif
#ifndef V_RETURN
#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif    
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p);   (p)=NULL; } }
#endif    
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif



//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct DXUTD3D9DeviceSettings
{
    UINT AdapterOrdinal;
    D3DDEVTYPE DeviceType;
    D3DFORMAT AdapterFormat;
    DWORD BehaviorFlags;
    D3DPRESENT_PARAMETERS pp;
};

struct DXUTD3D10DeviceSettings
{
    UINT AdapterOrdinal;
    D3D10_DRIVER_TYPE DriverType;
    UINT Output;
    DXGI_SWAP_CHAIN_DESC sd;
    UINT32 CreateFlags;
    UINT32 SyncInterval;
    DWORD PresentFlags;
    bool AutoCreateDepthStencil; // DXUT will create the a depth stencil resource and view if true
    DXGI_FORMAT AutoDepthStencilFormat;
};

enum DXUTDeviceVersion { DXUT_D3D9_DEVICE, DXUT_D3D10_DEVICE };
struct DXUTDeviceSettings
{
    DXUTDeviceVersion ver;
    union
    {
        DXUTD3D9DeviceSettings d3d9; // only valid if ver == DXUT_D3D9_DEVICE
        DXUTD3D10DeviceSettings d3d10; // only valid if ver == DXUT_D3D10_DEVICE
    };
};


//--------------------------------------------------------------------------------------
// Error codes
//--------------------------------------------------------------------------------------
#define DXUTERR_NODIRECT3D              MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0901)
#define DXUTERR_NOCOMPATIBLEDEVICES     MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0902)
#define DXUTERR_MEDIANOTFOUND           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0903)
#define DXUTERR_NONZEROREFCOUNT         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0904)
#define DXUTERR_CREATINGDEVICE          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0905)
#define DXUTERR_RESETTINGDEVICE         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0906)
#define DXUTERR_CREATINGDEVICEOBJECTS   MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0907)
#define DXUTERR_RESETTINGDEVICEOBJECTS  MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0908)
#define DXUTERR_DEVICEREMOVED           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x090A)

//--------------------------------------------------------------------------------------
// Callback registration 
//--------------------------------------------------------------------------------------

// General callbacks
typedef void    (CALLBACK *LPDXUTCALLBACKFRAMEMOVE)( double fTime, float fElapsedTime, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKKEYBOARD)( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKMOUSE)( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext );
typedef LRESULT (CALLBACK *LPDXUTCALLBACKMSGPROC)( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKTIMER)( UINT idEvent, void* pUserContext );
typedef bool    (CALLBACK *LPDXUTCALLBACKMODIFYDEVICESETTINGS)( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
typedef bool    (CALLBACK *LPDXUTCALLBACKDEVICEREMOVED)( void* pUserContext );

// Direct3D 9 callbacks
typedef bool    (CALLBACK *LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE)( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D9DEVICECREATED)( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D9DEVICERESET)( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D9FRAMERENDER)( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D9DEVICELOST)( void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D9DEVICEDESTROYED)( void* pUserContext );

// Direct3D 10 callbacks
typedef bool    (CALLBACK *LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE)( UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D10DEVICECREATED)( ID3D10Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef HRESULT (CALLBACK *LPDXUTCALLBACKD3D10SWAPCHAINRESIZED)( ID3D10Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D10FRAMERENDER)( ID3D10Device* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D10SWAPCHAINRELEASING)( void* pUserContext );
typedef void    (CALLBACK *LPDXUTCALLBACKD3D10DEVICEDESTROYED)( void* pUserContext );

// General callbacks
void WINAPI DXUTSetCallbackFrameMove( LPDXUTCALLBACKFRAMEMOVE pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackKeyboard( LPDXUTCALLBACKKEYBOARD pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackMouse( LPDXUTCALLBACKMOUSE pCallback, bool bIncludeMouseMove = false, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackMsgProc( LPDXUTCALLBACKMSGPROC pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackDeviceChanging( LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackDeviceRemoved( LPDXUTCALLBACKDEVICEREMOVED pCallback, void* pUserContext = NULL );

// Direct3D 9 callbacks
void WINAPI DXUTSetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D9DeviceCreated( LPDXUTCALLBACKD3D9DEVICECREATED pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D9DeviceReset( LPDXUTCALLBACKD3D9DEVICERESET pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D9FrameRender( LPDXUTCALLBACKD3D9FRAMERENDER pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D9DeviceLost( LPDXUTCALLBACKD3D9DEVICELOST pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D9DeviceDestroyed( LPDXUTCALLBACKD3D9DEVICEDESTROYED pCallback, void* pUserContext = NULL );

// Direct3D 10 callbacks
void WINAPI DXUTSetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D10DeviceCreated( LPDXUTCALLBACKD3D10DEVICECREATED pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D10SwapChainResized( LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D10FrameRender( LPDXUTCALLBACKD3D10FRAMERENDER pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D10SwapChainReleasing( LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallback, void* pUserContext = NULL );
void WINAPI DXUTSetCallbackD3D10DeviceDestroyed( LPDXUTCALLBACKD3D10DEVICEDESTROYED pCallback, void* pUserContext = NULL );


//--------------------------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTInit( bool bParseCommandLine = true, 
                        bool bShowMsgBoxOnError = true, 
                        __in_opt WCHAR* strExtraCommandLineParams = NULL, 
                        bool bThreadSafeDXUT = false );

// Choose either DXUTCreateWindow or DXUTSetWindow.  If using DXUTSetWindow, consider using DXUTStaticWndProc
HRESULT WINAPI DXUTCreateWindow( const WCHAR* strWindowTitle = L"Direct3D Window", 
                                HINSTANCE hInstance = NULL, HICON hIcon = NULL, HMENU hMenu = NULL,
                                int x = CW_USEDEFAULT, int y = CW_USEDEFAULT );
HRESULT WINAPI DXUTSetWindow( HWND hWndFocus, HWND hWndDeviceFullScreen, HWND hWndDeviceWindowed, bool bHandleMessages = true );
LRESULT CALLBACK DXUTStaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

// Choose either DXUTCreateDevice or DXUTSetD3D*Device or DXUTCreateD3DDeviceFromSettings
HRESULT WINAPI DXUTCreateDevice( bool bWindowed = true, int nSuggestedWidth = 0, int nSuggestedHeight = 0 );
HRESULT WINAPI DXUTCreateDeviceFromSettings( DXUTDeviceSettings* pDeviceSettings, bool bPreserveInput = false, bool bClipWindowToSingleAdapter = true );
HRESULT WINAPI DXUTSetD3D9Device( IDirect3DDevice9* pd3dDevice );
HRESULT WINAPI DXUTSetD3D10Device( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain );

// Choose either DXUTMainLoop or implement your own main loop 
HRESULT WINAPI DXUTMainLoop( HACCEL hAccel = NULL );

// If not using DXUTMainLoop consider using DXUTRender3DEnvironment
void WINAPI DXUTRender3DEnvironment(); 


//--------------------------------------------------------------------------------------
// Common Tasks 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleFullScreen();
HRESULT WINAPI DXUTToggleREF();
HRESULT WINAPI DXUTToggleWARP();
void    WINAPI DXUTPause( bool bPauseTime, bool bPauseRendering );
void    WINAPI DXUTSetConstantFrameTime( bool bConstantFrameTime, float fTimePerFrame = 0.0333f );
void    WINAPI DXUTSetCursorSettings( bool bShowCursorWhenFullScreen = false, bool bClipCursorWhenFullScreen = false );
void    WINAPI DXUTSetD3DVersionSupport( bool bAppCanUseD3D9 = true, bool bAppCanUseD3D10 = true );
void    WINAPI DXUTSetHotkeyHandling( bool bAltEnterToToggleFullscreen = true, bool bEscapeToQuit = true, bool bPauseToToggleTimePause = true );
void    WINAPI DXUTSetMultimonSettings( bool bAutoChangeAdapter = true );
void    WINAPI DXUTSetShortcutKeySettings( bool bAllowWhenFullscreen = false, bool bAllowWhenWindowed = true ); // Controls the Windows key, and accessibility shortcut keys
void    WINAPI DXUTSetWindowSettings( bool bCallDefWindowProc = true );
HRESULT WINAPI DXUTSetTimer( LPDXUTCALLBACKTIMER pCallbackTimer, float fTimeoutInSecs = 1.0f, UINT* pnIDEvent = NULL, void* pCallbackUserContext = NULL );
HRESULT WINAPI DXUTKillTimer( UINT nIDEvent );
void    WINAPI DXUTResetFrameworkState();
void    WINAPI DXUTShutdown( int nExitCode = 0 );
void    WINAPI DXUTSetIsInGammaCorrectMode( bool bGammaCorrect );


//--------------------------------------------------------------------------------------
// State Retrieval  
//--------------------------------------------------------------------------------------

// Direct3D 9
IDirect3D9*              WINAPI DXUTGetD3D9Object(); // Does not addref unlike typical Get* APIs
IDirect3DDevice9*        WINAPI DXUTGetD3D9Device(); // Does not addref unlike typical Get* APIs
D3DPRESENT_PARAMETERS    WINAPI DXUTGetD3D9PresentParameters();
const D3DSURFACE_DESC*   WINAPI DXUTGetD3D9BackBufferSurfaceDesc();
const D3DCAPS9*          WINAPI DXUTGetD3D9DeviceCaps();
HRESULT                  WINAPI DXUTGetD3D9DeviceCaps( DXUTDeviceSettings* pDeviceSettings, D3DCAPS9* pCaps );
bool                     WINAPI DXUTDoesAppSupportD3D9();
bool                     WINAPI DXUTIsAppRenderingWithD3D9();

// Direct3D 10
bool                     WINAPI DXUTIsD3D10Available(); // If D3D10 APIs are availible
IDXGIFactory*            WINAPI DXUTGetDXGIFactory(); // Does not addref unlike typical Get* APIs
ID3D10Device*            WINAPI DXUTGetD3D10Device(); // Does not addref unlike typical Get* APIs
ID3D10Device1*           WINAPI DXUTGetD3D10Device1(); // Does not addref unlike typical Get* APIs
IDXGISwapChain*          WINAPI DXUTGetDXGISwapChain(); // Does not addref unlike typical Get* APIs
ID3D10RenderTargetView*  WINAPI DXUTGetD3D10RenderTargetView(); // Does not addref unlike typical Get* APIs
ID3D10DepthStencilView*  WINAPI DXUTGetD3D10DepthStencilView(); // Does not addref unlike typical Get* APIs
const DXGI_SURFACE_DESC* WINAPI DXUTGetDXGIBackBufferSurfaceDesc();
bool                     WINAPI DXUTDoesAppSupportD3D10();
bool                     WINAPI DXUTIsAppRenderingWithD3D10();

// General
DXUTDeviceSettings WINAPI DXUTGetDeviceSettings(); 
HINSTANCE WINAPI DXUTGetHINSTANCE();
HWND      WINAPI DXUTGetHWND();
HWND      WINAPI DXUTGetHWNDFocus();
HWND      WINAPI DXUTGetHWNDDeviceFullScreen();
HWND      WINAPI DXUTGetHWNDDeviceWindowed();
RECT      WINAPI DXUTGetWindowClientRect();
LONG      WINAPI DXUTGetWindowWidth();
LONG      WINAPI DXUTGetWindowHeight();
RECT      WINAPI DXUTGetWindowClientRectAtModeChange(); // Useful for returning to windowed mode with the same resolution as before toggle to full screen mode
RECT      WINAPI DXUTGetFullsceenClientRectAtModeChange(); // Useful for returning to full screen mode with the same resolution as before toggle to windowed mode
double    WINAPI DXUTGetTime();
float     WINAPI DXUTGetElapsedTime();
bool      WINAPI DXUTIsWindowed();
bool	  WINAPI DXUTIsInGammaCorrectMode();
float     WINAPI DXUTGetFPS();
LPCWSTR   WINAPI DXUTGetWindowTitle();
LPCWSTR   WINAPI DXUTGetFrameStats( bool bIncludeFPS = false );
LPCWSTR   WINAPI DXUTGetDeviceStats();
LPCWSTR   WINAPI DXUTGetD3D10CounterStats();
bool      WINAPI DXUTIsVsyncEnabled();
bool      WINAPI DXUTIsRenderingPaused();
bool      WINAPI DXUTIsTimePaused();
bool      WINAPI DXUTIsActive();
int       WINAPI DXUTGetExitCode();
bool      WINAPI DXUTGetShowMsgBoxOnError();
bool      WINAPI DXUTGetAutomation();  // Returns true if -automation parameter is used to launch the app
bool      WINAPI DXUTIsKeyDown( BYTE vKey ); // Pass a virtual-key code, ex. VK_F1, 'A', VK_RETURN, VK_LSHIFT, etc
bool      WINAPI DXUTWasKeyPressed( BYTE vKey );  // Like DXUTIsKeyDown() but return true only if the key was just pressed
bool      WINAPI DXUTIsMouseButtonDown( BYTE vButton ); // Pass a virtual-key code: VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2
HRESULT   WINAPI DXUTCreateState(); // Optional method to create DXUT's memory.  If its not called by the application it will be automatically called when needed
void      WINAPI DXUTDestroyState(); // Optional method to destroy DXUT's memory.  If its not called by the application it will be automatically called after the application exits WinMain 

//--------------------------------------------------------------------------------------
// DXUT core layer includes
//--------------------------------------------------------------------------------------
#include "DXUTmisc.h"
#include "DXUTenum.h"


#endif




