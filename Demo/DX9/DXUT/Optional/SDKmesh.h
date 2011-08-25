//--------------------------------------------------------------------------------------
// File: SDKMesh.h
//
// Disclaimer:  
//   The SDK Mesh format (.sdkmesh) is not a recommended file format for shipping titles.  
//   It was designed to meet the specific needs of the SDK samples.  Any real-world 
//   applications should avoid this file format in favor of a destination format that 
//   meets the specific needs of the application.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef _SDKMESH_
#define _SDKMESH_

//--------------------------------------------------------------------------------------
// Hard Defines for the various structures
//--------------------------------------------------------------------------------------
#define SDKMESH_FILE_VERSION 101
#define MAX_VERTEX_ELEMENTS 32
#define MAX_VERTEX_STREAMS 16
#define MAX_FRAME_NAME 100
#define MAX_MESH_NAME 100
#define MAX_SUBSET_NAME 100
#define MAX_MATERIAL_NAME 100
#define MAX_TEXTURE_NAME MAX_PATH
#define MAX_MATERIAL_PATH MAX_PATH
#define INVALID_FRAME ((UINT)-1)
#define INVALID_MESH ((UINT)-1)
#define INVALID_MATERIAL ((UINT)-1)
#define INVALID_SUBSET ((UINT)-1)
#define INVALID_ANIMATION_DATA ((UINT)-1)
#define ERROR_RESOURCE_VALUE 1
#define INVALID_SAMPLER_SLOT ((UINT)-1)

template<typename TYPE> BOOL IsErrorResource( TYPE data )
{
    if( ( TYPE )ERROR_RESOURCE_VALUE == data )
        return TRUE;
    return FALSE;
}
//--------------------------------------------------------------------------------------
// Enumerated Types.  These will have mirrors in both D3D9 and D3D10
//--------------------------------------------------------------------------------------
enum SDKMESH_PRIMITIVE_TYPE
{
    PT_TRIANGLE_LIST = 0,
    PT_TRIANGLE_STRIP,
    PT_LINE_LIST,
    PT_LINE_STRIP,
    PT_POINT_LIST,
    PT_TRIANGLE_LIST_ADJ,
    PT_TRIANGLE_STRIP_ADJ,
    PT_LINE_LIST_ADJ,
    PT_LINE_STRIP_ADJ,
};

enum SDKMESH_INDEX_TYPE
{
    IT_16BIT = 0,
    IT_32BIT,
};

enum FRAME_TRANSFORM_TYPE
{
    FTT_RELATIVE = 0,
    FTT_ABSOLUTE,		//This is not currently used but is here to support absolute transformations in the future
};

//--------------------------------------------------------------------------------------
// Structures.  Unions with pointers are forced to 64bit.
//--------------------------------------------------------------------------------------
struct SDKMESH_HEADER
{
    //Basic Info and sizes
    UINT Version;
    BYTE IsBigEndian;
    UINT64 HeaderSize;
    UINT64 NonBufferDataSize;
    UINT64 BufferDataSize;

    //Stats
    UINT NumVertexBuffers;
    UINT NumIndexBuffers;
    UINT NumMeshes;
    UINT NumTotalSubsets;
    UINT NumFrames;
    UINT NumMaterials;

    //Offsets to Data
    UINT64 VertexStreamHeadersOffset;
    UINT64 IndexStreamHeadersOffset;
    UINT64 MeshDataOffset;
    UINT64 SubsetDataOffset;
    UINT64 FrameDataOffset;
    UINT64 MaterialDataOffset;
};

struct SDKMESH_VERTEX_BUFFER_HEADER
{
    UINT64 NumVertices;
    UINT64 SizeBytes;
    UINT64 StrideBytes;
    D3DVERTEXELEMENT9 Decl[MAX_VERTEX_ELEMENTS];
    union
    {
        UINT64 DataOffset;				//(This also forces the union to 64bits)
        IDirect3DVertexBuffer9* pVB9;
#ifdef D3D10_SDK_VERSION
        ID3D10Buffer*			pVB10;
#endif
    };
};

struct SDKMESH_INDEX_BUFFER_HEADER
{
    UINT64 NumIndices;
    UINT64 SizeBytes;
    UINT IndexType;
    union
    {
        UINT64 DataOffset;				//(This also forces the union to 64bits)
        IDirect3DIndexBuffer9* pIB9;
#ifdef D3D10_SDK_VERSION
        ID3D10Buffer*			pIB10;
#endif
    };
};

struct SDKMESH_MESH
{
    char    Name[MAX_MESH_NAME];
    BYTE NumVertexBuffers;
    UINT    VertexBuffers[MAX_VERTEX_STREAMS];
    UINT IndexBuffer;
    UINT NumSubsets;
    UINT NumFrameInfluences; //aka bones

    D3DXVECTOR3 BoundingBoxCenter;
    D3DXVECTOR3 BoundingBoxExtents;

    union
    {
        UINT64 SubsetOffset;	//Offset to list of subsets (This also forces the union to 64bits)
        UINT* pSubsets;	    //Pointer to list of subsets
    };
    union
    {
        UINT64 FrameInfluenceOffset;  //Offset to list of frame influences (This also forces the union to 64bits)
        UINT* pFrameInfluences;      //Pointer to list of frame influences
    };
};

struct SDKMESH_SUBSET
{
    char Name[MAX_SUBSET_NAME];
    UINT MaterialID;
    UINT PrimitiveType;
    UINT64 IndexStart;
    UINT64 IndexCount;
    UINT64 VertexStart;
    UINT64 VertexCount;
};

struct SDKMESH_FRAME
{
    char Name[MAX_FRAME_NAME];
    UINT Mesh;
    UINT ParentFrame;
    UINT ChildFrame;
    UINT SiblingFrame;
    D3DXMATRIX Matrix;
    UINT AnimationDataIndex;		//Used to index which set of keyframes transforms this frame
};

struct SDKMESH_MATERIAL
{
    char    Name[MAX_MATERIAL_NAME];

    // Use MaterialInstancePath
    char    MaterialInstancePath[MAX_MATERIAL_PATH];

    // Or fall back to d3d8-type materials
    char    DiffuseTexture[MAX_TEXTURE_NAME];
    char    NormalTexture[MAX_TEXTURE_NAME];
    char    SpecularTexture[MAX_TEXTURE_NAME];

    D3DXVECTOR4 Diffuse;
    D3DXVECTOR4 Ambient;
    D3DXVECTOR4 Specular;
    D3DXVECTOR4 Emissive;
    FLOAT Power;

    union
    {
        UINT64 Force64_1;			//Force the union to 64bits
        IDirect3DTexture9* pDiffuseTexture9;
#ifdef D3D10_SDK_VERSION
        ID3D10Texture2D*			pDiffuseTexture10;
#endif
    };
    union
    {
        UINT64 Force64_2;			//Force the union to 64bits
        IDirect3DTexture9* pNormalTexture9;
#ifdef D3D10_SDK_VERSION
        ID3D10Texture2D*			pNormalTexture10;
#endif
    };
    union
    {
        UINT64 Force64_3;			//Force the union to 64bits
        IDirect3DTexture9* pSpecularTexture9;
#ifdef D3D10_SDK_VERSION
        ID3D10Texture2D*			pSpecularTexture10;
#endif
    };

    union
    {
        UINT64 Force64_4;			//Force the union to 64bits
#ifdef D3D10_SDK_VERSION
        ID3D10ShaderResourceView*	pDiffuseRV10;
#endif
    };
    union
    {
        UINT64 Force64_5;		    //Force the union to 64bits
#ifdef D3D10_SDK_VERSION
        ID3D10ShaderResourceView*	pNormalRV10;
#endif
    };
    union
    {
        UINT64 Force64_6;			//Force the union to 64bits
#ifdef D3D10_SDK_VERSION
        ID3D10ShaderResourceView*	pSpecularRV10;
#endif
    };

};

struct SDKANIMATION_FILE_HEADER
{
    UINT Version;
    BYTE IsBigEndian;
    UINT FrameTransformType;
    UINT NumFrames;
    UINT NumAnimationKeys;
    UINT AnimationFPS;
    UINT64 AnimationDataSize;
    UINT64 AnimationDataOffset;
};

struct SDKANIMATION_DATA
{
    D3DXVECTOR3 Translation;
    D3DXVECTOR4 Orientation;
    D3DXVECTOR3 Scaling;
};

struct SDKANIMATION_FRAME_DATA
{
    char FrameName[MAX_FRAME_NAME];
    union
    {
        UINT64 DataOffset;
        SDKANIMATION_DATA* pAnimationData;
    };
};

#ifndef _CONVERTER_APP_

//--------------------------------------------------------------------------------------
// AsyncLoading callbacks
//--------------------------------------------------------------------------------------
typedef void ( CALLBACK*LPCREATETEXTUREFROMFILE9 )( IDirect3DDevice9* pDev, char* szFileName,
                                                    IDirect3DTexture9** ppTexture, void* pContext );
typedef void ( CALLBACK*LPCREATEVERTEXBUFFER9 )( IDirect3DDevice9* pDev, IDirect3DVertexBuffer9** ppBuffer,
                                                 UINT iSizeBytes, DWORD Usage, DWORD FVF, D3DPOOL Pool, void* pData,
                                                 void* pContext );
typedef void ( CALLBACK*LPCREATEINDEXBUFFER9 )( IDirect3DDevice9* pDev, IDirect3DIndexBuffer9** ppBuffer,
                                                UINT iSizeBytes, DWORD Usage, D3DFORMAT ibFormat, D3DPOOL Pool,
                                                void* pData, void* pContext );
struct SDKMESH_CALLBACKS9
{
    LPCREATETEXTUREFROMFILE9 pCreateTextureFromFile;
    LPCREATEVERTEXBUFFER9 pCreateVertexBuffer;
    LPCREATEINDEXBUFFER9 pCreateIndexBuffer;
    void* pContext;
};
typedef void ( CALLBACK*LPCREATETEXTUREFROMFILE10 )( ID3D10Device* pDev, char* szFileName,
                                                     ID3D10ShaderResourceView** ppRV, void* pContext );
typedef void ( CALLBACK*LPCREATEVERTEXBUFFER10 )( ID3D10Device* pDev, ID3D10Buffer** ppBuffer,
                                                  D3D10_BUFFER_DESC BufferDesc, void* pData, void* pContext );
typedef void ( CALLBACK*LPCREATEINDEXBUFFER10 )( ID3D10Device* pDev, ID3D10Buffer** ppBuffer,
                                                 D3D10_BUFFER_DESC BufferDesc, void* pData, void* pContext );
struct SDKMESH_CALLBACKS10
{
    LPCREATETEXTUREFROMFILE10 pCreateTextureFromFile;
    LPCREATEVERTEXBUFFER10 pCreateVertexBuffer;
    LPCREATEINDEXBUFFER10 pCreateIndexBuffer;
    void* pContext;
};

//--------------------------------------------------------------------------------------
// CDXUTSDKMesh class.  This class reads the sdkmesh file format for use by the samples
//--------------------------------------------------------------------------------------
class CDXUTSDKMesh
{
private:
    UINT m_NumOutstandingResources;
    bool m_bLoading;
    //BYTE*                         m_pBufferData;
    HANDLE m_hFile;
    HANDLE m_hFileMappingObject;
    CGrowableArray <BYTE*> m_MappedPointers;
    IDirect3DDevice9* m_pDev9;
    ID3D10Device* m_pDev10;

protected:
    //These are the pointers to the two chunks of data loaded in from the mesh file
    BYTE* m_pStaticMeshData;
    BYTE* m_pHeapData;
    BYTE* m_pAnimationData;
    BYTE** m_ppVertices;
    BYTE** m_ppIndices;

    //Keep track of the path
    WCHAR                           m_strPathW[MAX_PATH];
    char                            m_strPath[MAX_PATH];

    //General mesh info
    SDKMESH_HEADER* m_pMeshHeader;
    SDKMESH_VERTEX_BUFFER_HEADER* m_pVertexBufferArray;
    SDKMESH_INDEX_BUFFER_HEADER* m_pIndexBufferArray;
    SDKMESH_MESH* m_pMeshArray;
    SDKMESH_SUBSET* m_pSubsetArray;
    SDKMESH_FRAME* m_pFrameArray;
    SDKMESH_MATERIAL* m_pMaterialArray;

    // Adjacency information (not part of the m_pStaticMeshData, so it must be created and destroyed separately )
    SDKMESH_INDEX_BUFFER_HEADER* m_pAdjacencyIndexBufferArray;

    //Animation (TODO: Add ability to load/track multiple animation sets)
    SDKANIMATION_FILE_HEADER* m_pAnimationHeader;
    SDKANIMATION_FRAME_DATA* m_pAnimationFrameData;
    D3DXMATRIX* m_pBindPoseFrameMatrices;
    D3DXMATRIX* m_pTransformedFrameMatrices;

protected:
    void                            LoadMaterials( ID3D10Device* pd3dDevice, SDKMESH_MATERIAL* pMaterials,
                                                   UINT NumMaterials, SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
    void                            LoadMaterials( IDirect3DDevice9* pd3dDevice, SDKMESH_MATERIAL* pMaterials,
                                                   UINT NumMaterials, SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );
    HRESULT                         CreateVertexBuffer( ID3D10Device* pd3dDevice,
                                                        SDKMESH_VERTEX_BUFFER_HEADER* pHeader, void* pVertices,
                                                        SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
    HRESULT                         CreateVertexBuffer( IDirect3DDevice9* pd3dDevice,
                                                        SDKMESH_VERTEX_BUFFER_HEADER* pHeader, void* pVertices,
                                                        SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );
    HRESULT                         CreateIndexBuffer( ID3D10Device* pd3dDevice, SDKMESH_INDEX_BUFFER_HEADER* pHeader,
                                                       void* pIndices, SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
    HRESULT                         CreateIndexBuffer( IDirect3DDevice9* pd3dDevice,
                                                       SDKMESH_INDEX_BUFFER_HEADER* pHeader, void* pIndices,
                                                       SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );

    virtual HRESULT                 CreateFromFile( ID3D10Device* pDev10, IDirect3DDevice9* pDev9, LPCTSTR szFileName,
                                                    bool bCreateAdjacencyIndices,
                                                    SDKMESH_CALLBACKS10* pLoaderCallbacks10=NULL,
                                                    SDKMESH_CALLBACKS9* pLoaderCallbacks9=NULL );
    virtual HRESULT                 CreateFromMemory( ID3D10Device* pDev10,
                                                      IDirect3DDevice9* pDev9,
                                                      BYTE* pData,
                                                      UINT DataBytes,
                                                      bool bCreateAdjacencyIndices,
                                                      bool bCopyStatic,
                                                      SDKMESH_CALLBACKS10* pLoaderCallbacks10=NULL,
                                                      SDKMESH_CALLBACKS9* pLoaderCallbacks9=NULL );

    //frame manipulation
    void                            TransformBindPoseFrame( UINT iFrame, D3DXMATRIX* pParentWorld );
    void                            TransformFrame( UINT iFrame, D3DXMATRIX* pParentWorld, double fTime );
    void                            TransformFrameAbsolute( UINT iFrame, double fTime );

    //Direct3D 10 rendering helpers
    void                            RenderMesh( UINT iMesh,
                                                bool bAdjacent,
                                                ID3D10Device* pd3dDevice,
                                                ID3D10EffectTechnique* pTechnique,
                                                ID3D10EffectShaderResourceVariable* ptxDiffuse,
                                                ID3D10EffectShaderResourceVariable* ptxNormal,
                                                ID3D10EffectShaderResourceVariable* ptxSpecular,
                                                ID3D10EffectVectorVariable* pvDiffuse,
                                                ID3D10EffectVectorVariable* pvSpecular );
    void                            RenderFrame( UINT iFrame,
                                                 bool bAdjacent,
                                                 ID3D10Device* pd3dDevice,
                                                 ID3D10EffectTechnique* pTechnique,
                                                 ID3D10EffectShaderResourceVariable* ptxDiffuse,
                                                 ID3D10EffectShaderResourceVariable* ptxNormal,
                                                 ID3D10EffectShaderResourceVariable* ptxSpecular,
                                                 ID3D10EffectVectorVariable* pvDiffuse,
                                                 ID3D10EffectVectorVariable* pvSpecular );

    void                                RenderFrame( UINT iFrame,
                                                bool bAdjacent,
                                                ID3D10Device* pd3dDevice,
                                                UINT iDiffuseSlot,
                                                UINT iNormalSlot,
                                                UINT iSpecularSlot );

    void                                RenderMesh( UINT iMesh,
                                               bool bAdjacent,
                                               ID3D10Device* pd3dDevice,
                                               UINT iDiffuseSlot,
                                               UINT iNormalSlot,
                                               UINT iSpecularSlot );
//--------------------------------------------------------------------------------------


    //Direct3D 9 rendering helpers
    void                            RenderMesh( UINT iMesh,
                                                LPDIRECT3DDEVICE9 pd3dDevice,
                                                LPD3DXEFFECT pEffect,
                                                D3DXHANDLE hTechnique,
                                                D3DXHANDLE htxDiffuse,
                                                D3DXHANDLE htxNormal,
                                                D3DXHANDLE htxSpecular );
    void                            RenderFrame( UINT iFrame,
                                                 LPDIRECT3DDEVICE9 pd3dDevice,
                                                 LPD3DXEFFECT pEffect,
                                                 D3DXHANDLE hTechnique,
                                                 D3DXHANDLE htxDiffuse,
                                                 D3DXHANDLE htxNormal,
                                                 D3DXHANDLE htxSpecular );

public:
                                    CDXUTSDKMesh();
    virtual                         ~CDXUTSDKMesh();

    virtual HRESULT                 Create( ID3D10Device* pDev10, LPCTSTR szFileName, bool bCreateAdjacencyIndices=
                                            false, SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
    virtual HRESULT                 Create( IDirect3DDevice9* pDev9, LPCTSTR szFileName, bool bCreateAdjacencyIndices=
                                            false, SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );
    virtual HRESULT                 Create( ID3D10Device* pDev10, BYTE* pData, UINT DataBytes,
                                            bool bCreateAdjacencyIndices=false, bool bCopyStatic=false,
                                            SDKMESH_CALLBACKS10* pLoaderCallbacks=NULL );
    virtual HRESULT                 Create( IDirect3DDevice9* pDev9, BYTE* pData, UINT DataBytes,
                                            bool bCreateAdjacencyIndices=false, bool bCopyStatic=false,
                                            SDKMESH_CALLBACKS9* pLoaderCallbacks=NULL );
    virtual HRESULT                 LoadAnimation( WCHAR* szFileName );
    virtual void                    Destroy();

    //Frame manipulation
    void                            TransformBindPose( D3DXMATRIX* pWorld );
    void                            TransformMesh( D3DXMATRIX* pWorld, double fTime );

    //Adjacency
    HRESULT                         CreateAdjacencyIndices( ID3D10Device* pd3dDevice, float fEpsilon,
                                                            BYTE* pBufferData );

    //Direct3D 10 Rendering
//--------------------------------------------------------------------------------------
    void Render( ID3D10Device* pd3dDevice,
                           UINT iDiffuseSlot,
                           UINT iNormalSlot,
                           UINT iSpecularSlot );
    
    virtual void                    Render( ID3D10Device* pd3dDevice,
                                            ID3D10EffectTechnique* pTechnique,
                                            ID3D10EffectShaderResourceVariable* ptxDiffuse = NULL,
                                            ID3D10EffectShaderResourceVariable* ptxNormal = NULL,
                                            ID3D10EffectShaderResourceVariable* ptxSpecular = NULL,
                                            ID3D10EffectVectorVariable* pvDiffuse = NULL,
                                            ID3D10EffectVectorVariable* pvSpecular = NULL );
    virtual void                    RenderAdjacent( ID3D10Device* pd3dDevice,
                                                    ID3D10EffectTechnique* pTechnique,
                                                    ID3D10EffectShaderResourceVariable* ptxDiffuse = NULL,
                                                    ID3D10EffectShaderResourceVariable* ptxNormal = NULL,
                                                    ID3D10EffectShaderResourceVariable* ptxSpecular = NULL,
                                                    ID3D10EffectVectorVariable* pvDiffuse = NULL,
                                                    ID3D10EffectVectorVariable* pvSpecular = NULL );
    //Direct3D 9 Rendering
    virtual void                    Render( LPDIRECT3DDEVICE9 pd3dDevice,
                                            LPD3DXEFFECT pEffect,
                                            D3DXHANDLE hTechnique,
                                            D3DXHANDLE htxDiffuse = 0,
                                            D3DXHANDLE htxNormal = 0,
                                            D3DXHANDLE htxSpecular = 0 );


    //Helpers (D3D10 specific)
    static D3D10_PRIMITIVE_TOPOLOGY GetPrimitiveType10( SDKMESH_PRIMITIVE_TYPE PrimType );
    DXGI_FORMAT                     GetIBFormat10( UINT iMesh );
    ID3D10Buffer* GetVB10( UINT iMesh, UINT iVB );
    ID3D10Buffer* GetIB10( UINT iMesh );
    ID3D10Buffer* GetAdjIB10( UINT iMesh );
    SDKMESH_INDEX_TYPE GetIndexType( UINT iMesh );

    //Helpers (D3D9 specific)
    static D3DPRIMITIVETYPE         GetPrimitiveType9( SDKMESH_PRIMITIVE_TYPE PrimType );
    D3DFORMAT                       GetIBFormat9( UINT iMesh );
    IDirect3DVertexBuffer9* GetVB9( UINT iMesh, UINT iVB );
    IDirect3DIndexBuffer9* GetIB9( UINT iMesh );

    //Helpers (general)
    char* GetMeshPathA();
    WCHAR* GetMeshPathW();
    UINT                            GetNumMeshes();
    UINT                            GetNumMaterials();
    UINT                            GetNumVBs();
    UINT                            GetNumIBs();
    IDirect3DVertexBuffer9* GetVB9At( UINT iVB );
    IDirect3DIndexBuffer9* GetIB9At( UINT iIB );
    ID3D10Buffer* GetVB10At( UINT iVB );
    ID3D10Buffer* GetIB10At( UINT iIB );
    BYTE* GetRawVerticesAt( UINT iVB );
    BYTE* GetRawIndicesAt( UINT iIB );
    SDKMESH_MATERIAL* GetMaterial( UINT iMaterial );
    SDKMESH_MESH* GetMesh( UINT iMesh );
    UINT                            GetNumSubsets( UINT iMesh );
    SDKMESH_SUBSET* GetSubset( UINT iMesh, UINT iSubset );
    UINT                            GetVertexStride( UINT iMesh, UINT iVB );
    SDKMESH_FRAME* FindFrame( char* pszName );
    UINT64                          GetNumVertices( UINT iMesh, UINT iVB );
    UINT64                          GetNumIndices( UINT iMesh );
    D3DXVECTOR3                     GetMeshBBoxCenter( UINT iMesh );
    D3DXVECTOR3                     GetMeshBBoxExtents( UINT iMesh );
    UINT                            GetOutstandingResources();
    UINT                            GetOutstandingBufferResources();
    bool                            CheckLoadDone();
    bool                            IsLoaded();
    bool                            IsLoading();
    void                            SetLoading( bool bLoading );
    BOOL                            HadLoadingError();

    //Animation
    UINT                            GetNumInfluences( UINT iMesh );
    const D3DXMATRIX* GetMeshInfluenceMatrix( UINT iMesh, UINT iInfluence );
    UINT                            GetAnimationKeyFromTime( double fTime );
};

//-----------------------------------------------------------------------------
// Name: class CDXUTXFileMesh
// Desc: Class for loading and rendering file-based meshes
//-----------------------------------------------------------------------------
class CDXUTXFileMesh
{
public:
    WCHAR       m_strName[512];
    LPD3DXMESH m_pMesh;   // Managed mesh

    // Cache of data in m_pMesh for easy access
    IDirect3DVertexBuffer9* m_pVB;
    IDirect3DIndexBuffer9* m_pIB;
    IDirect3DVertexDeclaration9* m_pDecl;
    DWORD m_dwNumVertices;
    DWORD m_dwNumFaces;
    DWORD m_dwBytesPerVertex;

    DWORD m_dwNumMaterials; // Materials for the mesh
    D3DMATERIAL9* m_pMaterials;
    CHAR        (*m_strMaterials )[MAX_PATH];
    IDirect3DBaseTexture9** m_pTextures;
    bool m_bUseMaterials;

public:
    // Rendering
    HRESULT     Render( LPDIRECT3DDEVICE9 pd3dDevice,
                        bool bDrawOpaqueSubsets = true,
                        bool bDrawAlphaSubsets = true );
    HRESULT     Render( ID3DXEffect* pEffect,
                        D3DXHANDLE hTexture = NULL,
                        D3DXHANDLE hDiffuse = NULL,
                        D3DXHANDLE hAmbient = NULL,
                        D3DXHANDLE hSpecular = NULL,
                        D3DXHANDLE hEmissive = NULL,
                        D3DXHANDLE hPower = NULL,
                        bool bDrawOpaqueSubsets = true,
                        bool bDrawAlphaSubsets = true );

    // Mesh access
    LPD3DXMESH  GetMesh()
    {
        return m_pMesh;
    }

    // Rendering options
    void        UseMeshMaterials( bool bFlag )
    {
        m_bUseMaterials = bFlag;
    }
    HRESULT     SetFVF( LPDIRECT3DDEVICE9 pd3dDevice, DWORD dwFVF );
    HRESULT     SetVertexDecl( LPDIRECT3DDEVICE9 pd3dDevice, const D3DVERTEXELEMENT9* pDecl,
                               bool bAutoComputeNormals = true, bool bAutoComputeTangents = true,
                               bool bSplitVertexForOptimalTangents = false );

    // Initializing
    HRESULT     RestoreDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice );
    HRESULT     InvalidateDeviceObjects();

    // Creation/destruction
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, LPCWSTR strFilename );
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, LPD3DXFILEDATA pFileData );
    HRESULT     Create( LPDIRECT3DDEVICE9 pd3dDevice, ID3DXMesh* pInMesh, D3DXMATERIAL* pd3dxMaterials,
                        DWORD dwMaterials );
    HRESULT     CreateMaterials( LPCWSTR strPath, IDirect3DDevice9* pd3dDevice, D3DXMATERIAL* d3dxMtrls,
                                 DWORD dwNumMaterials );
    HRESULT     Destroy();

                CDXUTXFileMesh( LPCWSTR strName = L"CDXUTXMeshFile_Mesh" );
    virtual     ~CDXUTXFileMesh();
};


#endif

#endif

