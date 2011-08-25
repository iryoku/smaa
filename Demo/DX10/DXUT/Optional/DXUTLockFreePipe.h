//--------------------------------------------------------------------------------------
// DXUTLockFreePipe.h
//
// See the "Lockless Programming Considerations for Xbox 360 and Microsoft Windows"
// article in the DirectX SDK for more details.
//
// http://msdn2.microsoft.com/en-us/library/bb310595.aspx
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef _XBOX_VER
#pragma pack(push)
#pragma pack(8)
#include <windows.h>
#pragma pack (pop)

extern "C"
    void _ReadWriteBarrier();
#pragma intrinsic(_ReadWriteBarrier)
#endif

//
// Pipe class designed for use by at most two threads: one reader, one writer.
// Access by more than two threads isn't guaranteed to be safe. 
// 
// In order to provide efficient access the size of the buffer is passed
// as a template parameter and restricted to powers of two less than 31.
//

template <BYTE cbBufferSizeLog2> class DXUTLockFreePipe
{
public:
    DXUTLockFreePipe() : m_readOffset(0), m_writeOffset(0) {}

    DWORD GetBufferSize() const { return c_cbBufferSize; }

    __forceinline unsigned long BytesAvailable() const
    {
        return m_writeOffset - m_readOffset;
    }

    bool __forceinline          Read( void* pvDest, unsigned long cbDest )
    {
        // Store the read and write offsets into local variables--this is
        // essentially a snapshot of their values so that they stay constant
        // for the duration of the function (and so we don't end up with cache 
        // misses due to false sharing).
        DWORD readOffset = m_readOffset;
        DWORD writeOffset = m_writeOffset;

        // Compare the two offsets to see if we have anything to read.
        // Note that we don't do anything to synchronize the offsets here.
        // Really there's not much we *can* do unless we're willing to completely
        // synchronize access to the entire object. We have to assume that as we 
        // read, someone else may be writing, and the write offset we have now
        // may be out of date by the time we read it. Fortunately that's not a
        // very big deal. We might miss reading some data that was just written.
        // But the assumption is that we'll be back before long to grab more data
        // anyway.
        //
        // Note that this comparison works because we're careful to constrain
        // the total buffer size to be a power of 2, which means it will divide
        // evenly into ULONG_MAX+1. That, and the fact that the offsets are 
        // unsigned, means that the calculation returns correct results even
        // when the values wrap around.
        DWORD cbAvailable = writeOffset - readOffset;
        if( cbDest > cbAvailable )
        {
            return false;
        }

#ifdef _XBOX_VER
        // at this point, since we're still in this function, we've obviously 
        // decided that there's enough data in the buffer for us to do this read.
        // But there's one complication: The data we're looking for might still 
        // be moseying its way on up from the L2 cache. That's a problem, because
        // our CPU has no patience for moseying and would much rather load from
        // the L1 cache if the data is in there. And if it does that, we will be
        // sad because that data will be truly stale. To prevent that from happening,
        // we need to emit an lwsync--that'll cause the CPU to ensure that all 
        // subsequent reads are accessing data that is at least as fresh (with
        // respect to the L2 cache, anyway) as any of the reads that were issued
        // before the lwsync.
        //
        // Technically this little dance is a "read-acquire."
        __lwsync();
#else
        // For x86 and x64, the only hazzard here is that the compiler reorders
        // the operations, so we make sure to tell the compiler not to do that
        _ReadWriteBarrier();
#endif

        unsigned char* pbDest = ( unsigned char* )pvDest;

        unsigned long actualReadOffset = readOffset & c_sizeMask;
        unsigned long bytesLeft = cbDest;

        //
        // Copy from the tail, then the head. Note that there's no explicit
        // check to see if the write offset comes between the read offset
        // and the end of the buffer--that particular condition is implicitly
        // checked by the comparison with AvailableToRead(), above. If copying
        // cbDest bytes off the tail would cause us to cross the write offset,
        // then the previous comparison would have failed since that would imply
        // that there were less than cbDest bytes available to read.
        //
        unsigned long cbTailBytes = min( bytesLeft, c_cbBufferSize - actualReadOffset );
#ifdef _XBOX_VER
        XMemCpy( pbDest, m_pbBuffer + actualReadOffset, cbTailBytes );
#else
        memcpy( pbDest, m_pbBuffer + actualReadOffset, cbTailBytes );
#endif
        bytesLeft -= cbTailBytes;

        if( bytesLeft )
        {
#ifdef _XBOX_VER
            XMemCpy( pbDest + cbTailBytes, m_pbBuffer, bytesLeft );
#else
            memcpy( pbDest + cbTailBytes, m_pbBuffer, bytesLeft );
#endif
        }

        // Advance the read offset. From the CPUs point of view this is several
        // operations--read, modify, store--and we'd normally want to make sure that
        // all of the operations happened atomically. But in the case of a single
        // reader, only one thread updates this value and so the only operation that
        // must be atomic is the store. That's lucky, because 32-bit aligned stores are
        // atomic on all modern processors.
        // 
        readOffset += cbDest;
        m_readOffset = readOffset;

        return true;
    }

    bool __forceinline          Write( const void* pvSrc, unsigned long cbSrc )
    {
        // Reading the read offset here has the same caveats as reading
        // the write offset had in the Read() function above. 
        DWORD readOffset = m_readOffset;
        DWORD writeOffset = m_writeOffset;

        // Compute the available write size. This comparison relies on
        // the fact that the buffer size is always a power of 2, and the
        // offsets are unsigned integers, so that when the write pointer
        // wraps around the subtraction still yields a value (assuming
        // we haven't messed up somewhere else) between 0 and c_cbBufferSize - 1.
        DWORD cbAvailable = c_cbBufferSize - ( writeOffset - readOffset );
        if( cbSrc > cbAvailable )
        {
            return false;
        }

        // Write the data
        const unsigned char* pbSrc = ( const unsigned char* )pvSrc;
        unsigned long actualWriteOffset = writeOffset & c_sizeMask;
        unsigned long bytesLeft = cbSrc;

        // See the explanation in the Read() function as to why we don't 
        // explicitly check against the read offset here.
        unsigned long cbTailBytes = min( bytesLeft, c_cbBufferSize - actualWriteOffset );
#ifdef _XBOX_VER
        XMemCpy( m_pbBuffer + actualWriteOffset, pbSrc, cbTailBytes );
#else
        memcpy( m_pbBuffer + actualWriteOffset, pbSrc, cbTailBytes );
#endif
        bytesLeft -= cbTailBytes;

        if( bytesLeft )
        {
#ifdef _XBOX_VER
            XMemCpy( m_pbBuffer, pbSrc + cbTailBytes, bytesLeft );
#else
            memcpy( m_pbBuffer, pbSrc + cbTailBytes, bytesLeft );
#endif
        }

#ifdef _XBOX_VER
        // Now it's time to update the write offset, but since the updated position
        // of the write offset will imply that there's data to be read, we need to 
        // make sure that the data all actually gets written before the update to
        // the write offset makes it out into L2. The CPU doesn't guarantee the order
        // in which writes are performed once they leave this core, so it could
        // (and will) decide that it would be better to retire the write offset
        // update before finishing up the mem copies that we just asked for. That
        // would make us sad. The solution is to emit an lwsync here, which effectively
        // acts as a write barrier--it guarantees that all of the store instructions 
        // issued before the sync are fully complete before any of the store instructions
        // issued after the sync are allowed to continue.
        //
        // If you want to get fancy, this is called "write-release."
        __lwsync();
#else
        // For x86 and x64, the only hazzard here is that the compiler reorders
        // the operations, so we make sure to tell the compiler not to do that
        _ReadWriteBarrier();
#endif

        // See comments in Read() as to why this operation isn't interlocked.
        writeOffset += cbSrc;
        m_writeOffset = writeOffset;

        return true;
    }

private:
    // Values derived from the buffer size template parameter
    //
    const static BYTE c_cbBufferSizeLog2 = min( cbBufferSizeLog2, 31 );
    const static DWORD c_cbBufferSize = ( 1 << c_cbBufferSizeLog2 );
    const static DWORD c_sizeMask = c_cbBufferSize - 1;

    // Leave these undefined to prevent their use
                                DXUTLockFreePipe( const DXUTLockFreePipe& );
    DXUTLockFreePipe& operator =( const DXUTLockFreePipe& );

    // Member data
    //
    BYTE                        m_pbBuffer[c_cbBufferSize];
    volatile DWORD __declspec( align( 4 ) ) m_readOffset;
    volatile DWORD __declspec( align( 4 ) ) m_writeOffset;
};
