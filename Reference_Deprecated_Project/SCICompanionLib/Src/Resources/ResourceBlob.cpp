/***************************************************************************
    Copyright (c) 2015 Philip Fortier

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
***************************************************************************/

#include "stdafx.h"
#include "Codec.h"
#include "CodecAlt.h"
#include <errno.h>
#include "crc.h"
#include "ResourceBlob.h"
#include "GameFolderHelper.h"
#include <atomic>
#include "format.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool DoesPackageFormatIncludeHeaderInCompressedSize(SCIVersion version)
{
    return version.PackageFormat != ResourcePackageFormat::SCI11 && version.PackageFormat != ResourcePackageFormat::SCI2;
}

const uint8_t *ResourceBlob::GetData() const
{
    if (_pData.empty())
    {
        return nullptr;
    }
    return &_pData[0];
}

const uint8_t *ResourceBlob::GetDataCompressed() const
{
    if (_pDataCompressed.empty())
    {
        return nullptr;
    }
    return &_pDataCompressed[0];
}

bool IsValidResourceName(PCTSTR pszName)
{
    size_t cChars = lstrlen(pszName);
    for (size_t i = 0; i < cChars; i++)
    {
        char ch = pszName[i];
        if (!isalpha(ch) && !isdigit(ch) && (ch != '_') && (ch != ' ') && (ch != '-'))
        {
            return false;
        }
    }
    return true;
}

bool IsValidResourceSize(const SCIVersion &version, DWORD cb, ResourceType type)
{
    bool fRet = true;
    if (cb > ((type == ResourceType::Audio) ? MaxResourceSizeLarge : ((version.MapFormat >= ResourceMapFormat::SCI1) ? MaxResourceSizeLarge : MaxResourceSize)))
    {
        fRet = false;
    }
    return fRet;
}

int g_dwID = 0;
int CreateUniqueRuntimeID()
{
    return g_dwID++;
}

ResourceBlob::ResourceBlob()
{
    header.Version = sciVersion0; // Just something by default;
    _id = CreateUniqueRuntimeID();
    header.CompressionMethod = 0;
    header.Number = -1;
    header.Type = ResourceType::None;
    header.PackageHint = 1;
    _fComputedChecksum = false;
    _resourceLoadStatus = ResourceLoadStatusFlags::None;
    _hasNumber = false;
}

ResourceBlob::ResourceBlob(const GameFolderHelper &helper, PCTSTR pszName, ResourceType iType, const std::vector<BYTE> &data, int iPackageHint, int iNumber, uint32_t base36Number, SCIVersion version, ResourceSourceFlags sourceFlags)
{
    _resourceLoadStatus = ResourceLoadStatusFlags::None;

    if (!data.empty())
    {
        _pData.allocate(data.size());
        _pData.assign(&data[0], &data[0] + data.size()); // Yuck
    }

    // REVIEW: do some validation
    header.Version = version;
    header.SourceFlags = sourceFlags;
    header.Type = iType;
    header.Number = (WORD)iNumber;
    _hasNumber = (iNumber != -1);
    header.CompressionMethod = 0;
    header.cbDecompressed = (DWORD)data.size();
    header.cbCompressed = header.cbDecompressed;
    _iChecksum = 0;
    _fComputedChecksum = false;

    header.PackageHint = iPackageHint;
    if (!pszName || (*pszName == 0))
    {
        _strName = helper.FigureOutName(iType, iNumber, base36Number);
    }
    else
    {
        _strName = pszName;
    }
}

int ResourceBlob::GetLengthOnDisk() const
{
    // The size of the header plus the compressed size.
    uint32_t headerSize =
        (header.Version.MapFormat > ResourceMapFormat::SCI0_LayoutSCI1) ? sizeof(RESOURCEHEADER_SCI1) : sizeof(RESOURCEHEADER_SCI0);

    return headerSize + header.cbCompressed;
}

HRESULT ResourceBlob::CreateFromBits(const GameFolderHelper &helper, PCTSTR pszName, ResourceType iType, sci::istream *pStream, int iPackageHint, int iNumber, uint32_t base36Number, SCIVersion version, ResourceSourceFlags sourceFlags)
{
    HRESULT hr = E_FAIL;
    if (pStream)
    {
        hr = S_OK;

        _pData.allocate(pStream->GetDataSize());
        pStream->read_data(&_pData[0], pStream->GetDataSize());

        assert(pStream->good()); // It told us the size, so it should always succeed.
        // REVIEW: do some validation
        header.Type = iType;
        header.Number = (WORD)iNumber;
        header.Base36Number = base36Number;
        _hasNumber = (iNumber != -1);
        header.CompressionMethod = 0;
        header.cbDecompressed = pStream->GetDataSize();
        header.cbCompressed = header.cbDecompressed;
        header.PackageHint = iPackageHint;
        header.Version = version;
        header.SourceFlags = sourceFlags;

        if (!pszName || (*pszName == 0))
        {
            _strName = helper.FigureOutName(iType, iNumber, base36Number);
        }
        else
        {
            _strName = pszName;
        }
    }
    return hr; // TODO Do data validation
}

//
// Loads a resource that was saved into its own file by this application, or SCIStudio.
// These resources are not prepended with a header.  Instead, they have a WORD that is 0x80 | type,
// where type is the resource type.
//
HRESULT ResourceBlob::CreateFromFile(PCTSTR pszName, std::string strFileName, SCIVersion version, ResourceSaveLocation saveLocation, int iPackage, int iNumber)
{
    HRESULT hr;
    HANDLE hFile = CreateFile(strFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        hr = CreateFromHandle(pszName, hFile, -1, version, saveLocation);
        if (SUCCEEDED(hr))
        {
            if (iNumber == -1)
            {
                iNumber = ResourceNumberFromFileName(strFileName.c_str());
            }

            _Init(iPackage, iNumber);
        }
        CloseHandle(hFile);
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}

sci::istream ResourceBlob::GetReadStream() const
{
    return (header.cbDecompressed > 0) ? sci::istream(&_pData[0], header.cbDecompressed) : sci::istream();
}

DecompressionAlgorithm VersionAndCompressionNumberToAlgorithm(SCIVersion version, int compressionNumber)
{
    switch (compressionNumber)
    {
    case 0:
        return DecompressionAlgorithm::None;
    case 1:
        return (version.CompressionFormat == CompressionFormat::SCI0) ? DecompressionAlgorithm::LZW : DecompressionAlgorithm::Huffman;
    case 2:
        return (version.CompressionFormat == CompressionFormat::SCI0) ? DecompressionAlgorithm::Huffman : DecompressionAlgorithm::LZW1;
    case 3:
        return DecompressionAlgorithm::LZW_View;
    case 4:
        return DecompressionAlgorithm::LZW_Pic;

    case 8:
        // Testing for Imagination Network
        return DecompressionAlgorithm::DCL;
    case 18:
    case 19:
    case 20:
        return DecompressionAlgorithm::DCL;
    case 32:
        return DecompressionAlgorithm::STACpack;
    }
    return DecompressionAlgorithm::Unknown;
}

void ResourceBlob::SetKeyValue(BlobKey key, uint32_t value)
{
    header.PropertyBag[key] = value;
}

bool ResourceBlob::GetKeyValue(BlobKey key, uint32_t &value) const
{
    auto it = header.PropertyBag.find(key);
    if (it != header.PropertyBag.end())
    {
        value = it->second;
        return true;
    }
    return false;
}

std::string ResourceBlob::GetEncodingString() const
{
    switch (VersionAndCompressionNumberToAlgorithm(this->GetVersion(), header.CompressionMethod))
    {
    case DecompressionAlgorithm::None:
        return "None";
    case DecompressionAlgorithm::Huffman:
        return "Huffman";
    case DecompressionAlgorithm::LZW:
        return "LZW";
    case DecompressionAlgorithm::LZW1:
        return "LZW1";
    case DecompressionAlgorithm::LZW_View:
        return "LZW_View";
    case DecompressionAlgorithm::LZW_Pic:
        return "LZW_Pic";
    case DecompressionAlgorithm::DCL:
        return "DCL";
    case DecompressionAlgorithm::STACpack:
        return "STAC";
    }
    return "Unknown";
}

bool _SanityCheckHeader(const ResourceHeaderAgnostic &header)
{
    return ((header.cbCompressed < 0x01000000) && (header.cbDecompressed < 0x01000000));
}

void ResourceBlob::_EnsureDecompressed()
{
    if (IsFlagSet(_resourceLoadStatus, ResourceLoadStatusFlags::Delayed) && !_pDataCompressed.empty())
    {
        uint32_t cbCompressedRemaining = _pDataCompressed.size();
        ClearFlag(_resourceLoadStatus, ResourceLoadStatusFlags::Delayed);
        int iResult = SCI_ERROR_UNKNOWN_COMPRESSION;
        DecompressionAlgorithm algorithm = VersionAndCompressionNumberToAlgorithm(this->GetVersion(), header.CompressionMethod);
        switch (algorithm)
        {
            case DecompressionAlgorithm::Huffman:
                iResult = decompressHuffman(&_pData[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining);
                break;
            case DecompressionAlgorithm::LZW:
                iResult = decompressLZW(&_pData[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining);
                break;
            case DecompressionAlgorithm::LZW1:
                iResult = decompressLZW_1(&_pData[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining);
                break;
            case DecompressionAlgorithm::LZW_View:
            {
                std::unique_ptr<uint8_t[]> pTemp = std::make_unique<uint8_t[]>(header.cbDecompressed);
                iResult = decompressLZW_1(&pTemp[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining);
                if (iResult == 0)
                {
                    BoundsCheckedArray<BYTE> dest(&_pData[0], (int)_pData.size());
                    reorderView(&pTemp[0], dest);
                }
            }
            break;
            case DecompressionAlgorithm::LZW_Pic:
            {
                std::unique_ptr<uint8_t[]> pTemp = std::make_unique<uint8_t[]>(header.cbDecompressed);
                iResult = decompressLZW_1(&pTemp[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining);
                if (iResult == 0)
                {
                    reorderPic(&pTemp[0], &_pData[0], header.cbDecompressed);
                }
            }

            break;
            case DecompressionAlgorithm::DCL:
                iResult =
                    decompressDCL(&_pData[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining) ?
                    0 : -1;
                break;

            case DecompressionAlgorithm::STACpack:
                iResult =
                    decompressLZS(&_pData[0], &_pDataCompressed[0], header.cbDecompressed, cbCompressedRemaining) ?
                    0 : -1;
                break;
        }
        if (iResult != 0)
        {
            _resourceLoadStatus |= ResourceLoadStatusFlags::DecompressionFailed;
        }
    }
}

void ResourceBlob::_DecompressFromBits(sci::istream &byteStream, bool delay)
{
    uint32_t cbCompressedRemaining = header.cbCompressed; // Because cbCompressed includes 4 bytes of the header.
    assert(_pData.empty());

    // If the sizes are ridiculous, catch the corruption here.
    if (_SanityCheckHeader(header))
    {
        _pData.allocate(header.cbDecompressed);
        bool cantDecompress = false;

        int iResult = SCI_ERROR_UNKNOWN_COMPRESSION;
        DecompressionAlgorithm algorithm = VersionAndCompressionNumberToAlgorithm(header.Version, header.CompressionMethod);
        if (algorithm == DecompressionAlgorithm::None)
        {
            iResult = 0;
            if (header.cbDecompressed > 0)
            {
                byteStream.read_data(&_pData[0], header.cbDecompressed);
            }
        }
        else
        {
            assert(_pDataCompressed.empty()); // Verify no leaks
            _pDataCompressed.allocate(cbCompressedRemaining);
            byteStream.read_data(&_pDataCompressed[0], cbCompressedRemaining);
            _resourceLoadStatus |= ResourceLoadStatusFlags::Delayed;
            if (!delay)
            {
                _EnsureDecompressed();
            }
        }
    }
    else
    {
        _pData.allocate(1);   // To avoid problems...
        _resourceLoadStatus |= ResourceLoadStatusFlags::Corrupted;
        header.cbCompressed = 0;
        header.cbDecompressed = 0;
    }
}

void ResourceBlob::CreateFromPackageBits(const std::string &name, const ResourceHeaderAgnostic &prh, sci::istream &byteStream, bool delay)
{
    header = prh;
    _hasNumber = true;
    _strName = name;

    _DecompressFromBits(byteStream, delay);
}

uint32_t GetResourceOffsetInFile(uint8_t secondHeaderByte)
{
    // The upper byte appears to indicate where the data starts.
    // Some isolated resource files have random data stuffed at the beginning (e.g. name and such)

    // In SQ5 though, we need to special case:
    if (secondHeaderByte & 0x80)
    {
        switch (secondHeaderByte & 0x7f)
        {
        case 0:
            return 24;
        case 1:
            return 2;
        case 4:
            return 8;
        }
    }
    return secondHeaderByte;
}

void _AssignDefaultResourceSourceFlags(ResourceBlob &blob, ResourceSaveLocation saveLocation)
{
    switch (blob.GetType())
    {
        case ResourceType::Audio:
            blob.GetHeader().SourceFlags = ResourceSourceFlags::AudioCache;
            break;

        case ResourceType::Message:
            switch (blob.GetHeader().Version.MessageMapSource)
            {
                case MessageMapSource::Included:
                    blob.GetHeader().SourceFlags = ResourceSourceFlags::ResourceMap;
                    break;
                case MessageMapSource::MessageMap:
                    blob.GetHeader().SourceFlags = ResourceSourceFlags::MessageMap;
                    break;
                case MessageMapSource::AltResMap:
                    blob.GetHeader().SourceFlags = ResourceSourceFlags::AltMap;
                    break;
            }
            break;

        default:
            blob.GetHeader().SourceFlags = (saveLocation == ResourceSaveLocation::Patch) ? ResourceSourceFlags::PatchFile : ResourceSourceFlags::ResourceMap;
            break;
    }
}

//
// hFile is assumed to be offset to the beginning of a resource header.
//
HRESULT ResourceBlob::CreateFromHandle(PCTSTR pszName, HANDLE hFile, int iPackageHint, SCIVersion version, ResourceSaveLocation saveLocation)
{
    header.PackageHint = (uint16_t)iPackageHint;
    HRESULT hr = E_FAIL;
    // This is the case of a resource existing in a solitary file.
    // Just a WORD consisting of 0x80 | type
    uint16_t w;
    DWORD cbRequested = sizeof(w);
    DWORD cbRead;
    BOOL fRead = ReadFile(hFile, &w, cbRequested, &cbRead, NULL);
    fRead = (cbRead == cbRequested);
    if (fRead)
    {
        if ((w & 0x80) == 0)
        {
            // No good.
            fRead = FALSE;
            SetLastError(ERROR_INVALID_DATA);
        }
        else
        {
            // Set the type.
            header.Type = (ResourceType)((w & 0x00ff) & ~0x0080);
            header.Version = version;

            // Figure out the size from the filesize.
            DWORD dwSize = GetFileSize(hFile, NULL);
            if (dwSize != INVALID_FILE_SIZE)
            {
                DWORD gapUntilData = GetResourceOffsetInFile((uint8_t)(w >> 8));
                SetFilePointer(hFile, gapUntilData, nullptr, FILE_CURRENT);
                dwSize -= gapUntilData;

                header.cbDecompressed = dwSize - sizeof(w);
                // Fake this value up for consistency:
                header.cbCompressed = header.cbDecompressed;
                // No compression:
                header.CompressionMethod = 0;
            }
            else
            {
                fRead = FALSE;
            }
        }
    }
    if (pszName && *pszName)
    {
        _strName = pszName;
    }

    if (fRead)
    {
        hr = S_OK;
    }
    else
    {
        hr = ResultFromLastError();
    }

    if (SUCCEEDED(hr))
    {
        // Now it is time to read in the bits.
        // We're already at the end of the header, so just start reading.
        sci::streamOwner streamOwner(hFile, header.cbCompressed);
        _DecompressFromBits(streamOwner.getReader(), false);
        _AssignDefaultResourceSourceFlags(*this, saveLocation);
    }    
    return hr;
}

void ResourceBlob::EnsureRealized()
{
    _EnsureDecompressed();
}

//
// If pszName is null, the resource is called nxxx, where xxx is the resource number.
//
void ResourceBlob::_SetName(PCTSTR pszName)
{
    if (pszName)
    {
        _strName = pszName;
    }
    else
    {
        _strName = default_reskey(header.Number, header.Base36Number);
    }
}


//
// Export to a separate file
//
HRESULT ResourceBlob::SaveToFile(const std::string &strFileName) const
{
    // Open the file
    HRESULT hr;
    HANDLE hFile = CreateFile(strFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        // Save it
        hr = SaveToHandle(hFile, true);
        CloseHandle(hFile);
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}

template<typename _THeader>
BOOL _WriteHeaderToFile(HANDLE hFile, const ResourceHeaderAgnostic &header, DWORD *pcbWrittenHeader)
{
    _THeader rh;
    rh.FromAgnostic(header);
    return WriteFile(hFile, &rh, sizeof(rh), pcbWrittenHeader, nullptr) && (*pcbWrittenHeader == sizeof(rh));
}

//
// Save into a resource file
//
HRESULT ResourceBlob::SaveToHandle(HANDLE hFile, bool fNoHeader, DWORD *pcbWritten) const
{
    HRESULT hr;
    DWORD cbWrittenHeader;
    BOOL fWrote = FALSE;
    bool fWriteCompressedIfPossible;
    DWORD checkSize = max(header.cbCompressed, header.cbDecompressed);
    if (!IsValidResourceSize(header.Version, checkSize, header.Type))
    {
        SetLastError(ERROR_OUTOFMEMORY);  // Not the best, but it will do.
    }
    else
    {
        if (fNoHeader)
        {
            // No resource header - this is in its own file.  It is just an indicator of what type it is.
            uint16_t w = 0x80 | (int)header.Type;
            // For SCI1.1 and above, views and pics appear to have 0x8? as the second byte. The high bit is a
            // marker that extra data is there, and 0x00 indicates 24 bytes of extra data.
            uint8_t secondByte = 0x00;
            if (header.Version.PackageFormat >= ResourcePackageFormat::SCI11)
            {
                if (header.Type == ResourceType::View)
                {
                    secondByte = 0x80;
                }
                else if (header.Type == ResourceType::Pic)
                {
                    secondByte = 0x81;
                }
            }
            w |= ((uint16_t)secondByte) << 8;
            fWrote = WriteFile(hFile, &w, sizeof(w), &cbWrittenHeader, nullptr) && (cbWrittenHeader == sizeof(w));

            uint32_t sizeOfExtraStuff = GetResourceOffsetInFile(secondByte);
            if (fWrote && sizeOfExtraStuff)
            {
                std::vector<uint8_t> emptyData(sizeOfExtraStuff);
                fWrote = WriteFile(hFile, &emptyData[0], emptyData.size(), &cbWrittenHeader, nullptr) && (cbWrittenHeader == emptyData.size());
            }

            // When we're writing a resource to a separate file, we don't have a header, so we can't have any compression methods.
            fWriteCompressedIfPossible = FALSE;
        }
        else
        {
            if (header.Version.MapFormat > ResourceMapFormat::SCI0_LayoutSCI1)
            {
                fWrote = _WriteHeaderToFile<RESOURCEHEADER_SCI1>(hFile, header, &cbWrittenHeader);
            }
            else
            {
                fWrote = _WriteHeaderToFile<RESOURCEHEADER_SCI0>(hFile, header, &cbWrittenHeader);
            }
            fWriteCompressedIfPossible = TRUE;
        }
    }

    DWORD cbWrittenData;
    if (fWrote)
    {
        if (fWriteCompressedIfPossible && !_pDataCompressed.empty())
        {
            // Write the compressed version (e.g. when rebuilding resources, etc...)
            DWORD cbActual = header.cbCompressed - 4;
            fWrote = WriteFile(hFile, &_pDataCompressed[0], cbActual, &cbWrittenData, NULL) && (cbWrittenData == cbActual);
        }
        else if (!_pData.empty() || (header.cbDecompressed == 0)) // Check for _cb == 0, since we might just have an empty resource, which is ok (not an error)
        {
            // Write the uncompressed version.
            fWrote = WriteFile(hFile, &_pData[0], header.cbDecompressed, &cbWrittenData, NULL) && (cbWrittenData == header.cbDecompressed);
        }
        else
        {
            fWrote = FALSE;
            SetLastError(ERROR_INVALID_PARAMETER);
        }
    }

    if (fWrote)
    {
        if (pcbWritten)
        {
            *pcbWritten = cbWrittenHeader + cbWrittenData;
        }
        hr = S_OK;
    }
    else
    {
        hr = ResultFromLastError();
    }
    return hr;
}

std::atomic<int> g_nextUniqueId = 0;

int ResourceBlob::GetChecksum() const
{
    if (!_fComputedChecksum)
    {
        size_t size = _pData.size();
        if (header.Version.IsMapAppend())
        {
            _iChecksum = (size > 0) ? crcFast(&_pData[0], _pData.size()) : 0;
        }
        else
        {
            // Testing out just using an incrementing id instead of calculating the hash. crc is super slow
            // for big resources. The "checksum" is used for determining if a resource is "most recent", for blob id
            // for background threads. What else?
            // There are issues with doing this... so for SCI0 (above) we'll still calculate the hash.
            _iChecksum = g_nextUniqueId.fetch_add(1);
        }
        _fComputedChecksum = true;
    }
    return _iChecksum;
}

bool operator==(const ResourceMapEntryAgnostic &one, const ResourceMapEntryAgnostic &two)
{
    // Don't do a memcmp, because different resource sources don't set the ExtraData values reliably.
    // We don't care about ExtraData
    return one.Number == two.Number &&
        one.Offset == two.Offset &&
        one.PackageNumber == two.PackageNumber &&
        one.Type == two.Type &&
        one.Base36Number == two.Base36Number;
}

void ThrowExceptionIfOverflow(uint32_t sizeNeeded, uint32_t sizeAvailable, const char *name)
{
    if (sizeNeeded > sizeAvailable)
    {
        throw std::exception(fmt::format("{} ({} bytes) is too large for this version of SCI. Reduce the size to {} bytes or less.", name, sizeNeeded, sizeAvailable).c_str());
    }
}
