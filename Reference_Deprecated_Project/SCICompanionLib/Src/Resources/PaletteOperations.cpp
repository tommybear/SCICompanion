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
#include "PaletteOperations.h"
#include "ResourceEntity.h"
#include "AppState.h"
#include "ImageUtil.h"
#include "GameFolderHelper.h"

using namespace std;

const char riffMarker[] = "RIFF";
const char dataMarker[] = "data";
const char palMarker[] = "PAL ";
const uint16_t palVersion = 0x0300;

PaletteComponent g_egaDummyPalette;

uint32_t ToUint32(const char *marker)
{
    return *static_cast<const uint32_t *>(static_cast<const void *>(marker));
}

void SavePALFile(const std::string &filename, PaletteComponent &palette, int startIndex, int count)
{
    // TODO: http://worms2d.info/Palette_file#File_format
    ScopedFile file(filename, GENERIC_WRITE, 0, CREATE_ALWAYS);
    sci::ostream byteStream;

    uint16_t colorEntryCount = (uint16_t)count;
    uint32_t dataSize = colorEntryCount * 4 + sizeof(palVersion) + sizeof(colorEntryCount);

    uint32_t fileSize = dataSize + sizeof(palMarker) + sizeof(dataMarker) + sizeof(dataSize);
    // Riff header:
    byteStream << ToUint32(riffMarker);
    byteStream << fileSize;
    byteStream << ToUint32(palMarker);

    // Data header:
    byteStream << ToUint32(dataMarker);
    byteStream << dataSize; // Looking at pal files, it seems this includes the following 4 bytes too:
    byteStream << palVersion;
    byteStream << colorEntryCount;

    for (int i = 0; i < count; i++)
    {
        byteStream << palette.Colors[i + startIndex].rgbRed;
        byteStream << palette.Colors[i + startIndex].rgbGreen;
        byteStream << palette.Colors[i + startIndex].rgbBlue;
        byteStream << palette.Colors[i + startIndex].rgbReserved;    // Or should we write 0?
    }

    file.Write(byteStream.GetInternalPointer(), byteStream.GetDataSize());
}

void LoadPALFile(const std::string &filename, PaletteComponent &palette, int startIndex)
{
    ScopedFile file(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
    sci::streamOwner owner(file.hFile);
    sci::istream byteStream = owner.getReader();
    uint32_t riff, pal, dataSize, chunk, chunkSize;
    byteStream >> riff;
    byteStream >> dataSize;
    byteStream >> pal;
    byteStream >> chunk;
    byteStream >> chunkSize;
    uint16_t palVersion, palEntries;
    byteStream >> palVersion;
    byteStream >> palEntries;

    for (uint16_t i = 0; i < palEntries; i++)
    {
        if ((i + startIndex) < ARRAYSIZE(palette.Colors))
        {
            byteStream >> palette.Colors[i + startIndex].rgbRed;
            byteStream >> palette.Colors[i + startIndex].rgbGreen;
            byteStream >> palette.Colors[i + startIndex].rgbBlue;
            byteStream >> palette.Colors[i + startIndex].rgbReserved;
            palette.Colors[i + startIndex].rgbReserved = 0x1; // Actually, set this to used.
        }
    }
}

PaletteComponent::PaletteComponent() : Compression(PaletteCompression::Unknown), EntryType(PaletteEntryType::Unknown)
{
    // 1-to-1 mapping by default
    for (int i = 0; i < ARRAYSIZE(Mapping); i++)
    {
        Mapping[i] = (uint8_t)i;
    }
}

void PaletteComponent::MergeFromOther(const PaletteComponent *globalPalette)
{
    if (globalPalette)
    {
        for (int i = 0; i < ARRAYSIZE(Colors); i++)
        {
            // I thought it was a priority thing, but it looks like any rgbReserved that isn't
            // zero in the main palette will take precedence (e.g. KQ6 view 105)
            if (Colors[i].rgbReserved == 0)
            //if (globalPalette->Colors[i].rgbReserved > Colors[i].rgbReserved)
            {
                // Global has priority...
                Colors[i] = globalPalette->Colors[i];
            }
        }
    }
}

const int SquareSize = 12;

void DrawUnderline(uint8_t *bits, int cx, int cy, int x, int y, int border, uint8_t value)
{
    int rowSpan = cx;
    // Make a little square with the value from the palette
    int line = SquareSize - 1;
    int rowOffset = (y * SquareSize + line + border) * rowSpan;
    int columnOffset = x * SquareSize + border;
    memset(bits + rowOffset + columnOffset, value, SquareSize);
}

void DrawSquare(uint8_t *bits, int cx, int cy, int x, int y, int border, uint8_t value)
{
    int rowSpan = cx;
    // Make a little square with the value from the palette
    for (int line = 0; line < SquareSize; line++)
    {
        int rowOffset = (y * SquareSize + line + border) * rowSpan;
        int columnOffset = x * SquareSize + border;
        memset(bits + rowOffset + columnOffset, value, SquareSize);
    }
}

void DrawDot(uint8_t *bits, int cx, int cy, int x, int y, int border, uint8_t value)
{
    int rowSpan = cx;
    int line = -border + SquareSize / 2;
    int column = x * SquareSize + SquareSize / 2;
    int rowOffset = (y * SquareSize + line + border) * rowSpan;
    *(bits + rowOffset + column) = value;
}

HBITMAP CreateBitmapFromPaletteComponent(const PaletteComponent &palette, SCIBitmapInfo *pbmi, uint8_t **ppBitsDest, COLORREF *transparentColor, const std::vector<const Cel*> *imageCels)
{
    HBITMAP hbmpRet = nullptr;

    const int border = 0;

    int cx = SquareSize * 16 + 2 * border;
    int cy = SquareSize * 16 + 2 * border;

    bool reallyUsed[256] = { 0 };
    int reallyUsedCount = 0;
    if (imageCels && !imageCels->empty())
    {
        reallyUsedCount = CountActualUsedColors(*imageCels, reallyUsed);
    }

    CDC dc;
    if (dc.CreateCompatibleDC(nullptr))
    {
        SCIBitmapInfo bmi(cx, cy, palette.Colors, ARRAYSIZE(palette.Colors));
        bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;
        uint8_t *pBitsDest;
        CBitmap bitmap;

        // Allow for callers to specify a "transparent" color to render unused palette entries.
        uint8_t transparentIndex = 0;
        RGBQUAD transparent = { GetBValue(*transparentColor), GetGValue(*transparentColor), GetRValue(*transparentColor), 0 };
        // Scan for the first unused entry and set its color to this
        for (int i = 0; i < 256; i++)
        {
            if (transparentColor)
            {
                if (bmi.bmiColors[i].rgbReserved == 0x0)
                {
                    // This entry is unused. Replace it with the transparent color and use this index to
                    // render unused entries.
                    bmi.bmiColors[i] = transparent;
                    transparentIndex = (uint8_t)i;
                }
            }
        }

        if (bitmap.Attach(CreateDIBSection((HDC)dc, &bmi, DIB_RGB_COLORS, (void**)&pBitsDest, nullptr, 0)))
        {
            for (int y = 0; y < 16; y++)
            {
                for (int x = 0; x < 16; x++)
                {
                    int paletteIndex = x + y * 16;
                    uint8_t value = palette.Mapping[paletteIndex];
                    // If this one isn't used, make it transparent.
                    if (transparentColor)
                    {
                        if (palette.Colors[paletteIndex].rgbReserved == 0x0)
                        {
                            value = transparentIndex;
                        }
                    }
                    DrawSquare(pBitsDest, cx, cy, x, y, border, value);
                    if (palette.Colors[paletteIndex].rgbReserved == 0x3)
                    {
                        DrawUnderline(pBitsDest, cx, cy, x, y, border, 0);
                    }
                }
            }

            if (imageCels)
            {
                // "highlight" which colors are actually used.
                for (int y = 0; y < 16; y++)
                {
                    for (int x = 0; x < 16; x++)
                    {
                        int paletteIndex = x + y * 16;
                        if (reallyUsed[paletteIndex])
                        {
                            DrawDot(pBitsDest, cx, cy, x, y, border, 255);
                        }
                    }
                }
            }

            hbmpRet = (HBITMAP)bitmap.Detach();
            *pbmi = bmi;
            *ppBitsDest = pBitsDest;
        }
    }

    return hbmpRet;
}

// If imageCels are provided, we look into which palette entries are ACTUALLY used, instead of just going off the palette data.
HBITMAP CreateBitmapFromPaletteResource(const ResourceEntity *prb, SCIBitmapInfo *pbmi, uint8_t **ppBitsDest, COLORREF *transparentColor, const std::vector<const Cel*> *imageCels)
{
    const PaletteComponent &palette = prb->GetComponent<PaletteComponent>();
    return CreateBitmapFromPaletteComponent(palette, pbmi, ppBitsDest, transparentColor, imageCels);
}

void PaletteWriteTo_SCI10(const ResourceEntity &resource, sci::ostream &byteStream, std::map<BlobKey, uint32_t> &propertyBag)
{
    WritePalette(byteStream, resource.GetComponent<PaletteComponent>());
}

void PaletteWriteTo_SCI11(const ResourceEntity &resource, sci::ostream &byteStream, std::map<BlobKey, uint32_t> &propertyBag)
{
    WritePaletteShortForm(byteStream, resource.GetComponent<PaletteComponent>());
}


#define SCI_PAL_FORMAT_VARIABLE 0
#define SCI_PAL_FORMAT_CONSTANT 1

RGBQUAD black = { 0, 0, 0, 0 };

void WritePalette(sci::ostream &byteStream, const PaletteComponent &palette)
{
    // We currently only support writing out the full palette.
    byteStream.WriteBytes(palette.Mapping, ARRAYSIZE(palette.Mapping) * sizeof(*palette.Mapping));
    // There seem to be 4 bytes of zeroes following that:
    byteStream.WriteWord(0);
    byteStream.WriteWord(0);
    // Unfortunately we need to swizzle RGBQUAD 
    for (int i = 0; i < ARRAYSIZE(palette.Colors); i++)
    {
        byteStream.WriteByte(palette.Colors[i].rgbReserved);
        byteStream.WriteByte(palette.Colors[i].rgbRed);
        byteStream.WriteByte(palette.Colors[i].rgbGreen);
        byteStream.WriteByte(palette.Colors[i].rgbBlue);
    }
}

#include <pshpack1.h>
struct PaletteHeader
{
    uint8_t marker;         // Or header? It's usually 0xE
    uint8_t garbage[9];     // Honestly looks like garbage, pieces of ascii text sometimes.
    uint16_t always1;
    uint8_t always0;
    uint16_t offsetToEndOfPaletteData; // post offset offset
    uint32_t globalPaletteMarker;   // ascii \0999
    uint32_t moreGarbage;   // Stuff in here, but I don't know what it is
    uint16_t always0_B;
    uint8_t palColorStart;
    uint8_t unknown3, unknown4, unknown5;
    uint16_t palColorCount;
    uint8_t unknown7;
    uint8_t palFormat;
    uint32_t unknown8;
};
#include <poppack.h>

void WritePaletteShortForm(sci::ostream &byteStream, const PaletteComponent &palette)
{
    // This is the one with the 37 byte header.
    assert(sizeof(PaletteHeader) == 37);

    PaletteHeader header = { 0 };
    header.marker = 0xe;
    header.palColorStart = 0;
    header.palColorCount = 256;
    header.palFormat = SCI_PAL_FORMAT_VARIABLE; // Make it easy on ourselves for now, so we don't need to interpret rgbReserved.
    header.always1 = 1;
    header.globalPaletteMarker = 0x00393939;    // This isn't always true, but...
    header.offsetToEndOfPaletteData = ((header.palFormat == SCI_PAL_FORMAT_VARIABLE) ? 4 : 3) * header.palColorCount + 22;

    header.unknown7 = 0x3; // Needed for SQ5 views to work????

    byteStream << header;

    for (uint16_t i = header.palColorStart; i < header.palColorCount; i++)
    {
        byteStream.WriteByte(palette.Colors[i].rgbReserved);
        byteStream.WriteByte(palette.Colors[i].rgbRed);
        byteStream.WriteByte(palette.Colors[i].rgbGreen);
        byteStream.WriteByte(palette.Colors[i].rgbBlue);
    }
}

// REVIEW: This is still a work in process, and doesn't work yet.
void WritePaletteSCI2(sci::ostream &byteStream, const PaletteComponent &palette)
{
    PaletteHeader header = { 0 };
    header.marker = 0xe; // Not sure, but it seems to be.
    header.always1 = 1;
    header.palColorStart = 0;
    header.palColorCount = 255;
    header.unknown7 = 1;
    header.palFormat = SCI_PAL_FORMAT_VARIABLE;
    header.offsetToEndOfPaletteData = ((header.palFormat == SCI_PAL_FORMAT_VARIABLE) ? 4 : 3) * header.palColorCount + 22;
    byteStream << header;

    for (uint16_t i = header.palColorStart; i < header.palColorCount; i++)
    {
        byteStream.WriteByte(palette.Colors[i].rgbReserved);
        byteStream.WriteByte(palette.Colors[i].rgbRed);
        byteStream.WriteByte(palette.Colors[i].rgbGreen);
        byteStream.WriteByte(palette.Colors[i].rgbBlue);
    }
}

void ReadPalette(PaletteComponent &palette, sci::istream &byteStream)
{
    uint32_t startPosition = byteStream.tellg();
    int end = 0;
    if (byteStream.getBytesRemaining() >= 37)   // This is a special case for pic 0 in SQ5.
    {
        PaletteHeader header;
        byteStream >> header;

        uint16_t palOffset;

        //if ((first == 0 && second == 1) || (first == 0 && second == 0 && palColorCount == 0))
        // [00],[01], the beginning of a palette color mapping table.
        if ((header.marker == 0x0 && header.garbage[0] == 0x1) || (header.marker == 0 && header.palColorCount == 0))
        {
            header.palFormat = SCI_PAL_FORMAT_VARIABLE;
            palOffset = 260;
            header.palColorStart = 0;
            header.palColorCount = 256;
            palette.Compression = PaletteCompression::None;
        }
        else
        {
            palette.Compression = PaletteCompression::Header;
            //assert(header.always1 == 1);
            //assert(header.always0 == 0);
            //assert(header.always0_B == 0);
            //assert(header.unknown8 == 0);
            palOffset = 37;
        }

        byteStream.seekg(startPosition + palOffset);

        end = (header.palColorStart + header.palColorCount);

        if (end <= ARRAYSIZE(palette.Colors))
        {
            if (header.palFormat == SCI_PAL_FORMAT_CONSTANT)
            {
                palette.EntryType = PaletteEntryType::ThreeByte;
                for (int entry = 0; entry < header.palColorStart; entry++)
                {
                    palette.Colors[entry] = black;
                }
                for (int entry = header.palColorStart; entry < end; entry++)
                {
                    palette.Colors[entry].rgbReserved = 1;
                    byteStream >> palette.Colors[entry].rgbRed;
                    byteStream >> palette.Colors[entry].rgbGreen;
                    byteStream >> palette.Colors[entry].rgbBlue;
                }
            }
            else if (header.palFormat == SCI_PAL_FORMAT_VARIABLE)
            {
                palette.EntryType = PaletteEntryType::FourByte;
                for (int entry = 0; entry < header.palColorStart; entry++)
                {
                    palette.Colors[entry] = black;
                }
                for (int entry = header.palColorStart; entry < end; entry++)
                {
                    byteStream >> palette.Colors[entry].rgbReserved;
                    byteStream >> palette.Colors[entry].rgbRed;
                    byteStream >> palette.Colors[entry].rgbGreen;
                    byteStream >> palette.Colors[entry].rgbBlue;
                }
            }
            else
            {
                throw std::exception("Invalid palette.");
            }
        }
        else
        {
            appState->LogInfo("Corrupt palette.");
            end = 0; // So we fill in with black below...
        }
    }

    for (int entry = end; entry < ARRAYSIZE(palette.Colors); entry++)
    {
        palette.Colors[entry] = black;
    }
}

void PaletteReadFrom(ResourceEntity &resource, sci::istream &byteStream, const std::map<BlobKey, uint32_t> &propertyBag)
{
    ReadPalette(resource.GetComponent<PaletteComponent>(), byteStream);
}

bool PaletteComponent::operator == (const PaletteComponent &src)
{
    return 0 == memcmp(this, &src, sizeof(*this));
}
bool PaletteComponent::operator != (const PaletteComponent &src)
{
    return 0 != memcmp(this, &src, sizeof(*this));
}

ResourceTraits paletteTraits_SCI10 =
{
    ResourceType::Palette,
    &PaletteReadFrom,
    &PaletteWriteTo_SCI10,
    &NoValidationFunc,
    nullptr
};

ResourceTraits paletteTraits_SCI11 =
{
    ResourceType::Palette,
    &PaletteReadFrom,
    PaletteWriteTo_SCI11,
    &NoValidationFunc,
    nullptr
};

ResourceEntity *CreatePaletteResource(SCIVersion version)
{
    std::unique_ptr<ResourceEntity> pResource = std::make_unique<ResourceEntity>(version.sci11Palettes ? paletteTraits_SCI11 : paletteTraits_SCI10);
    pResource->AddComponent(move(make_unique<PaletteComponent>()));
    return pResource.release();
}
