#include "MemoryBuffer.h"

MemoryBuffer::MemoryBuffer()
: m_pBuffer(nullptr), m_bufferSize(0), m_pos(0), m_ableToWrite(false), m_countOnly(true)
{
}

MemoryBuffer::MemoryBuffer(void *pData, uint32 size)
: m_pBuffer((uint8 *)pData), m_bufferSize(size), m_pos(0), m_ableToWrite(true), m_countOnly(false)
{
}

MemoryBuffer::MemoryBuffer(const void* pData, uint32 size)
: m_pBuffer((uint8*)pData), m_bufferSize(size), m_pos(0), m_ableToWrite(false), m_countOnly(false)
{
}


MemoryBuffer::MemoryBuffer(std::vector<uint8>& data)
: m_pBuffer(data.data()), m_bufferSize(data.size()), m_pos(0), m_ableToWrite(true), m_countOnly(false)
{
}

MemoryBuffer::MemoryBuffer(const std::vector<uint8>& data)
: m_pBuffer((uint8*)data.data()), m_bufferSize(data.size()), m_pos(0), m_ableToWrite(false), m_countOnly(false)
{
}

uint32 MemoryBuffer::ReadRaw(void* pDest, uint32 size)
{
    if(size == 0) {
        return 0;
    }

    if(m_countOnly) {
        m_pos += size;
        return size;
    }

    if(!pDest || m_pos + size > m_bufferSize) {
        return 0;
    }

    memcpy(pDest, m_pBuffer + m_pos, size);
    m_pos += size;

    return size;
}

uint32 MemoryBuffer::WriteRaw(const void* pData, uint32 size)
{
    if(size == 0) {
        return 0;
    }

    if(m_countOnly) {
        m_pos += size;
        return size;
    }

    if(!m_ableToWrite || !pData || m_pos + size > m_bufferSize) {
        return 0;
    }

    memcpy(m_pBuffer + m_pos, pData, size);
    m_pos += size;

    return size;
}

uint32 MemoryBuffer::ReadStringRaw(string& pDest)
{
    uint16 strLen = 0;
    if(ReadRaw(&strLen, 2) != 2) {
        return 0;
    }

    if(m_countOnly) {
        m_pos += strLen;
        return strLen + 2;
    }

    if(m_pos + strLen > m_bufferSize) {
        return 0;
    }

    pDest.resize(strLen);

    memcpy(pDest.data(), m_pBuffer + m_pos, strLen);
    m_pos += strLen;

    return strLen + 2;
}

uint32 MemoryBuffer::WriteStringRaw(const string& pData)
{
    if(!m_ableToWrite && !m_countOnly) {
        return 0;
    }

    uint16 strLen = (uint16)pData.size();
    WriteRaw(&strLen, 2);
    
    WriteRaw(pData.data(), strLen);
    return strLen + 2;
}

uint32 MemoryBuffer::Seek(uint32 position)
{
    if(position > m_bufferSize) {
        position = m_bufferSize;
    }

    m_pos = position;
    return m_pos;
}

#ifdef USE_ZLIB
#include <zlib.h>
uint8* zLibInflateToMemory(uint8 *pCompData, uint32 compSize, uint32 decompSize)
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    uint8* pOut = new uint8[decompSize + 1];
    strm.avail_in = compSize;
    strm.next_in = pCompData;
    strm.avail_out = decompSize;
    strm.next_out = pOut;

    int32 ret = inflateInit(&strm);
    if(ret != Z_OK) {
        SAFE_DELETE_ARRAY(pOut);
        return nullptr;
    }

    ret = inflate(&strm, Z_FINISH);
    if(ret != Z_STREAM_END) {
        SAFE_DELETE_ARRAY(pOut);
        inflateEnd(&strm);
        return nullptr;
    }

    ret = inflateEnd(&strm);
    return pOut;
}
#endif