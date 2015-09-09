#ifndef DXFIX_H
#define DXFIX_H


// http://www.gamedev.net/topic/619239-directx-11-mingw-w64/


#include <iostream>
#include <map>
#include <string>


#define __in
#define __out
#define __inout
#define __in_bcount(x)
#define __out_bcount(x)
#define __in_ecount(x)
#define __out_ecount(x)
#define __in_ecount_opt(x)
#define __out_ecount_opt(x)
#define __in_bcount_opt(x)
#define __out_bcount_opt(x)
#define __in_opt
#define __inout_opt
#define __out_opt
#define __out_ecount_part_opt(x,y)
#define __deref_out
#define __deref_out_opt
#define __RPC__deref_out
#define __in_range(x, y)
#define __in_z
#define __in_z_opt
#define __noop {}
#define __reserved


#include <d3dx9math.h>
#include <d3d10.h>


typedef D3DXMATRIX D3DXMATRIXA16 __attribute__ ((aligned (16)));


__CRT_UUID_DECL(ID3D10Device,0x9B7E4C0F,0x342C,0x4106,0xA1,0x9F,0x4F,0x27,0x04,0xF6,0x89,0xF0);
__CRT_UUID_DECL(ID3D10ShaderResourceView,0x9B7E4C07,0x342C,0x4106,0xA1,0x9F,0x4F,0x27,0x04,0xF6,0x89,0xF0);
__CRT_UUID_DECL(ID3D10Texture2D,0x9B7E4C04,0x342C,0x4106,0xA1,0x9F,0x4F,0x27,0x04,0xF6,0x89,0xF0);
__CRT_UUID_DECL(ID3D10SwitchToRef,0x9B7E4E02,0x342C,0x4106,0xA1,0x9F,0x4F,0x27,0x04,0xF6,0x89,0xF0);


#endif  // DXFIX_H
