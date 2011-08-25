//--------------------------------------------------------------------------------------
// File: DXUT.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#define DXUT_MIN_WINDOW_SIZE_X 200
#define DXUT_MIN_WINDOW_SIZE_Y 200
#define DXUT_COUNTER_STAT_LENGTH 2048
#undef min // use __min instead inside this source file
#undef max // use __max instead inside this source file

#ifndef ARRAYSIZE
extern "C++" // templates cannot be declared to have 'C' linkage
template <typename T, size_t N>
char (*RtlpNumberOf( UNALIGNED T (&)[N] ))[N];

#define RTL_NUMBER_OF_V2(A) (sizeof(*RtlpNumberOf(A)))
#define ARRAYSIZE(A)    RTL_NUMBER_OF_V2(A)
#endif

//--------------------------------------------------------------------------------------
// Thread safety
//--------------------------------------------------------------------------------------
CRITICAL_SECTION    g_cs;
bool                g_bThreadSafe = true;


//--------------------------------------------------------------------------------------
// Automatically enters & leaves the CS upon object creation/deletion
//--------------------------------------------------------------------------------------
class DXUTLock
{
public:
    inline  DXUTLock()
    {
        if( g_bThreadSafe ) EnterCriticalSection( &g_cs );
    }
    inline  ~DXUTLock()
    {
        if( g_bThreadSafe ) LeaveCriticalSection( &g_cs );
    }
};



//--------------------------------------------------------------------------------------
// Helper macros to build member functions that access member variables with thread safety
//--------------------------------------------------------------------------------------
#define SET_ACCESSOR( x, y )       inline void Set##y( x t )   { DXUTLock l; m_state.m_##y = t; };
#define GET_ACCESSOR( x, y )       inline x Get##y()           { DXUTLock l; return m_state.m_##y; };
#define GET_SET_ACCESSOR( x, y )   SET_ACCESSOR( x, y ) GET_ACCESSOR( x, y )

#define SETP_ACCESSOR( x, y )      inline void Set##y( x* t )  { DXUTLock l; m_state.m_##y = *t; };
#define GETP_ACCESSOR( x, y )      inline x* Get##y()          { DXUTLock l; return &m_state.m_##y; };
#define GETP_SETP_ACCESSOR( x, y ) SETP_ACCESSOR( x, y ) GETP_ACCESSOR( x, y )


//--------------------------------------------------------------------------------------
// Stores timer callback info
//--------------------------------------------------------------------------------------
struct DXUT_TIMER
{
    LPDXUTCALLBACKTIMER pCallbackTimer;
    void* pCallbackUserContext;
    float fTimeoutInSecs;
    float fCountdown;
    bool bEnabled;
    UINT nID;
};

//--------------------------------------------------------------------------------------
// D3D10 Counters
//--------------------------------------------------------------------------------------
struct D3D10_COUNTERS
{
    float fGPUIdle;
    float fVertexProcessing;
    float fGeometryProcessing;
    float fPixelProcessing;
    float fOtherGPUProcessing;
    float fHostAdapterBandwidthUtilization;
    float fLocalVidmemBandwidthUtilization;
    float fVertexThroughputUtilization;
    float fTriangleSetupThroughputUtilization;
    float fFillrateThroughputUtilization;
    float fVSMemoryLimited;
    float fVSComputationLimited;
    float fGSMemoryLimited;
    float fGSComputationLimited;
    float fPSMemoryLimited;
    float fPSComputationLimited;
    float fPostTransformCacheHitRate;
    float fTextureCacheHitRate;
};

//--------------------------------------------------------------------------------------
// Stores DXUT state and data access is done with thread safety (if g_bThreadSafe==true)
//--------------------------------------------------------------------------------------
class DXUTState
{
protected:
    struct STATE
    {
        // D3D9 specific
        IDirect3D9* m_D3D9;                    // the main D3D9 object
        IDirect3DDevice9* m_D3D9Device;              // the D3D9 rendering device
        DXUTDeviceSettings* m_CurrentDeviceSettings;   // current device settings
        D3DSURFACE_DESC m_BackBufferSurfaceDesc9;  // D3D9 back buffer surface description
        D3DCAPS9 m_Caps;                    // D3D caps for current device

        // D3D10 specific
        ID3D10RasterizerState*  m_D3D10RasterizerState;    // the D3D11 Rasterizer state
        bool m_D3D10Available;          // if true, then D3D10 is available 
        IDXGIFactory* m_DXGIFactory;             // DXGI Factory object
        IDXGIAdapter* m_D3D10Adapter;            // The DXGI adapter object for the D3D10 device
        IDXGIOutput** m_D3D10OutputArray;        // The array of output obj for the D3D10 adapter obj
        UINT m_D3D10OutputArraySize;    // Number of elements in m_D3D10OutputArray
        ID3D10Device* m_D3D10Device;             // the D3D10.0 rendering device
        ID3D10Device1* m_D3D10Device1;            // the D3D10.1 rendering device
        IDXGISwapChain* m_D3D10SwapChain;          // the D3D10 swapchain
        ID3D10Texture2D* m_D3D10DepthStencil;       // the D3D10 depth stencil texture (optional)
        ID3D10DepthStencilView* m_D3D10DepthStencilView;   // the D3D10 depth stencil view (optional)
        ID3D10RenderTargetView* m_D3D10RenderTargetView;   // the D3D10 render target view
        ID3D10RasterizerState*  m_D3D10DefaultRasterizerState; // Default rasterizer state for d3d10 apps
        DXGI_SURFACE_DESC m_BackBufferSurfaceDesc10; // D3D10 back buffer surface description
        bool m_RenderingOccluded;       // Rendering is occluded by another window
        bool m_DoNotStoreBufferSize;    // Do not store the buffer size on WM_SIZE messages
        ID3D10Counter* m_Counter_GPUIdle;
        ID3D10Counter* m_Counter_VertexProcessing;
        ID3D10Counter* m_Counter_GeometryProcessing;
        ID3D10Counter* m_Counter_PixelProcessing;
        ID3D10Counter* m_Counter_OtherGPUProcessing;
        ID3D10Counter* m_Counter_HostAdapterBandwidthUtilization;
        ID3D10Counter* m_Counter_LocalVidmemBandwidthUtilization;
        ID3D10Counter* m_Counter_VertexThroughputUtilization;
        ID3D10Counter* m_Counter_TriangleSetupThroughputUtilization;
        ID3D10Counter* m_Counter_FillrateThrougputUtilization;
        ID3D10Counter* m_Counter_VSMemoryLimited;
        ID3D10Counter* m_Counter_VSComputationLimited;
        ID3D10Counter* m_Counter_GSMemoryLimited;
        ID3D10Counter* m_Counter_GSComputationLimited;
        ID3D10Counter* m_Counter_PSMemoryLimited;
        ID3D10Counter* m_Counter_PSComputationLimited;
        ID3D10Counter* m_Counter_PostTransformCacheHitRate;
        ID3D10Counter* m_Counter_TextureCacheHitRate;
        D3D10_COUNTERS m_CounterData;

        // General
        HWND m_HWNDFocus;                  // the main app focus window
        HWND m_HWNDDeviceFullScreen;       // the main app device window in fullscreen mode
        HWND m_HWNDDeviceWindowed;         // the main app device window in windowed mode
        HMONITOR m_AdapterMonitor;          // the monitor of the adapter 
        HMENU m_Menu;                       // handle to menu

        UINT m_FullScreenBackBufferWidthAtModeChange;  // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_FullScreenBackBufferHeightAtModeChange; // back buffer size of fullscreen mode right before switching to windowed mode.  Used to restore to same resolution when toggling back to fullscreen
        UINT m_WindowBackBufferWidthAtModeChange;  // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode
        UINT m_WindowBackBufferHeightAtModeChange; // back buffer size of windowed mode right before switching to fullscreen mode.  Used to restore to same resolution when toggling back to windowed mode
        DWORD m_WindowedStyleAtModeChange;  // window style
        WINDOWPLACEMENT m_WindowedPlacement;// record of windowed HWND position/show state/etc
        bool m_TopmostWhileWindowed;       // if true, the windowed HWND is topmost 
        bool m_Minimized;                  // if true, the HWND is minimized
        bool m_Maximized;                  // if true, the HWND is maximized
        bool m_MinimizedWhileFullscreen;   // if true, the HWND is minimized due to a focus switch away when fullscreen mode
        bool m_IgnoreSizeChange;           // if true, DXUT won't reset the device upon HWND size change

        double m_Time;                      // current time in seconds
        double m_AbsoluteTime;              // absolute time in seconds
        float m_ElapsedTime;                // time elapsed since last frame

        HINSTANCE m_HInstance;              // handle to the app instance
        double m_LastStatsUpdateTime;       // last time the stats were updated
        DWORD m_LastStatsUpdateFrames;      // frames count since last time the stats were updated
        float m_FPS;                        // frames per second
        int m_CurrentFrameNumber;         // the current frame number
        HHOOK m_KeyboardHook;               // handle to keyboard hook
        bool m_AllowShortcutKeysWhenFullscreen; // if true, when fullscreen enable shortcut keys (Windows keys, StickyKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut) 
        bool m_AllowShortcutKeysWhenWindowed;   // if true, when windowed enable shortcut keys (Windows keys, StickyKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut) 
        bool m_AllowShortcutKeys;          // if true, then shortcut keys are currently disabled (Windows key, etc)
        bool m_CallDefWindowProc;          // if true, DXUTStaticWndProc will call DefWindowProc for unhandled messages. Applications rendering to a dialog may need to set this to false.
        STICKYKEYS m_StartupStickyKeys;     // StickyKey settings upon startup so they can be restored later
        TOGGLEKEYS m_StartupToggleKeys;     // ToggleKey settings upon startup so they can be restored later
        FILTERKEYS m_StartupFilterKeys;     // FilterKey settings upon startup so they can be restored later

        bool m_AppSupportsD3D9Override;    // true if app sets via DXUTSetD3DVersionSupport()
        bool m_AppSupportsD3D10Override;   // true if app sets via DXUTSetD3DVersionSupport()
        bool m_UseD3DVersionOverride;      // true if the app ever calls DXUTSetD3DVersionSupport()

        bool m_HandleEscape;               // if true, then DXUT will handle escape to quit
        bool m_HandleAltEnter;             // if true, then DXUT will handle alt-enter to toggle fullscreen
        bool m_HandlePause;                // if true, then DXUT will handle pause to toggle time pausing
        bool m_ShowMsgBoxOnError;          // if true, then msgboxes are displayed upon errors
        bool m_NoStats;                    // if true, then DXUTGetFrameStats() and DXUTGetDeviceStats() will return blank strings
        bool m_ClipCursorWhenFullScreen;   // if true, then DXUT will keep the cursor from going outside the window when full screen
        bool m_ShowCursorWhenFullScreen;   // if true, then DXUT will show a cursor when full screen
        bool m_ConstantFrameTime;          // if true, then elapsed frame time will always be 0.05f seconds which is good for debugging or automated capture
        float m_TimePerFrame;               // the constant time per frame in seconds, only valid if m_ConstantFrameTime==true
        bool m_WireframeMode;              // if true, then D3DRS_FILLMODE==D3DFILL_WIREFRAME else D3DRS_FILLMODE==D3DFILL_SOLID 
        bool m_AutoChangeAdapter;          // if true, then the adapter will automatically change if the window is different monitor
        bool m_WindowCreatedWithDefaultPositions; // if true, then CW_USEDEFAULT was used and the window should be moved to the right adapter
        int m_ExitCode;                   // the exit code to be returned to the command line

        bool m_DXUTInited;                 // if true, then DXUTInit() has succeeded
        bool m_WindowCreated;              // if true, then DXUTCreateWindow() or DXUTSetWindow() has succeeded
        bool m_DeviceCreated;              // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() has succeeded

        bool m_DXUTInitCalled;             // if true, then DXUTInit() was called
        bool m_WindowCreateCalled;         // if true, then DXUTCreateWindow() or DXUTSetWindow() was called
        bool m_DeviceCreateCalled;         // if true, then DXUTCreateDevice() or DXUTSetD3D*Device() was called

        bool m_DeviceObjectsCreated;       // if true, then DeviceCreated callback has been called (if non-NULL)
        bool m_DeviceObjectsReset;         // if true, then DeviceReset callback has been called (if non-NULL)
        bool m_InsideDeviceCallback;       // if true, then the framework is inside an app device callback
        bool m_InsideMainloop;             // if true, then the framework is inside the main loop
        bool m_Active;                     // if true, then the app is the active top level window
        bool m_TimePaused;                 // if true, then time is paused
        bool m_RenderingPaused;            // if true, then rendering is paused
        int m_PauseRenderingCount;        // pause rendering ref count
        int m_PauseTimeCount;             // pause time ref count
        bool m_DeviceLost;                 // if true, then the device is lost and needs to be reset
        bool m_NotifyOnMouseMove;          // if true, include WM_MOUSEMOVE in mousecallback
        bool m_Automation;                 // if true, automation is enabled
        bool m_InSizeMove;                 // if true, app is inside a WM_ENTERSIZEMOVE
        UINT m_TimerLastID;               // last ID of the DXUT timer

        int m_OverrideForceAPI;           // if != -1, then override to use this Direct3D API version
        int m_OverrideAdapterOrdinal;     // if != -1, then override to use this adapter ordinal
        bool m_OverrideWindowed;           // if true, then force to start windowed
        int m_OverrideOutput;             // if != -1, then override to use the particular output on the adapter
        bool m_OverrideFullScreen;         // if true, then force to start full screen
        int m_OverrideStartX;             // if != -1, then override to this X position of the window
        int m_OverrideStartY;             // if != -1, then override to this Y position of the window
        int m_OverrideWidth;              // if != 0, then override to this width
        int m_OverrideHeight;             // if != 0, then override to this height
        bool m_OverrideForceHAL;           // if true, then force to HAL device (failing if one doesn't exist)
        bool m_OverrideForceREF;           // if true, then force to REF device (failing if one doesn't exist)
        bool m_OverrideForceWARP;          // if true, then force to WARP device (failing if one doesn't exist)
        bool m_OverrideForcePureHWVP;      // if true, then force to use pure HWVP (failing if device doesn't support it)
        bool m_OverrideForceHWVP;          // if true, then force to use HWVP (failing if device doesn't support it)
        bool m_OverrideForceSWVP;          // if true, then force to use SWVP 
        bool m_OverrideConstantFrameTime;  // if true, then force to constant frame time
        float m_OverrideConstantTimePerFrame; // the constant time per frame in seconds if m_OverrideConstantFrameTime==true
        int m_OverrideQuitAfterFrame;     // if != 0, then it will force the app to quit after that frame
        int m_OverrideForceVsync;         // if == 0, then it will force the app to use D3DPRESENT_INTERVAL_IMMEDIATE, if == 1 force use of D3DPRESENT_INTERVAL_DEFAULT
        bool m_OverrideRelaunchMCE;          // if true, then force relaunch of MCE at exit
        bool m_AppCalledWasKeyPressed;      // true if the app ever calls DXUTWasKeyPressed().  Allows for optimzation
        bool m_ReleasingSwapChain;		  // if true, the app is releasing its swapchain
        bool m_IsInGammaCorrectMode;		// Tell DXUTRes and DXUTMisc that we are in gamma correct mode

        LPDXUTCALLBACKMODIFYDEVICESETTINGS m_ModifyDeviceSettingsFunc; // modify Direct3D device settings callback
        LPDXUTCALLBACKDEVICEREMOVED m_DeviceRemovedFunc;        // Direct3D device removed callback
        LPDXUTCALLBACKFRAMEMOVE m_FrameMoveFunc;            // frame move callback
        LPDXUTCALLBACKKEYBOARD m_KeyboardFunc;             // keyboard callback
        LPDXUTCALLBACKMOUSE m_MouseFunc;                // mouse callback
        LPDXUTCALLBACKMSGPROC m_WindowMsgFunc;            // window messages callback

        LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE m_IsD3D9DeviceAcceptableFunc;   // D3D9 is device acceptable callback
        LPDXUTCALLBACKD3D9DEVICECREATED m_D3D9DeviceCreatedFunc;        // D3D9 device created callback
        LPDXUTCALLBACKD3D9DEVICERESET m_D3D9DeviceResetFunc;          // D3D9 device reset callback
        LPDXUTCALLBACKD3D9DEVICELOST m_D3D9DeviceLostFunc;           // D3D9 device lost callback
        LPDXUTCALLBACKD3D9DEVICEDESTROYED m_D3D9DeviceDestroyedFunc;      // D3D9 device destroyed callback
        LPDXUTCALLBACKD3D9FRAMERENDER m_D3D9FrameRenderFunc;          // D3D9 frame render callback

        LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE m_IsD3D10DeviceAcceptableFunc;  // D3D10 is device acceptable callback
        LPDXUTCALLBACKD3D10DEVICECREATED m_D3D10DeviceCreatedFunc;       // D3D10 device created callback
        LPDXUTCALLBACKD3D10SWAPCHAINRESIZED m_D3D10SwapChainResizedFunc;    // D3D10 SwapChain reset callback
        LPDXUTCALLBACKD3D10SWAPCHAINRELEASING m_D3D10SwapChainReleasingFunc;  // D3D10 SwapChain lost callback
        LPDXUTCALLBACKD3D10DEVICEDESTROYED m_D3D10DeviceDestroyedFunc;     // D3D10 device destroyed callback
        LPDXUTCALLBACKD3D10FRAMERENDER m_D3D10FrameRenderFunc;         // D3D10 frame render callback

        void* m_ModifyDeviceSettingsFuncUserContext;     // user context for modify Direct3D device settings callback
        void* m_DeviceRemovedFuncUserContext;            // user context for Direct3D device removed callback
        void* m_FrameMoveFuncUserContext;                // user context for frame move callback
        void* m_KeyboardFuncUserContext;                 // user context for keyboard callback
        void* m_MouseFuncUserContext;                    // user context for mouse callback
        void* m_WindowMsgFuncUserContext;                // user context for window messages callback

        void* m_IsD3D9DeviceAcceptableFuncUserContext;   // user context for is D3D9 device acceptable callback
        void* m_D3D9DeviceCreatedFuncUserContext;        // user context for D3D9 device created callback
        void* m_D3D9DeviceResetFuncUserContext;          // user context for D3D9 device reset callback
        void* m_D3D9DeviceLostFuncUserContext;           // user context for D3D9 device lost callback
        void* m_D3D9DeviceDestroyedFuncUserContext;      // user context for D3D9 device destroyed callback
        void* m_D3D9FrameRenderFuncUserContext;          // user context for D3D9 frame render callback

        void* m_IsD3D10DeviceAcceptableFuncUserContext;  // user context for is D3D10 device acceptable callback
        void* m_D3D10DeviceCreatedFuncUserContext;       // user context for D3D10 device created callback
        void* m_D3D10SwapChainResizedFuncUserContext;    // user context for D3D10 SwapChain resized callback
        void* m_D3D10SwapChainReleasingFuncUserContext;  // user context for D3D10 SwapChain releasing callback
        void* m_D3D10DeviceDestroyedFuncUserContext;     // user context for D3D10 device destroyed callback
        void* m_D3D10FrameRenderFuncUserContext;         // user context for D3D10 frame render callback

        bool    m_Keys[256];                                // array of key state
        bool    m_LastKeys[256];                            // array of last key state
        bool    m_MouseButtons[5];                          // array of mouse states

        CGrowableArray <DXUT_TIMER>* m_TimerList;        // list of DXUT_TIMER structs
        WCHAR   m_StaticFrameStats[256];                   // static part of frames stats 
        WCHAR   m_FPSStats[64];                            // fps stats
        WCHAR   m_FrameStats[256];                         // frame stats (fps, width, etc)
        WCHAR   m_DeviceStats[256];                        // device stats (description, device type, etc)
        WCHAR   m_D3D10CounterStats[DXUT_COUNTER_STAT_LENGTH]; // d3d10 pipeline statistics
        WCHAR   m_WindowTitle[256];                        // window title
    };

    STATE m_state;

public:
    DXUTState()  { Create(); }
    ~DXUTState() { Destroy(); }

    void Create()
    {
        g_bThreadSafe = true;
        InitializeCriticalSectionAndSpinCount( &g_cs, 1000 );

        ZeroMemory( &m_state, sizeof( STATE ) );
        m_state.m_OverrideStartX = -1;
        m_state.m_OverrideStartY = -1;
        m_state.m_OverrideForceAPI = -1;
        m_state.m_OverrideAdapterOrdinal = -1;
        m_state.m_OverrideOutput = -1;
        m_state.m_OverrideForceVsync = -1;
        m_state.m_AutoChangeAdapter = true;
        m_state.m_ShowMsgBoxOnError = true;
        m_state.m_AllowShortcutKeysWhenWindowed = true;
        m_state.m_Active = true;
        m_state.m_CallDefWindowProc = true;
        m_state.m_HandleEscape = true;
        m_state.m_HandleAltEnter = true;
        m_state.m_HandlePause = true;
        m_state.m_IsInGammaCorrectMode = true;

        m_state.m_CounterData.fGPUIdle = -1.0f;
        m_state.m_CounterData.fVertexProcessing = -1.0f;
        m_state.m_CounterData.fGeometryProcessing = -1.0f;
        m_state.m_CounterData.fPixelProcessing = -1.0f;
        m_state.m_CounterData.fOtherGPUProcessing = -1.0f;
        m_state.m_CounterData.fHostAdapterBandwidthUtilization = -1.0f;
        m_state.m_CounterData.fLocalVidmemBandwidthUtilization = -1.0f;
        m_state.m_CounterData.fVertexThroughputUtilization = -1.0f;
        m_state.m_CounterData.fTriangleSetupThroughputUtilization = -1.0f;
        m_state.m_CounterData.fFillrateThroughputUtilization = -1.0f;
        m_state.m_CounterData.fVSMemoryLimited = -1.0f;
        m_state.m_CounterData.fVSComputationLimited = -1.0f;
        m_state.m_CounterData.fGSMemoryLimited = -1.0f;
        m_state.m_CounterData.fGSComputationLimited = -1.0f;
        m_state.m_CounterData.fPSMemoryLimited = -1.0f;
        m_state.m_CounterData.fPSComputationLimited = -1.0f;
        m_state.m_CounterData.fPostTransformCacheHitRate = -1.0f;
        m_state.m_CounterData.fTextureCacheHitRate = -1.0f;
    }

    void Destroy()
    {
        SAFE_DELETE( m_state.m_TimerList );
        DXUTShutdown();
        DeleteCriticalSection( &g_cs );
    }

    // Macros to define access functions for thread safe access into m_state 

    // D3D9 specific
    GET_SET_ACCESSOR( IDirect3D9*, D3D9 );
    GET_SET_ACCESSOR( IDirect3DDevice9*, D3D9Device );
    GET_SET_ACCESSOR( DXUTDeviceSettings*, CurrentDeviceSettings );
    GETP_SETP_ACCESSOR( D3DSURFACE_DESC, BackBufferSurfaceDesc9 );
    GETP_SETP_ACCESSOR( D3DCAPS9, Caps );

    // D3D10 specific
    GET_SET_ACCESSOR( ID3D10RasterizerState*, D3D10RasterizerState );

    GET_SET_ACCESSOR( bool, D3D10Available );
    GET_SET_ACCESSOR( IDXGIFactory*, DXGIFactory );
    GET_SET_ACCESSOR( IDXGIAdapter*, D3D10Adapter );
    GET_SET_ACCESSOR( IDXGIOutput**, D3D10OutputArray );
    GET_SET_ACCESSOR( UINT, D3D10OutputArraySize );
    GET_SET_ACCESSOR( ID3D10Device*, D3D10Device );
    GET_SET_ACCESSOR( ID3D10Device1*, D3D10Device1 );
    GET_SET_ACCESSOR( IDXGISwapChain*, D3D10SwapChain );
    GET_SET_ACCESSOR( ID3D10Texture2D*, D3D10DepthStencil );
    GET_SET_ACCESSOR( ID3D10DepthStencilView*, D3D10DepthStencilView );   
    GET_SET_ACCESSOR( ID3D10RenderTargetView*, D3D10RenderTargetView );
    GET_SET_ACCESSOR( ID3D10RasterizerState*, D3D10DefaultRasterizerState );
    GETP_SETP_ACCESSOR( DXGI_SURFACE_DESC, BackBufferSurfaceDesc10 );
    GET_SET_ACCESSOR( bool, RenderingOccluded );
    GET_SET_ACCESSOR( bool, DoNotStoreBufferSize );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_GPUIdle );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_VertexProcessing );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_GeometryProcessing );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_PixelProcessing );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_OtherGPUProcessing );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_HostAdapterBandwidthUtilization );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_LocalVidmemBandwidthUtilization );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_VertexThroughputUtilization );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_TriangleSetupThroughputUtilization );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_FillrateThrougputUtilization );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_VSMemoryLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_VSComputationLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_GSMemoryLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_GSComputationLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_PSMemoryLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_PSComputationLimited );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_PostTransformCacheHitRate );
    GET_SET_ACCESSOR( ID3D10Counter*, Counter_TextureCacheHitRate );
    GETP_SETP_ACCESSOR( D3D10_COUNTERS, CounterData );

    GET_SET_ACCESSOR( HWND, HWNDFocus );
    GET_SET_ACCESSOR( HWND, HWNDDeviceFullScreen );
    GET_SET_ACCESSOR( HWND, HWNDDeviceWindowed );
    GET_SET_ACCESSOR( HMONITOR, AdapterMonitor );
    GET_SET_ACCESSOR( HMENU, Menu );   

    GET_SET_ACCESSOR( UINT, FullScreenBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, FullScreenBackBufferHeightAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferWidthAtModeChange );
    GET_SET_ACCESSOR( UINT, WindowBackBufferHeightAtModeChange );
    GETP_SETP_ACCESSOR( WINDOWPLACEMENT, WindowedPlacement );
    GET_SET_ACCESSOR( DWORD, WindowedStyleAtModeChange );
    GET_SET_ACCESSOR( bool, TopmostWhileWindowed );
    GET_SET_ACCESSOR( bool, Minimized );
    GET_SET_ACCESSOR( bool, Maximized );
    GET_SET_ACCESSOR( bool, MinimizedWhileFullscreen );
    GET_SET_ACCESSOR( bool, IgnoreSizeChange );   

    GET_SET_ACCESSOR( double, Time );
    GET_SET_ACCESSOR( double, AbsoluteTime );
    GET_SET_ACCESSOR( float, ElapsedTime );

    GET_SET_ACCESSOR( HINSTANCE, HInstance );
    GET_SET_ACCESSOR( double, LastStatsUpdateTime );   
    GET_SET_ACCESSOR( DWORD, LastStatsUpdateFrames );   
    GET_SET_ACCESSOR( float, FPS );    
    GET_SET_ACCESSOR( int, CurrentFrameNumber );
    GET_SET_ACCESSOR( HHOOK, KeyboardHook );
    GET_SET_ACCESSOR( bool, AllowShortcutKeysWhenFullscreen );
    GET_SET_ACCESSOR( bool, AllowShortcutKeysWhenWindowed );
    GET_SET_ACCESSOR( bool, AllowShortcutKeys );
    GET_SET_ACCESSOR( bool, CallDefWindowProc );
    GET_SET_ACCESSOR( STICKYKEYS, StartupStickyKeys );
    GET_SET_ACCESSOR( TOGGLEKEYS, StartupToggleKeys );
    GET_SET_ACCESSOR( FILTERKEYS, StartupFilterKeys );

    GET_SET_ACCESSOR( bool, AppSupportsD3D9Override );
    GET_SET_ACCESSOR( bool, AppSupportsD3D10Override );
    GET_SET_ACCESSOR( bool, UseD3DVersionOverride );

    GET_SET_ACCESSOR( bool, HandleEscape );
    GET_SET_ACCESSOR( bool, HandleAltEnter );
    GET_SET_ACCESSOR( bool, HandlePause );
    GET_SET_ACCESSOR( bool, ShowMsgBoxOnError );
    GET_SET_ACCESSOR( bool, NoStats );
    GET_SET_ACCESSOR( bool, ClipCursorWhenFullScreen );   
    GET_SET_ACCESSOR( bool, ShowCursorWhenFullScreen );
    GET_SET_ACCESSOR( bool, ConstantFrameTime );
    GET_SET_ACCESSOR( float, TimePerFrame );
    GET_SET_ACCESSOR( bool, WireframeMode );   
    GET_SET_ACCESSOR( bool, AutoChangeAdapter );
    GET_SET_ACCESSOR( bool, WindowCreatedWithDefaultPositions );
    GET_SET_ACCESSOR( int, ExitCode );

    GET_SET_ACCESSOR( bool, DXUTInited );
    GET_SET_ACCESSOR( bool, WindowCreated );
    GET_SET_ACCESSOR( bool, DeviceCreated );
    GET_SET_ACCESSOR( bool, DXUTInitCalled );
    GET_SET_ACCESSOR( bool, WindowCreateCalled );
    GET_SET_ACCESSOR( bool, DeviceCreateCalled );
    GET_SET_ACCESSOR( bool, InsideDeviceCallback );
    GET_SET_ACCESSOR( bool, InsideMainloop );
    GET_SET_ACCESSOR( bool, DeviceObjectsCreated );
    GET_SET_ACCESSOR( bool, DeviceObjectsReset );
    GET_SET_ACCESSOR( bool, Active );
    GET_SET_ACCESSOR( bool, RenderingPaused );
    GET_SET_ACCESSOR( bool, TimePaused );
    GET_SET_ACCESSOR( int, PauseRenderingCount );
    GET_SET_ACCESSOR( int, PauseTimeCount );
    GET_SET_ACCESSOR( bool, DeviceLost );
    GET_SET_ACCESSOR( bool, NotifyOnMouseMove );
    GET_SET_ACCESSOR( bool, Automation );
    GET_SET_ACCESSOR( bool, InSizeMove );
    GET_SET_ACCESSOR( UINT, TimerLastID );
    GET_SET_ACCESSOR( bool, AppCalledWasKeyPressed );

    GET_SET_ACCESSOR( int, OverrideForceAPI );
    GET_SET_ACCESSOR( int, OverrideAdapterOrdinal );
    GET_SET_ACCESSOR( bool, OverrideWindowed );
    GET_SET_ACCESSOR( int, OverrideOutput );
    GET_SET_ACCESSOR( bool, OverrideFullScreen );
    GET_SET_ACCESSOR( int, OverrideStartX );
    GET_SET_ACCESSOR( int, OverrideStartY );
    GET_SET_ACCESSOR( int, OverrideWidth );
    GET_SET_ACCESSOR( int, OverrideHeight );
    GET_SET_ACCESSOR( bool, OverrideForceHAL );
    GET_SET_ACCESSOR( bool, OverrideForceREF );
    GET_SET_ACCESSOR( bool, OverrideForceWARP );
    GET_SET_ACCESSOR( bool, OverrideForcePureHWVP );
    GET_SET_ACCESSOR( bool, OverrideForceHWVP );
    GET_SET_ACCESSOR( bool, OverrideForceSWVP );
    GET_SET_ACCESSOR( bool, OverrideConstantFrameTime );
    GET_SET_ACCESSOR( float, OverrideConstantTimePerFrame );
    GET_SET_ACCESSOR( int, OverrideQuitAfterFrame );
    GET_SET_ACCESSOR( int, OverrideForceVsync );
    GET_SET_ACCESSOR( bool, OverrideRelaunchMCE );
    GET_SET_ACCESSOR( bool, ReleasingSwapChain );
    GET_SET_ACCESSOR( bool, IsInGammaCorrectMode );

    GET_SET_ACCESSOR( LPDXUTCALLBACKMODIFYDEVICESETTINGS, ModifyDeviceSettingsFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKDEVICEREMOVED, DeviceRemovedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKFRAMEMOVE, FrameMoveFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKKEYBOARD, KeyboardFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMOUSE, MouseFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKMSGPROC, WindowMsgFunc );

    GET_SET_ACCESSOR( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE, IsD3D9DeviceAcceptableFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICECREATED, D3D9DeviceCreatedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICERESET, D3D9DeviceResetFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICELOST, D3D9DeviceLostFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9DEVICEDESTROYED, D3D9DeviceDestroyedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D9FRAMERENDER, D3D9FrameRenderFunc );

    GET_SET_ACCESSOR( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE, IsD3D10DeviceAcceptableFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10DEVICECREATED, D3D10DeviceCreatedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10SWAPCHAINRESIZED, D3D10SwapChainResizedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10SWAPCHAINRELEASING, D3D10SwapChainReleasingFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10DEVICEDESTROYED, D3D10DeviceDestroyedFunc );
    GET_SET_ACCESSOR( LPDXUTCALLBACKD3D10FRAMERENDER, D3D10FrameRenderFunc );

    GET_SET_ACCESSOR( void*, ModifyDeviceSettingsFuncUserContext );
    GET_SET_ACCESSOR( void*, DeviceRemovedFuncUserContext );
    GET_SET_ACCESSOR( void*, FrameMoveFuncUserContext );
    GET_SET_ACCESSOR( void*, KeyboardFuncUserContext );
    GET_SET_ACCESSOR( void*, MouseFuncUserContext );
    GET_SET_ACCESSOR( void*, WindowMsgFuncUserContext );

    GET_SET_ACCESSOR( void*, IsD3D9DeviceAcceptableFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceCreatedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceResetFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceLostFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9DeviceDestroyedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D9FrameRenderFuncUserContext );

    GET_SET_ACCESSOR( void*, IsD3D10DeviceAcceptableFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10DeviceCreatedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10DeviceDestroyedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10SwapChainResizedFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10SwapChainReleasingFuncUserContext );
    GET_SET_ACCESSOR( void*, D3D10FrameRenderFuncUserContext );

    GET_SET_ACCESSOR( CGrowableArray<DXUT_TIMER>*, TimerList );
    GET_ACCESSOR( bool*, Keys );
    GET_ACCESSOR( bool*, LastKeys );
    GET_ACCESSOR( bool*, MouseButtons );
    GET_ACCESSOR( WCHAR*, StaticFrameStats );
    GET_ACCESSOR( WCHAR*, FPSStats );
    GET_ACCESSOR( WCHAR*, FrameStats );
    GET_ACCESSOR( WCHAR*, DeviceStats );    
    GET_ACCESSOR( WCHAR*, D3D10CounterStats );
    GET_ACCESSOR( WCHAR*, WindowTitle );
};


//--------------------------------------------------------------------------------------
// Global state 
//--------------------------------------------------------------------------------------
DXUTState*          g_pDXUTState = NULL;

HRESULT WINAPI DXUTCreateState()
{
    if( g_pDXUTState == NULL )
    {
        g_pDXUTState = new DXUTState;
        if( NULL == g_pDXUTState )
            return E_OUTOFMEMORY;
    }
    return S_OK;
}

void WINAPI DXUTDestroyState()
{
    SAFE_DELETE( g_pDXUTState );
}

class DXUTMemoryHelper
{
public:
DXUTMemoryHelper()
{
    DXUTCreateState();
}
~DXUTMemoryHelper()
{
    DXUTDestroyState();
}
};


DXUTState& GetDXUTState()
{
    // This class will auto create the memory when its first accessed and delete it after the program exits WinMain.
    // However the application can also call DXUTCreateState() & DXUTDestroyState() independantly if its wants 
    static DXUTMemoryHelper memory;
    assert( g_pDXUTState != NULL );
    return *g_pDXUTState;
}


//--------------------------------------------------------------------------------------
// Internal functions forward declarations
//--------------------------------------------------------------------------------------
void DXUTParseCommandLine(  __inout WCHAR* strCommandLine, 
                            bool bIgnoreFirstCommand = true );
bool DXUTIsNextArg(__inout WCHAR*& strCmdLine, __in const WCHAR* strArg );
bool DXUTGetCmdParam( __inout WCHAR*& strCmdLine, __out WCHAR* strFlag );
void DXUTAllowShortcutKeys( bool bAllowKeys );
void DXUTUpdateStaticFrameStats();
void DXUTUpdateFrameStats();
void DXUTUpdateD3D10PipelineStats();
LRESULT CALLBACK DXUTStaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
void DXUTHandleTimers();
void DXUTDisplayErrorMessage( HRESULT hr );
int DXUTMapButtonToArrayIndex( BYTE vButton );

HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings, IDirect3DDevice9* pd3d9DeviceFromApp,
                          ID3D10Device* pd3d10DeviceFromApp, bool bForceRecreate, bool bClipWindowToSingleAdapter );
bool DXUTCanDeviceBeReset( DXUTDeviceSettings* pOldDeviceSettings, DXUTDeviceSettings* pNewDeviceSettings,
                           IDirect3DDevice9* pd3d9DeviceFromApp, ID3D10Device* pd3d10DeviceFromApp );
HRESULT DXUTDelayLoadDXGI();
HRESULT DXUTDelayLoadD3D9();
void DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings );
void DXUTCheckForDXGIFullScreenSwitch();
void DXUTResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullscreen );
void DXUTCheckForDXGIBufferChange();
void DXUTCheckForWindowSizeChange();
void DXUTCheckForWindowChangingMonitors();
void DXUTCleanup3DEnvironment( bool bReleaseSettings );
HMONITOR DXUTGetMonitorFromAdapter( DXUTDeviceSettings* pDeviceSettings );
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal );
HRESULT DXUTGetOutputOrdinalFromMonitor( HMONITOR hMonitor, UINT* pOutputOrdinal );
HRESULT DXUTHandleDeviceRemoved();
void DXUTUpdateBackBufferDesc();
void DXUTSetupCursor();

HRESULT DXUTCreate3DEnvironment9( IDirect3DDevice9* pd3dDeviceFromApp );
HRESULT DXUTReset3DEnvironment9();
void DXUTRender3DEnvironment9();
void DXUTCleanup3DEnvironment9( bool bReleaseSettings = true );
HRESULT DXUTSetD3D9DeviceCursor( IDirect3DDevice9* pd3dDevice, HCURSOR hCursor, bool bAddWatermark );
void DXUTUpdateD3D9DeviceStats( D3DDEVTYPE DeviceType, DWORD BehaviorFlags,
                                D3DADAPTER_IDENTIFIER9* pAdapterIdentifier );
HRESULT DXUTFindD3D9AdapterFormat( UINT AdapterOrdinal, D3DDEVTYPE DeviceType, D3DFORMAT BackBufferFormat,
                                   BOOL Windowed, D3DFORMAT* pAdapterFormat );

HRESULT DXUTSetupD3D10Views( ID3D10Device* pd3dDevice, DXUTDeviceSettings* pDeviceSettings );
HRESULT DXUTCreate3DEnvironment10( ID3D10Device* pd3dDeviceFromApp );
HRESULT DXUTReset3DEnvironment10();
void DXUTRender3DEnvironment10();
void DXUTCleanup3DEnvironment10( bool bReleaseSettings = true );
void DXUTCreateD3D10Counters( ID3D10Device* pd3dDevice );
void DXUTDestroyD3D10Counters();
void DXUTStartPerformanceCounters();
void DXUTStopPerformanceCounters();
void DXUTUpdateD3D10CounterStats();
void DXUTUpdateD3D10DeviceStats( D3D10_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc );


//--------------------------------------------------------------------------------------
// Internal helper functions 
//--------------------------------------------------------------------------------------
bool DXUTIsD3D9( DXUTDeviceSettings* pDeviceSettings )                          { return (pDeviceSettings && pDeviceSettings->ver == DXUT_D3D9_DEVICE ); };
bool DXUTIsCurrentDeviceD3D9()                                                  { DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();  return DXUTIsD3D9(pDeviceSettings); };
UINT DXUTGetBackBufferWidthFromDS( DXUTDeviceSettings* pNewDeviceSettings )     { return DXUTIsD3D9(pNewDeviceSettings) ? pNewDeviceSettings->d3d9.pp.BackBufferWidth : pNewDeviceSettings->d3d10.sd.BufferDesc.Width; }
UINT DXUTGetBackBufferHeightFromDS( DXUTDeviceSettings* pNewDeviceSettings )    { return DXUTIsD3D9(pNewDeviceSettings) ? pNewDeviceSettings->d3d9.pp.BackBufferHeight : pNewDeviceSettings->d3d10.sd.BufferDesc.Height; }
bool DXUTGetIsWindowedFromDS( DXUTDeviceSettings* pNewDeviceSettings )          { if (!pNewDeviceSettings) return true; return ((DXUTIsD3D9(pNewDeviceSettings) ? pNewDeviceSettings->d3d9.pp.Windowed : pNewDeviceSettings->d3d10.sd.Windowed) == 1); }


//--------------------------------------------------------------------------------------
// External state access functions
//--------------------------------------------------------------------------------------
IDirect3DDevice9* WINAPI DXUTGetD3D9Device()               { return GetDXUTState().GetD3D9Device(); }
const D3DSURFACE_DESC* WINAPI DXUTGetD3D9BackBufferSurfaceDesc() { return GetDXUTState().GetBackBufferSurfaceDesc9(); }
const D3DCAPS9* WINAPI DXUTGetD3D9DeviceCaps()             { return GetDXUTState().GetCaps(); }
ID3D10Device* WINAPI DXUTGetD3D10Device()                  { return GetDXUTState().GetD3D10Device(); }
ID3D10Device1* WINAPI DXUTGetD3D10Device1()                { return GetDXUTState().GetD3D10Device1(); }
IDXGISwapChain* WINAPI DXUTGetDXGISwapChain()              { return GetDXUTState().GetD3D10SwapChain(); }
ID3D10RenderTargetView* WINAPI DXUTGetD3D10RenderTargetView() { return GetDXUTState().GetD3D10RenderTargetView(); }
ID3D10DepthStencilView* WINAPI DXUTGetD3D10DepthStencilView() { return GetDXUTState().GetD3D10DepthStencilView(); }
const DXGI_SURFACE_DESC* WINAPI DXUTGetDXGIBackBufferSurfaceDesc() { return GetDXUTState().GetBackBufferSurfaceDesc10(); }
HINSTANCE WINAPI DXUTGetHINSTANCE()                        { return GetDXUTState().GetHInstance(); }
HWND WINAPI DXUTGetHWND()                                  { return DXUTIsWindowed() ? GetDXUTState().GetHWNDDeviceWindowed() : GetDXUTState().GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDFocus()                             { return GetDXUTState().GetHWNDFocus(); }
HWND WINAPI DXUTGetHWNDDeviceFullScreen()                  { return GetDXUTState().GetHWNDDeviceFullScreen(); }
HWND WINAPI DXUTGetHWNDDeviceWindowed()                    { return GetDXUTState().GetHWNDDeviceWindowed(); }
RECT WINAPI DXUTGetWindowClientRect()                      { RECT rc; GetClientRect( DXUTGetHWND(), &rc ); return rc; }
LONG WINAPI DXUTGetWindowWidth()                           { RECT rc = DXUTGetWindowClientRect(); return ((LONG)rc.right - rc.left); }
LONG WINAPI DXUTGetWindowHeight()                          { RECT rc = DXUTGetWindowClientRect(); return ((LONG)rc.bottom - rc.top); }
RECT WINAPI DXUTGetWindowClientRectAtModeChange()          { RECT rc = { 0, 0, GetDXUTState().GetWindowBackBufferWidthAtModeChange(), GetDXUTState().GetWindowBackBufferHeightAtModeChange() }; return rc; }
RECT WINAPI DXUTGetFullsceenClientRectAtModeChange()       { RECT rc = { 0, 0, GetDXUTState().GetFullScreenBackBufferWidthAtModeChange(), GetDXUTState().GetFullScreenBackBufferHeightAtModeChange() }; return rc; }
double WINAPI DXUTGetTime()                                { return GetDXUTState().GetTime(); }
float WINAPI DXUTGetElapsedTime()                          { return GetDXUTState().GetElapsedTime(); }
float WINAPI DXUTGetFPS()                                  { return GetDXUTState().GetFPS(); }
LPCWSTR WINAPI DXUTGetWindowTitle()                        { return GetDXUTState().GetWindowTitle(); }
LPCWSTR WINAPI DXUTGetDeviceStats()                        { return GetDXUTState().GetDeviceStats(); }
LPCWSTR WINAPI DXUTGetD3D10CounterStats()                  { return GetDXUTState().GetD3D10CounterStats(); }
bool WINAPI DXUTIsRenderingPaused()                        { return GetDXUTState().GetPauseRenderingCount() > 0; }
bool WINAPI DXUTIsTimePaused()                             { return GetDXUTState().GetPauseTimeCount() > 0; }
bool WINAPI DXUTIsActive()                                 { return GetDXUTState().GetActive(); }
int WINAPI DXUTGetExitCode()                               { return GetDXUTState().GetExitCode(); }
bool WINAPI DXUTGetShowMsgBoxOnError()                     { return GetDXUTState().GetShowMsgBoxOnError(); }
bool WINAPI DXUTGetAutomation()                            { return GetDXUTState().GetAutomation(); }
bool WINAPI DXUTIsWindowed()                               { return DXUTGetIsWindowedFromDS( GetDXUTState().GetCurrentDeviceSettings() ); }
bool WINAPI DXUTIsInGammaCorrectMode()                     { return GetDXUTState().GetIsInGammaCorrectMode(); }
IDirect3D9* WINAPI DXUTGetD3D9Object()                     { DXUTDelayLoadD3D9(); return GetDXUTState().GetD3D9(); }
IDXGIFactory* WINAPI DXUTGetDXGIFactory()                  { DXUTDelayLoadDXGI(); return GetDXUTState().GetDXGIFactory(); }
bool WINAPI DXUTIsD3D10Available()                         { DXUTDelayLoadDXGI(); return GetDXUTState().GetD3D10Available(); }
bool WINAPI DXUTIsAppRenderingWithD3D9()                   { return (GetDXUTState().GetD3D9Device() != NULL); }
bool WINAPI DXUTIsAppRenderingWithD3D10()                  { return (GetDXUTState().GetD3D10Device() != NULL); }


//--------------------------------------------------------------------------------------
// External callback setup functions
//--------------------------------------------------------------------------------------

// General callbacks
void WINAPI DXUTSetCallbackDeviceChanging( LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallback, void* pUserContext )                  { GetDXUTState().SetModifyDeviceSettingsFunc( pCallback ); GetDXUTState().SetModifyDeviceSettingsFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackDeviceRemoved( LPDXUTCALLBACKDEVICEREMOVED pCallback, void* pUserContext )                          { GetDXUTState().SetDeviceRemovedFunc( pCallback ); GetDXUTState().SetDeviceRemovedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackFrameMove( LPDXUTCALLBACKFRAMEMOVE pCallback, void* pUserContext )                                  { GetDXUTState().SetFrameMoveFunc( pCallback );  GetDXUTState().SetFrameMoveFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackKeyboard( LPDXUTCALLBACKKEYBOARD pCallback, void* pUserContext )                                    { GetDXUTState().SetKeyboardFunc( pCallback );  GetDXUTState().SetKeyboardFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackMouse( LPDXUTCALLBACKMOUSE pCallback, bool bIncludeMouseMove, void* pUserContext )                  { GetDXUTState().SetMouseFunc( pCallback ); GetDXUTState().SetNotifyOnMouseMove( bIncludeMouseMove );  GetDXUTState().SetMouseFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackMsgProc( LPDXUTCALLBACKMSGPROC pCallback, void* pUserContext )                                      { GetDXUTState().SetWindowMsgFunc( pCallback );  GetDXUTState().SetWindowMsgFuncUserContext( pUserContext ); }

// Direct3D 9 callbacks
void WINAPI DXUTSetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE pCallback, void* pUserContext )          { GetDXUTState().SetIsD3D9DeviceAcceptableFunc( pCallback ); GetDXUTState().SetIsD3D9DeviceAcceptableFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceCreated( LPDXUTCALLBACKD3D9DEVICECREATED pCallback, void* pUserContext )                  { GetDXUTState().SetD3D9DeviceCreatedFunc( pCallback ); GetDXUTState().SetD3D9DeviceCreatedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceReset( LPDXUTCALLBACKD3D9DEVICERESET pCallback, void* pUserContext )                      { GetDXUTState().SetD3D9DeviceResetFunc( pCallback );  GetDXUTState().SetD3D9DeviceResetFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceLost( LPDXUTCALLBACKD3D9DEVICELOST pCallback, void* pUserContext )                        { GetDXUTState().SetD3D9DeviceLostFunc( pCallback );  GetDXUTState().SetD3D9DeviceLostFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9DeviceDestroyed( LPDXUTCALLBACKD3D9DEVICEDESTROYED pCallback, void* pUserContext )              { GetDXUTState().SetD3D9DeviceDestroyedFunc( pCallback );  GetDXUTState().SetD3D9DeviceDestroyedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D9FrameRender( LPDXUTCALLBACKD3D9FRAMERENDER pCallback, void* pUserContext )                      { GetDXUTState().SetD3D9FrameRenderFunc( pCallback );  GetDXUTState().SetD3D9FrameRenderFuncUserContext( pUserContext ); }
void DXUTGetCallbackD3D9DeviceAcceptable( LPDXUTCALLBACKISD3D9DEVICEACCEPTABLE* ppCallback, void** ppUserContext )             { *ppCallback = GetDXUTState().GetIsD3D9DeviceAcceptableFunc(); *ppUserContext = GetDXUTState().GetIsD3D9DeviceAcceptableFuncUserContext(); }

// Direct3D 10 callbacks
void WINAPI DXUTSetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE pCallback, void* pUserContext )        { GetDXUTState().SetIsD3D10DeviceAcceptableFunc( pCallback ); GetDXUTState().SetIsD3D10DeviceAcceptableFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10DeviceCreated( LPDXUTCALLBACKD3D10DEVICECREATED pCallback, void* pUserContext )                { GetDXUTState().SetD3D10DeviceCreatedFunc( pCallback ); GetDXUTState().SetD3D10DeviceCreatedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10SwapChainResized( LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallback, void* pUserContext )          { GetDXUTState().SetD3D10SwapChainResizedFunc( pCallback );  GetDXUTState().SetD3D10SwapChainResizedFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10FrameRender( LPDXUTCALLBACKD3D10FRAMERENDER pCallback, void* pUserContext )                    { GetDXUTState().SetD3D10FrameRenderFunc( pCallback );  GetDXUTState().SetD3D10FrameRenderFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10SwapChainReleasing( LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallback, void* pUserContext )      { GetDXUTState().SetD3D10SwapChainReleasingFunc( pCallback );  GetDXUTState().SetD3D10SwapChainReleasingFuncUserContext( pUserContext ); }
void WINAPI DXUTSetCallbackD3D10DeviceDestroyed( LPDXUTCALLBACKD3D10DEVICEDESTROYED pCallback, void* pUserContext )            { GetDXUTState().SetD3D10DeviceDestroyedFunc( pCallback );  GetDXUTState().SetD3D10DeviceDestroyedFuncUserContext( pUserContext ); }
void DXUTGetCallbackD3D10DeviceAcceptable( LPDXUTCALLBACKISD3D10DEVICEACCEPTABLE* ppCallback, void** ppUserContext )           { *ppCallback = GetDXUTState().GetIsD3D10DeviceAcceptableFunc(); *ppUserContext = GetDXUTState().GetIsD3D10DeviceAcceptableFuncUserContext(); }


//--------------------------------------------------------------------------------------
// Optionally parses the command line and sets if default hotkeys are handled
//
//       Possible command line parameters are:
//          -forceapi:#             forces app to use specified Direct3D API version (fails if the application doesn't support this API or if no device is found)
//          -adapter:#              forces app to use this adapter # (fails if the adapter doesn't exist)
//          -output:#               [D3D10 only] forces app to use a particular output on the adapter (fails if the output doesn't exist) 
//          -windowed               forces app to start windowed
//          -fullscreen             forces app to start full screen
//          -forcehal               forces app to use HAL (fails if HAL doesn't exist)
//          -forceref               forces app to use REF (fails if REF doesn't exist)
//          -forcewarp              forces app to use WARP (fails if WARP doesn't exist)
//          -forcepurehwvp          [D3D9 only] forces app to use pure HWVP (fails if device doesn't support it)
//          -forcehwvp              [D3D9 only] forces app to use HWVP (fails if device doesn't support it)
//          -forceswvp              [D3D9 only] forces app to use SWVP 
//          -forcevsync:#           if # is 0, then vsync is disabled 
//          -width:#                forces app to use # for width. for full screen, it will pick the closest possible supported mode
//          -height:#               forces app to use # for height. for full screen, it will pick the closest possible supported mode
//          -startx:#               forces app to use # for the x coord of the window position for windowed mode
//          -starty:#               forces app to use # for the y coord of the window position for windowed mode
//          -constantframetime:#    forces app to use constant frame time, where # is the time/frame in seconds
//          -quitafterframe:x       forces app to quit after # frames
//          -noerrormsgboxes        prevents the display of message boxes generated by the framework so the application can be run without user interaction
//          -nostats                prevents the display of the stats
//          -relaunchmce            re-launches the MCE UI after the app exits
//          -automation             a hint to other components that automation is active 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTInit( bool bParseCommandLine, 
                         bool bShowMsgBoxOnError, 
                         __in_opt WCHAR* strExtraCommandLineParams,
                         bool bThreadSafeDXUT )
{
    g_bThreadSafe = bThreadSafeDXUT;

    GetDXUTState().SetDXUTInitCalled( true );

    // Not always needed, but lets the app create GDI dialogs
    InitCommonControls();

    // Save the current sticky/toggle/filter key settings so DXUT can restore them later
    STICKYKEYS sk = {sizeof( STICKYKEYS ), 0};
    SystemParametersInfo( SPI_GETSTICKYKEYS, sizeof( STICKYKEYS ), &sk, 0 );
    GetDXUTState().SetStartupStickyKeys( sk );

    TOGGLEKEYS tk = {sizeof( TOGGLEKEYS ), 0};
    SystemParametersInfo( SPI_GETTOGGLEKEYS, sizeof( TOGGLEKEYS ), &tk, 0 );
    GetDXUTState().SetStartupToggleKeys( tk );

    FILTERKEYS fk = {sizeof( FILTERKEYS ), 0};
    SystemParametersInfo( SPI_GETFILTERKEYS, sizeof( FILTERKEYS ), &fk, 0 );
    GetDXUTState().SetStartupFilterKeys( fk );

    GetDXUTState().SetShowMsgBoxOnError( bShowMsgBoxOnError );

    if( bParseCommandLine )
        DXUTParseCommandLine( GetCommandLine() );
    if( strExtraCommandLineParams )
        DXUTParseCommandLine( strExtraCommandLineParams, false );

    // Declare this process to be high DPI aware, and prevent automatic scaling
    // Warning: This is better done as a <dpiaware> manifest element to avoid
    //          problems based on code that has run before this point.
    HINSTANCE hUser32 = LoadLibrary( L"user32.dll" );
    if( hUser32 )
    {
        typedef BOOL ( WINAPI* LPSetProcessDPIAware )( void );
        LPSetProcessDPIAware pSetProcessDPIAware = ( LPSetProcessDPIAware )GetProcAddress( hUser32,
                                                                                           "SetProcessDPIAware" );
        if( pSetProcessDPIAware )
        {
            pSetProcessDPIAware();
        }
        FreeLibrary( hUser32 );
    }

    // Reset the timer
    DXUTGetGlobalTimer()->Reset();

    GetDXUTState().SetDXUTInited( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Parses the command line for parameters.  See DXUTInit() for list 
//--------------------------------------------------------------------------------------
void DXUTParseCommandLine( __inout WCHAR* strCommandLine, 
                           bool bIgnoreFirstCommand )
{
    WCHAR* strCmdLine;
    WCHAR strFlag[MAX_PATH];

    int nNumArgs;
    LPWSTR* pstrArgList = CommandLineToArgvW( strCommandLine, &nNumArgs );
    int iArgStart = 0;
    if( bIgnoreFirstCommand )
        iArgStart = 1;
    for( int iArg = iArgStart; iArg < nNumArgs; iArg++ )
    {
        strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            strCmdLine++;

            if( DXUTIsNextArg( strCmdLine, L"forceapi" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAPIVersion = _wtoi( strFlag );
                    GetDXUTState().SetOverrideForceAPI( nAPIVersion );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"adapter" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nAdapter = _wtoi( strFlag );
                    GetDXUTState().SetOverrideAdapterOrdinal( nAdapter );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"windowed" ) )
            {
                GetDXUTState().SetOverrideWindowed( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"output" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int Output = _wtoi( strFlag );
                    GetDXUTState().SetOverrideOutput( Output );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"fullscreen" ) )
            {
                GetDXUTState().SetOverrideFullScreen( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcehal" ) )
            {
                GetDXUTState().SetOverrideForceHAL( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forceref" ) )
            {
                GetDXUTState().SetOverrideForceREF( true );
                continue;
            }

			if( DXUTIsNextArg( strCmdLine, L"forcewarp" ) )
            {
				GetDXUTState().SetOverrideForceWARP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcepurehwvp" ) )
            {
                GetDXUTState().SetOverrideForcePureHWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcehwvp" ) )
            {
                GetDXUTState().SetOverrideForceHWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forceswvp" ) )
            {
                GetDXUTState().SetOverrideForceSWVP( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"forcevsync" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nOn = _wtoi( strFlag );
                    GetDXUTState().SetOverrideForceVsync( nOn );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"width" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nWidth = _wtoi( strFlag );
                    GetDXUTState().SetOverrideWidth( nWidth );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"height" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nHeight = _wtoi( strFlag );
                    GetDXUTState().SetOverrideHeight( nHeight );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"startx" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nX = _wtoi( strFlag );
                    GetDXUTState().SetOverrideStartX( nX );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"starty" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nY = _wtoi( strFlag );
                    GetDXUTState().SetOverrideStartY( nY );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"constantframetime" ) )
            {
                float fTimePerFrame;
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                    fTimePerFrame = ( float )wcstod( strFlag, NULL );
                else
                    fTimePerFrame = 0.0333f;
                GetDXUTState().SetOverrideConstantFrameTime( true );
                GetDXUTState().SetOverrideConstantTimePerFrame( fTimePerFrame );
                DXUTSetConstantFrameTime( true, fTimePerFrame );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"quitafterframe" ) )
            {
                if( DXUTGetCmdParam( strCmdLine, strFlag ) )
                {
                    int nFrame = _wtoi( strFlag );
                    GetDXUTState().SetOverrideQuitAfterFrame( nFrame );
                    continue;
                }
            }

            if( DXUTIsNextArg( strCmdLine, L"noerrormsgboxes" ) )
            {
                GetDXUTState().SetShowMsgBoxOnError( false );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"nostats" ) )
            {
                GetDXUTState().SetNoStats( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"relaunchmce" ) && GetSystemMetrics( SM_MEDIACENTER ) != 0 )
            {
                GetDXUTState().SetOverrideRelaunchMCE( true );
                continue;
            }

            if( DXUTIsNextArg( strCmdLine, L"automation" ) )
            {
                GetDXUTState().SetAutomation( true );
                continue;
            }
        }

        // Unrecognized flag
        wcscpy_s( strFlag, MAX_PATH, strCmdLine );
        WCHAR* strSpace = strFlag;
        while( *strSpace && ( *strSpace > L' ' ) )
            strSpace++;
        *strSpace = 0;

        DXUTOutputDebugString( L"Unrecognized flag: %s", strFlag );
        strCmdLine += wcslen( strFlag );
    }

    LocalFree( pstrArgList );
}


//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine
//--------------------------------------------------------------------------------------
bool DXUTIsNextArg( WCHAR*& strCmdLine, const WCHAR* strArg )
{
    int nArgLen = ( int )wcslen( strArg );
    int nCmdLen = ( int )wcslen( strCmdLine );

    if( nCmdLen >= nArgLen &&
        _wcsnicmp( strCmdLine, strArg, nArgLen ) == 0 &&
        ( strCmdLine[nArgLen] == 0 || strCmdLine[nArgLen] == L':' ) )
    {
        strCmdLine += nArgLen;
        return true;
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Helper function for DXUTParseCommandLine.  Updates strCmdLine and strFlag 
//      Example: if strCmdLine=="-width:1024 -forceref"
// then after: strCmdLine==" -forceref" and strFlag=="1024"
//--------------------------------------------------------------------------------------
bool DXUTGetCmdParam( WCHAR*& strCmdLine, WCHAR* strFlag )
{
    if( *strCmdLine == L':' )
    {
        strCmdLine++; // Skip ':'

        // Place NULL terminator in strFlag after current token
        wcscpy_s( strFlag, MAX_PATH, strCmdLine );
        WCHAR* strSpace = strFlag;
        while( *strSpace && ( *strSpace > L' ' ) )
            strSpace++;
        *strSpace = 0;

        // Update strCmdLine
        strCmdLine += wcslen( strFlag );
        return true;
    }
    else
    {
        strFlag[0] = 0;
        return false;
    }
}


//--------------------------------------------------------------------------------------
// Creates a window with the specified window title, icon, menu, and 
// starting position.  If DXUTInit() has not already been called, it will
// call it with the default parameters.  Instead of calling this, you can 
// call DXUTSetWindow() to use an existing window.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateWindow( const WCHAR* strWindowTitle, HINSTANCE hInstance,
                                 HICON hIcon, HMENU hMenu, int x, int y )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTCreateWindow", E_FAIL );

    GetDXUTState().SetWindowCreateCalled( true );

    if( !GetDXUTState().GetDXUTInited() )
    {
        // If DXUTInit() was already called and failed, then fail.
        // DXUTInit() must first succeed for this function to succeed
        if( GetDXUTState().GetDXUTInitCalled() )
            return E_FAIL;

        // If DXUTInit() hasn't been called, then automatically call it
        // with default params
        hr = DXUTInit();
        if( FAILED( hr ) )
            return hr;
    }

    if( DXUTGetHWNDFocus() == NULL )
    {
        if( hInstance == NULL )
            hInstance = ( HINSTANCE )GetModuleHandle( NULL );
        GetDXUTState().SetHInstance( hInstance );

        WCHAR szExePath[MAX_PATH];
        GetModuleFileName( NULL, szExePath, MAX_PATH );
        if( hIcon == NULL ) // If the icon is NULL, then use the first one found in the exe
            hIcon = ExtractIcon( hInstance, szExePath, 0 );

        // Register the windows class
        WNDCLASS wndClass;
        wndClass.style = CS_DBLCLKS;
        wndClass.lpfnWndProc = DXUTStaticWndProc;
        wndClass.cbClsExtra = 0;
        wndClass.cbWndExtra = 0;
        wndClass.hInstance = hInstance;
        wndClass.hIcon = hIcon;
        wndClass.hCursor = LoadCursor( NULL, IDC_ARROW );
        wndClass.hbrBackground = ( HBRUSH )GetStockObject( BLACK_BRUSH );
        wndClass.lpszMenuName = NULL;
        wndClass.lpszClassName = L"Direct3DWindowClass";

        if( !RegisterClass( &wndClass ) )
        {
            DWORD dwError = GetLastError();
            if( dwError != ERROR_CLASS_ALREADY_EXISTS )
                return DXUT_ERR_MSGBOX( L"RegisterClass", HRESULT_FROM_WIN32(dwError) );
        }

        // Override the window's initial & size position if there were cmd line args
        if( GetDXUTState().GetOverrideStartX() != -1 )
            x = GetDXUTState().GetOverrideStartX();
        if( GetDXUTState().GetOverrideStartY() != -1 )
            y = GetDXUTState().GetOverrideStartY();

        GetDXUTState().SetWindowCreatedWithDefaultPositions( false );
        if( x == CW_USEDEFAULT && y == CW_USEDEFAULT )
            GetDXUTState().SetWindowCreatedWithDefaultPositions( true );

        // Find the window's initial size, but it might be changed later
        int nDefaultWidth = 640;
        int nDefaultHeight = 480;
        if( GetDXUTState().GetOverrideWidth() != 0 )
            nDefaultWidth = GetDXUTState().GetOverrideWidth();
        if( GetDXUTState().GetOverrideHeight() != 0 )
            nDefaultHeight = GetDXUTState().GetOverrideHeight();

        RECT rc;
        SetRect( &rc, 0, 0, nDefaultWidth, nDefaultHeight );
        AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, ( hMenu != NULL ) ? true : false );

        WCHAR* strCachedWindowTitle = GetDXUTState().GetWindowTitle();
        wcscpy_s( strCachedWindowTitle, 256, strWindowTitle );

        // Create the render window
        HWND hWnd = CreateWindow( L"Direct3DWindowClass", strWindowTitle, WS_OVERLAPPEDWINDOW,
                                  x, y, ( rc.right - rc.left ), ( rc.bottom - rc.top ), 0,
                                  hMenu, hInstance, 0 );
        if( hWnd == NULL )
        {
            DWORD dwError = GetLastError();
            return DXUT_ERR_MSGBOX( L"CreateWindow", HRESULT_FROM_WIN32(dwError) );
        }

        GetDXUTState().SetWindowCreated( true );
        GetDXUTState().SetHWNDFocus( hWnd );
        GetDXUTState().SetHWNDDeviceFullScreen( hWnd );
        GetDXUTState().SetHWNDDeviceWindowed( hWnd );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Sets a previously created window for the framework to use.  If DXUTInit() 
// has not already been called, it will call it with the default parameters.  
// Instead of calling this, you can call DXUTCreateWindow() to create a new window.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetWindow( HWND hWndFocus, HWND hWndDeviceFullScreen, HWND hWndDeviceWindowed,
                              bool bHandleMessages )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTCreateWindow", E_FAIL );

    GetDXUTState().SetWindowCreateCalled( true );

    // To avoid confusion, we do not allow any HWND to be NULL here.  The
    // caller must pass in valid HWND for all three parameters.  The same
    // HWND may be used for more than one parameter.
    if( hWndFocus == NULL || hWndDeviceFullScreen == NULL || hWndDeviceWindowed == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetWindow", E_INVALIDARG );

    // If subclassing the window, set the pointer to the local window procedure
    if( bHandleMessages )
    {
        // Switch window procedures
        LONG_PTR nResult = SetWindowLongPtr( hWndFocus, GWLP_WNDPROC, (LONG_PTR)DXUTStaticWndProc );

        DWORD dwError = GetLastError();
        if( nResult == 0 )
            return DXUT_ERR_MSGBOX( L"SetWindowLongPtr", HRESULT_FROM_WIN32(dwError) );
    }

    if( !GetDXUTState().GetDXUTInited() )
    {
        // If DXUTInit() was already called and failed, then fail.
        // DXUTInit() must first succeed for this function to succeed
        if( GetDXUTState().GetDXUTInitCalled() )
            return E_FAIL;

        // If DXUTInit() hasn't been called, then automatically call it
        // with default params
        hr = DXUTInit();
        if( FAILED( hr ) )
            return hr;
    }

    WCHAR* strCachedWindowTitle = GetDXUTState().GetWindowTitle();
    GetWindowText( hWndFocus, strCachedWindowTitle, 255 );
    strCachedWindowTitle[255] = 0;

    HINSTANCE hInstance = ( HINSTANCE )( LONG_PTR )GetWindowLongPtr( hWndFocus, GWLP_HINSTANCE );
    GetDXUTState().SetHInstance( hInstance );
    GetDXUTState().SetWindowCreatedWithDefaultPositions( false );
    GetDXUTState().SetWindowCreated( true );
    GetDXUTState().SetHWNDFocus( hWndFocus );
    GetDXUTState().SetHWNDDeviceFullScreen( hWndDeviceFullScreen );
    GetDXUTState().SetHWNDDeviceWindowed( hWndDeviceWindowed );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handles window messages 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DXUTStaticWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    // Consolidate the keyboard messages and pass them to the app's keyboard callback
    if( uMsg == WM_KEYDOWN ||
        uMsg == WM_SYSKEYDOWN ||
        uMsg == WM_KEYUP ||
        uMsg == WM_SYSKEYUP )
    {
        bool bKeyDown = ( uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN );
        DWORD dwMask = ( 1 << 29 );
        bool bAltDown = ( ( lParam & dwMask ) != 0 );

        bool* bKeys = GetDXUTState().GetKeys();
        bKeys[ ( BYTE )( wParam & 0xFF ) ] = bKeyDown;

        LPDXUTCALLBACKKEYBOARD pCallbackKeyboard = GetDXUTState().GetKeyboardFunc();
        if( pCallbackKeyboard )
            pCallbackKeyboard( ( UINT )wParam, bKeyDown, bAltDown, GetDXUTState().GetKeyboardFuncUserContext() );
    }

    // Consolidate the mouse button messages and pass them to the app's mouse callback
    if( uMsg == WM_LBUTTONDOWN ||
        uMsg == WM_LBUTTONUP ||
        uMsg == WM_LBUTTONDBLCLK ||
        uMsg == WM_MBUTTONDOWN ||
        uMsg == WM_MBUTTONUP ||
        uMsg == WM_MBUTTONDBLCLK ||
        uMsg == WM_RBUTTONDOWN ||
        uMsg == WM_RBUTTONUP ||
        uMsg == WM_RBUTTONDBLCLK ||
        uMsg == WM_XBUTTONDOWN ||
        uMsg == WM_XBUTTONUP ||
        uMsg == WM_XBUTTONDBLCLK ||
        uMsg == WM_MOUSEWHEEL ||
        ( GetDXUTState().GetNotifyOnMouseMove() && uMsg == WM_MOUSEMOVE ) )
    {
        int xPos = ( short )LOWORD( lParam );
        int yPos = ( short )HIWORD( lParam );

        if( uMsg == WM_MOUSEWHEEL )
        {
            // WM_MOUSEWHEEL passes screen mouse coords
            // so convert them to client coords
            POINT pt;
            pt.x = xPos; pt.y = yPos;
            ScreenToClient( hWnd, &pt );
            xPos = pt.x; yPos = pt.y;
        }

        int nMouseWheelDelta = 0;
        if( uMsg == WM_MOUSEWHEEL )
            nMouseWheelDelta = ( short )HIWORD( wParam );

        int nMouseButtonState = LOWORD( wParam );
        bool bLeftButton = ( ( nMouseButtonState & MK_LBUTTON ) != 0 );
        bool bRightButton = ( ( nMouseButtonState & MK_RBUTTON ) != 0 );
        bool bMiddleButton = ( ( nMouseButtonState & MK_MBUTTON ) != 0 );
        bool bSideButton1 = ( ( nMouseButtonState & MK_XBUTTON1 ) != 0 );
        bool bSideButton2 = ( ( nMouseButtonState & MK_XBUTTON2 ) != 0 );

        bool* bMouseButtons = GetDXUTState().GetMouseButtons();
        bMouseButtons[0] = bLeftButton;
        bMouseButtons[1] = bMiddleButton;
        bMouseButtons[2] = bRightButton;
        bMouseButtons[3] = bSideButton1;
        bMouseButtons[4] = bSideButton2;

        LPDXUTCALLBACKMOUSE pCallbackMouse = GetDXUTState().GetMouseFunc();
        if( pCallbackMouse )
            pCallbackMouse( bLeftButton, bRightButton, bMiddleButton, bSideButton1, bSideButton2, nMouseWheelDelta,
                            xPos, yPos, GetDXUTState().GetMouseFuncUserContext() );
    }

    // Pass all messages to the app's MsgProc callback, and don't 
    // process further messages if the apps says not to.
    LPDXUTCALLBACKMSGPROC pCallbackMsgProc = GetDXUTState().GetWindowMsgFunc();
    if( pCallbackMsgProc )
    {
        bool bNoFurtherProcessing = false;
        LRESULT nResult = pCallbackMsgProc( hWnd, uMsg, wParam, lParam, &bNoFurtherProcessing,
                                            GetDXUTState().GetWindowMsgFuncUserContext() );
        if( bNoFurtherProcessing )
            return nResult;
    }

    switch( uMsg )
    {
        case WM_PAINT:
        {
            // Handle paint messages when the app is paused
            if( DXUTIsRenderingPaused() &&
                GetDXUTState().GetDeviceObjectsCreated() && GetDXUTState().GetDeviceObjectsReset() )
            {
                HRESULT hr;
                double fTime = DXUTGetTime();
                float fElapsedTime = DXUTGetElapsedTime();

                if( DXUTIsCurrentDeviceD3D9() )
                {
                    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
                    if( pd3dDevice )
                    {
                        LPDXUTCALLBACKD3D9FRAMERENDER pCallbackFrameRender = GetDXUTState().GetD3D9FrameRenderFunc();
                        if( pCallbackFrameRender != NULL )
                            pCallbackFrameRender( pd3dDevice, fTime, fElapsedTime,
                                                  GetDXUTState().GetD3D9FrameRenderFuncUserContext() );

                        hr = pd3dDevice->Present( NULL, NULL, NULL, NULL );
                        if( D3DERR_DEVICELOST == hr )
                        {
                            GetDXUTState().SetDeviceLost( true );
                        }
                        else if( D3DERR_DRIVERINTERNALERROR == hr )
                        {
                            // When D3DERR_DRIVERINTERNALERROR is returned from Present(),
                            // the application can do one of the following:
                            // 
                            // - End, with the pop-up window saying that the application cannot continue 
                            //   because of problems in the display adapter and that the user should 
                            //   contact the adapter manufacturer.
                            //
                            // - Attempt to restart by calling IDirect3DDevice9::Reset, which is essentially the same 
                            //   path as recovering from a lost device. If IDirect3DDevice9::Reset fails with 
                            //   D3DERR_DRIVERINTERNALERROR, the application should end immediately with the message 
                            //   that the user should contact the adapter manufacturer.
                            // 
                            // The framework attempts the path of resetting the device
                            // 
                            GetDXUTState().SetDeviceLost( true );
                        }
                    }
                }
                else
                {
                    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
                    if( pd3dDevice )
                    {
                        LPDXUTCALLBACKD3D10FRAMERENDER pCallbackFrameRender = GetDXUTState().GetD3D10FrameRenderFunc();
                        if( pCallbackFrameRender != NULL &&
                            !GetDXUTState().GetRenderingOccluded() )
                        {
                            pCallbackFrameRender( pd3dDevice, fTime, fElapsedTime,
                                                  GetDXUTState().GetD3D10FrameRenderFuncUserContext() );
                        }

                        DWORD dwFlags = 0;
                        if( GetDXUTState().GetRenderingOccluded() )
                            dwFlags = DXGI_PRESENT_TEST;
                        else
                            dwFlags = GetDXUTState().GetCurrentDeviceSettings()->d3d10.PresentFlags;

                        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
                        hr = pSwapChain->Present( 0, GetDXUTState().GetCurrentDeviceSettings()->d3d10.PresentFlags );
                        if( DXGI_STATUS_OCCLUDED == hr )
                        {
                            // There is a window covering our entire rendering area.
                            // Don't render until we're visible again.
                            GetDXUTState().SetRenderingOccluded( true );
                        }
                        else if( SUCCEEDED( hr ) )
                        {
                            if( GetDXUTState().GetRenderingOccluded() )
                            {
                                // Now that we're no longer occluded
                                // allow us to render again
                                GetDXUTState().SetRenderingOccluded( false );
                            }
                        }
                    }
                }
            }
            break;
        }

        case WM_SIZE:
            if( SIZE_MINIMIZED == wParam )
            {
                DXUTPause( true, true ); // Pause while we're minimized

                GetDXUTState().SetMinimized( true );
                GetDXUTState().SetMaximized( false );
            }
            else
            {
                RECT rcCurrentClient;
                GetClientRect( DXUTGetHWND(), &rcCurrentClient );
                if( rcCurrentClient.top == 0 && rcCurrentClient.bottom == 0 )
                {
                    // Rapidly clicking the task bar to minimize and restore a window
                    // can cause a WM_SIZE message with SIZE_RESTORED when 
                    // the window has actually become minimized due to rapid change
                    // so just ignore this message
                }
                else if( SIZE_MAXIMIZED == wParam )
                {
                    if( GetDXUTState().GetMinimized() )
                        DXUTPause( false, false ); // Unpause since we're no longer minimized
                    GetDXUTState().SetMinimized( false );
                    GetDXUTState().SetMaximized( true );
                    DXUTCheckForWindowSizeChange();
                    DXUTCheckForWindowChangingMonitors();
                }
                else if( SIZE_RESTORED == wParam )
                {
                    //DXUTCheckForDXGIFullScreenSwitch();
                    if( GetDXUTState().GetMaximized() )
                    {
                        GetDXUTState().SetMaximized( false );
                        DXUTCheckForWindowSizeChange();
                        DXUTCheckForWindowChangingMonitors();
                    }
                    else if( GetDXUTState().GetMinimized() )
                    {
                        DXUTPause( false, false ); // Unpause since we're no longer minimized
                        GetDXUTState().SetMinimized( false );
                        DXUTCheckForWindowSizeChange();
                        DXUTCheckForWindowChangingMonitors();
                    }
                    else if( GetDXUTState().GetInSizeMove() )
                    {
                        // If we're neither maximized nor minimized, the window size 
                        // is changing by the user dragging the window edges.  In this 
                        // case, we don't reset the device yet -- we wait until the 
                        // user stops dragging, and a WM_EXITSIZEMOVE message comes.
                    }
                    else
                    {
                        // This WM_SIZE come from resizing the window via an API like SetWindowPos() so 
                        // resize and reset the device now.
                        DXUTCheckForWindowSizeChange();
                        DXUTCheckForWindowChangingMonitors();
                    }
                }
            }
            break;

        case WM_GETMINMAXINFO:
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.x = DXUT_MIN_WINDOW_SIZE_X;
            ( ( MINMAXINFO* )lParam )->ptMinTrackSize.y = DXUT_MIN_WINDOW_SIZE_Y;
            break;

        case WM_ENTERSIZEMOVE:
            // Halt frame movement while the app is sizing or moving
            DXUTPause( true, true );
            GetDXUTState().SetInSizeMove( true );
            break;

        case WM_EXITSIZEMOVE:
            DXUTPause( false, false );
            DXUTCheckForWindowSizeChange();
            DXUTCheckForWindowChangingMonitors();
            GetDXUTState().SetInSizeMove( false );
            break;

        case WM_MOUSEMOVE:
            if( DXUTIsActive() && !DXUTIsWindowed() )
            {
                if( DXUTIsCurrentDeviceD3D9() )
                {
                    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
                    if( pd3dDevice )
                    {
                        POINT ptCursor;
                        GetCursorPos( &ptCursor );
                        pd3dDevice->SetCursorPosition( ptCursor.x, ptCursor.y, 0 );
                    }
                }
                else
                {
                    // For D3D10, no processing is necessary.  D3D10 cursor
                    // is handled in the traditional Windows manner.
                }
            }
            break;

        case WM_SETCURSOR:
            if( DXUTIsActive() && !DXUTIsWindowed() )
            {
                if( DXUTIsCurrentDeviceD3D9() )
                {
                    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
                    if( pd3dDevice && GetDXUTState().GetShowCursorWhenFullScreen() )
                        pd3dDevice->ShowCursor( true );
                }
                else
                {
                    if( !GetDXUTState().GetShowCursorWhenFullScreen() )
                        SetCursor( NULL );
                }

                return true; // prevent Windows from setting cursor to window class cursor
            }
            break;

        case WM_ACTIVATEAPP:
            if( wParam == TRUE && !DXUTIsActive() ) // Handle only if previously not active 
            {
                GetDXUTState().SetActive( true );

                // Enable controller rumble & input when activating app
                DXUTEnableXInput( true );

                // The GetMinimizedWhileFullscreen() varible is used instead of !DXUTIsWindowed()
                // to handle the rare case toggling to windowed mode while the fullscreen application 
                // is minimized and thus making the pause count wrong
                if( GetDXUTState().GetMinimizedWhileFullscreen() )
                {
                    if( DXUTIsD3D9( GetDXUTState().GetCurrentDeviceSettings() ) )
                        DXUTPause( false, false ); // Unpause since we're no longer minimized
                    GetDXUTState().SetMinimizedWhileFullscreen( false );

                    if( DXUTIsAppRenderingWithD3D10() )
                    {
                        DXUTToggleFullScreen();
                    }
                }

                // Upon returning to this app, potentially disable shortcut keys 
                // (Windows key, accessibility shortcuts) 
                DXUTAllowShortcutKeys( ( DXUTIsWindowed() ) ? GetDXUTState().GetAllowShortcutKeysWhenWindowed() :
                                       GetDXUTState().GetAllowShortcutKeysWhenFullscreen() );

            }
            else if( wParam == FALSE && DXUTIsActive() ) // Handle only if previously active 
            {
                GetDXUTState().SetActive( false );

                // Disable any controller rumble & input when de-activating app
                DXUTEnableXInput( false );

                if( !DXUTIsWindowed() )
                {
                    // Going from full screen to a minimized state 
                    ClipCursor( NULL );      // don't limit the cursor anymore
                    if( DXUTIsD3D9( GetDXUTState().GetCurrentDeviceSettings() ) )
                        DXUTPause( true, true ); // Pause while we're minimized (take care not to pause twice by handling this message twice)
                    GetDXUTState().SetMinimizedWhileFullscreen( true );
                }

                // Restore shortcut keys (Windows key, accessibility shortcuts) to original state
                //
                // This is important to call here if the shortcuts are disabled, 
                // because if this is not done then the Windows key will continue to 
                // be disabled while this app is running which is very bad.
                // If the app crashes, the Windows key will return to normal.
                DXUTAllowShortcutKeys( true );
            }
            break;

        case WM_ENTERMENULOOP:
            // Pause the app when menus are displayed
            DXUTPause( true, true );
            break;

        case WM_EXITMENULOOP:
            DXUTPause( false, false );
            break;

        case WM_MENUCHAR:
            // A menu is active and the user presses a key that does not correspond to any mnemonic or accelerator key
            // So just ignore and don't beep
            return MAKELRESULT( 0, MNC_CLOSE );
            break;

        case WM_NCHITTEST:
            // Prevent the user from selecting the menu in full screen mode
            if( !DXUTIsWindowed() )
                return HTCLIENT;
            break;

        case WM_POWERBROADCAST:
            switch( wParam )
            {
#ifndef PBT_APMQUERYSUSPEND
#define PBT_APMQUERYSUSPEND 0x0000
#endif
                case PBT_APMQUERYSUSPEND:
                    // At this point, the app should save any data for open
                    // network connections, files, etc., and prepare to go into
                    // a suspended mode.  The app can use the MsgProc callback
                    // to handle this if desired.
                    return true;

#ifndef PBT_APMRESUMESUSPEND
#define PBT_APMRESUMESUSPEND 0x0007
#endif
                case PBT_APMRESUMESUSPEND:
                    // At this point, the app should recover any data, network
                    // connections, files, etc., and resume running from when
                    // the app was suspended. The app can use the MsgProc callback
                    // to handle this if desired.

                    // QPC may lose consistency when suspending, so reset the timer
                    // upon resume.
                    DXUTGetGlobalTimer()->Reset();
                    GetDXUTState().SetLastStatsUpdateTime( 0 );
                    return true;
            }
            break;

        case WM_SYSCOMMAND:
            // Prevent moving/sizing in full screen mode
            switch( ( wParam & 0xFFF0 ) )
            {
                case SC_MOVE:
                case SC_SIZE:
                case SC_MAXIMIZE:
                case SC_KEYMENU:
                    if( !DXUTIsWindowed() )
                        return 0;
                    break;
            }
            break;

        case WM_SYSKEYDOWN:
        {
            switch( wParam )
            {
                case VK_RETURN:
                {
                    if( GetDXUTState().GetHandleAltEnter() && DXUTIsAppRenderingWithD3D9() )
                    {
                        // Toggle full screen upon alt-enter 
                        DWORD dwMask = ( 1 << 29 );
                        if( ( lParam & dwMask ) != 0 ) // Alt is down also
                        {
                            // Toggle the full screen/window mode
                            DXUTPause( true, true );
                            DXUTToggleFullScreen();
                            DXUTPause( false, false );
                            return 0;
                        }
                    }
                }
            }
            break;
        }

        case WM_KEYDOWN:
        {
            switch( wParam )
            {
                case VK_ESCAPE:
                {
                    if( GetDXUTState().GetHandleEscape() )
                        SendMessage( hWnd, WM_CLOSE, 0, 0 );
                    break;
                }

                case VK_PAUSE:
                {
                    if( GetDXUTState().GetHandlePause() )
                    {
                        bool bTimePaused = DXUTIsTimePaused();
                        bTimePaused = !bTimePaused;
                        if( bTimePaused )
                            DXUTPause( true, false );
                        else
                            DXUTPause( false, false );
                    }
                    break;
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            HMENU hMenu;
            hMenu = GetMenu( hWnd );
            if( hMenu != NULL )
                DestroyMenu( hMenu );
            DestroyWindow( hWnd );
            UnregisterClass( L"Direct3DWindowClass", NULL );
            GetDXUTState().SetHWNDFocus( NULL );
            GetDXUTState().SetHWNDDeviceFullScreen( NULL );
            GetDXUTState().SetHWNDDeviceWindowed( NULL );
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;
    }

    // Don't allow the F10 key to act as a shortcut to the menu bar
    // by not passing these messages to the DefWindowProc only when
    // there's no menu present
    if( !GetDXUTState().GetCallDefWindowProc() || GetDXUTState().GetMenu() == NULL &&
        ( uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP ) && wParam == VK_F10 )
        return 0;
    else
        return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


//--------------------------------------------------------------------------------------
// Handles app's message loop and rendering when idle.  If DXUTCreateDevice() or DXUTSetD3D*Device() 
// has not already been called, it will call DXUTCreateWindow() with the default parameters.  
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTMainLoop( HACCEL hAccel )
{
    HRESULT hr;

    // Not allowed to call this from inside the device callbacks or reenter
    if( GetDXUTState().GetInsideDeviceCallback() || GetDXUTState().GetInsideMainloop() )
    {
        if( ( GetDXUTState().GetExitCode() == 0 ) || ( GetDXUTState().GetExitCode() == 10 ) )
            GetDXUTState().SetExitCode( 1 );
        return DXUT_ERR_MSGBOX( L"DXUTMainLoop", E_FAIL );
    }

    GetDXUTState().SetInsideMainloop( true );

    // If DXUTCreateDevice() or DXUTSetD3D*Device() has not already been called, 
    // then call DXUTCreateDevice() with the default parameters.         
    if( !GetDXUTState().GetDeviceCreated() )
    {
        if( GetDXUTState().GetDeviceCreateCalled() )
        {
            if( ( GetDXUTState().GetExitCode() == 0 ) || ( GetDXUTState().GetExitCode() == 10 ) )
                GetDXUTState().SetExitCode( 1 );
            return E_FAIL; // DXUTCreateDevice() must first succeed for this function to succeed
        }

        hr = DXUTCreateDevice();
        if( FAILED( hr ) )
        {
            if( ( GetDXUTState().GetExitCode() == 0 ) || ( GetDXUTState().GetExitCode() == 10 ) )
                GetDXUTState().SetExitCode( 1 );
            return hr;
        }
    }

    HWND hWnd = DXUTGetHWND();

    // DXUTInit() must have been called and succeeded for this function to proceed
    // DXUTCreateWindow() or DXUTSetWindow() must have been called and succeeded for this function to proceed
    // DXUTCreateDevice() or DXUTCreateDeviceFromSettings() or DXUTSetD3D*Device() must have been called and succeeded for this function to proceed
    if( !GetDXUTState().GetDXUTInited() || !GetDXUTState().GetWindowCreated() || !GetDXUTState().GetDeviceCreated() )
    {
        if( ( GetDXUTState().GetExitCode() == 0 ) || ( GetDXUTState().GetExitCode() == 10 ) )
            GetDXUTState().SetExitCode( 1 );
        return DXUT_ERR_MSGBOX( L"DXUTMainLoop", E_FAIL );
    }

    // Now we're ready to receive and process Windows messages.
    bool bGotMsg;
    MSG msg;
    msg.message = WM_NULL;
    PeekMessage( &msg, NULL, 0U, 0U, PM_NOREMOVE );

    while( WM_QUIT != msg.message )
    {
        // Use PeekMessage() so we can use idle time to render the scene. 
        bGotMsg = ( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) != 0 );

        if( bGotMsg )
        {
            // Translate and dispatch the message
            if( hAccel == NULL || hWnd == NULL ||
                0 == TranslateAccelerator( hWnd, hAccel, &msg ) )
            {
                TranslateMessage( &msg );
                DispatchMessage( &msg );
            }
        }
        else
        {
            // Render a frame during idle time (no messages are waiting)
            DXUTRender3DEnvironment();
        }
    }

    // Cleanup the accelerator table
    if( hAccel != NULL )
        DestroyAcceleratorTable( hAccel );

    GetDXUTState().SetInsideMainloop( false );

    return S_OK;
}


//======================================================================================
//======================================================================================
// Direct3D section
//======================================================================================
//======================================================================================


//--------------------------------------------------------------------------------------
// Creates a Direct3D device. If DXUTCreateWindow() or DXUTSetWindow() has not already 
// been called, it will call DXUTCreateWindow() with the default parameters.  
// Instead of calling this, you can call DXUTSetD3D*Device() or DXUTCreateDeviceFromSettings().
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateDevice( bool bWindowed, int nSuggestedWidth, int nSuggestedHeight )
{
    HRESULT hr = S_OK;

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTCreateWindow", E_FAIL );

    GetDXUTState().SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !GetDXUTState().GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( GetDXUTState().GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion = DXUTMT_IGNORE_INPUT;
    matchOptions.eAdapterOrdinal = DXUTMT_IGNORE_INPUT;
    matchOptions.eDeviceType = DXUTMT_IGNORE_INPUT;
    matchOptions.eOutput = DXUTMT_IGNORE_INPUT;
    matchOptions.eWindowed = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.eVertexProcessing = DXUTMT_IGNORE_INPUT;
    if( bWindowed || ( nSuggestedWidth != 0 && nSuggestedHeight != 0 ) )
        matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
    else
        matchOptions.eResolution = DXUTMT_IGNORE_INPUT;
    matchOptions.eBackBufferFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.eBackBufferCount = DXUTMT_IGNORE_INPUT;
    matchOptions.eMultiSample = DXUTMT_IGNORE_INPUT;
    matchOptions.eSwapEffect = DXUTMT_IGNORE_INPUT;
    matchOptions.eDepthFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.eStencilFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentFlags = DXUTMT_IGNORE_INPUT;
    matchOptions.eRefreshRate = DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentInterval = DXUTMT_IGNORE_INPUT;

    // Building D3D9 device settings for mathch options.  These
    // will be converted to D3D10 settings if app can use D3D10
    DXUTDeviceSettings deviceSettings;
    ZeroMemory( &deviceSettings, sizeof( DXUTDeviceSettings ) );
    deviceSettings.ver = DXUT_D3D9_DEVICE;
    deviceSettings.d3d9.pp.Windowed = bWindowed;
    deviceSettings.d3d9.pp.BackBufferWidth = nSuggestedWidth;
    deviceSettings.d3d9.pp.BackBufferHeight = nSuggestedHeight;

    // Override with settings from the command line
    if( GetDXUTState().GetOverrideWidth() != 0 )
    {
        deviceSettings.d3d9.pp.BackBufferWidth = GetDXUTState().GetOverrideWidth();
        matchOptions.eResolution = DXUTMT_PRESERVE_INPUT;
    }
    if( GetDXUTState().GetOverrideHeight() != 0 )
    {
        deviceSettings.d3d9.pp.BackBufferHeight = GetDXUTState().GetOverrideHeight();
        matchOptions.eResolution = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
    {
        deviceSettings.d3d9.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideFullScreen() )
    {
        deviceSettings.d3d9.pp.Windowed = FALSE;
        if( GetDXUTState().GetOverrideWidth() == 0 && GetDXUTState().GetOverrideHeight() == 0 )
            matchOptions.eResolution = DXUTMT_IGNORE_INPUT;
    }
    if( GetDXUTState().GetOverrideWindowed() )
        deviceSettings.d3d9.pp.Windowed = TRUE;

    if( GetDXUTState().GetOverrideForceHAL() )
    {
        deviceSettings.d3d9.DeviceType = D3DDEVTYPE_HAL;
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }
    if( GetDXUTState().GetOverrideForceREF() )
    {
        deviceSettings.d3d9.DeviceType = D3DDEVTYPE_REF;
        matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideForcePureHWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceHWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceSWVP() )
    {
        deviceSettings.d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        matchOptions.eVertexProcessing = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideForceVsync() == 0 )
    {
        deviceSettings.d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        matchOptions.ePresentInterval = DXUTMT_PRESERVE_INPUT;
    }
    else if( GetDXUTState().GetOverrideForceVsync() == 1 )
    {
        deviceSettings.d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        matchOptions.ePresentInterval = DXUTMT_PRESERVE_INPUT;
    }

    if( GetDXUTState().GetOverrideForceAPI() != -1 )
    {
        if( GetDXUTState().GetOverrideForceAPI() == 9 )
        {
            deviceSettings.ver = DXUT_D3D9_DEVICE;
            matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
        }
        else if( GetDXUTState().GetOverrideForceAPI() == 10 )
        {
            deviceSettings.ver = DXUT_D3D10_DEVICE;
            matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;

            // Convert the struct we're making to be D3D10 settings since 
            // that is what DXUTFindValidDeviceSettings will expect
            DXUTD3D10DeviceSettings d3d10In;
            ZeroMemory( &d3d10In, sizeof( DXUTD3D10DeviceSettings ) );
            DXUTConvertDeviceSettings9to10( &deviceSettings.d3d9, &d3d10In );
            deviceSettings.d3d10 = d3d10In;
        }
    }

    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( FAILED( hr ) ) // the call will fail if no valid devices were found
    {
        DXUTDisplayErrorMessage( hr );
        return DXUT_ERR( L"DXUTFindValidDeviceSettings", hr );
    }

    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreated the scene
    hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, true );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Tells the framework to change to a device created from the passed in device settings
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() 
// or DXUTSetD3D*Device() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTCreateDeviceFromSettings( DXUTDeviceSettings* pDeviceSettings, bool bPreserveInput,
                                             bool bClipWindowToSingleAdapter )
{
    HRESULT hr;

    GetDXUTState().SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !GetDXUTState().GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( GetDXUTState().GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }

    if( !bPreserveInput )
    {
        // If not preserving the input, then find the closest valid to it
        DXUTMatchOptions matchOptions;
        matchOptions.eAPIVersion = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterOrdinal = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eOutput = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDeviceType = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eWindowed = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

        hr = DXUTFindValidDeviceSettings( pDeviceSettings, pDeviceSettings, &matchOptions );
        if( FAILED( hr ) ) // the call will fail if no valid devices were found
        {
            DXUTDisplayErrorMessage( hr );
            return DXUT_ERR( L"DXUTFindValidD3D9DeviceSettings", hr );
        }
    }

    // Change to a Direct3D device created from the new device settings.  
    // If there is an existing device, then either reset or recreate the scene
    hr = DXUTChangeDevice( pDeviceSettings, NULL, NULL, false, bClipWindowToSingleAdapter );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// All device changes are sent to this function.  It looks at the current 
// device (if any) and the new device and determines the best course of action.  It 
// also remembers and restores the window state if toggling between windowed and fullscreen
// as well as sets the proper window and system state for switching to the new device.
//--------------------------------------------------------------------------------------
HRESULT DXUTChangeDevice( DXUTDeviceSettings* pNewDeviceSettings,
                          IDirect3DDevice9* pd3d9DeviceFromApp, ID3D10Device* pd3d10DeviceFromApp,
                          bool bForceRecreate, bool bClipWindowToSingleAdapter )
{
    HRESULT hr;
    DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

    if( !pNewDeviceSettings )
        return S_FALSE;

    if( pNewDeviceSettings->ver == DXUT_D3D9_DEVICE )
        hr = DXUTDelayLoadD3D9();
    else
        hr = DXUTDelayLoadDXGI();
    if( FAILED( hr ) )
        return DXUTERR_NODIRECT3D;

    // Make a copy of the pNewDeviceSettings on the heap
    DXUTDeviceSettings* pNewDeviceSettingsOnHeap = new DXUTDeviceSettings;
    if( pNewDeviceSettingsOnHeap == NULL )
        return E_OUTOFMEMORY;
    memcpy( pNewDeviceSettingsOnHeap, pNewDeviceSettings, sizeof( DXUTDeviceSettings ) );
    pNewDeviceSettings = pNewDeviceSettingsOnHeap;

    // If the ModifyDeviceSettings callback is non-NULL, then call it to let the app 
    // change the settings or reject the device change by returning false.
    LPDXUTCALLBACKMODIFYDEVICESETTINGS pCallbackModifyDeviceSettings = GetDXUTState().GetModifyDeviceSettingsFunc();
    if( pCallbackModifyDeviceSettings && pd3d9DeviceFromApp == NULL && pd3d10DeviceFromApp == NULL )
    {
        bool bContinue = pCallbackModifyDeviceSettings( pNewDeviceSettings,
                                                        GetDXUTState().GetModifyDeviceSettingsFuncUserContext() );
        if( !bContinue )
        {
            // The app rejected the device change by returning false, so just use the current device if there is one.
            if( pOldDeviceSettings == NULL )
                DXUTDisplayErrorMessage( DXUTERR_NOCOMPATIBLEDEVICES );
            SAFE_DELETE( pNewDeviceSettings );
            return E_ABORT;
        }
        if( GetDXUTState().GetD3D9() == NULL && GetDXUTState().GetDXGIFactory() == NULL ) // if DXUTShutdown() was called in the modify callback, just return
        {
            SAFE_DELETE( pNewDeviceSettings );
            return S_FALSE;
        }
    }

    GetDXUTState().SetCurrentDeviceSettings( pNewDeviceSettings );

    DXUTPause( true, true );

    // When a WM_SIZE message is received, it calls DXUTCheckForWindowSizeChange().
    // A WM_SIZE message might be sent when adjusting the window, so tell 
    // DXUTCheckForWindowSizeChange() to ignore size changes temporarily
    if( DXUTIsCurrentDeviceD3D9() )
        GetDXUTState().SetIgnoreSizeChange( true );

    // Only apply the cmd line overrides if this is the first device created
    // and DXUTSetD3D*Device() isn't used
    if( NULL == pd3d9DeviceFromApp && NULL == pd3d10DeviceFromApp && NULL == pOldDeviceSettings )
    {
        // Updates the device settings struct based on the cmd line args.
        // Warning: if the device doesn't support these new settings then CreateDevice9() will fail.
        DXUTUpdateDeviceSettingsWithOverrides( pNewDeviceSettings );
    }

    // Take note if the backbuffer width & height are 0 now as they will change after pd3dDevice->Reset()
    bool bKeepCurrentWindowSize = false;
    if( DXUTGetBackBufferWidthFromDS( pNewDeviceSettings ) == 0 &&
        DXUTGetBackBufferHeightFromDS( pNewDeviceSettings ) == 0 )
        bKeepCurrentWindowSize = true;

    //////////////////////////
    // Before reset
    /////////////////////////

    // If we are using D3D9, adjust window style when switching from windowed to fullscreen and
    // vice versa.  Note that this is not necessary in D3D10 because DXGI handles this.  If both
    // DXUT and DXGI handle this, incorrect behavior would result.
    if( DXUTIsCurrentDeviceD3D9() )
    {
        if( DXUTGetIsWindowedFromDS( pNewDeviceSettings ) )
        {
            // Going to windowed mode

            if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS( pOldDeviceSettings ) )
            {
                // Going from fullscreen -> windowed
                GetDXUTState().SetFullScreenBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(
                                                                         pOldDeviceSettings ) );
                GetDXUTState().SetFullScreenBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(
                                                                          pOldDeviceSettings ) );

                // Restore windowed mode style
                SetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE, GetDXUTState().GetWindowedStyleAtModeChange() );
            }

            // If different device windows are used for windowed mode and fullscreen mode,
            // hide the fullscreen window so that it doesn't obscure the screen.
            if( DXUTGetHWNDDeviceFullScreen() != DXUTGetHWNDDeviceWindowed() )
                ShowWindow( DXUTGetHWNDDeviceFullScreen(), SW_HIDE );

            // If using the same window for windowed and fullscreen mode, reattach menu if one exists
            if( DXUTGetHWNDDeviceFullScreen() == DXUTGetHWNDDeviceWindowed() )
            {
                if( GetDXUTState().GetMenu() != NULL )
                    SetMenu( DXUTGetHWNDDeviceWindowed(), GetDXUTState().GetMenu() );
            }
        }
        else
        {
            // Going to fullscreen mode

            if( pOldDeviceSettings == NULL || ( pOldDeviceSettings && DXUTGetIsWindowedFromDS( pOldDeviceSettings ) ) )
            {
                // Transistioning to full screen mode from a standard window so 
                // save current window position/size/style now in case the user toggles to windowed mode later 
                WINDOWPLACEMENT* pwp = GetDXUTState().GetWindowedPlacement();
                ZeroMemory( pwp, sizeof( WINDOWPLACEMENT ) );
                pwp->length = sizeof( WINDOWPLACEMENT );
                GetWindowPlacement( DXUTGetHWNDDeviceWindowed(), pwp );
                bool bIsTopmost = ( ( GetWindowLong( DXUTGetHWNDDeviceWindowed(),
                                                     GWL_EXSTYLE ) & WS_EX_TOPMOST ) != 0 );
                GetDXUTState().SetTopmostWhileWindowed( bIsTopmost );
                DWORD dwStyle = GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE );
                dwStyle &= ~WS_MAXIMIZE & ~WS_MINIMIZE; // remove minimize/maximize style
                GetDXUTState().SetWindowedStyleAtModeChange( dwStyle );
                if( pOldDeviceSettings )
                {
                    GetDXUTState().SetWindowBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(
                                                                         pOldDeviceSettings ) );
                    GetDXUTState().SetWindowBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(
                                                                          pOldDeviceSettings ) );
                }
            }

            // Hide the window to avoid animation of blank windows
            ShowWindow( DXUTGetHWNDDeviceFullScreen(), SW_HIDE );

            // Set FS window style
            SetWindowLong( DXUTGetHWNDDeviceFullScreen(), GWL_STYLE, WS_POPUP | WS_SYSMENU );

            // If using the same window for windowed and fullscreen mode, save and remove menu 
            if( DXUTGetHWNDDeviceFullScreen() == DXUTGetHWNDDeviceWindowed() )
            {
                HMENU hMenu = GetMenu( DXUTGetHWNDDeviceFullScreen() );
                GetDXUTState().SetMenu( hMenu );
                SetMenu( DXUTGetHWNDDeviceFullScreen(), NULL );
            }

            WINDOWPLACEMENT wpFullscreen;
            ZeroMemory( &wpFullscreen, sizeof( WINDOWPLACEMENT ) );
            wpFullscreen.length = sizeof( WINDOWPLACEMENT );
            GetWindowPlacement( DXUTGetHWNDDeviceFullScreen(), &wpFullscreen );
            if( ( wpFullscreen.flags & WPF_RESTORETOMAXIMIZED ) != 0 )
            {
                // Restore the window to normal if the window was maximized then minimized.  This causes the 
                // WPF_RESTORETOMAXIMIZED flag to be set which will cause SW_RESTORE to restore the 
                // window from minimized to maxmized which isn't what we want
                wpFullscreen.flags &= ~WPF_RESTORETOMAXIMIZED;
                wpFullscreen.showCmd = SW_RESTORE;
                SetWindowPlacement( DXUTGetHWNDDeviceFullScreen(), &wpFullscreen );
            }
        }
    }
    else
    {
        if( DXUTGetIsWindowedFromDS( pNewDeviceSettings ) )
        {
            // Going to windowed mode
            if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS( pOldDeviceSettings ) )
            {
                // Going from fullscreen -> windowed
                GetDXUTState().SetFullScreenBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(
                                                                         pOldDeviceSettings ) );
                GetDXUTState().SetFullScreenBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(
                                                                          pOldDeviceSettings ) );
                //DXGI should handle this, but in the case where switching from d3d9 full screen to windowed d3d11 it does not.
                SetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE, GetDXUTState().GetWindowedStyleAtModeChange() );
            }
        }
        else
        {
            // Going to fullscreen mode
            if( pOldDeviceSettings == NULL || ( pOldDeviceSettings && DXUTGetIsWindowedFromDS( pOldDeviceSettings ) ) )
            {
                // Transistioning to full screen mode from a standard window so 
                if( pOldDeviceSettings )
                {
                    GetDXUTState().SetWindowBackBufferWidthAtModeChange( DXUTGetBackBufferWidthFromDS(
                                                                         pOldDeviceSettings ) );
                    GetDXUTState().SetWindowBackBufferHeightAtModeChange( DXUTGetBackBufferHeightFromDS(
                                                                          pOldDeviceSettings ) );
                }
            }
        }
    }

    // If API version, AdapterOrdinal and DeviceType are the same, we can just do a Reset().
    // If they've changed, we need to do a complete device tear down/rebuild.
    // Also only allow a reset if pd3dDevice is the same as the current device 
    if( !bForceRecreate &&
        DXUTCanDeviceBeReset( pOldDeviceSettings, pNewDeviceSettings, pd3d9DeviceFromApp, pd3d10DeviceFromApp ) )
    {
        // Reset the Direct3D device and call the app's device callbacks
        if( DXUTIsD3D9( pOldDeviceSettings ) )
            hr = DXUTReset3DEnvironment9();
        else
            hr = DXUTReset3DEnvironment10();
        if( FAILED( hr ) )
        {
            if( D3DERR_DEVICELOST == hr )
            {
                // The device is lost, just mark it as so and continue on with 
                // capturing the state and resizing the window/etc.
                GetDXUTState().SetDeviceLost( true );
            }
            else if( DXUTERR_RESETTINGDEVICEOBJECTS == hr ||
                     DXUTERR_MEDIANOTFOUND == hr )
            {
                // Something bad happened in the app callbacks
                SAFE_DELETE( pOldDeviceSettings );
                DXUTDisplayErrorMessage( hr );
                DXUTShutdown();
                return hr;
            }
            else // DXUTERR_RESETTINGDEVICE
            {
                // Reset failed and the device wasn't lost and it wasn't the apps fault, 
                // so recreate the device to try to recover
                GetDXUTState().SetCurrentDeviceSettings( pOldDeviceSettings );
                if( FAILED( DXUTChangeDevice( pNewDeviceSettings, pd3d9DeviceFromApp, pd3d10DeviceFromApp, true,
                                              bClipWindowToSingleAdapter ) ) )
                {
                    // If that fails, then shutdown
                    DXUTShutdown();
                    return DXUTERR_CREATINGDEVICE;
                }
                else
                {
                    DXUTPause( false, false );
                    return S_OK;
                }
            }
        }
    }
    else
    {
        // Cleanup if not first device created
        if( pOldDeviceSettings )
            DXUTCleanup3DEnvironment( false );

        // Create the D3D device and call the app's device callbacks
        if( DXUTIsD3D9( pNewDeviceSettings ) )
            hr = DXUTCreate3DEnvironment9( pd3d9DeviceFromApp );
        else
            hr = DXUTCreate3DEnvironment10( pd3d10DeviceFromApp );
        if( FAILED( hr ) )
        {
            SAFE_DELETE( pOldDeviceSettings );
            DXUTCleanup3DEnvironment( true );
            DXUTDisplayErrorMessage( hr );
            DXUTPause( false, false );
            GetDXUTState().SetIgnoreSizeChange( false );
            return hr;
        }
    }

    // Enable/disable StickKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut, and Windows key 
    // to prevent accidental task switching
    DXUTAllowShortcutKeys( ( DXUTGetIsWindowedFromDS( pNewDeviceSettings ) ) ?
                           GetDXUTState().GetAllowShortcutKeysWhenWindowed() :
                           GetDXUTState().GetAllowShortcutKeysWhenFullscreen() );

    HMONITOR hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );
    GetDXUTState().SetAdapterMonitor( hAdapterMonitor );

    // Update the device stats text
    DXUTUpdateStaticFrameStats();

    if( pOldDeviceSettings && !DXUTGetIsWindowedFromDS( pOldDeviceSettings ) &&
        DXUTGetIsWindowedFromDS( pNewDeviceSettings ) )
    {
        // Going from fullscreen -> windowed

        // Restore the show state, and positions/size of the window to what it was
        // It is important to adjust the window size 
        // after resetting the device rather than beforehand to ensure 
        // that the monitor resolution is correct and does not limit the size of the new window.
        WINDOWPLACEMENT* pwp = GetDXUTState().GetWindowedPlacement();
        SetWindowPlacement( DXUTGetHWNDDeviceWindowed(), pwp );

        // Also restore the z-order of window to previous state
        HWND hWndInsertAfter = GetDXUTState().GetTopmostWhileWindowed() ? HWND_TOPMOST : HWND_NOTOPMOST;
        SetWindowPos( DXUTGetHWNDDeviceWindowed(), hWndInsertAfter, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE );
    }

    // Check to see if the window needs to be resized.  
    // Handle cases where the window is minimized and maxmimized as well.
    bool bNeedToResize = false;
    if( DXUTGetIsWindowedFromDS( pNewDeviceSettings ) && // only resize if in windowed mode
        !bKeepCurrentWindowSize )                      // only resize if pp.BackbufferWidth/Height were not 0
    {
        UINT nClientWidth;
        UINT nClientHeight;
        if( IsIconic( DXUTGetHWNDDeviceWindowed() ) )
        {
            // Window is currently minimized. To tell if it needs to resize, 
            // get the client rect of window when its restored the 
            // hard way using GetWindowPlacement()
            WINDOWPLACEMENT wp;
            ZeroMemory( &wp, sizeof( WINDOWPLACEMENT ) );
            wp.length = sizeof( WINDOWPLACEMENT );
            GetWindowPlacement( DXUTGetHWNDDeviceWindowed(), &wp );

            if( ( wp.flags & WPF_RESTORETOMAXIMIZED ) != 0 && wp.showCmd == SW_SHOWMINIMIZED )
            {
                // WPF_RESTORETOMAXIMIZED means that when the window is restored it will
                // be maximized.  So maximize the window temporarily to get the client rect 
                // when the window is maximized.  GetSystemMetrics( SM_CXMAXIMIZED ) will give this 
                // information if the window is on the primary but this will work on multimon.
                ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_RESTORE );
                RECT rcClient;
                GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
                nClientWidth = ( UINT )( rcClient.right - rcClient.left );
                nClientHeight = ( UINT )( rcClient.bottom - rcClient.top );
                ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_MINIMIZE );
            }
            else
            {
                // Use wp.rcNormalPosition to get the client rect, but wp.rcNormalPosition 
                // includes the window frame so subtract it
                RECT rcFrame = {0};
                AdjustWindowRect( &rcFrame, GetDXUTState().GetWindowedStyleAtModeChange(),
                                  GetDXUTState().GetMenu() != NULL );
                LONG nFrameWidth = rcFrame.right - rcFrame.left;
                LONG nFrameHeight = rcFrame.bottom - rcFrame.top;
                nClientWidth = ( UINT )( wp.rcNormalPosition.right - wp.rcNormalPosition.left - nFrameWidth );
                nClientHeight = ( UINT )( wp.rcNormalPosition.bottom - wp.rcNormalPosition.top - nFrameHeight );
            }
        }
        else
        {
            // Window is restored or maximized so just get its client rect
            RECT rcClient;
            GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
            nClientWidth = ( UINT )( rcClient.right - rcClient.left );
            nClientHeight = ( UINT )( rcClient.bottom - rcClient.top );
        }

        // Now that we know the client rect, compare it against the back buffer size
        // to see if the client rect is already the right size
        if( nClientWidth != DXUTGetBackBufferWidthFromDS( pNewDeviceSettings ) ||
            nClientHeight != DXUTGetBackBufferHeightFromDS( pNewDeviceSettings ) )
        {
            bNeedToResize = true;
        }

        if( bClipWindowToSingleAdapter && !IsIconic( DXUTGetHWNDDeviceWindowed() ) )
        {
            // Get the rect of the monitor attached to the adapter
            MONITORINFO miAdapter;
            miAdapter.cbSize = sizeof( MONITORINFO );
            hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );
            DXUTGetMonitorInfo( hAdapterMonitor, &miAdapter );
            HMONITOR hWindowMonitor = DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY );

            // Get the rect of the window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceWindowed(), &rcWindow );

            // Check if the window rect is fully inside the adapter's vitural screen rect
            if( ( rcWindow.left < miAdapter.rcWork.left ||
                  rcWindow.right > miAdapter.rcWork.right ||
                  rcWindow.top < miAdapter.rcWork.top ||
                  rcWindow.bottom > miAdapter.rcWork.bottom ) )
            {
                if( hWindowMonitor == hAdapterMonitor && IsZoomed( DXUTGetHWNDDeviceWindowed() ) )
                {
                    // If the window is maximized and on the same monitor as the adapter, then 
                    // no need to clip to single adapter as the window is already clipped 
                    // even though the rcWindow rect is outside of the miAdapter.rcWork
                }
                else
                {
                    bNeedToResize = true;
                }
            }
        }
    }

    // Only resize window if needed 
    if( bNeedToResize )
    {
        // Need to resize, so if window is maximized or minimized then restore the window
        if( IsIconic( DXUTGetHWNDDeviceWindowed() ) )
            ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_RESTORE );
        if( IsZoomed( DXUTGetHWNDDeviceWindowed() ) ) // doing the IsIconic() check first also handles the WPF_RESTORETOMAXIMIZED case
            ShowWindow( DXUTGetHWNDDeviceWindowed(), SW_RESTORE );
        // D3D10 software rasterizer don't need an associated monitor and don't need to be clipped to a single adapater
        if( bClipWindowToSingleAdapter && 
            ( ( DXUTGetDeviceSettings().ver == DXUT_D3D10_DEVICE && DXUTGetDeviceSettings().d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE )
            || DXUTGetDeviceSettings().ver == DXUT_D3D9_DEVICE ) ) 
        {
            // Get the rect of the monitor attached to the adapter
            MONITORINFO miAdapter;
            miAdapter.cbSize = sizeof( MONITORINFO );
            hAdapterMonitor = DXUTGetMonitorFromAdapter( pNewDeviceSettings );

			DXUTGetMonitorInfo( hAdapterMonitor, &miAdapter);

			// Get the rect of the monitor attached to the window
			MONITORINFO miWindow;
			miWindow.cbSize = sizeof( MONITORINFO );
			DXUTGetMonitorInfo( DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY ), &miWindow );

			// Do something reasonable if the BackBuffer size is greater than the monitor size
			int nAdapterMonitorWidth = miAdapter.rcWork.right - miAdapter.rcWork.left;
			int nAdapterMonitorHeight = miAdapter.rcWork.bottom - miAdapter.rcWork.top;

			int nClientWidth = DXUTGetBackBufferWidthFromDS( pNewDeviceSettings );
			int nClientHeight = DXUTGetBackBufferHeightFromDS( pNewDeviceSettings );

			// Get the rect of the window
			RECT rcWindow;
			GetWindowRect( DXUTGetHWNDDeviceWindowed(), &rcWindow );

			// Make a window rect with a client rect that is the same size as the backbuffer
			RECT rcResizedWindow;
			rcResizedWindow.left = 0;
			rcResizedWindow.right = nClientWidth;
			rcResizedWindow.top = 0;
			rcResizedWindow.bottom = nClientHeight;
			AdjustWindowRect( &rcResizedWindow, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ),
							  GetDXUTState().GetMenu() != NULL );

			int nWindowWidth = rcResizedWindow.right - rcResizedWindow.left;
			int nWindowHeight = rcResizedWindow.bottom - rcResizedWindow.top;

			if( nWindowWidth > nAdapterMonitorWidth )
				nWindowWidth = nAdapterMonitorWidth;
			if( nWindowHeight > nAdapterMonitorHeight )
				nWindowHeight = nAdapterMonitorHeight;

			if( rcResizedWindow.left < miAdapter.rcWork.left ||
				rcResizedWindow.top < miAdapter.rcWork.top ||
				rcResizedWindow.right > miAdapter.rcWork.right ||
				rcResizedWindow.bottom > miAdapter.rcWork.bottom )
			{
				int nWindowOffsetX = ( nAdapterMonitorWidth - nWindowWidth ) / 2;
				int nWindowOffsetY = ( nAdapterMonitorHeight - nWindowHeight ) / 2;

				rcResizedWindow.left = miAdapter.rcWork.left + nWindowOffsetX;
				rcResizedWindow.top = miAdapter.rcWork.top + nWindowOffsetY;
				rcResizedWindow.right = miAdapter.rcWork.left + nWindowOffsetX + nWindowWidth;
				rcResizedWindow.bottom = miAdapter.rcWork.top + nWindowOffsetY + nWindowHeight;
			}

			// Resize the window.  It is important to adjust the window size 
			// after resetting the device rather than beforehand to ensure 
			// that the monitor resolution is correct and does not limit the size of the new window.
			SetWindowPos( DXUTGetHWNDDeviceWindowed(), 0, rcResizedWindow.left, rcResizedWindow.top, nWindowWidth,
						  nWindowHeight, SWP_NOZORDER );
        }
        else
        {
            // Make a window rect with a client rect that is the same size as the backbuffer
            RECT rcWindow = {0};
            rcWindow.right = ( long )( DXUTGetBackBufferWidthFromDS( pNewDeviceSettings ) );
            rcWindow.bottom = ( long )( DXUTGetBackBufferHeightFromDS( pNewDeviceSettings ) );
            AdjustWindowRect( &rcWindow, GetWindowLong( DXUTGetHWNDDeviceWindowed(), GWL_STYLE ),
                              GetDXUTState().GetMenu() != NULL );

            // Resize the window.  It is important to adjust the window size 
            // after resetting the device rather than beforehand to ensure 
            // that the monitor resolution is correct and does not limit the size of the new window.
            int cx = ( int )( rcWindow.right - rcWindow.left );
            int cy = ( int )( rcWindow.bottom - rcWindow.top );
            SetWindowPos( DXUTGetHWNDDeviceWindowed(), 0, 0, 0, cx, cy, SWP_NOZORDER | SWP_NOMOVE );
        }

        // Its possible that the new window size is not what we asked for.  
        // No window can be sized larger than the desktop, so see if the Windows OS resized the 
        // window to something smaller to fit on the desktop.  Also if WM_GETMINMAXINFO
        // will put a limit on the smallest/largest window size.
        RECT rcClient;
        GetClientRect( DXUTGetHWNDDeviceWindowed(), &rcClient );
        UINT nClientWidth = ( UINT )( rcClient.right - rcClient.left );
        UINT nClientHeight = ( UINT )( rcClient.bottom - rcClient.top );
        if( nClientWidth != DXUTGetBackBufferWidthFromDS( pNewDeviceSettings ) ||
            nClientHeight != DXUTGetBackBufferHeightFromDS( pNewDeviceSettings ) )
        {
            // If its different, then resize the backbuffer again.  This time create a backbuffer that matches the 
            // client rect of the current window w/o resizing the window.
            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
            if( DXUTIsD3D9( &deviceSettings ) ) deviceSettings.d3d9.pp.BackBufferWidth = 0;
            else
                deviceSettings.d3d10.sd.BufferDesc.Width = 0;
            if( DXUTIsD3D9( &deviceSettings ) ) deviceSettings.d3d9.pp.BackBufferHeight = 0;
            else
                deviceSettings.d3d10.sd.BufferDesc.Height = 0;

            hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, bClipWindowToSingleAdapter );
            if( FAILED( hr ) )
            {
                SAFE_DELETE( pOldDeviceSettings );
                DXUTCleanup3DEnvironment( true );
                DXUTPause( false, false );
                GetDXUTState().SetIgnoreSizeChange( false );
                return hr;
            }
        }
    }

    // Make the window visible
    if( !IsWindowVisible( DXUTGetHWND() ) )
        ShowWindow( DXUTGetHWND(), SW_SHOW );

    // Ensure that the display doesn't power down when fullscreen but does when windowed
    if( !DXUTIsWindowed() )
        SetThreadExecutionState( ES_DISPLAY_REQUIRED | ES_CONTINUOUS );
    else
        SetThreadExecutionState( ES_CONTINUOUS );

    SAFE_DELETE( pOldDeviceSettings );
    GetDXUTState().SetIgnoreSizeChange( false );
    DXUTPause( false, false );
    GetDXUTState().SetDeviceCreated( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Check if the new device is close enough to the old device to simply reset or 
// resize the back buffer
//--------------------------------------------------------------------------------------
bool DXUTCanDeviceBeReset( DXUTDeviceSettings* pOldDeviceSettings, DXUTDeviceSettings* pNewDeviceSettings,
                           IDirect3DDevice9* pd3d9DeviceFromApp, ID3D10Device* pd3d10DeviceFromApp )
{
    if( pOldDeviceSettings == NULL || pOldDeviceSettings->ver != pNewDeviceSettings->ver )
        return false;

    if( pNewDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        if( DXUTGetD3D9Device() &&
            ( pd3d9DeviceFromApp == NULL || pd3d9DeviceFromApp == DXUTGetD3D9Device() ) &&
            ( pOldDeviceSettings->d3d9.AdapterOrdinal == pNewDeviceSettings->d3d9.AdapterOrdinal ) &&
            ( pOldDeviceSettings->d3d9.DeviceType == pNewDeviceSettings->d3d9.DeviceType ) &&
            ( pOldDeviceSettings->d3d9.BehaviorFlags == pNewDeviceSettings->d3d9.BehaviorFlags ) )
            return true;

        return false;
    }
    else
    {
        if( DXUTGetD3D10Device() &&
            ( pd3d10DeviceFromApp == NULL || pd3d10DeviceFromApp == DXUTGetD3D10Device() ) &&
            ( pOldDeviceSettings->d3d10.AdapterOrdinal == pNewDeviceSettings->d3d10.AdapterOrdinal ) &&
            ( pOldDeviceSettings->d3d10.DriverType == pNewDeviceSettings->d3d10.DriverType ) &&
            ( pOldDeviceSettings->d3d10.CreateFlags == pNewDeviceSettings->d3d10.CreateFlags ) &&
            ( pOldDeviceSettings->d3d10.sd.SampleDesc.Count == pNewDeviceSettings->d3d10.sd.SampleDesc.Count ) &&
            ( pOldDeviceSettings->d3d10.sd.SampleDesc.Quality == pNewDeviceSettings->d3d10.sd.SampleDesc.Quality ) )
            return true;

        return false;
    }
}


//--------------------------------------------------------------------------------------
// Creates a DXGI factory object if one has not already been created  
//--------------------------------------------------------------------------------------
HRESULT DXUTDelayLoadDXGI()
{
    IDXGIFactory* pDXGIFactory = GetDXUTState().GetDXGIFactory();
    if( pDXGIFactory == NULL )
    {
        DXUT_Dynamic_CreateDXGIFactory( __uuidof( IDXGIFactory ), ( LPVOID* )&pDXGIFactory );
        GetDXUTState().SetDXGIFactory( pDXGIFactory );
        if( pDXGIFactory == NULL )
        {
            // If still NULL, then DXGI is not availible
            GetDXUTState().SetD3D10Available( false );
            return DXUTERR_NODIRECT3D;
        }

        GetDXUTState().SetD3D10Available( true );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Creates a Direct3D object if one has not already been created  
//--------------------------------------------------------------------------------------
HRESULT DXUTDelayLoadD3D9()
{
    IDirect3D9* pD3D = GetDXUTState().GetD3D9();
    if( pD3D == NULL )
    {
        // This may fail if Direct3D 9 isn't installed
        // This may also fail if the Direct3D headers are somehow out of sync with the installed Direct3D DLLs
        pD3D = DXUT_Dynamic_Direct3DCreate9( D3D_SDK_VERSION );
        if( pD3D == NULL )
        {
            // If still NULL, then D3D9 is not availible
            return DXUTERR_NODIRECT3D;
        }

        GetDXUTState().SetD3D9( pD3D );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Updates the device settings struct based on the cmd line args.  
//--------------------------------------------------------------------------------------
void DXUTUpdateDeviceSettingsWithOverrides( DXUTDeviceSettings* pDeviceSettings )
{
    if( DXUTIsD3D9( pDeviceSettings ) )
    {
        if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
            pDeviceSettings->d3d9.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();

        if( GetDXUTState().GetOverrideFullScreen() )
            pDeviceSettings->d3d9.pp.Windowed = false;
        if( GetDXUTState().GetOverrideWindowed() )
            pDeviceSettings->d3d9.pp.Windowed = true;

        if( GetDXUTState().GetOverrideForceREF() )
            pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
        else if( GetDXUTState().GetOverrideForceHAL() )
            pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_HAL;

        if( GetDXUTState().GetOverrideWidth() != 0 )
            pDeviceSettings->d3d9.pp.BackBufferWidth = GetDXUTState().GetOverrideWidth();
        if( GetDXUTState().GetOverrideHeight() != 0 )
            pDeviceSettings->d3d9.pp.BackBufferHeight = GetDXUTState().GetOverrideHeight();

        if( GetDXUTState().GetOverrideForcePureHWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_PUREDEVICE;
        }
        else if( GetDXUTState().GetOverrideForceHWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
        }
        else if( GetDXUTState().GetOverrideForceSWVP() )
        {
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
            pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
            pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        }
    }
    else
    {
        if( GetDXUTState().GetOverrideAdapterOrdinal() != -1 )
            pDeviceSettings->d3d10.AdapterOrdinal = GetDXUTState().GetOverrideAdapterOrdinal();

        if( GetDXUTState().GetOverrideFullScreen() )
        {
            pDeviceSettings->d3d10.sd.Windowed = false;
            pDeviceSettings->d3d10.sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        }
        if( GetDXUTState().GetOverrideWindowed() )
        {
            pDeviceSettings->d3d10.sd.Windowed = true;
            pDeviceSettings->d3d10.sd.Flags = 0;
        }

        if( GetDXUTState().GetOverrideForceREF() )
            pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
        else if( GetDXUTState().GetOverrideForceHAL() )
            pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
        else if( GetDXUTState().GetOverrideForceWARP() )
            pDeviceSettings->d3d10.DriverType = D3D10_DRIVER_TYPE_SOFTWARE;

        if( GetDXUTState().GetOverrideWidth() != 0 )
            pDeviceSettings->d3d10.sd.BufferDesc.Width = GetDXUTState().GetOverrideWidth();
        if( GetDXUTState().GetOverrideHeight() != 0 )
            pDeviceSettings->d3d10.sd.BufferDesc.Height = GetDXUTState().GetOverrideHeight();
    }
}


//--------------------------------------------------------------------------------------
// Allows the app to explictly state if it supports D3D9 or D3D10.  Typically
// calling this is not needed as DXUT will auto-detect this based on the callbacks set.
//--------------------------------------------------------------------------------------
void WINAPI DXUTSetD3DVersionSupport( bool bAppCanUseD3D9, bool bAppCanUseD3D10 )
{
    GetDXUTState().SetUseD3DVersionOverride( true );
    GetDXUTState().SetAppSupportsD3D9Override( bAppCanUseD3D9 );
    GetDXUTState().SetAppSupportsD3D10Override( bAppCanUseD3D10 );
}


//--------------------------------------------------------------------------------------
// Returns true if app has registered any D3D9 callbacks or 
// used the DXUTSetD3DVersionSupport API and passed true for bAppCanUseD3D9
//--------------------------------------------------------------------------------------
bool WINAPI DXUTDoesAppSupportD3D9()
{
    if( GetDXUTState().GetUseD3DVersionOverride() )
        return GetDXUTState().GetAppSupportsD3D9Override();
    else
        return GetDXUTState().GetIsD3D9DeviceAcceptableFunc() ||
            GetDXUTState().GetD3D9DeviceCreatedFunc() ||
            GetDXUTState().GetD3D9DeviceResetFunc() ||
            GetDXUTState().GetD3D9DeviceLostFunc() ||
            GetDXUTState().GetD3D9DeviceDestroyedFunc() ||
            GetDXUTState().GetD3D9FrameRenderFunc();
}


//--------------------------------------------------------------------------------------
// Returns true if app has registered any D3D10 callbacks or 
// used the DXUTSetD3DVersionSupport API and passed true for bAppCanUseD3D10
//--------------------------------------------------------------------------------------
bool WINAPI DXUTDoesAppSupportD3D10()
{
    if( GetDXUTState().GetUseD3DVersionOverride() )
        return GetDXUTState().GetAppSupportsD3D10Override();
    else
        return GetDXUTState().GetIsD3D10DeviceAcceptableFunc() ||
            GetDXUTState().GetD3D10DeviceCreatedFunc() ||
            GetDXUTState().GetD3D10SwapChainResizedFunc() ||
            GetDXUTState().GetD3D10FrameRenderFunc() ||
            GetDXUTState().GetD3D10SwapChainReleasingFunc() ||
            GetDXUTState().GetD3D10DeviceDestroyedFunc();
}


//======================================================================================
//======================================================================================
// Direct3D 9 section
//======================================================================================
//======================================================================================


//--------------------------------------------------------------------------------------
// Passes a previously created Direct3D9 device for use by the framework.  
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() or 
// DXUTCreateDeviceFromSettings() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetD3D9Device( IDirect3DDevice9* pd3dDevice )
{
    HRESULT hr;

    if( pd3dDevice == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D9Device", E_INVALIDARG );

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D9Device", E_FAIL );

    GetDXUTState().SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !GetDXUTState().GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( GetDXUTState().GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }

    DXUTDeviceSettings DeviceSettings;
    ZeroMemory( &DeviceSettings, sizeof( DXUTDeviceSettings ) );
    DeviceSettings.ver = DXUT_D3D9_DEVICE;

    // Get the present params from the swap chain
    IDirect3DSurface9* pBackBuffer = NULL;
    hr = pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
    if( SUCCEEDED( hr ) )
    {
        IDirect3DSwapChain9* pSwapChain = NULL;
        hr = pBackBuffer->GetContainer( IID_IDirect3DSwapChain9, ( void** )&pSwapChain );
        if( SUCCEEDED( hr ) )
        {
            pSwapChain->GetPresentParameters( &DeviceSettings.d3d9.pp );
            SAFE_RELEASE( pSwapChain );
        }

        SAFE_RELEASE( pBackBuffer );
    }

    D3DDEVICE_CREATION_PARAMETERS d3dCreationParams;
    pd3dDevice->GetCreationParameters( &d3dCreationParams );

    // Fill out the rest of the device settings struct
    DeviceSettings.d3d9.AdapterOrdinal = d3dCreationParams.AdapterOrdinal;
    DeviceSettings.d3d9.DeviceType = d3dCreationParams.DeviceType;
    DXUTFindD3D9AdapterFormat( DeviceSettings.d3d9.AdapterOrdinal, DeviceSettings.d3d9.DeviceType,
                               DeviceSettings.d3d9.pp.BackBufferFormat, DeviceSettings.d3d9.pp.Windowed,
                               &DeviceSettings.d3d9.AdapterFormat );
    DeviceSettings.d3d9.BehaviorFlags = d3dCreationParams.BehaviorFlags;

    // Change to the Direct3D device passed in
    hr = DXUTChangeDevice( &DeviceSettings, pd3dDevice, NULL, false, false );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Creates the 3D environment
//--------------------------------------------------------------------------------------
HRESULT DXUTCreate3DEnvironment9( IDirect3DDevice9* pd3dDeviceFromApp )
{
    HRESULT hr = S_OK;

    IDirect3DDevice9* pd3dDevice = NULL;
    DXUTDeviceSettings* pNewDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    assert( pNewDeviceSettings != NULL );

    // Only create a Direct3D device if one hasn't been supplied by the app
    if( pd3dDeviceFromApp == NULL )
    {
        // Try to create the device with the chosen settings
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        assert( pD3D != NULL );
        hr = pD3D->CreateDevice( pNewDeviceSettings->d3d9.AdapterOrdinal, pNewDeviceSettings->d3d9.DeviceType,
                                 DXUTGetHWNDFocus(), pNewDeviceSettings->d3d9.BehaviorFlags,
                                 &pNewDeviceSettings->d3d9.pp, &pd3dDevice );
        if( hr == D3DERR_DEVICELOST )
        {
            GetDXUTState().SetDeviceLost( true );
            return S_OK;
        }
        else if( FAILED( hr ) )
        {
            DXUT_ERR( L"CreateDevice", hr );
            return DXUTERR_CREATINGDEVICE;
        }
    }
    else
    {
        pd3dDeviceFromApp->AddRef();
        pd3dDevice = pd3dDeviceFromApp;
    }

    GetDXUTState().SetD3D9Device( pd3dDevice );

    // If switching to REF, set the exit code to 10.  If switching to HAL and exit code was 10, then set it back to 0.
    if( pNewDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF && GetDXUTState().GetExitCode() == 0 )
        GetDXUTState().SetExitCode( 10 );
    else if( pNewDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_HAL && GetDXUTState().GetExitCode() == 10 )
        GetDXUTState().SetExitCode( 0 );

    // Update back buffer desc before calling app's device callbacks
    DXUTUpdateBackBufferDesc();

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Update GetDXUTState()'s copy of D3D caps 
    D3DCAPS9* pd3dCaps = GetDXUTState().GetCaps();
    pd3dDevice->GetDeviceCaps( pd3dCaps );

    // Update the device stats text
    CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
    assert( pd3dEnum != NULL );
    CD3D9EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo( pNewDeviceSettings->d3d9.AdapterOrdinal );
    DXUTUpdateD3D9DeviceStats( pNewDeviceSettings->d3d9.DeviceType,
                               pNewDeviceSettings->d3d9.BehaviorFlags,
                               &pAdapterInfo->AdapterIdentifier );

    // Call the app's device created callback if non-NULL
    const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D9DEVICECREATED pCallbackDeviceCreated = GetDXUTState().GetD3D9DeviceCreatedFunc();
    hr = S_OK;
    if( pCallbackDeviceCreated != NULL )
        hr = pCallbackDeviceCreated( DXUTGetD3D9Device(), pBackBufferSurfaceDesc,
                                     GetDXUTState().GetD3D9DeviceCreatedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D9Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DeviceCreated callback", hr );
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_CREATINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsCreated( true );

    // Call the app's device reset callback if non-NULL
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D9DEVICERESET pCallbackDeviceReset = GetDXUTState().GetD3D9DeviceResetFunc();
    hr = S_OK;
    if( pCallbackDeviceReset != NULL )
        hr = pCallbackDeviceReset( DXUTGetD3D9Device(), pBackBufferSurfaceDesc,
                                   GetDXUTState().GetD3D9DeviceResetFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D9Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DeviceReset callback", hr );
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_RESETTINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resets the 3D environment by:
//      - Calls the device lost callback 
//      - Resets the device
//      - Stores the back buffer description
//      - Sets up the full screen Direct3D cursor if requested
//      - Calls the device reset callback 
//--------------------------------------------------------------------------------------
HRESULT DXUTReset3DEnvironment9()
{
    HRESULT hr;

    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
    assert( pd3dDevice != NULL );

    // Call the app's device lost callback
    if( GetDXUTState().GetDeviceObjectsReset() == true )
    {
        GetDXUTState().SetInsideDeviceCallback( true );
        LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
        if( pCallbackDeviceLost != NULL )
            pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
        GetDXUTState().SetDeviceObjectsReset( false );
        GetDXUTState().SetInsideDeviceCallback( false );
    }

    // Reset the device
    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    assert( pDeviceSettings != NULL );
    hr = pd3dDevice->Reset( &pDeviceSettings->d3d9.pp );
    if( FAILED( hr ) )
    {
        if( hr == D3DERR_DEVICELOST )
            return D3DERR_DEVICELOST; // Reset could legitimately fail if the device is lost
        else
            return DXUT_ERR( L"Reset", DXUTERR_RESETTINGDEVICE );
    }

    // Update back buffer desc before calling app's device callbacks
    DXUTUpdateBackBufferDesc();

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Call the app's OnDeviceReset callback
    GetDXUTState().SetInsideDeviceCallback( true );
    const D3DSURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetD3D9BackBufferSurfaceDesc();
    LPDXUTCALLBACKD3D9DEVICERESET pCallbackDeviceReset = GetDXUTState().GetD3D9DeviceResetFunc();
    hr = S_OK;
    if( pCallbackDeviceReset != NULL )
        hr = pCallbackDeviceReset( pd3dDevice, pBackBufferSurfaceDesc,
                                   GetDXUTState().GetD3D9DeviceResetFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( FAILED( hr ) )
    {
        // If callback failed, cleanup
        DXUT_ERR( L"DeviceResetCallback", hr );
        if( hr != DXUTERR_MEDIANOTFOUND )
            hr = DXUTERR_RESETTINGDEVICEOBJECTS;

        GetDXUTState().SetInsideDeviceCallback( true );
        LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
        if( pCallbackDeviceLost != NULL )
            pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
        GetDXUTState().SetInsideDeviceCallback( false );
        return hr;
    }

    // Success
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the 3D environment by:
//      - Checking if the device is lost and trying to reset it if it is
//      - Get the elapsed time since the last frame
//      - Calling the app's framemove and render callback
//      - Calling Present()
//--------------------------------------------------------------------------------------
void DXUTRender3DEnvironment9()
{
    HRESULT hr;

    if( GetDXUTState().GetDeviceLost() || DXUTIsRenderingPaused() || !DXUTIsActive() )
    {
        // Window is minimized or paused so yield CPU time to other processes
        Sleep( 50 );
    }

    // If no device created yet because device was lost (ie. another fullscreen exclusive device exists), 
    // then wait and try to create every so often.
    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
    if( NULL == pd3dDevice )
    {
        if( GetDXUTState().GetDeviceLost() )
        {
            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
            DXUTChangeDevice( &deviceSettings, NULL, NULL, false, true );
        }

        return;
    }

    if( GetDXUTState().GetDeviceLost() && !GetDXUTState().GetRenderingPaused() )
    {
        // Test the cooperative level to see if it's okay to render.
        if( FAILED( hr = pd3dDevice->TestCooperativeLevel() ) )
        {
            if( D3DERR_DEVICELOST == hr )
            {
                // The device has been lost but cannot be reset at this time.
                // So wait until it can be reset.
                return;
            }

            // If we are windowed, read the desktop format and 
            // ensure that the Direct3D device is using the same format 
            // since the user could have changed the desktop bitdepth 
            if( DXUTIsWindowed() )
            {
                D3DDISPLAYMODE adapterDesktopDisplayMode;
                IDirect3D9* pD3D = DXUTGetD3D9Object();
                DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
                pD3D->GetAdapterDisplayMode( pDeviceSettings->d3d9.AdapterOrdinal, &adapterDesktopDisplayMode );
                if( pDeviceSettings->d3d9.AdapterFormat != adapterDesktopDisplayMode.Format )
                {
                    DXUTMatchOptions matchOptions;
                    matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
                    matchOptions.eAdapterOrdinal = DXUTMT_PRESERVE_INPUT;
                    matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
                    matchOptions.eWindowed = DXUTMT_PRESERVE_INPUT;
                    matchOptions.eAdapterFormat = DXUTMT_PRESERVE_INPUT;
                    matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
                    matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

                    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
                    deviceSettings.d3d9.AdapterFormat = adapterDesktopDisplayMode.Format;

                    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
                    if( FAILED( hr ) ) // the call will fail if no valid devices were found
                    {
                        DXUTDisplayErrorMessage( DXUTERR_NOCOMPATIBLEDEVICES );
                        DXUTShutdown();
                    }

                    // Change to a Direct3D device created from the new device settings.
                    // If there is an existing device, then either reset or recreate the scene
                    hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );
                    if( FAILED( hr ) )
                    {
                        // If this fails, try to go fullscreen and if this fails also shutdown.
                        if( FAILED( DXUTToggleFullScreen() ) )
                            DXUTShutdown();
                    }

                    return;
                }
            }

            // Try to reset the device
            if( FAILED( hr = DXUTReset3DEnvironment9() ) )
            {
                if( D3DERR_DEVICELOST == hr )
                {
                    // The device was lost again, so continue waiting until it can be reset.
                    return;
                }
                else if( DXUTERR_RESETTINGDEVICEOBJECTS == hr ||
                         DXUTERR_MEDIANOTFOUND == hr )
                {
                    DXUTDisplayErrorMessage( hr );
                    DXUTShutdown();
                    return;
                }
                else
                {
                    // Reset failed, but the device wasn't lost so something bad happened, 
                    // so recreate the device to try to recover
                    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
                    if( FAILED( DXUTChangeDevice( pDeviceSettings, NULL, NULL, true, false ) ) )
                    {
                        DXUTShutdown();
                        return;
                    }
                }
            }
        }

        GetDXUTState().SetDeviceLost( false );
    }

    // Get the app's time, in seconds. Skip rendering if no time elapsed
    double fTime, fAbsTime; float fElapsedTime;
    DXUTGetGlobalTimer()->GetTimeValues( &fTime, &fAbsTime, &fElapsedTime );

    // Store the time for the app
    if( GetDXUTState().GetConstantFrameTime() )
    {
        fElapsedTime = GetDXUTState().GetTimePerFrame();
        fTime = DXUTGetTime() + fElapsedTime;
    }

    GetDXUTState().SetTime( fTime );
    GetDXUTState().SetAbsoluteTime( fAbsTime );
    GetDXUTState().SetElapsedTime( fElapsedTime );

    // Update the FPS stats
    DXUTUpdateFrameStats();

    DXUTHandleTimers();

    // Animate the scene by calling the app's frame move callback
    LPDXUTCALLBACKFRAMEMOVE pCallbackFrameMove = GetDXUTState().GetFrameMoveFunc();
    if( pCallbackFrameMove != NULL )
    {
        pCallbackFrameMove( fTime, fElapsedTime, GetDXUTState().GetFrameMoveFuncUserContext() );
        pd3dDevice = DXUTGetD3D9Device();
        if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
            return;
    }

    if( !GetDXUTState().GetRenderingPaused() )
    {
        // Render the scene by calling the app's render callback
        LPDXUTCALLBACKD3D9FRAMERENDER pCallbackFrameRender = GetDXUTState().GetD3D9FrameRenderFunc();
        if( pCallbackFrameRender != NULL )
        {
            pCallbackFrameRender( pd3dDevice, fTime, fElapsedTime,
                                  GetDXUTState().GetD3D9FrameRenderFuncUserContext() );
            pd3dDevice = DXUTGetD3D9Device();
            if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
                return;
        }

#if defined(DEBUG) || defined(_DEBUG)
        // The back buffer should always match the client rect 
        // if the Direct3D backbuffer covers the entire window
        RECT rcClient;
        GetClientRect( DXUTGetHWND(), &rcClient );
        if( !IsIconic( DXUTGetHWND() ) )
        {
            GetClientRect( DXUTGetHWND(), &rcClient );
            assert( DXUTGetD3D9BackBufferSurfaceDesc()->Width == (UINT)rcClient.right );
            assert( DXUTGetD3D9BackBufferSurfaceDesc()->Height == (UINT)rcClient.bottom );
        }
#endif

        // Show the frame on the primary surface.
        hr = pd3dDevice->Present( NULL, NULL, NULL, NULL );
        if( FAILED( hr ) )
        {
            if( D3DERR_DEVICELOST == hr )
            {
                GetDXUTState().SetDeviceLost( true );
            }
            else if( D3DERR_DRIVERINTERNALERROR == hr )
            {
                // When D3DERR_DRIVERINTERNALERROR is returned from Present(),
                // the application can do one of the following:
                // 
                // - End, with the pop-up window saying that the application cannot continue 
                //   because of problems in the display adapter and that the user should 
                //   contact the adapter manufacturer.
                //
                // - Attempt to restart by calling IDirect3DDevice9::Reset, which is essentially the same 
                //   path as recovering from a lost device. If IDirect3DDevice9::Reset fails with 
                //   D3DERR_DRIVERINTERNALERROR, the application should end immediately with the message 
                //   that the user should contact the adapter manufacturer.
                // 
                // The framework attempts the path of resetting the device
                // 
                GetDXUTState().SetDeviceLost( true );
            }
        }
    }

    // If the app called DXUTWasKeyPressed() then do the work 
    // to store the current state of the keys in bLastKeys
    if( GetDXUTState().GetAppCalledWasKeyPressed() )
    {
        bool* bLastKeys = GetDXUTState().GetLastKeys();
        bool* bKeys = GetDXUTState().GetKeys();
        memcpy( bLastKeys, bKeys, sizeof( bool ) * 256 );
    }

    // Update current frame #
    int nFrame = GetDXUTState().GetCurrentFrameNumber();
    nFrame++;
    GetDXUTState().SetCurrentFrameNumber( nFrame );

    // Check to see if the app should shutdown due to cmdline
    if( GetDXUTState().GetOverrideQuitAfterFrame() != 0 )
    {
        if( nFrame > GetDXUTState().GetOverrideQuitAfterFrame() )
            DXUTShutdown();
    }

    return;
}


//--------------------------------------------------------------------------------------
// Cleans up the 3D environment by:
//      - Calls the device lost callback 
//      - Calls the device destroyed callback 
//      - Releases the D3D device
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment9( bool bReleaseSettings )
{
    IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
    if( pd3dDevice != NULL )
    {
        GetDXUTState().SetInsideDeviceCallback( true );

        // Call the app's device lost callback
        if( GetDXUTState().GetDeviceObjectsReset() == true )
        {
            LPDXUTCALLBACKD3D9DEVICELOST pCallbackDeviceLost = GetDXUTState().GetD3D9DeviceLostFunc();
            if( pCallbackDeviceLost != NULL )
                pCallbackDeviceLost( GetDXUTState().GetD3D9DeviceLostFuncUserContext() );
            GetDXUTState().SetDeviceObjectsReset( false );
        }

        // Call the app's device destroyed callback
        if( GetDXUTState().GetDeviceObjectsCreated() == true )
        {
            LPDXUTCALLBACKD3D9DEVICEDESTROYED pCallbackDeviceDestroyed = GetDXUTState().GetD3D9DeviceDestroyedFunc();
            if( pCallbackDeviceDestroyed != NULL )
                pCallbackDeviceDestroyed( GetDXUTState().GetD3D9DeviceDestroyedFuncUserContext() );
            GetDXUTState().SetDeviceObjectsCreated( false );
        }

        GetDXUTState().SetInsideDeviceCallback( false );

        // Release the D3D device and in debug configs, displays a message box if there 
        // are unrelease objects.
        if( pd3dDevice )
        {
            UINT references = pd3dDevice->Release();
            if( references > 0 )
            {
                DXUTDisplayErrorMessage( DXUTERR_NONZEROREFCOUNT );
                DXUT_ERR( L"DXUTCleanup3DEnvironment", DXUTERR_NONZEROREFCOUNT );
            }
        }
        GetDXUTState().SetD3D9Device( NULL );

        if( bReleaseSettings )
        {
            DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
            SAFE_DELETE(pOldDeviceSettings);
            GetDXUTState().SetCurrentDeviceSettings( NULL );
        }

        D3DSURFACE_DESC* pBackBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc9();
        ZeroMemory( pBackBufferSurfaceDesc, sizeof( D3DSURFACE_DESC ) );

        D3DCAPS9* pd3dCaps = GetDXUTState().GetCaps();
        ZeroMemory( pd3dCaps, sizeof( D3DCAPS9 ) );

        GetDXUTState().SetDeviceCreated( false );
    }
}


//--------------------------------------------------------------------------------------
// Gives the D3D device a cursor with image and hotspot from hCursor.
//--------------------------------------------------------------------------------------
HRESULT DXUTSetD3D9DeviceCursor( IDirect3DDevice9* pd3dDevice, HCURSOR hCursor, bool bAddWatermark )
{
    HRESULT hr = E_FAIL;
    ICONINFO iconinfo;
    bool bBWCursor = false;
    LPDIRECT3DSURFACE9 pCursorSurface = NULL;
    HDC hdcColor = NULL;
    HDC hdcMask = NULL;
    HDC hdcScreen = NULL;
    BITMAP bm;
    DWORD dwWidth = 0;
    DWORD dwHeightSrc = 0;
    DWORD dwHeightDest = 0;
    COLORREF crColor;
    COLORREF crMask;
    UINT x;
    UINT y;
    BITMAPINFO bmi;
    COLORREF* pcrArrayColor = NULL;
    COLORREF* pcrArrayMask = NULL;
    DWORD* pBitmap;
    HGDIOBJ hgdiobjOld;

    ZeroMemory( &iconinfo, sizeof( iconinfo ) );
    if( !GetIconInfo( hCursor, &iconinfo ) )
        goto End;

    if( 0 == GetObject( ( HGDIOBJ )iconinfo.hbmMask, sizeof( BITMAP ), ( LPVOID )&bm ) )
        goto End;
    dwWidth = bm.bmWidth;
    dwHeightSrc = bm.bmHeight;

    if( iconinfo.hbmColor == NULL )
    {
        bBWCursor = TRUE;
        dwHeightDest = dwHeightSrc / 2;
    }
    else
    {
        bBWCursor = FALSE;
        dwHeightDest = dwHeightSrc;
    }

    // Create a surface for the fullscreen cursor
    if( FAILED( hr = pd3dDevice->CreateOffscreenPlainSurface( dwWidth, dwHeightDest,
                                                              D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pCursorSurface,
                                                              NULL ) ) )
    {
        goto End;
    }

    pcrArrayMask = new DWORD[dwWidth * dwHeightSrc];

    ZeroMemory( &bmi, sizeof( bmi ) );
    bmi.bmiHeader.biSize = sizeof( bmi.bmiHeader );
    bmi.bmiHeader.biWidth = dwWidth;
    bmi.bmiHeader.biHeight = dwHeightSrc;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hdcScreen = GetDC( NULL );
    hdcMask = CreateCompatibleDC( hdcScreen );
    if( hdcMask == NULL )
    {
        hr = E_FAIL;
        goto End;
    }
    hgdiobjOld = SelectObject( hdcMask, iconinfo.hbmMask );
    GetDIBits( hdcMask, iconinfo.hbmMask, 0, dwHeightSrc,
               pcrArrayMask, &bmi, DIB_RGB_COLORS );
    SelectObject( hdcMask, hgdiobjOld );

    if( !bBWCursor )
    {
        pcrArrayColor = new DWORD[dwWidth * dwHeightDest];
        hdcColor = CreateCompatibleDC( hdcScreen );
        if( hdcColor == NULL )
        {
            hr = E_FAIL;
            goto End;
        }
        SelectObject( hdcColor, iconinfo.hbmColor );
        GetDIBits( hdcColor, iconinfo.hbmColor, 0, dwHeightDest,
                   pcrArrayColor, &bmi, DIB_RGB_COLORS );
    }

    // Transfer cursor image into the surface
    D3DLOCKED_RECT lr;
    pCursorSurface->LockRect( &lr, NULL, 0 );
    pBitmap = ( DWORD* )lr.pBits;
    for( y = 0; y < dwHeightDest; y++ )
    {
        for( x = 0; x < dwWidth; x++ )
        {
            if( bBWCursor )
            {
                crColor = pcrArrayMask[dwWidth * ( dwHeightDest - 1 - y ) + x];
                crMask = pcrArrayMask[dwWidth * ( dwHeightSrc - 1 - y ) + x];
            }
            else
            {
                crColor = pcrArrayColor[dwWidth * ( dwHeightDest - 1 - y ) + x];
                crMask = pcrArrayMask[dwWidth * ( dwHeightDest - 1 - y ) + x];
            }
            if( crMask == 0 )
                pBitmap[dwWidth * y + x] = 0xff000000 | crColor;
            else
                pBitmap[dwWidth * y + x] = 0x00000000;

            // It may be helpful to make the D3D cursor look slightly 
            // different from the Windows cursor so you can distinguish 
            // between the two when developing/testing code.  When
            // bAddWatermark is TRUE, the following code adds some
            // small grey "D3D" characters to the upper-left corner of
            // the D3D cursor image.
            if( bAddWatermark && x < 12 && y < 5 )
            {
                // 11.. 11.. 11.. .... CCC0
                // 1.1. ..1. 1.1. .... A2A0
                // 1.1. .1.. 1.1. .... A4A0
                // 1.1. ..1. 1.1. .... A2A0
                // 11.. 11.. 11.. .... CCC0

                const WORD wMask[5] = { 0xccc0, 0xa2a0, 0xa4a0, 0xa2a0, 0xccc0 };
                if( wMask[y] & ( 1 << ( 15 - x ) ) )
                {
                    pBitmap[dwWidth * y + x] |= 0xff808080;
                }
            }
        }
    }
    pCursorSurface->UnlockRect();

    // Set the device cursor
    if( FAILED( hr = pd3dDevice->SetCursorProperties( iconinfo.xHotspot,
                                                      iconinfo.yHotspot, pCursorSurface ) ) )
    {
        goto End;
    }

    hr = S_OK;

End:
    if( iconinfo.hbmMask != NULL )
        DeleteObject( iconinfo.hbmMask );
    if( iconinfo.hbmColor != NULL )
        DeleteObject( iconinfo.hbmColor );
    if( hdcScreen != NULL )
        ReleaseDC( NULL, hdcScreen );
    if( hdcColor != NULL )
        DeleteDC( hdcColor );
    if( hdcMask != NULL )
        DeleteDC( hdcMask );
    SAFE_DELETE_ARRAY( pcrArrayColor );
    SAFE_DELETE_ARRAY( pcrArrayMask );
    SAFE_RELEASE( pCursorSurface );
    return hr;
}


//--------------------------------------------------------------------------------------
// Internal helper function to return the adapter format from the first device settings 
// combo that matches the passed adapter ordinal, device type, backbuffer format, and windowed.  
//--------------------------------------------------------------------------------------
HRESULT DXUTFindD3D9AdapterFormat( UINT AdapterOrdinal, D3DDEVTYPE DeviceType, D3DFORMAT BackBufferFormat,
                                   BOOL Windowed, D3DFORMAT* pAdapterFormat )
{
    CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration( false );
    assert( pd3dEnum != NULL );
    CD3D9EnumDeviceInfo* pDeviceInfo = pd3dEnum->GetDeviceInfo( AdapterOrdinal, DeviceType );
    if( pDeviceInfo )
    {
        for( int iDeviceCombo = 0; iDeviceCombo < pDeviceInfo->deviceSettingsComboList.GetSize(); iDeviceCombo++ )
        {
            CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pDeviceInfo->deviceSettingsComboList.GetAt(
                iDeviceCombo );
            if( pDeviceSettingsCombo->BackBufferFormat == BackBufferFormat &&
                pDeviceSettingsCombo->Windowed == Windowed )
            {
                // Return the adapter format from the first match
                *pAdapterFormat = pDeviceSettingsCombo->AdapterFormat;
                return S_OK;
            }
        }
    }

    *pAdapterFormat = BackBufferFormat;
    return E_FAIL;
}


//======================================================================================
//======================================================================================
// Direct3D 10 section
//======================================================================================
//======================================================================================


//--------------------------------------------------------------------------------------
// Passes a previously created Direct3D 10 device for use by the framework.  
// If DXUTCreateWindow() has not already been called, it will call it with the 
// default parameters.  Instead of calling this, you can call DXUTCreateDevice() or 
// DXUTCreateDeviceFromSettings() 
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetD3D10Device( ID3D10Device* pd3dDevice, IDXGISwapChain* pSwapChain )
{
    HRESULT hr;

    if( pd3dDevice == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D10Device", E_INVALIDARG );

    // Not allowed to call this from inside the device callbacks
    if( GetDXUTState().GetInsideDeviceCallback() )
        return DXUT_ERR_MSGBOX( L"DXUTSetD3D10Device", E_FAIL );

    GetDXUTState().SetDeviceCreateCalled( true );

    // If DXUTCreateWindow() or DXUTSetWindow() has not already been called, 
    // then call DXUTCreateWindow() with the default parameters.         
    if( !GetDXUTState().GetWindowCreated() )
    {
        // If DXUTCreateWindow() or DXUTSetWindow() was already called and failed, then fail.
        // DXUTCreateWindow() or DXUTSetWindow() must first succeed for this function to succeed
        if( GetDXUTState().GetWindowCreateCalled() )
            return E_FAIL;

        // If DXUTCreateWindow() or DXUTSetWindow() hasn't been called, then 
        // automatically call DXUTCreateWindow() with default params
        hr = DXUTCreateWindow();
        if( FAILED( hr ) )
            return hr;
    }

    DXUTDeviceSettings DeviceSettings;
    ZeroMemory( &DeviceSettings, sizeof( DXUTDeviceSettings ) );

    // Get adapter info
    IDXGIDevice* pDXGIDevice = NULL;
    if( SUCCEEDED( hr = pd3dDevice->QueryInterface( __uuidof( *pDXGIDevice ),
                                                    reinterpret_cast< void** >( &pDXGIDevice ) ) ) )
    {
        IDXGIAdapter* pAdapter = NULL;
        hr = pDXGIDevice->GetAdapter( &pAdapter );
        if( SUCCEEDED( hr ) )
        {
            DXGI_ADAPTER_DESC id;
            pAdapter->GetDesc( &id );

            // Find the ordinal by inspecting the enum list.
            DeviceSettings.d3d10.AdapterOrdinal = 0;  // Default
            CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
            CGrowableArray <CD3D10EnumAdapterInfo*>* pAdapterInfoList = pd3dEnum->GetAdapterInfoList();
            for( int i = 0; i < pAdapterInfoList->GetSize(); ++i )
            {
                CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterInfoList->GetAt( i );
                if( !wcscmp( pAdapterInfo->AdapterDesc.Description, id.Description ) )
                {
                    DeviceSettings.d3d10.AdapterOrdinal = i;
                    break;
                }
            }
            SAFE_RELEASE( pAdapter );
        }
        SAFE_RELEASE( pDXGIDevice );
    }

    if( FAILED( hr ) )
        return hr;

    // Get the swap chain description
    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc( &sd );
    DeviceSettings.ver = DXUT_D3D10_DEVICE;
    DeviceSettings.d3d10.sd = sd;

    // Fill out the rest of the device settings struct
    DeviceSettings.d3d10.CreateFlags = 0;
    DeviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
    DeviceSettings.d3d10.Output = 0;
    DeviceSettings.d3d10.PresentFlags = 0;
    DeviceSettings.d3d10.SyncInterval = 0;

    // Change to the Direct3D device passed in
    hr = DXUTChangeDevice( &DeviceSettings, NULL, pd3dDevice, false, false );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Sets the viewport, creates a render target view, and depth scencil texture and view.
//--------------------------------------------------------------------------------------
HRESULT DXUTSetupD3D10Views( ID3D10Device* pd3dDevice, DXUTDeviceSettings* pDeviceSettings )
{
    HRESULT hr = S_OK;
    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    assert( pSwapChain != NULL );
    ID3D10DepthStencilView* pDSV = NULL;
    ID3D10RenderTargetView* pRTV = NULL;

    // Get the back buffer and desc
    ID3D10Texture2D* pBackBuffer;
    hr = pSwapChain->GetBuffer( 0, __uuidof( *pBackBuffer ), ( LPVOID* )&pBackBuffer );
    if( FAILED( hr ) )
        return hr;
    D3D10_TEXTURE2D_DESC backBufferSurfaceDesc;
    pBackBuffer->GetDesc( &backBufferSurfaceDesc );

    // Setup the viewport to match the backbuffer
    D3D10_VIEWPORT vp;
    vp.Width = backBufferSurfaceDesc.Width;
    vp.Height = backBufferSurfaceDesc.Height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    pd3dDevice->RSSetViewports( 1, &vp );

    // Create the render target view
    hr = pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &pRTV );
    SAFE_RELEASE( pBackBuffer );
    if( FAILED( hr ) )
        return hr;
    DXUT_SetDebugName( pRTV, "DXUT" );
    GetDXUTState().SetD3D10RenderTargetView( pRTV );

    if( pDeviceSettings->d3d10.AutoCreateDepthStencil )
    {
        // Create depth stencil texture
        ID3D10Texture2D* pDepthStencil = NULL;
        D3D10_TEXTURE2D_DESC descDepth;
        descDepth.Width = backBufferSurfaceDesc.Width;
        descDepth.Height = backBufferSurfaceDesc.Height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = pDeviceSettings->d3d10.AutoDepthStencilFormat;
        descDepth.SampleDesc.Count = pDeviceSettings->d3d10.sd.SampleDesc.Count;
        descDepth.SampleDesc.Quality = pDeviceSettings->d3d10.sd.SampleDesc.Quality;
        descDepth.Usage = D3D10_USAGE_DEFAULT;
        descDepth.BindFlags = D3D10_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = pd3dDevice->CreateTexture2D( &descDepth, NULL, &pDepthStencil );
        if( FAILED( hr ) )
            return hr;
        DXUT_SetDebugName( pDepthStencil, "DXUT" );
        GetDXUTState().SetD3D10DepthStencil( pDepthStencil );

        // Create the depth stencil view
        D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
        descDSV.Format = descDepth.Format;
        if( descDepth.SampleDesc.Count > 1 )
            descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
        else
            descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = pd3dDevice->CreateDepthStencilView( pDepthStencil, &descDSV, &pDSV );
        if( FAILED( hr ) )
            return hr;
        DXUT_SetDebugName( pDSV, "DXUT" );
        GetDXUTState().SetD3D10DepthStencilView( pDSV );

        // Create a default rasterizer state that enables MSAA
        D3D10_RASTERIZER_DESC RSDesc;
        RSDesc.FillMode = D3D10_FILL_SOLID;
        RSDesc.CullMode = D3D10_CULL_BACK;
        RSDesc.FrontCounterClockwise = FALSE;
        RSDesc.DepthBias = 0;
        RSDesc.SlopeScaledDepthBias = 0.0f;
        RSDesc.DepthBiasClamp= 0;
        RSDesc.DepthClipEnable = TRUE;
        RSDesc.ScissorEnable = FALSE;
        RSDesc.AntialiasedLineEnable = FALSE;
        if( descDepth.SampleDesc.Count > 1 )
            RSDesc.MultisampleEnable = TRUE;
        else
            RSDesc.MultisampleEnable = FALSE;

        ID3D10RasterizerState* pRState = NULL;
        hr = pd3dDevice->CreateRasterizerState( &RSDesc, &pRState );
        if( FAILED( hr ) )
            return hr;
        DXUT_SetDebugName( pRState, "DXUT Default" );

        GetDXUTState().SetD3D10DefaultRasterizerState( pRState );
        pd3dDevice->RSSetState( pRState );
    }

    // Set the render targets
    pDSV = GetDXUTState().GetD3D10DepthStencilView();
    pd3dDevice->OMSetRenderTargets( 1, &pRTV, pDSV );

    return hr;
}


//--------------------------------------------------------------------------------------
// Creates the 3D environment
//--------------------------------------------------------------------------------------
HRESULT DXUTCreate3DEnvironment10( ID3D10Device* pd3d10DeviceFromApp )
{
    HRESULT hr = S_OK;

    ID3D10Device* pd3d10Device = NULL;
    ID3D10Device1* pd3d10Device1 = NULL;
    IDXGISwapChain* pSwapChain = NULL;
    DXUTDeviceSettings* pNewDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    assert( pNewDeviceSettings != NULL );

    IDXGIFactory* pDXGIFactory = DXUTGetDXGIFactory();
    assert( pDXGIFactory != NULL );
    hr = pDXGIFactory->MakeWindowAssociation( NULL, 0 );

    // Only create a Direct3D device if one hasn't been supplied by the app
    if( pd3d10DeviceFromApp == NULL )
    {
        // Try to create the device with the chosen settings
        IDXGIAdapter* pAdapter = NULL;
        HMODULE wrp = NULL;
        if (pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_SOFTWARE) wrp = LoadLibrary(L"D3D10WARP.dll");

        hr = S_OK;
        if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE )
            hr = pDXGIFactory->EnumAdapters( pNewDeviceSettings->d3d10.AdapterOrdinal, &pAdapter );
        if( SUCCEEDED( hr ) )
        {
            // Try creating the D3D10.1 device first
            hr = DXUT_Dynamic_D3D10CreateDevice1( pAdapter, pNewDeviceSettings->d3d10.DriverType, ( HMODULE )wrp,
                                                  pNewDeviceSettings->d3d10.CreateFlags,
                                                  D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &pd3d10Device1 );
            if( SUCCEEDED( hr ) )
            {
                // Get a D3D10.0 device pointer from the D3D10.1 device
                hr = pd3d10Device1->QueryInterface( __uuidof( ID3D10Device ), ( LPVOID* )&pd3d10Device );
                assert( SUCCEEDED( hr ) ); // this should never fail                
            }
            else // if( FAILED(hr) )
            {
                // If D3D10.1 doesn't exist, then fallback to D3D10.0
                hr = DXUT_Dynamic_D3D10CreateDevice( pAdapter, pNewDeviceSettings->d3d10.DriverType, ( HMODULE )wrp,
                                                     pNewDeviceSettings->d3d10.CreateFlags,
                                                     NULL, D3D10_SDK_VERSION, &pd3d10Device );
            }

            if( SUCCEEDED( hr ) )
            {
                if( pNewDeviceSettings->d3d10.DriverType != D3D10_DRIVER_TYPE_HARDWARE )
                {
                    IDXGIDevice* pDXGIDev = NULL;
                    hr = pd3d10Device->QueryInterface( __uuidof( IDXGIDevice ), ( LPVOID* )&pDXGIDev );
                    if( SUCCEEDED( hr ) && pDXGIDev )
                    {
                        pDXGIDev->GetAdapter( &pAdapter );
                    }
                    SAFE_RELEASE( pDXGIDev );
                }

                GetDXUTState().SetD3D10Adapter( pAdapter );
            }
        }

        if( FAILED( hr ) )
        {
            DXUT_ERR( L"D3D10CreateDevice", hr );
            return DXUTERR_CREATINGDEVICE;
        }

        // Enumerate its outputs.
        UINT OutputCount, iOutput;
        for( OutputCount = 0; ; ++OutputCount )
        {
            IDXGIOutput* pOutput;
            if( FAILED( pAdapter->EnumOutputs( OutputCount, &pOutput ) ) )
                break;
            SAFE_RELEASE( pOutput );
        }
        IDXGIOutput** ppOutputArray = new IDXGIOutput*[OutputCount];
        if( !ppOutputArray )
            return E_OUTOFMEMORY;
        for( iOutput = 0; iOutput < OutputCount; ++iOutput )
            pAdapter->EnumOutputs( iOutput, ppOutputArray + iOutput );
        GetDXUTState().SetD3D10OutputArray( ppOutputArray );
        GetDXUTState().SetD3D10OutputArraySize( OutputCount );

        // Create the swapchain
        hr = pDXGIFactory->CreateSwapChain( pd3d10Device, &pNewDeviceSettings->d3d10.sd, &pSwapChain );
        if( FAILED( hr ) )
        {
            DXUT_ERR( L"CreateSwapChain", hr );
            return DXUTERR_CREATINGDEVICE;
        }
    }
    else
    {
        pd3d10DeviceFromApp->AddRef();
        pd3d10Device = pd3d10DeviceFromApp;
    }

    GetDXUTState().SetD3D10Device( pd3d10Device );
    GetDXUTState().SetD3D10Device1( pd3d10Device1 );
    GetDXUTState().SetD3D10SwapChain( pSwapChain );

    // If switching to REF, set the exit code to 10.  If switching to HAL and exit code was 10, then set it back to 0.
    if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE && GetDXUTState().GetExitCode() == 0 )
        GetDXUTState().SetExitCode( 10 );
    else if( pNewDeviceSettings->d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE && GetDXUTState().GetExitCode() == 10 )
        GetDXUTState().SetExitCode( 0 );

    // Update back buffer desc before calling app's device callbacks
    DXUTUpdateBackBufferDesc();

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Update the device stats text
    CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
    assert( pd3dEnum != NULL );
    CD3D10EnumAdapterInfo* pAdapterInfo = pd3dEnum->GetAdapterInfo( pNewDeviceSettings->d3d10.AdapterOrdinal );
    DXUTUpdateD3D10DeviceStats( pNewDeviceSettings->d3d10.DriverType, &pAdapterInfo->AdapterDesc );

    // Call the app's device created callback if non-NULL
    const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D10DEVICECREATED pCallbackDeviceCreated = GetDXUTState().GetD3D10DeviceCreatedFunc();
    hr = S_OK;
    if( pCallbackDeviceCreated != NULL )
        hr = pCallbackDeviceCreated( DXUTGetD3D10Device(), pBackBufferSurfaceDesc,
                                     GetDXUTState().GetD3D10DeviceCreatedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D10Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DeviceCreated callback", hr );
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_CREATINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsCreated( true );

    // Setup the render target view and viewport
    hr = DXUTSetupD3D10Views( pd3d10Device, pNewDeviceSettings );
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DXUTSetupD3D10Views", hr );
        return DXUTERR_CREATINGDEVICEOBJECTS;
    }

    // Create performance counters
    //DXUTCreateD3D10Counters( pd3d10Device );

    // Call the app's swap chain reset callback if non-NULL
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallbackSwapChainResized = GetDXUTState().GetD3D10SwapChainResizedFunc();
    hr = S_OK;
    if( pCallbackSwapChainResized != NULL )
        hr = pCallbackSwapChainResized( DXUTGetD3D10Device(), pSwapChain, pBackBufferSurfaceDesc,
                                        GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( DXUTGetD3D10Device() == NULL ) // Handle DXUTShutdown from inside callback
        return E_FAIL;
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DeviceReset callback", hr );
        return ( hr == DXUTERR_MEDIANOTFOUND ) ? DXUTERR_MEDIANOTFOUND : DXUTERR_RESETTINGDEVICEOBJECTS;
    }
    GetDXUTState().SetDeviceObjectsReset( true );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Resets the 3D environment by:
//      - Calls the device lost callback 
//      - Resets the device
//      - Stores the back buffer description
//      - Sets up the full screen Direct3D cursor if requested
//      - Calls the device reset callback 
//--------------------------------------------------------------------------------------
HRESULT DXUTReset3DEnvironment10()
{
    HRESULT hr;

    GetDXUTState().SetDeviceObjectsReset( false );
    DXUTPause( true, true );

    bool bDeferredDXGIAction = false;
    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    assert( pSwapChain != NULL );

    DXGI_SWAP_CHAIN_DESC SCDesc;
    pSwapChain->GetDesc( &SCDesc );

    // Resize backbuffer and target of the swapchain in case they have changed.
    // For windowed mode, use the client rect as the desired size. Unlike D3D9,
    // we can't use 0 for width or height.  Therefore, fill in the values from
    // the window size. For fullscreen mode, the width and height should have
    // already been filled with the desktop resolution, so don't change it.
    if( pDeviceSettings->d3d10.sd.Windowed && SCDesc.Windowed )
    {
        RECT rcWnd;
        GetClientRect( DXUTGetHWND(), &rcWnd );
        pDeviceSettings->d3d10.sd.BufferDesc.Width = rcWnd.right - rcWnd.left;
        pDeviceSettings->d3d10.sd.BufferDesc.Height = rcWnd.bottom - rcWnd.top;
    }

    // If the app wants to switch from windowed to fullscreen or vice versa,
    // call the swapchain's SetFullscreenState
    // mode.
    if( SCDesc.Windowed != pDeviceSettings->d3d10.sd.Windowed )
    {
        // Set the fullscreen state
        if( pDeviceSettings->d3d10.sd.Windowed )
        {
            V_RETURN( pSwapChain->SetFullscreenState( FALSE, NULL ) );
            bDeferredDXGIAction = true;
        }
        else
        {
            // Set fullscreen state by setting the display mode to fullscreen, then changing the resolution
            // to the desired value.

            // SetFullscreenState causes a WM_SIZE message to be sent to the window.  The WM_SIZE message calls
            // DXUTCheckForDXGIBufferChange which normally stores the new height and width in 
            // pDeviceSettings->d3d10.sd.BufferDesc.  SetDoNotStoreBufferSize tells DXUTCheckForDXGIBufferChange
            // not to store the height and width so that we have the correct values when calling ResizeTarget.

            GetDXUTState().SetDoNotStoreBufferSize( true );
            V_RETURN( pSwapChain->SetFullscreenState( TRUE, NULL ) );
            GetDXUTState().SetDoNotStoreBufferSize( false );

            V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d10.sd.BufferDesc ) );
            DXUTResizeDXGIBuffers( 0, 0, true );

            bDeferredDXGIAction = true;
        }
    }
    else
    {
        if( pDeviceSettings->d3d10.sd.BufferDesc.Width == SCDesc.BufferDesc.Width &&
            pDeviceSettings->d3d10.sd.BufferDesc.Height == SCDesc.BufferDesc.Height &&
            pDeviceSettings->d3d10.sd.BufferDesc.Format != SCDesc.BufferDesc.Format )
        {
            DXUTResizeDXGIBuffers( 0, 0, !pDeviceSettings->d3d10.sd.Windowed );
            bDeferredDXGIAction = true;
        }
        else if( pDeviceSettings->d3d10.sd.BufferDesc.Width != SCDesc.BufferDesc.Width ||
                 pDeviceSettings->d3d10.sd.BufferDesc.Height != SCDesc.BufferDesc.Height )
        {
            V_RETURN( pSwapChain->ResizeTarget( &pDeviceSettings->d3d10.sd.BufferDesc ) );
            bDeferredDXGIAction = true;
        }
    }

    // If no deferred DXGI actions are to take place, mark the device as reset.
    // If there is a deferred DXGI action, then the device isn't reset until DXGI sends us a 
    // window message.  Only then can we mark the device as reset.
    if( !bDeferredDXGIAction )
        GetDXUTState().SetDeviceObjectsReset( true );
    DXUTPause( false, false );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the 3D environment by:
//      - Checking if the device is lost and trying to reset it if it is
//      - Get the elapsed time since the last frame
//      - Calling the app's framemove and render callback
//      - Calling Present()
//--------------------------------------------------------------------------------------
void DXUTRender3DEnvironment10()
{
    HRESULT hr;

    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    if( NULL == pd3dDevice )
        return;

    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
    if( NULL == pSwapChain )
        return;

    if( DXUTIsRenderingPaused() || !DXUTIsActive() || GetDXUTState().GetRenderingOccluded() )
    {
        // Window is minimized/paused/occluded/or not exclusive so yield CPU time to other processes
        Sleep( 50 );
    }

    // Get the app's time, in seconds. Skip rendering if no time elapsed
    double fTime, fAbsTime; float fElapsedTime;
    DXUTGetGlobalTimer()->GetTimeValues( &fTime, &fAbsTime, &fElapsedTime );

    // Store the time for the app
    if( GetDXUTState().GetConstantFrameTime() )
    {
        fElapsedTime = GetDXUTState().GetTimePerFrame();
        fTime = DXUTGetTime() + fElapsedTime;
    }

    GetDXUTState().SetTime( fTime );
    GetDXUTState().SetAbsoluteTime( fAbsTime );
    GetDXUTState().SetElapsedTime( fElapsedTime );

    // Start Performance Counters
    DXUTStartPerformanceCounters();

    // Update the FPS stats
    DXUTUpdateFrameStats();

    DXUTHandleTimers();

    // Animate the scene by calling the app's frame move callback
    LPDXUTCALLBACKFRAMEMOVE pCallbackFrameMove = GetDXUTState().GetFrameMoveFunc();
    if( pCallbackFrameMove != NULL )
    {
        pCallbackFrameMove( fTime, fElapsedTime, GetDXUTState().GetFrameMoveFuncUserContext() );
        pd3dDevice = DXUTGetD3D10Device();
        if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
            return;
    }

    if( !GetDXUTState().GetRenderingPaused() )
    {
        // Render the scene by calling the app's render callback
        LPDXUTCALLBACKD3D10FRAMERENDER pCallbackFrameRender = GetDXUTState().GetD3D10FrameRenderFunc();
        if( pCallbackFrameRender != NULL && !GetDXUTState().GetRenderingOccluded() )
        {
            pCallbackFrameRender( pd3dDevice, fTime, fElapsedTime,
                                  GetDXUTState().GetD3D10FrameRenderFuncUserContext() );
            pd3dDevice = DXUTGetD3D10Device();
            if( NULL == pd3dDevice ) // Handle DXUTShutdown from inside callback
                return;
        }

#if defined(DEBUG) || defined(_DEBUG)
        // The back buffer should always match the client rect 
        // if the Direct3D backbuffer covers the entire window
        RECT rcClient;
        GetClientRect( DXUTGetHWND(), &rcClient );
        if( !IsIconic( DXUTGetHWND() ) )
        {
            GetClientRect( DXUTGetHWND(), &rcClient );
            assert( DXUTGetDXGIBackBufferSurfaceDesc()->Width == (UINT)rcClient.right );
            assert( DXUTGetDXGIBackBufferSurfaceDesc()->Height == (UINT)rcClient.bottom );
        }
#endif
    }

    DWORD dwFlags = 0;
    if( GetDXUTState().GetRenderingOccluded() )
        dwFlags = DXGI_PRESENT_TEST;
    else
        dwFlags = GetDXUTState().GetCurrentDeviceSettings()->d3d10.PresentFlags;
    UINT SyncInterval = GetDXUTState().GetCurrentDeviceSettings()->d3d10.SyncInterval;

    // Show the frame on the primary surface.
    hr = pSwapChain->Present( SyncInterval, dwFlags );
    if( DXGI_STATUS_OCCLUDED == hr )
    {
        // There is a window covering our entire rendering area.
        // Don't render until we're visible again.
        GetDXUTState().SetRenderingOccluded( true );
    }
    else if( DXGI_ERROR_DEVICE_RESET == hr )
    {
        // If a mode change happened, we must reset the device
        if( FAILED( hr = DXUTReset3DEnvironment10() ) )
        {
            if( DXUTERR_RESETTINGDEVICEOBJECTS == hr ||
                DXUTERR_MEDIANOTFOUND == hr )
            {
                DXUTDisplayErrorMessage( hr );
                DXUTShutdown();
                return;
            }
            else
            {
                // Reset failed, but the device wasn't lost so something bad happened, 
                // so recreate the device to try to recover
                DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
                if( FAILED( DXUTChangeDevice( pDeviceSettings, NULL, NULL, true, false ) ) )
                {
                    DXUTShutdown();
                    return;
                }

                // How to handle display orientation changes in full-screen mode?
            }
        }
    }
    else if( DXGI_ERROR_DEVICE_REMOVED == hr )
    {
        // Use a callback to ask the app if it would like to find a new device.  
        // If no device removed callback is set, then look for a new device
        if( FAILED( DXUTHandleDeviceRemoved() ) )
        {
            // Maybe use pD3DDevice->GetDeviceRemovedReason() for more detail?
            DXUTDisplayErrorMessage( DXUTERR_DEVICEREMOVED );
            DXUTShutdown();
            return;
        }
    }
    else if( SUCCEEDED( hr ) )
    {
        if( GetDXUTState().GetRenderingOccluded() )
        {
            // Now that we're no longer occluded
            // allow us to render again
            GetDXUTState().SetRenderingOccluded( false );
        }
    }

    // Update current frame #
    int nFrame = GetDXUTState().GetCurrentFrameNumber();
    nFrame++;
    GetDXUTState().SetCurrentFrameNumber( nFrame );

    // Stop performance counters
    DXUTStopPerformanceCounters();

    // Update the D3D10 counter stats
    DXUTUpdateD3D10CounterStats();

    // Check to see if the app should shutdown due to cmdline
    if( GetDXUTState().GetOverrideQuitAfterFrame() != 0 )
    {
        if( nFrame > GetDXUTState().GetOverrideQuitAfterFrame() )
            DXUTShutdown();
    }

    return;
}


//--------------------------------------------------------------------------------------
// Cleans up the 3D environment by:
//      - Calls the device lost callback 
//      - Calls the device destroyed callback 
//      - Releases the D3D device
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment10( bool bReleaseSettings )
{
    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();

    if( pd3dDevice != NULL )
    {
        if (GetDXUTState().GetD3D10RasterizerState()!= NULL ) 
            GetDXUTState().GetD3D10RasterizerState()->Release();
        // Call ClearState to avoid tons of messy debug spew telling us that we're deleting bound objects
        pd3dDevice->ClearState();

        // Call the app's SwapChain lost callback
        GetDXUTState().SetInsideDeviceCallback( true );
        if( GetDXUTState().GetDeviceObjectsReset() )
        {
            LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallbackSwapChainReleasing =
                GetDXUTState().GetD3D10SwapChainReleasingFunc();
            if( pCallbackSwapChainReleasing != NULL )
                pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainReleasingFuncUserContext() );
            GetDXUTState().SetDeviceObjectsReset( false );
        }

        // Release our old depth stencil texture and view 
        ID3D10Texture2D* pDS = GetDXUTState().GetD3D10DepthStencil();
        SAFE_RELEASE( pDS );
        GetDXUTState().SetD3D10DepthStencil( NULL );
        ID3D10DepthStencilView* pDSV = GetDXUTState().GetD3D10DepthStencilView();
        SAFE_RELEASE( pDSV );
        GetDXUTState().SetD3D10DepthStencilView( NULL );

        // Release our rasterizer state
        ID3D10RasterizerState* pRState = GetDXUTState().GetD3D10DefaultRasterizerState();
        SAFE_RELEASE( pRState );
        GetDXUTState().SetD3D10DefaultRasterizerState( NULL );

        // Cleanup the render target view
        ID3D10RenderTargetView* pRTV = GetDXUTState().GetD3D10RenderTargetView();
        SAFE_RELEASE( pRTV );
        GetDXUTState().SetD3D10RenderTargetView( NULL );

        // Call the app's device destroyed callback
        if( GetDXUTState().GetDeviceObjectsCreated() )
        {
            LPDXUTCALLBACKD3D10DEVICEDESTROYED pCallbackDeviceDestroyed = GetDXUTState().GetD3D10DeviceDestroyedFunc();
            if( pCallbackDeviceDestroyed != NULL )
                pCallbackDeviceDestroyed( GetDXUTState().GetD3D10DeviceDestroyedFuncUserContext() );
            GetDXUTState().SetDeviceObjectsCreated( false );
        }

        GetDXUTState().SetInsideDeviceCallback( false );

        // Release the swap chain
        GetDXUTState().SetReleasingSwapChain( true );
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
        if( pSwapChain )
        {
            pSwapChain->SetFullscreenState( FALSE, 0 );
        }
        SAFE_RELEASE( pSwapChain );
        GetDXUTState().SetD3D10SwapChain( NULL );
        GetDXUTState().SetReleasingSwapChain( false );

        // Release the outputs.
        IDXGIOutput** ppOutputArray = GetDXUTState().GetD3D10OutputArray();
        UINT OutputCount = GetDXUTState().GetD3D10OutputArraySize();
        for( UINT o = 0; o < OutputCount; ++o )
        SAFE_RELEASE( ppOutputArray[o] );
        delete[] ppOutputArray;
        GetDXUTState().SetD3D10OutputArray( NULL );
        GetDXUTState().SetD3D10OutputArraySize( 0 );

        // Release the D3D adapter.
        IDXGIAdapter* pAdapter = GetDXUTState().GetD3D10Adapter();
        SAFE_RELEASE( pAdapter );
        GetDXUTState().SetD3D10Adapter( NULL );

        // Release the counters
        DXUTDestroyD3D10Counters();

        // Release the D3D10.1 device (if it exists) because it has a extra ref count on it
        ID3D10Device1* pd3d10Device1 = GetDXUTState().GetD3D10Device1();
        SAFE_RELEASE( pd3d10Device1 );
        GetDXUTState().SetD3D10Device1( NULL );

        // Release the D3D device and in debug configs, displays a message box if there 
        // are unrelease objects.
        if( pd3dDevice )
        {
            UINT references = pd3dDevice->Release();
            if( references > 0 )
            {
                DXUTDisplayErrorMessage( DXUTERR_NONZEROREFCOUNT );
                DXUT_ERR( L"DXUTCleanup3DEnvironment", DXUTERR_NONZEROREFCOUNT );
            }
        }
        GetDXUTState().SetD3D10Device( NULL );

        if( bReleaseSettings )
        {
            DXUTDeviceSettings* pOldDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
            SAFE_DELETE(pOldDeviceSettings);
            GetDXUTState().SetCurrentDeviceSettings( NULL );
        }

        DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc10();
        ZeroMemory( pBackBufferSurfaceDesc, sizeof( DXGI_SURFACE_DESC ) );

        GetDXUTState().SetDeviceCreated( false );
    }
}

//--------------------------------------------------------------------------------------
// Setup D3D10 counters for various statistics
//--------------------------------------------------------------------------------------
void DXUTCreateD3D10Counters( ID3D10Device* pd3dDevice )
{
    if( GetDXUTState().GetNoStats() )
        return;

    ID3D10Counter* pCounter = NULL;
    D3D10_COUNTER_DESC Desc;
    Desc.MiscFlags = 0;

    Desc.Counter = D3D10_COUNTER_GPU_IDLE;
    HRESULT hr = DXGI_ERROR_UNSUPPORTED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_GPUIdle( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_VERTEX_PROCESSING;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_VertexProcessing( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_GEOMETRY_PROCESSING;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_GeometryProcessing( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_PIXEL_PROCESSING;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_PixelProcessing( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_OTHER_GPU_PROCESSING;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_OtherGPUProcessing( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_HOST_ADAPTER_BANDWIDTH_UTILIZATION;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_HostAdapterBandwidthUtilization( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_LOCAL_VIDMEM_BANDWIDTH_UTILIZATION;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_LocalVidmemBandwidthUtilization( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_VERTEX_THROUGHPUT_UTILIZATION;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_VertexThroughputUtilization( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_TRIANGLE_SETUP_THROUGHPUT_UTILIZATION;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_TriangleSetupThroughputUtilization( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_FILLRATE_THROUGHPUT_UTILIZATION;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_FillrateThrougputUtilization( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_VS_MEMORY_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_VSMemoryLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_VS_COMPUTATION_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_VSComputationLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_GS_MEMORY_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_GSMemoryLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_GS_COMPUTATION_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_GSComputationLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_PS_MEMORY_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_PSMemoryLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_PS_COMPUTATION_LIMITED;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_PSComputationLimited( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_POST_TRANSFORM_CACHE_HIT_RATE;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_PostTransformCacheHitRate( pCounter );
    }

    Desc.Counter = D3D10_COUNTER_TEXTURE_CACHE_HIT_RATE;
    if( SUCCEEDED( hr = pd3dDevice->CreateCounter( &Desc, &pCounter ) ) )
    {
        DXUT_SetDebugName( pCounter, "DXUT" );
        GetDXUTState().SetCounter_TextureCacheHitRate( pCounter );
    }
}

//--------------------------------------------------------------------------------------
void DXUTDestroyD3D10Counters()
{
    ID3D10Counter* pCounter = GetDXUTState().GetCounter_GPUIdle();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_GPUIdle( NULL );

    pCounter = GetDXUTState().GetCounter_VertexProcessing();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_VertexProcessing( NULL );

    pCounter = GetDXUTState().GetCounter_GeometryProcessing();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_GeometryProcessing( NULL );

    pCounter = GetDXUTState().GetCounter_PixelProcessing();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_PixelProcessing( NULL );

    pCounter = GetDXUTState().GetCounter_OtherGPUProcessing();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_OtherGPUProcessing( NULL );

    pCounter = GetDXUTState().GetCounter_HostAdapterBandwidthUtilization();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_HostAdapterBandwidthUtilization( NULL );

    pCounter = GetDXUTState().GetCounter_LocalVidmemBandwidthUtilization();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_LocalVidmemBandwidthUtilization( NULL );

    pCounter = GetDXUTState().GetCounter_VertexThroughputUtilization();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_VertexThroughputUtilization( NULL );

    pCounter = GetDXUTState().GetCounter_TriangleSetupThroughputUtilization();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_TriangleSetupThroughputUtilization( NULL );

    pCounter = GetDXUTState().GetCounter_FillrateThrougputUtilization();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_FillrateThrougputUtilization( NULL );

    pCounter = GetDXUTState().GetCounter_VSMemoryLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_VSMemoryLimited( NULL );

    pCounter = GetDXUTState().GetCounter_VSComputationLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_VSComputationLimited( NULL );

    pCounter = GetDXUTState().GetCounter_GSMemoryLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_GSMemoryLimited( NULL );

    pCounter = GetDXUTState().GetCounter_GSComputationLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_GSComputationLimited( NULL );

    pCounter = GetDXUTState().GetCounter_PSMemoryLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_PSMemoryLimited( NULL );

    pCounter = GetDXUTState().GetCounter_PSComputationLimited();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_PSComputationLimited( NULL );

    pCounter = GetDXUTState().GetCounter_PostTransformCacheHitRate();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_PostTransformCacheHitRate( NULL );

    pCounter = GetDXUTState().GetCounter_TextureCacheHitRate();
    SAFE_RELEASE( pCounter );
    GetDXUTState().SetCounter_TextureCacheHitRate( NULL );
}

//--------------------------------------------------------------------------------------
void DXUTStartPerformanceCounters()
{
    if( GetDXUTState().GetNoStats() )
        return;

    ID3D10Counter* pCounter = GetDXUTState().GetCounter_GPUIdle();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_VertexProcessing();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_GeometryProcessing();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_PixelProcessing();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_OtherGPUProcessing();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_HostAdapterBandwidthUtilization();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_LocalVidmemBandwidthUtilization();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_VertexThroughputUtilization();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_TriangleSetupThroughputUtilization();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_FillrateThrougputUtilization();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_VSMemoryLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_VSComputationLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_GSMemoryLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_GSComputationLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_PSMemoryLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_PSComputationLimited();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_PostTransformCacheHitRate();
    if( pCounter )
        pCounter->Begin();

    pCounter = GetDXUTState().GetCounter_TextureCacheHitRate();
    if( pCounter )
        pCounter->Begin();
}

//--------------------------------------------------------------------------------------
void DXUTStopPerformanceCounters()
{
    if( GetDXUTState().GetNoStats() )
        return;

    ID3D10Counter* pCounter = GetDXUTState().GetCounter_GPUIdle();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_VertexProcessing();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_GeometryProcessing();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_PixelProcessing();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_OtherGPUProcessing();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_HostAdapterBandwidthUtilization();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_LocalVidmemBandwidthUtilization();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_VertexThroughputUtilization();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_TriangleSetupThroughputUtilization();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_FillrateThrougputUtilization();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_VSMemoryLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_VSComputationLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_GSMemoryLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_GSComputationLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_PSMemoryLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_PSComputationLimited();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_PostTransformCacheHitRate();
    if( pCounter )
        pCounter->End();

    pCounter = GetDXUTState().GetCounter_TextureCacheHitRate();
    if( pCounter )
        pCounter->End();
}

//--------------------------------------------------------------------------------------
void DXUTUpdateD3D10CounterStats()
{
    if( GetDXUTState().GetNoStats() )
        return;

    D3D10_COUNTERS* pCounterData = GetDXUTState().GetCounterData();

    float fData;
    ID3D10Counter* pCounter = GetDXUTState().GetCounter_GPUIdle();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fGPUIdle = fData;

    pCounter = GetDXUTState().GetCounter_VertexProcessing();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fVertexProcessing = fData;

    pCounter = GetDXUTState().GetCounter_GeometryProcessing();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fGeometryProcessing = fData;

    pCounter = GetDXUTState().GetCounter_PixelProcessing();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fPixelProcessing = fData;

    pCounter = GetDXUTState().GetCounter_OtherGPUProcessing();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fOtherGPUProcessing = fData;

    pCounter = GetDXUTState().GetCounter_HostAdapterBandwidthUtilization();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fHostAdapterBandwidthUtilization = fData;

    pCounter = GetDXUTState().GetCounter_LocalVidmemBandwidthUtilization();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fLocalVidmemBandwidthUtilization = fData;

    pCounter = GetDXUTState().GetCounter_VertexThroughputUtilization();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fVertexThroughputUtilization = fData;

    pCounter = GetDXUTState().GetCounter_TriangleSetupThroughputUtilization();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fTriangleSetupThroughputUtilization = fData;

    pCounter = GetDXUTState().GetCounter_FillrateThrougputUtilization();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fFillrateThroughputUtilization = fData;

    pCounter = GetDXUTState().GetCounter_VSMemoryLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fVSMemoryLimited = fData;

    pCounter = GetDXUTState().GetCounter_VSComputationLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fVSComputationLimited = fData;

    pCounter = GetDXUTState().GetCounter_GSMemoryLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fGSMemoryLimited = fData;

    pCounter = GetDXUTState().GetCounter_GSComputationLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fGSComputationLimited = fData;

    pCounter = GetDXUTState().GetCounter_PSMemoryLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fPSMemoryLimited = fData;

    pCounter = GetDXUTState().GetCounter_PSComputationLimited();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fPSComputationLimited = fData;

    pCounter = GetDXUTState().GetCounter_PostTransformCacheHitRate();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fPostTransformCacheHitRate = fData;

    pCounter = GetDXUTState().GetCounter_TextureCacheHitRate();
    if( pCounter )
        if( SUCCEEDED( pCounter->GetData( &fData, sizeof( float ), D3D10_ASYNC_GETDATA_DONOTFLUSH ) ) )
            pCounterData->fTextureCacheHitRate = fData;

    // plop everything into a string
    WCHAR* strStats = GetDXUTState().GetD3D10CounterStats();
    static WCHAR szFormat[] = L"GPUIdle: %f\n"\
        L"VertexProcessing: %f\n"\
        L"GeometryProcessing: %f\n"\
        L"PixelProcessing: %f\n"\
        L"OtherGPUProcessing: %f\n"\
        L"HostAdapterBandwidthUtilization: %f\n"\
        L"LocalVidmemBandwidthUtilization: %f\n"\
        L"VertexThroughputUtilization: %f\n"\
        L"TriangleSetupThroughputUtilization: %f\n"\
        L"FillrateThroughputUtilization: %f\n"\
        L"VSMemoryLimited: %f\n"\
        L"VSComputationLimited: %f\n"\
        L"GSMemoryLimited: %f\n"\
        L"GSComputationLimited: %f\n"\
        L"PSMemoryLimited: %f\n"\
        L"PSComputationLimited: %f\n"\
        L"PostTransformCacheHitRate: %f\n"\
        L"TextureCacheHitRate: %f\n";

    swprintf_s( strStats, DXUT_COUNTER_STAT_LENGTH, szFormat,
                     pCounterData->fGPUIdle,
                     pCounterData->fVertexProcessing,
                     pCounterData->fGeometryProcessing,
                     pCounterData->fPixelProcessing,
                     pCounterData->fOtherGPUProcessing,
                     pCounterData->fHostAdapterBandwidthUtilization,
                     pCounterData->fLocalVidmemBandwidthUtilization,
                     pCounterData->fVertexThroughputUtilization,
                     pCounterData->fTriangleSetupThroughputUtilization,
                     pCounterData->fFillrateThroughputUtilization,
                     pCounterData->fVSMemoryLimited,
                     pCounterData->fVSComputationLimited,
                     pCounterData->fGSMemoryLimited,
                     pCounterData->fGSComputationLimited,
                     pCounterData->fPSMemoryLimited,
                     pCounterData->fPSComputationLimited,
                     pCounterData->fPostTransformCacheHitRate,
                     pCounterData->fTextureCacheHitRate );
}

//--------------------------------------------------------------------------------------
// Low level keyboard hook to disable Windows key to prevent accidental task switching.  
//--------------------------------------------------------------------------------------
LRESULT CALLBACK DXUTLowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
    if( nCode < 0 || nCode != HC_ACTION )  // do not process message 
        return CallNextHookEx( GetDXUTState().GetKeyboardHook(), nCode, wParam, lParam );

    bool bEatKeystroke = false;
    KBDLLHOOKSTRUCT* p = ( KBDLLHOOKSTRUCT* )lParam;
    switch( wParam )
    {
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                bEatKeystroke = ( !GetDXUTState().GetAllowShortcutKeys() &&
                                  ( p->vkCode == VK_LWIN || p->vkCode == VK_RWIN ) );
                break;
            }
    }

    if( bEatKeystroke )
        return 1;
    else
        return CallNextHookEx( GetDXUTState().GetKeyboardHook(), nCode, wParam, lParam );
}



//--------------------------------------------------------------------------------------
// Controls how DXUT behaves when fullscreen and windowed mode with regard to 
// shortcut keys (Windows keys, StickyKeys shortcut, ToggleKeys shortcut, FilterKeys shortcut) 
//--------------------------------------------------------------------------------------
void WINAPI DXUTSetShortcutKeySettings( bool bAllowWhenFullscreen, bool bAllowWhenWindowed )
{
    GetDXUTState().SetAllowShortcutKeysWhenWindowed( bAllowWhenWindowed );
    GetDXUTState().SetAllowShortcutKeysWhenFullscreen( bAllowWhenFullscreen );

    // DXUTInit() records initial accessibility states so don't change them until then
    if( GetDXUTState().GetDXUTInited() )
    {
        if( DXUTIsWindowed() )
            DXUTAllowShortcutKeys( GetDXUTState().GetAllowShortcutKeysWhenWindowed() );
        else
            DXUTAllowShortcutKeys( GetDXUTState().GetAllowShortcutKeysWhenFullscreen() );
    }
}


//--------------------------------------------------------------------------------------
// Enables/disables Windows keys, and disables or restores the StickyKeys/ToggleKeys/FilterKeys 
// shortcut to help prevent accidental task switching
//--------------------------------------------------------------------------------------
void DXUTAllowShortcutKeys( bool bAllowKeys )
{
    GetDXUTState().SetAllowShortcutKeys( bAllowKeys );

    if( bAllowKeys )
    {
        // Restore StickyKeys/etc to original state and enable Windows key      
        STICKYKEYS sk = GetDXUTState().GetStartupStickyKeys();
        TOGGLEKEYS tk = GetDXUTState().GetStartupToggleKeys();
        FILTERKEYS fk = GetDXUTState().GetStartupFilterKeys();

        SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof( STICKYKEYS ), &sk, 0 );
        SystemParametersInfo( SPI_SETTOGGLEKEYS, sizeof( TOGGLEKEYS ), &tk, 0 );
        SystemParametersInfo( SPI_SETFILTERKEYS, sizeof( FILTERKEYS ), &fk, 0 );

        // Remove the keyboard hoook when it isn't needed to prevent any slow down of other apps
        if( GetDXUTState().GetKeyboardHook() )
        {
            UnhookWindowsHookEx( GetDXUTState().GetKeyboardHook() );
            GetDXUTState().SetKeyboardHook( NULL );
        }
    }
    else
    {
        // Set low level keyboard hook if haven't already
        if( GetDXUTState().GetKeyboardHook() == NULL )
        {
            // Set the low-level hook procedure.  Only works on Windows 2000 and above
            OSVERSIONINFO OSVersionInfo;
            OSVersionInfo.dwOSVersionInfoSize = sizeof( OSVersionInfo );
            GetVersionEx( &OSVersionInfo );
            if( OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT && OSVersionInfo.dwMajorVersion > 4 )
            {
                HHOOK hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD_LL, DXUTLowLevelKeyboardProc,
                                                        GetModuleHandle( NULL ), 0 );
                GetDXUTState().SetKeyboardHook( hKeyboardHook );
            }
        }

        // Disable StickyKeys/etc shortcuts but if the accessibility feature is on, 
        // then leave the settings alone as its probably being usefully used

        STICKYKEYS skOff = GetDXUTState().GetStartupStickyKeys();
        if( ( skOff.dwFlags & SKF_STICKYKEYSON ) == 0 )
        {
            // Disable the hotkey and the confirmation
            skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
            skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;

            SystemParametersInfo( SPI_SETSTICKYKEYS, sizeof( STICKYKEYS ), &skOff, 0 );
        }

        TOGGLEKEYS tkOff = GetDXUTState().GetStartupToggleKeys();
        if( ( tkOff.dwFlags & TKF_TOGGLEKEYSON ) == 0 )
        {
            // Disable the hotkey and the confirmation
            tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
            tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;

            SystemParametersInfo( SPI_SETTOGGLEKEYS, sizeof( TOGGLEKEYS ), &tkOff, 0 );
        }

        FILTERKEYS fkOff = GetDXUTState().GetStartupFilterKeys();
        if( ( fkOff.dwFlags & FKF_FILTERKEYSON ) == 0 )
        {
            // Disable the hotkey and the confirmation
            fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
            fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;

            SystemParametersInfo( SPI_SETFILTERKEYS, sizeof( FILTERKEYS ), &fkOff, 0 );
        }
    }
}


//--------------------------------------------------------------------------------------
// Pauses time or rendering.  Keeps a ref count so pausing can be layered
//--------------------------------------------------------------------------------------
void WINAPI DXUTPause( bool bPauseTime, bool bPauseRendering )
{
    int nPauseTimeCount = GetDXUTState().GetPauseTimeCount();
    if( bPauseTime ) nPauseTimeCount++;
    else
        nPauseTimeCount--;
    if( nPauseTimeCount < 0 ) nPauseTimeCount = 0;
    GetDXUTState().SetPauseTimeCount( nPauseTimeCount );

    int nPauseRenderingCount = GetDXUTState().GetPauseRenderingCount();
    if( bPauseRendering ) nPauseRenderingCount++;
    else
        nPauseRenderingCount--;
    if( nPauseRenderingCount < 0 ) nPauseRenderingCount = 0;
    GetDXUTState().SetPauseRenderingCount( nPauseRenderingCount );

    if( nPauseTimeCount > 0 )
    {
        // Stop the scene from animating
        DXUTGetGlobalTimer()->Stop();
    }
    else
    {
        // Restart the timer
        DXUTGetGlobalTimer()->Start();
    }

    GetDXUTState().SetRenderingPaused( nPauseRenderingCount > 0 );
    GetDXUTState().SetTimePaused( nPauseTimeCount > 0 );
}


//--------------------------------------------------------------------------------------
// Starts a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTSetTimer( LPDXUTCALLBACKTIMER pCallbackTimer, float fTimeoutInSecs, UINT* pnIDEvent,
                             void* pCallbackUserContext )
{
    if( pCallbackTimer == NULL )
        return DXUT_ERR_MSGBOX( L"DXUTSetTimer", E_INVALIDARG );

    HRESULT hr;
    DXUT_TIMER DXUTTimer;
    DXUTTimer.pCallbackTimer = pCallbackTimer;
    DXUTTimer.pCallbackUserContext = pCallbackUserContext;
    DXUTTimer.fTimeoutInSecs = fTimeoutInSecs;
    DXUTTimer.fCountdown = fTimeoutInSecs;
    DXUTTimer.bEnabled = true;
    DXUTTimer.nID = GetDXUTState().GetTimerLastID() + 1;
    GetDXUTState().SetTimerLastID( DXUTTimer.nID );

    CGrowableArray <DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
    {
        pTimerList = new CGrowableArray <DXUT_TIMER>;
        if( pTimerList == NULL )
            return E_OUTOFMEMORY;
        GetDXUTState().SetTimerList( pTimerList );
    }

    if( FAILED( hr = pTimerList->Add( DXUTTimer ) ) )
        return hr;

    if( pnIDEvent )
        *pnIDEvent = DXUTTimer.nID;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Stops a user defined timer callback
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTKillTimer( UINT nIDEvent )
{
    CGrowableArray <DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
        return S_FALSE;

    bool bFound = false;

    for( int i = 0; i < pTimerList->GetSize(); i++ )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt( i );
        if( DXUTTimer.nID == nIDEvent )
        {
            DXUTTimer.bEnabled = false;
            pTimerList->SetAt( i, DXUTTimer );
            bFound = true;
            break;
        }
    }

    if( !bFound )
        return DXUT_ERR_MSGBOX( L"DXUTKillTimer", E_INVALIDARG );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Internal helper function to handle calling the user defined timer callbacks
//--------------------------------------------------------------------------------------
void DXUTHandleTimers()
{
    float fElapsedTime = DXUTGetElapsedTime();

    CGrowableArray <DXUT_TIMER>* pTimerList = GetDXUTState().GetTimerList();
    if( pTimerList == NULL )
        return;

    // Walk through the list of timer callbacks
    for( int i = 0; i < pTimerList->GetSize(); i++ )
    {
        DXUT_TIMER DXUTTimer = pTimerList->GetAt( i );
        if( DXUTTimer.bEnabled )
        {
            DXUTTimer.fCountdown -= fElapsedTime;

            // Call the callback if count down expired
            if( DXUTTimer.fCountdown < 0 )
            {
                DXUTTimer.pCallbackTimer( DXUTTimer.nID, DXUTTimer.pCallbackUserContext );
                // The callback my have changed the timer.
                DXUTTimer = pTimerList->GetAt( i );
                DXUTTimer.fCountdown = DXUTTimer.fTimeoutInSecs;
            }
            pTimerList->SetAt( i, DXUTTimer );
        }
    }
}


//--------------------------------------------------------------------------------------
// Display an custom error msg box 
//--------------------------------------------------------------------------------------
void DXUTDisplayErrorMessage( HRESULT hr )
{
    WCHAR strBuffer[512];

    int nExitCode;
    bool bFound = true;
    switch( hr )
    {
        case DXUTERR_NODIRECT3D:
        {
            nExitCode = 2;
            if( DXUTDoesAppSupportD3D10() && !DXUTDoesAppSupportD3D9() )
                wcscpy_s( strBuffer,
                               ARRAYSIZE(strBuffer),
                               L"Could not initialize Direct3D 10. This application requires a Direct3D 10 class\ndevice (hardware or reference rasterizer) running on Windows Vista (or later)." );
            else
                wcscpy_s( strBuffer,
                               ARRAYSIZE(strBuffer),
                               L"Could not initialize Direct3D 9. Check that the latest version of DirectX is correctly installed on your system.  Also make sure that this program was compiled with header files that match the installed DirectX DLLs." );
            break;
        }
        case DXUTERR_NOCOMPATIBLEDEVICES:
            nExitCode = 3;
            if( GetSystemMetrics( 0x1000 ) != 0 ) // SM_REMOTESESSION
                wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Direct3D does not work over a remote session." );
            else
                wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Could not find any compatible Direct3D devices." );
            break;
        case DXUTERR_MEDIANOTFOUND:
            nExitCode = 4; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Could not find required media." ); break;
        case DXUTERR_NONZEROREFCOUNT:
            nExitCode = 5;
            wcscpy_s( strBuffer,
                           ARRAYSIZE(strBuffer),
                           L"The Direct3D device has a non-zero reference count, meaning some objects were not released." ); break;
        case DXUTERR_CREATINGDEVICE:
            nExitCode = 6; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Failed creating the Direct3D device." );
            break;
        case DXUTERR_RESETTINGDEVICE:
            nExitCode = 7; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"Failed resetting the Direct3D device." );
            break;
        case DXUTERR_CREATINGDEVICEOBJECTS:
            nExitCode = 8;
            wcscpy_s( strBuffer,
                           ARRAYSIZE(strBuffer), L"An error occurred in the device create callback function." ); break;
        case DXUTERR_RESETTINGDEVICEOBJECTS:
            nExitCode = 9;
            wcscpy_s( strBuffer,
                           ARRAYSIZE(strBuffer), L"An error occurred in the device reset callback function." ); break;
            // nExitCode 10 means the app exited using a REF device 
        case DXUTERR_DEVICEREMOVED:
            nExitCode = 11; wcscpy_s( strBuffer, ARRAYSIZE(strBuffer), L"The Direct3D device was removed." );
            break;
        default:
            bFound = false; nExitCode = 1; break; // nExitCode 1 means the API was incorrectly called

    }

    GetDXUTState().SetExitCode( nExitCode );

    bool bShowMsgBoxOnError = GetDXUTState().GetShowMsgBoxOnError();
    if( bFound && bShowMsgBoxOnError )
    {
        if( DXUTGetWindowTitle()[0] == 0 )
            MessageBox( DXUTGetHWND(), strBuffer, L"DXUT Application", MB_ICONERROR | MB_OK );
        else
            MessageBox( DXUTGetHWND(), strBuffer, DXUTGetWindowTitle(), MB_ICONERROR | MB_OK );
    }
}


//--------------------------------------------------------------------------------------
// Internal function to map MK_* to an array index
//--------------------------------------------------------------------------------------
int DXUTMapButtonToArrayIndex( BYTE vButton )
{
    switch( vButton )
    {
        case MK_LBUTTON:
            return 0;
        case VK_MBUTTON:
        case MK_MBUTTON:
            return 1;
        case MK_RBUTTON:
            return 2;
        case VK_XBUTTON1:
        case MK_XBUTTON1:
            return 3;
        case VK_XBUTTON2:
        case MK_XBUTTON2:
            return 4;
    }

    return 0;
}



//--------------------------------------------------------------------------------------
// Toggle between full screen and windowed
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleFullScreen()
{
    HRESULT hr;

    // Get the current device settings and flip the windowed state then
    // find the closest valid device settings with this change
    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Togggle windowed/fullscreen bit
    if( DXUTIsD3D9( &deviceSettings ) )
        deviceSettings.d3d9.pp.Windowed = !deviceSettings.d3d9.pp.Windowed;
    else
        deviceSettings.d3d10.sd.Windowed = !deviceSettings.d3d10.sd.Windowed;

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterOrdinal = DXUTMT_PRESERVE_INPUT;
    matchOptions.eDeviceType = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eWindowed = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferFormat = DXUTMT_IGNORE_INPUT;
    matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eRefreshRate = DXUTMT_IGNORE_INPUT;
    matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

    // Go back to previous state

    BOOL bIsWindowed = DXUTGetIsWindowedFromDS( &deviceSettings );
    UINT nWidth = ( bIsWindowed ) ? GetDXUTState().GetWindowBackBufferWidthAtModeChange() :
        GetDXUTState().GetFullScreenBackBufferWidthAtModeChange();
    UINT nHeight = ( bIsWindowed ) ? GetDXUTState().GetWindowBackBufferHeightAtModeChange() :
        GetDXUTState().GetFullScreenBackBufferHeightAtModeChange();

    if( nWidth > 0 && nHeight > 0 )
    {
        matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
        if( DXUTIsD3D9( &deviceSettings ) )
        {
            deviceSettings.d3d9.pp.BackBufferWidth = nWidth;
            deviceSettings.d3d9.pp.BackBufferHeight = nHeight;
        }
        else
        {
            deviceSettings.d3d10.sd.BufferDesc.Width = nWidth;
            deviceSettings.d3d10.sd.BufferDesc.Height = nHeight;
        }
    }
    else
    {
        // No previous data, so just switch to defaults
        matchOptions.eResolution = DXUTMT_IGNORE_INPUT;
    }

    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( SUCCEEDED( hr ) )
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && ( hr != E_ABORT ) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL, NULL, false, false );
            if( FAILED( hr2 ) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Toggle between HAL and WARP
//--------------------------------------------------------------------------------------

HRESULT WINAPI DXUTToggleWARP () {
    HRESULT hr;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Toggle between REF & HAL
    if( DXUTIsCurrentDeviceD3D9() )
    {

	}
    else
    {
        ID3D10SwitchToRef* pD3D10STR = NULL;
        ID3D10Device* pDev = DXUTGetD3D10Device();
        assert( pDev != NULL );
        hr = pDev->QueryInterface( __uuidof( *pD3D10STR ), ( LPVOID* )&pD3D10STR );
        if( SUCCEEDED( hr ) )
        {
            pD3D10STR->SetUseRef( pD3D10STR->GetUseRef() ? FALSE : TRUE );
            SAFE_RELEASE( pD3D10STR );
            return S_OK;
        }

        if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE || deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE)
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_SOFTWARE;
        else if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_SOFTWARE )
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
    }

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterOrdinal = DXUTMT_PRESERVE_INPUT;
    matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    matchOptions.eWindowed = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eAdapterFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( SUCCEEDED( hr ) )
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && ( hr != E_ABORT ) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL, NULL, false, false );
            if( FAILED( hr2 ) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;

}

//--------------------------------------------------------------------------------------
// Toggle between HAL and REF
//--------------------------------------------------------------------------------------
HRESULT WINAPI DXUTToggleREF()
{
    HRESULT hr;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    DXUTDeviceSettings orginalDeviceSettings = DXUTGetDeviceSettings();

    // Toggle between REF & HAL
    if( DXUTIsCurrentDeviceD3D9() )
    {
        if( deviceSettings.d3d9.DeviceType == D3DDEVTYPE_HAL )
            deviceSettings.d3d9.DeviceType = D3DDEVTYPE_REF;
        else if( deviceSettings.d3d9.DeviceType == D3DDEVTYPE_REF )
            deviceSettings.d3d9.DeviceType = D3DDEVTYPE_HAL;
    }
    else
    {
        ID3D10SwitchToRef* pD3D10STR = NULL;
        ID3D10Device* pDev = DXUTGetD3D10Device();
        assert( pDev != NULL );
        hr = pDev->QueryInterface( __uuidof( *pD3D10STR ), ( LPVOID* )&pD3D10STR );
        if( SUCCEEDED( hr ) )
        {
            pD3D10STR->SetUseRef( pD3D10STR->GetUseRef() ? FALSE : TRUE );
            SAFE_RELEASE( pD3D10STR );
            return S_OK;
        }

        if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_HARDWARE || deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_SOFTWARE)
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_REFERENCE;
        else if( deviceSettings.d3d10.DriverType == D3D10_DRIVER_TYPE_REFERENCE )
            deviceSettings.d3d10.DriverType = D3D10_DRIVER_TYPE_HARDWARE;
    }

    DXUTMatchOptions matchOptions;
    matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
    matchOptions.eAdapterOrdinal = DXUTMT_PRESERVE_INPUT;
    matchOptions.eDeviceType = DXUTMT_PRESERVE_INPUT;
    matchOptions.eWindowed = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eAdapterFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
    matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

    hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
    if( SUCCEEDED( hr ) )
    {
        // Create a Direct3D device using the new device settings.  
        // If there is an existing device, then it will either reset or recreate the scene.
        hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );

        // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback so nothing changed
        if( FAILED( hr ) && ( hr != E_ABORT ) )
        {
            // Failed creating device, try to switch back.
            HRESULT hr2 = DXUTChangeDevice( &orginalDeviceSettings, NULL, NULL, false, false );
            if( FAILED( hr2 ) )
            {
                // If this failed, then shutdown
                DXUTShutdown();
            }
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Checks to see if DXGI has switched us out of fullscreen or windowed mode
//--------------------------------------------------------------------------------------
void DXUTCheckForDXGIFullScreenSwitch()
{
    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    if( !DXUTIsD3D9( pDeviceSettings ) )
    {
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();
        assert( pSwapChain != NULL );
        DXGI_SWAP_CHAIN_DESC SCDesc;
        pSwapChain->GetDesc( &SCDesc );

        BOOL bIsWindowed = ( BOOL )DXUTIsWindowed();
        if( bIsWindowed != SCDesc.Windowed )
        {
            pDeviceSettings->d3d10.sd.Windowed = SCDesc.Windowed;

            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();

            if( bIsWindowed )
            {
                GetDXUTState().SetWindowBackBufferWidthAtModeChange( deviceSettings.d3d10.sd.BufferDesc.Width );
                GetDXUTState().SetWindowBackBufferHeightAtModeChange( deviceSettings.d3d10.sd.BufferDesc.Height );
            }
            else
            {
                GetDXUTState().SetFullScreenBackBufferWidthAtModeChange( deviceSettings.d3d10.sd.BufferDesc.Width );
                GetDXUTState().SetFullScreenBackBufferHeightAtModeChange( deviceSettings.d3d10.sd.BufferDesc.Height );
            }
        }
    }
}

void DXUTResizeDXGIBuffers( UINT Width, UINT Height, BOOL bFullScreen )
{
    HRESULT hr = S_OK;
    ID3D10Device* pd3dDevice = DXUTGetD3D10Device();
    RECT rcCurrentClient;
    GetClientRect( DXUTGetHWND(), &rcCurrentClient );

    DXUTDeviceSettings* pDevSettings = GetDXUTState().GetCurrentDeviceSettings();
    assert( pDevSettings != NULL );

    IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();

    // Determine if we're fullscreen
    pDevSettings->d3d10.sd.Windowed = !bFullScreen;

    // Call releasing
    GetDXUTState().SetInsideDeviceCallback( true );
    LPDXUTCALLBACKD3D10SWAPCHAINRELEASING pCallbackSwapChainReleasing = GetDXUTState().GetD3D10SwapChainReleasingFunc
        ();
    if( pCallbackSwapChainReleasing != NULL )
        pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );

    // Release our old depth stencil texture and view 
    ID3D10Texture2D* pDS = GetDXUTState().GetD3D10DepthStencil();
    SAFE_RELEASE( pDS );
    GetDXUTState().SetD3D10DepthStencil( NULL );
    ID3D10DepthStencilView* pDSV = GetDXUTState().GetD3D10DepthStencilView();
    SAFE_RELEASE( pDSV );
    GetDXUTState().SetD3D10DepthStencilView( NULL );

    // Release our old render target view
    ID3D10RenderTargetView* pRTV = GetDXUTState().GetD3D10RenderTargetView();
    SAFE_RELEASE( pRTV );
    GetDXUTState().SetD3D10RenderTargetView( NULL );

    // Release our rasterizer state
    ID3D10RasterizerState* pRState = GetDXUTState().GetD3D10DefaultRasterizerState();
    SAFE_RELEASE( pRState );
    GetDXUTState().SetD3D10DefaultRasterizerState( NULL );

    // Alternate between 0 and DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH when resizing buffers.
    // When in windowed mode, we want 0 since this allows the app to change to the desktop
    // resolution from windowed mode during alt+enter.  However, in fullscreen mode, we want
    // the ability to change display modes from the Device Settings dialog.  Therefore, we
    // want to set the DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH flag.
    UINT Flags = 0;
    if( bFullScreen )
        Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // ResizeBuffers
    V( pSwapChain->ResizeBuffers( pDevSettings->d3d10.sd.BufferCount,
                                      Width,
                                      Height,
                                      pDevSettings->d3d10.sd.BufferDesc.Format,
     Flags ) );

    if( !GetDXUTState().GetDoNotStoreBufferSize() )
    {
        pDevSettings->d3d10.sd.BufferDesc.Width = ( UINT )rcCurrentClient.right;
        pDevSettings->d3d10.sd.BufferDesc.Height = ( UINT )rcCurrentClient.bottom;
    }

    // Save off backbuffer desc
    DXUTUpdateBackBufferDesc();

    // Update the device stats text
    DXUTUpdateStaticFrameStats();

    // Setup the render target view and viewport
    hr = DXUTSetupD3D10Views( pd3dDevice, pDevSettings );
    if( FAILED( hr ) )
    {
        DXUT_ERR( L"DXUTSetupD3D10Views", hr );
        return;
    }

    // Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
    DXUTSetupCursor();

    // Call the app's SwapChain reset callback
    GetDXUTState().SetInsideDeviceCallback( true );
    const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
    LPDXUTCALLBACKD3D10SWAPCHAINRESIZED pCallbackSwapChainResized = GetDXUTState().GetD3D10SwapChainResizedFunc();
    hr = S_OK;
    if( pCallbackSwapChainResized != NULL )
        hr = pCallbackSwapChainResized( pd3dDevice, pSwapChain, pBackBufferSurfaceDesc,
                                        GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
    GetDXUTState().SetInsideDeviceCallback( false );
    if( FAILED( hr ) )
    {
        // If callback failed, cleanup
        DXUT_ERR( L"DeviceResetCallback", hr );
        if( hr != DXUTERR_MEDIANOTFOUND )
            hr = DXUTERR_RESETTINGDEVICEOBJECTS;

        GetDXUTState().SetInsideDeviceCallback( true );
        if( pCallbackSwapChainReleasing != NULL )
            pCallbackSwapChainReleasing( GetDXUTState().GetD3D10SwapChainResizedFuncUserContext() );
        GetDXUTState().SetInsideDeviceCallback( false );
        DXUTPause( false, false );
        PostQuitMessage( 0 );
    }
    else
    {
        GetDXUTState().SetDeviceObjectsReset( true );
        DXUTPause( false, false );
    }
}

//--------------------------------------------------------------------------------------
// Checks if DXGI buffers need to change
//--------------------------------------------------------------------------------------
void DXUTCheckForDXGIBufferChange()
{
    if( DXUTGetDXGISwapChain() != NULL && !GetDXUTState().GetReleasingSwapChain() )
    {
        IDXGISwapChain* pSwapChain = DXUTGetDXGISwapChain();

        // Determine if we're fullscreen
        BOOL bFullScreen;
        pSwapChain->GetFullscreenState( &bFullScreen, NULL );

        DXUTResizeDXGIBuffers( 0, 0, bFullScreen );

        ShowWindow( DXUTGetHWND(), SW_SHOW );
    }
}

//--------------------------------------------------------------------------------------
// Checks if the window client rect has changed and if it has, then reset the device
//--------------------------------------------------------------------------------------
void DXUTCheckForWindowSizeChange()
{
    // Skip the check for various reasons
    if( GetDXUTState().GetIgnoreSizeChange() || !GetDXUTState().GetDeviceCreated() ||
        ( DXUTIsCurrentDeviceD3D9() && !DXUTIsWindowed() ) )
        return;

    DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
    if( DXUTIsD3D9( &deviceSettings ) )
    {
        RECT rcCurrentClient;
        GetClientRect( DXUTGetHWND(), &rcCurrentClient );

        if( ( UINT )rcCurrentClient.right != DXUTGetBackBufferWidthFromDS( &deviceSettings ) ||
            ( UINT )rcCurrentClient.bottom != DXUTGetBackBufferHeightFromDS( &deviceSettings ) )
        {
            // A new window size will require a new backbuffer size size
            // Tell DXUTChangeDevice and D3D to size according to the HWND's client rect
            if( DXUTIsD3D9( &deviceSettings ) ) deviceSettings.d3d9.pp.BackBufferWidth = 0;
            else
                deviceSettings.d3d10.sd.BufferDesc.Width = 0;
            if( DXUTIsD3D9( &deviceSettings ) ) deviceSettings.d3d9.pp.BackBufferHeight = 0;
            else
                deviceSettings.d3d10.sd.BufferDesc.Height = 0;

            DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );
        }
    }
    else
    {
        DXUTCheckForDXGIBufferChange();
    }
}


//--------------------------------------------------------------------------------------
// Checks to see if the HWND changed monitors, and if it did it creates a device 
// from the monitor's adapter and recreates the scene.
//--------------------------------------------------------------------------------------
void DXUTCheckForWindowChangingMonitors()
{
    // Skip this check for various reasons
    if( !GetDXUTState().GetAutoChangeAdapter() ||
        GetDXUTState().GetIgnoreSizeChange() || !GetDXUTState().GetDeviceCreated() || !DXUTIsWindowed() )
        return;

    HRESULT hr;
    HMONITOR hWindowMonitor = DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTOPRIMARY );
    HMONITOR hAdapterMonitor = GetDXUTState().GetAdapterMonitor();
    if( hWindowMonitor != hAdapterMonitor )
    {
        UINT newOrdinal;
        if( SUCCEEDED( DXUTGetAdapterOrdinalFromMonitor( hWindowMonitor, &newOrdinal ) ) )
        {
            // Find the closest valid device settings with the new ordinal
            DXUTDeviceSettings deviceSettings = DXUTGetDeviceSettings();
            if( DXUTIsD3D9( &deviceSettings ) )
            {
                deviceSettings.d3d9.AdapterOrdinal = newOrdinal;
            }
            else
            {
                deviceSettings.d3d10.AdapterOrdinal = newOrdinal;
                UINT newOutput;
                if( SUCCEEDED( DXUTGetOutputOrdinalFromMonitor( hWindowMonitor, &newOutput ) ) )
                    deviceSettings.d3d10.Output = newOutput;
            }

            DXUTMatchOptions matchOptions;
            matchOptions.eAPIVersion = DXUTMT_PRESERVE_INPUT;
            matchOptions.eAdapterOrdinal = DXUTMT_PRESERVE_INPUT;
            matchOptions.eDeviceType = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eWindowed = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eAdapterFormat = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
            matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

            hr = DXUTFindValidDeviceSettings( &deviceSettings, &deviceSettings, &matchOptions );
            if( SUCCEEDED( hr ) )
            {
                // Create a Direct3D device using the new device settings.  
                // If there is an existing device, then it will either reset or recreate the scene.
                hr = DXUTChangeDevice( &deviceSettings, NULL, NULL, false, false );

                // If hr == E_ABORT, this means the app rejected the device settings in the ModifySettingsCallback
                if( hr == E_ABORT )
                {
                    // so nothing changed and keep from attempting to switch adapters next time
                    GetDXUTState().SetAutoChangeAdapter( false );
                }
                else if( FAILED( hr ) )
                {
                    DXUTShutdown();
                    DXUTPause( false, false );
                    return;
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Renders the scene using either D3D9 or D3D10
//--------------------------------------------------------------------------------------
void WINAPI DXUTRender3DEnvironment()
{
    if( DXUTIsCurrentDeviceD3D9() )
        DXUTRender3DEnvironment9();
    else
        DXUTRender3DEnvironment10();
}


//--------------------------------------------------------------------------------------
// Cleans up both the D3D9 and D3D10 3D environment (but only one should be active at a time)
//--------------------------------------------------------------------------------------
void DXUTCleanup3DEnvironment( bool bReleaseSettings )
{
    if( DXUTGetD3D9Device() )
        DXUTCleanup3DEnvironment9( bReleaseSettings );
    if( DXUTGetD3D10Device() )
        DXUTCleanup3DEnvironment10( bReleaseSettings );
}


//--------------------------------------------------------------------------------------
// Returns the HMONITOR attached to an adapter/output
//--------------------------------------------------------------------------------------
HMONITOR DXUTGetMonitorFromAdapter( DXUTDeviceSettings* pDeviceSettings )
{
    if( pDeviceSettings->ver == DXUT_D3D9_DEVICE )
    {
        IDirect3D9* pD3D = DXUTGetD3D9Object();
        assert( pD3D != NULL );
        return pD3D->GetAdapterMonitor( pDeviceSettings->d3d9.AdapterOrdinal );
    }
    else
    {
        CD3D10Enumeration* pD3DEnum = DXUTGetD3D10Enumeration();
        assert( pD3DEnum != NULL );
        CD3D10EnumOutputInfo* pOutputInfo = pD3DEnum->GetOutputInfo( pDeviceSettings->d3d10.AdapterOrdinal,
                                                                     pDeviceSettings->d3d10.Output );
        if( !pOutputInfo )
            return 0;
        return DXUTMonitorFromRect( &pOutputInfo->Desc.DesktopCoordinates, MONITOR_DEFAULTTONEAREST );
    }
}


//--------------------------------------------------------------------------------------
// Look for an adapter ordinal that is tied to a HMONITOR
//--------------------------------------------------------------------------------------
HRESULT DXUTGetAdapterOrdinalFromMonitor( HMONITOR hMonitor, UINT* pAdapterOrdinal )
{
    *pAdapterOrdinal = 0;

    if( DXUTIsCurrentDeviceD3D9() )
    {
        CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
        IDirect3D9* pD3D = DXUTGetD3D9Object();

        CGrowableArray <CD3D9EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
        for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); iAdapter++ )
        {
            CD3D9EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
            HMONITOR hAdapterMonitor = pD3D->GetAdapterMonitor( pAdapterInfo->AdapterOrdinal );
            if( hAdapterMonitor == hMonitor )
            {
                *pAdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                return S_OK;
            }
        }
    }
    else
    {
        // Get the monitor handle information
        MONITORINFOEX mi;
        mi.cbSize = sizeof( MONITORINFOEX );
        DXUTGetMonitorInfo( hMonitor, &mi );

        // Search for this monitor in our enumeration hierarchy.
        CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
        CGrowableArray <CD3D10EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
        for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); ++iAdapter )
        {
            CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
            for( int o = 0; o < pAdapterInfo->outputInfoList.GetSize(); ++o )
            {
                CD3D10EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( o );
                // Convert output device name from MBCS to Unicode
                if( wcsncmp( pOutputInfo->Desc.DeviceName, mi.szDevice, sizeof( mi.szDevice ) / sizeof
                             ( mi.szDevice[0] ) ) == 0 )
                {
                    *pAdapterOrdinal = pAdapterInfo->AdapterOrdinal;
                    return S_OK;
                }
            }
        }
    }

    return E_FAIL;
}

//--------------------------------------------------------------------------------------
// Look for a monitor ordinal that is tied to a HMONITOR (D3D10-only)
//--------------------------------------------------------------------------------------
HRESULT DXUTGetOutputOrdinalFromMonitor( HMONITOR hMonitor, UINT* pOutputOrdinal )
{
    // Get the monitor handle information
    MONITORINFOEX mi;
    mi.cbSize = sizeof( MONITORINFOEX );
    DXUTGetMonitorInfo( hMonitor, &mi );

    // Search for this monitor in our enumeration hierarchy.
    CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
    CGrowableArray <CD3D10EnumAdapterInfo*>* pAdapterList = pd3dEnum->GetAdapterInfoList();
    for( int iAdapter = 0; iAdapter < pAdapterList->GetSize(); ++iAdapter )
    {
        CD3D10EnumAdapterInfo* pAdapterInfo = pAdapterList->GetAt( iAdapter );
        for( int o = 0; o < pAdapterInfo->outputInfoList.GetSize(); ++o )
        {
            CD3D10EnumOutputInfo* pOutputInfo = pAdapterInfo->outputInfoList.GetAt( o );
            DXGI_OUTPUT_DESC Desc;
            pOutputInfo->m_pOutput->GetDesc( &Desc );

            if( hMonitor == Desc.Monitor )
            {
                *pOutputOrdinal = pOutputInfo->Output;
                return S_OK;
            }
        }
    }

    return E_FAIL;
}

//--------------------------------------------------------------------------------------
// This method is called when D3DERR_DEVICEREMOVED is returned from an API.  DXUT
// calls the application's DeviceRemoved callback to inform it of the event.  The
// application returns true if it wants DXUT to look for a closest device to run on.
// If no device is found, or the app returns false, DXUT shuts down.
//--------------------------------------------------------------------------------------
HRESULT DXUTHandleDeviceRemoved()
{
    HRESULT hr = S_OK;

    // Device has been removed. Call the application's callback if set.  If no callback
    // has been set, then just look for a new device
    bool bLookForNewDevice = true;
    LPDXUTCALLBACKDEVICEREMOVED pDeviceRemovedFunc = GetDXUTState().GetDeviceRemovedFunc();
    if( pDeviceRemovedFunc )
        bLookForNewDevice = pDeviceRemovedFunc( GetDXUTState().GetDeviceRemovedFuncUserContext() );

    if( bLookForNewDevice )
    {
        DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();

        DXUTMatchOptions matchOptions;
        matchOptions.eAPIVersion = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterOrdinal = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDeviceType = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eWindowed = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eAdapterFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eVertexProcessing = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eResolution = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eBackBufferCount = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eMultiSample = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eSwapEffect = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eDepthFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eStencilFormat = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentFlags = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.eRefreshRate = DXUTMT_CLOSEST_TO_INPUT;
        matchOptions.ePresentInterval = DXUTMT_CLOSEST_TO_INPUT;

        hr = DXUTFindValidDeviceSettings( pDeviceSettings, pDeviceSettings, &matchOptions );
        if( SUCCEEDED( hr ) )
        {
            // Change to a Direct3D device created from the new device settings
            // that is compatible with the removed device.
            hr = DXUTChangeDevice( pDeviceSettings, NULL, NULL, true, false );
            if( SUCCEEDED( hr ) )
                return S_OK;
        }
    }

    // The app does not wish to continue or continuing is not possible.
    return DXUTERR_DEVICEREMOVED;
}


//--------------------------------------------------------------------------------------
// Stores back buffer surface desc in GetDXUTState().GetBackBufferSurfaceDesc10()
//--------------------------------------------------------------------------------------
void DXUTUpdateBackBufferDesc()
{
    if( DXUTIsCurrentDeviceD3D9() )
    {
        HRESULT hr;
        IDirect3DSurface9* pBackBuffer;
        hr = GetDXUTState().GetD3D9Device()->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
        D3DSURFACE_DESC* pBBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc9();
        ZeroMemory( pBBufferSurfaceDesc, sizeof( D3DSURFACE_DESC ) );
        if( SUCCEEDED( hr ) )
        {
            pBackBuffer->GetDesc( pBBufferSurfaceDesc );
            SAFE_RELEASE( pBackBuffer );
        }
    }
    else
    {
        HRESULT hr;
        ID3D10Texture2D* pBackBuffer;
        IDXGISwapChain* pSwapChain = GetDXUTState().GetD3D10SwapChain();
        assert( pSwapChain != NULL );
        hr = pSwapChain->GetBuffer( 0, __uuidof( *pBackBuffer ), ( LPVOID* )&pBackBuffer );
        DXGI_SURFACE_DESC* pBBufferSurfaceDesc = GetDXUTState().GetBackBufferSurfaceDesc10();
        ZeroMemory( pBBufferSurfaceDesc, sizeof( DXGI_SURFACE_DESC ) );
        if( SUCCEEDED( hr ) )
        {
            D3D10_TEXTURE2D_DESC TexDesc;
            pBackBuffer->GetDesc( &TexDesc );
            pBBufferSurfaceDesc->Width = ( UINT )TexDesc.Width;
            pBBufferSurfaceDesc->Height = ( UINT )TexDesc.Height;
            pBBufferSurfaceDesc->Format = TexDesc.Format;
            pBBufferSurfaceDesc->SampleDesc = TexDesc.SampleDesc;
            SAFE_RELEASE( pBackBuffer );
        }
    }
}


//--------------------------------------------------------------------------------------
// Setup cursor based on current settings (window/fullscreen mode, show cursor state, clip cursor state)
//--------------------------------------------------------------------------------------
void DXUTSetupCursor()
{
    if( DXUTIsCurrentDeviceD3D9() )
    {
        // Show the cursor again if returning to fullscreen 
        IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
        if( !DXUTIsWindowed() && pd3dDevice )
        {
            if( GetDXUTState().GetShowCursorWhenFullScreen() )
            {
                SetCursor( NULL ); // Turn off Windows cursor in full screen mode
                HCURSOR hCursor = ( HCURSOR )( ULONG_PTR )GetClassLongPtr( DXUTGetHWNDDeviceFullScreen(),
                                                                           GCLP_HCURSOR );
                DXUTSetD3D9DeviceCursor( pd3dDevice, hCursor, false );
                DXUTGetD3D9Device()->ShowCursor( true );
            }
            else
            {
                SetCursor( NULL ); // Turn off Windows cursor in full screen mode
                DXUTGetD3D9Device()->ShowCursor( false );
            }
        }

        // Clip cursor if requested
        if( !DXUTIsWindowed() && GetDXUTState().GetClipCursorWhenFullScreen() )
        {
            // Confine cursor to full screen window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceFullScreen(), &rcWindow );
            ClipCursor( &rcWindow );
        }
        else
        {
            ClipCursor( NULL );
        }
    }
    else
    {
        // Clip cursor if requested
        if( !DXUTIsWindowed() && GetDXUTState().GetClipCursorWhenFullScreen() )
        {
            HCURSOR hCursor = ( HCURSOR )( ULONG_PTR )GetClassLongPtr( DXUTGetHWNDDeviceFullScreen(),
                                                                       GCLP_HCURSOR );
            SetCursor( hCursor ); // Reset the cursor in case we turned it off in fullscreen mode d3d9

            // Confine cursor to full screen window
            RECT rcWindow;
            GetWindowRect( DXUTGetHWNDDeviceFullScreen(), &rcWindow );
            ClipCursor( &rcWindow );
        }
        else
        {
            ClipCursor( NULL );
        }
    }
}


//--------------------------------------------------------------------------------------
// Updates the static part of the frame stats so it doesn't have be generated every frame
//--------------------------------------------------------------------------------------
void DXUTUpdateStaticFrameStats()
{
    if( GetDXUTState().GetNoStats() )
        return;

    DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
    if( NULL == pDeviceSettings )
        return;

    if( DXUTIsD3D9( pDeviceSettings ) )
    {
        CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
        if( NULL == pd3dEnum )
            return;

        CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo(
            pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType,
            pDeviceSettings->d3d9.AdapterFormat, pDeviceSettings->d3d9.pp.BackBufferFormat,
            pDeviceSettings->d3d9.pp.Windowed );
        if( NULL == pDeviceSettingsCombo )
            return;

        WCHAR strFmt[100];
        D3DPRESENT_PARAMETERS* pPP = &pDeviceSettings->d3d9.pp;

        if( pDeviceSettingsCombo->AdapterFormat == pDeviceSettingsCombo->BackBufferFormat )
        {
            wcscpy_s( strFmt, 100, DXUTD3DFormatToString( pDeviceSettingsCombo->AdapterFormat, false ) );
        }
        else
        {
            swprintf_s( strFmt, 100, L"backbuf %s, adapter %s",
                             DXUTD3DFormatToString( pDeviceSettingsCombo->BackBufferFormat, false ),
                             DXUTD3DFormatToString( pDeviceSettingsCombo->AdapterFormat, false ) );
        }

        WCHAR strDepthFmt[100];
        if( pPP->EnableAutoDepthStencil )
        {
            swprintf_s( strDepthFmt, 100, L" (%s)", DXUTD3DFormatToString( pPP->AutoDepthStencilFormat, false ) );
        }
        else
        {
            // No depth buffer
            strDepthFmt[0] = 0;
        }

        WCHAR strMultiSample[100];
        switch( pPP->MultiSampleType )
        {
            case D3DMULTISAMPLE_NONMASKABLE:
                wcscpy_s( strMultiSample, 100, L" (Nonmaskable Multisample)" ); break;
            case D3DMULTISAMPLE_NONE:
                wcscpy_s( strMultiSample, 100, L"" ); break;
            default:
                swprintf_s( strMultiSample, 100, L" (%dx Multisample)", pPP->MultiSampleType ); break;
        }

        WCHAR* pstrStaticFrameStats = GetDXUTState().GetStaticFrameStats();
        swprintf_s( pstrStaticFrameStats, 256, L"D3D9 %%sVsync %s (%dx%d), %s%s%s",
                         ( pPP->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE ) ? L"off" : L"on",
                         pPP->BackBufferWidth, pPP->BackBufferHeight,
                         strFmt, strDepthFmt, strMultiSample );
    }
    else
    {
        // D3D10
        CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
        if( NULL == pd3dEnum )
            return;

        CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo(
            pDeviceSettings->d3d10.AdapterOrdinal, pDeviceSettings->d3d10.DriverType, pDeviceSettings->d3d10.Output,
            pDeviceSettings->d3d10.sd.BufferDesc.Format, pDeviceSettings->d3d10.sd.Windowed );
        if( NULL == pDeviceSettingsCombo )
            return;

        WCHAR strFmt[100];

        wcscpy_s( strFmt, 100, DXUTDXGIFormatToString( pDeviceSettingsCombo->BackBufferFormat, false ) );

        WCHAR strMultiSample[100];
        swprintf_s( strMultiSample, 100, L" (MS%u, Q%u)", pDeviceSettings->d3d10.sd.SampleDesc.Count,
                         pDeviceSettings->d3d10.sd.SampleDesc.Quality );

        WCHAR* pstrStaticFrameStats = GetDXUTState().GetStaticFrameStats();
        swprintf_s( pstrStaticFrameStats, 256, L"D3D10 %%sVsync %s (%dx%d), %s%s",
                         ( pDeviceSettings->d3d10.SyncInterval == 0 ) ? L"off" : L"on",
                         pDeviceSettings->d3d10.sd.BufferDesc.Width, pDeviceSettings->d3d10.sd.BufferDesc.Height,
                         strFmt, strMultiSample );
    }
}


//--------------------------------------------------------------------------------------
// Updates the frames/sec stat once per second
//--------------------------------------------------------------------------------------
void DXUTUpdateFrameStats()
{
    if( GetDXUTState().GetNoStats() )
        return;

    // Keep track of the frame count
    double fLastTime = GetDXUTState().GetLastStatsUpdateTime();
    DWORD dwFrames = GetDXUTState().GetLastStatsUpdateFrames();
    double fAbsTime = GetDXUTState().GetAbsoluteTime();
    dwFrames++;
    GetDXUTState().SetLastStatsUpdateFrames( dwFrames );

    // Update the scene stats once per second
    if( fAbsTime - fLastTime > 1.0f )
    {
        float fFPS = ( float )( dwFrames / ( fAbsTime - fLastTime ) );
        GetDXUTState().SetFPS( fFPS );
        GetDXUTState().SetLastStatsUpdateTime( fAbsTime );
        GetDXUTState().SetLastStatsUpdateFrames( 0 );

        WCHAR* pstrFPS = GetDXUTState().GetFPSStats();
        swprintf_s( pstrFPS, 64, L"%0.2f fps ", fFPS );
    }
}

//--------------------------------------------------------------------------------------
// Returns a string describing the current device.  If bShowFPS is true, then
// the string contains the frames/sec.  If "-nostats" was used in 
// the command line, the string will be blank
//--------------------------------------------------------------------------------------
LPCWSTR WINAPI DXUTGetFrameStats( bool bShowFPS )
{
    WCHAR* pstrFrameStats = GetDXUTState().GetFrameStats();
    WCHAR* pstrFPS = ( bShowFPS ) ? GetDXUTState().GetFPSStats() : L"";
    swprintf_s( pstrFrameStats, 256, GetDXUTState().GetStaticFrameStats(), pstrFPS );
    return pstrFrameStats;
}


//--------------------------------------------------------------------------------------
// Updates the string which describes the device 
//--------------------------------------------------------------------------------------
void DXUTUpdateD3D9DeviceStats( D3DDEVTYPE DeviceType, DWORD BehaviorFlags,
                                D3DADAPTER_IDENTIFIER9* pAdapterIdentifier )
{
    if( GetDXUTState().GetNoStats() )
        return;

    // Store device description
    WCHAR* pstrDeviceStats = GetDXUTState().GetDeviceStats();
    if( DeviceType == D3DDEVTYPE_REF )
        wcscpy_s( pstrDeviceStats, 256, L"REF" );
    else if( DeviceType == D3DDEVTYPE_HAL )
        wcscpy_s( pstrDeviceStats, 256, L"HAL" );
    else if( DeviceType == D3DDEVTYPE_SW )
        wcscpy_s( pstrDeviceStats, 256, L"SW" );

    if( BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING &&
        BehaviorFlags & D3DCREATE_PUREDEVICE )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            wcscat_s( pstrDeviceStats, 256, L" (pure hw vp)" );
        else
            wcscat_s( pstrDeviceStats, 256, L" (simulated pure hw vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            wcscat_s( pstrDeviceStats, 256, L" (hw vp)" );
        else
            wcscat_s( pstrDeviceStats, 256, L" (simulated hw vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING )
    {
        if( DeviceType == D3DDEVTYPE_HAL )
            wcscat_s( pstrDeviceStats, 256, L" (mixed vp)" );
        else
            wcscat_s( pstrDeviceStats, 256, L" (simulated mixed vp)" );
    }
    else if( BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING )
    {
        wcscat_s( pstrDeviceStats, 256, L" (sw vp)" );
    }

    if( DeviceType == D3DDEVTYPE_HAL )
    {
        // Be sure not to overflow m_strDeviceStats when appending the adapter 
        // description, since it can be long.  
        wcscat_s( pstrDeviceStats, 256, L": " );

        // Try to get a unique description from the CD3D9EnumDeviceSettingsCombo
        DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
        if( !pDeviceSettings )
            return;

        CD3D9Enumeration* pd3dEnum = DXUTGetD3D9Enumeration();
        CD3D9EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo(
            pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType,
            pDeviceSettings->d3d9.AdapterFormat, pDeviceSettings->d3d9.pp.BackBufferFormat,
            pDeviceSettings->d3d9.pp.Windowed );
        if( pDeviceSettingsCombo )
        {
            wcscat_s( pstrDeviceStats, 256, pDeviceSettingsCombo->pAdapterInfo->szUniqueDescription );
        }
        else
        {
            const int cchDesc = sizeof( pAdapterIdentifier->Description );
            WCHAR szDescription[cchDesc];
            MultiByteToWideChar( CP_ACP, 0, pAdapterIdentifier->Description, -1, szDescription, cchDesc );
            szDescription[cchDesc - 1] = 0;
            wcscat_s( pstrDeviceStats, 256, szDescription );
        }
    }
}


//--------------------------------------------------------------------------------------
// Updates the string which describes the device 
//--------------------------------------------------------------------------------------
void DXUTUpdateD3D10DeviceStats( D3D10_DRIVER_TYPE DeviceType, DXGI_ADAPTER_DESC* pAdapterDesc )
{
    if( GetDXUTState().GetNoStats() )
        return;

    // Store device description
    WCHAR* pstrDeviceStats = GetDXUTState().GetDeviceStats();
    if( DeviceType == D3D10_DRIVER_TYPE_REFERENCE )
        wcscpy_s( pstrDeviceStats, 256, L"REFERENCE" );
    else if( DeviceType == D3D10_DRIVER_TYPE_SOFTWARE )
        wcscpy_s( pstrDeviceStats, 256, L"WARP" );
    else if( DeviceType == D3D10_DRIVER_TYPE_HARDWARE )
        wcscpy_s( pstrDeviceStats, 256, L"HARDWARE" );
    else if( DeviceType == D3D10_DRIVER_TYPE_SOFTWARE )
        wcscpy_s( pstrDeviceStats, 256, L"SOFTWARE" );

    if( DeviceType == D3D10_DRIVER_TYPE_HARDWARE )
    {
        // Be sure not to overflow m_strDeviceStats when appending the adapter 
        // description, since it can be long.  
        wcscat_s( pstrDeviceStats, 256, L": " );

        // Try to get a unique description from the CD3D10EnumDeviceSettingsCombo
        DXUTDeviceSettings* pDeviceSettings = GetDXUTState().GetCurrentDeviceSettings();
        if( !pDeviceSettings )
            return;

        CD3D10Enumeration* pd3dEnum = DXUTGetD3D10Enumeration();
        assert( pd3dEnum != NULL );
        CD3D10EnumDeviceSettingsCombo* pDeviceSettingsCombo = pd3dEnum->GetDeviceSettingsCombo(
            pDeviceSettings->d3d10.AdapterOrdinal, pDeviceSettings->d3d10.DriverType, pDeviceSettings->d3d10.Output,
            pDeviceSettings->d3d10.sd.BufferDesc.Format, pDeviceSettings->d3d10.sd.Windowed );
        if( pDeviceSettingsCombo )
            wcscat_s( pstrDeviceStats, 256, pDeviceSettingsCombo->pAdapterInfo->szUniqueDescription );
        else
            wcscat_s( pstrDeviceStats, 256, pAdapterDesc->Description );
    }
}

//--------------------------------------------------------------------------------------
// Misc functions
//--------------------------------------------------------------------------------------
DXUTDeviceSettings WINAPI DXUTGetDeviceSettings()
{
    // Return a copy of device settings of the current device.  If no device exists yet, then
    // return a blank device settings struct
    DXUTDeviceSettings* pDS = GetDXUTState().GetCurrentDeviceSettings();
    if( pDS )
    {
        return *pDS;
    }
    else
    {
        DXUTDeviceSettings ds;
        ZeroMemory( &ds, sizeof( DXUTDeviceSettings ) );
        return ds;
    }
}

D3DPRESENT_PARAMETERS WINAPI DXUTGetD3D9PresentParameters()
{
    // Return a copy of the present params of the current device.  If no device exists yet, then
    // return blank present params
    DXUTDeviceSettings* pDS = GetDXUTState().GetCurrentDeviceSettings();
    if( pDS )
    {
        return pDS->d3d9.pp;
    }
    else
    {
        D3DPRESENT_PARAMETERS pp;
        ZeroMemory( &pp, sizeof( D3DPRESENT_PARAMETERS ) );
        return pp;
    }
}

bool WINAPI DXUTIsVsyncEnabled()
{
    DXUTDeviceSettings* pDS = GetDXUTState().GetCurrentDeviceSettings();
    if( pDS )
    {
        if( DXUTIsD3D9( pDS ) )
            return ( pDS->d3d9.pp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE );
        else
            return ( pDS->d3d10.SyncInterval == 0 );
    }
    else
    {
        return true;
    }
};

HRESULT WINAPI DXUTGetD3D9DeviceCaps( DXUTDeviceSettings* pDeviceSettings, D3DCAPS9* pCaps )
{
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    assert( pD3D != NULL );
    return pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal, pDeviceSettings->d3d9.DeviceType, pCaps );
}


bool WINAPI DXUTIsKeyDown( BYTE vKey )
{
    bool* bKeys = GetDXUTState().GetKeys();
    if( vKey >= 0xA0 && vKey <= 0xA5 )  // VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU
        return GetAsyncKeyState( vKey ) != 0; // these keys only are tracked via GetAsyncKeyState()
    else if( vKey >= 0x01 && vKey <= 0x06 && vKey != 0x03 ) // mouse buttons (VK_*BUTTON)
        return DXUTIsMouseButtonDown( vKey );
    else
        return bKeys[vKey];
}

bool WINAPI DXUTWasKeyPressed( BYTE vKey )
{
    bool* bLastKeys = GetDXUTState().GetLastKeys();
    bool* bKeys = GetDXUTState().GetKeys();
    GetDXUTState().SetAppCalledWasKeyPressed( true );
    return ( !bLastKeys[vKey] && bKeys[vKey] );
}

bool WINAPI DXUTIsMouseButtonDown( BYTE vButton )
{
    bool* bMouseButtons = GetDXUTState().GetMouseButtons();
    int nIndex = DXUTMapButtonToArrayIndex( vButton );
    return bMouseButtons[nIndex];
}

void WINAPI DXUTSetMultimonSettings( bool bAutoChangeAdapter )
{
    GetDXUTState().SetAutoChangeAdapter( bAutoChangeAdapter );
}

void WINAPI DXUTSetHotkeyHandling( bool bAltEnterToToggleFullscreen, bool bEscapeToQuit, bool bPauseToToggleTimePause )
{
    GetDXUTState().SetHandleEscape( bEscapeToQuit );
    GetDXUTState().SetHandleAltEnter( bAltEnterToToggleFullscreen );
    GetDXUTState().SetHandlePause( bPauseToToggleTimePause );
}

void WINAPI DXUTSetCursorSettings( bool bShowCursorWhenFullScreen, bool bClipCursorWhenFullScreen )
{
    GetDXUTState().SetClipCursorWhenFullScreen( bClipCursorWhenFullScreen );
    GetDXUTState().SetShowCursorWhenFullScreen( bShowCursorWhenFullScreen );
    DXUTSetupCursor();
}

void WINAPI DXUTSetWindowSettings( bool bCallDefWindowProc )
{
    GetDXUTState().SetCallDefWindowProc( bCallDefWindowProc );
}

void WINAPI DXUTSetConstantFrameTime( bool bEnabled, float fTimePerFrame )
{
    if( GetDXUTState().GetOverrideConstantFrameTime() )
    {
        bEnabled = GetDXUTState().GetOverrideConstantFrameTime();
        fTimePerFrame = GetDXUTState().GetOverrideConstantTimePerFrame();
    }
    GetDXUTState().SetConstantFrameTime( bEnabled );
    GetDXUTState().SetTimePerFrame( fTimePerFrame );
}


//--------------------------------------------------------------------------------------
// Resets the state associated with DXUT 
//--------------------------------------------------------------------------------------
void WINAPI DXUTResetFrameworkState()
{
    GetDXUTState().Destroy();
    GetDXUTState().Create();
}


//--------------------------------------------------------------------------------------
// Closes down the window.  When the window closes, it will cleanup everything
//--------------------------------------------------------------------------------------
void WINAPI DXUTShutdown( int nExitCode )
{
    HWND hWnd = DXUTGetHWND();
    if( hWnd != NULL )
        SendMessage( hWnd, WM_CLOSE, 0, 0 );

    GetDXUTState().SetExitCode( nExitCode );

    DXUTCleanup3DEnvironment( true );

    // Restore shortcut keys (Windows key, accessibility shortcuts) to original state
    // This is important to call here if the shortcuts are disabled, 
    // because accessibility setting changes are permanent.
    // This means that if this is not done then the accessibility settings 
    // might not be the same as when the app was started. 
    // If the app crashes without restoring the settings, this is also true so it
    // would be wise to backup/restore the settings from a file so they can be 
    // restored when the crashed app is run again.
    DXUTAllowShortcutKeys( true );

    // Shutdown D3D9
    IDirect3D9* pD3D = GetDXUTState().GetD3D9();
    SAFE_RELEASE( pD3D );
    GetDXUTState().SetD3D9( NULL );

    // Shutdown D3D10
    IDXGIFactory* pDXGIFactory = GetDXUTState().GetDXGIFactory();
    SAFE_RELEASE( pDXGIFactory );
    GetDXUTState().SetDXGIFactory( NULL );

    if( GetDXUTState().GetOverrideRelaunchMCE() )
        DXUTReLaunchMediaCenter();
}

//--------------------------------------------------------------------------------------
// Tells DXUT whether to operate in gamma correct mode
//--------------------------------------------------------------------------------------
void WINAPI DXUTSetIsInGammaCorrectMode( bool bGammaCorrect )
{
    GetDXUTState().SetIsInGammaCorrectMode( bGammaCorrect );
}

