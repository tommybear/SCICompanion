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

// RasterResourceDocument.cpp : implementation file
//

#include "stdafx.h"
#include "AppState.h"
#include "NewRasterResourceDocument.h"
#include "AnimateDialog.h"
#include "FontOperations.h"
#include "PaletteOperations.h"
#include "format.h"
#include "ImageUtil.h"
#include "CustomMessageBox.h"
#include "BitmapToVGADialog.h"
#include "ResourceBlob.h"

// A sort of workaround
CHintWithObject<CelIndex> WrapRasterChange(RasterChange change) { return CHintWithObject<CelIndex>(static_cast<uint32_t>(change.hint), change.index); }

using namespace std;
using namespace Gdiplus;


const int EmbeddedPaletteId = -1;
const int EmbeddedPaletteOnlyId = -2;

IMPLEMENT_DYNCREATE(CNewRasterResourceDocument, CResourceDocument)

BEGIN_MESSAGE_MAP(CNewRasterResourceDocument, TCLASS_2(CUndoResource, CResourceDocument, ResourceEntity))
    ON_COMMAND(ID_FILE_EXPORTASBITMAP, CResourceDocument::OnExportAsBitmap)
    ON_UPDATE_COMMAND_UI(ID_FILE_EXPORTASBITMAP, OnUpdateEGA)
    ON_COMMAND(ID_ANIMATE, OnAnimate)
    ON_COMMAND(ID_IMPORT_IMAGESEQUENCE, OnImportImageSequence)
    ON_COMMAND(ID_IMPORT_IMAGE, OnImportImage)
    ON_COMMAND(ID_MAKEFONT, MakeFont)
    ON_COMMAND(ID_VIEW_EXPORTASGIF, ExportAsGif)
    ON_UPDATE_COMMAND_UI(ID_IMPORT_IMAGESEQUENCE, OnUpdateImportImage)
    ON_UPDATE_COMMAND_UI(ID_IMPORT_IMAGE, OnUpdateImportImage)
    ON_UPDATE_COMMAND_UI(ID_VIEW_EXPORTASGIF, CResourceDocument::OnUpdateAlwaysOn)
    ON_UPDATE_COMMAND_UI(ID_ANIMATE, OnUpdateAnimate)
    ON_UPDATE_COMMAND_UI(ID_MAKEFONT, OnUpdateFont)
    ON_COMMAND(ID_EXPORT_CELASIMAGE, OnExportCelAsImage)
    ON_COMMAND(ID_EXPORT_LOOPASIMAGE, OnExportLoopAsImage)
    ON_COMMAND(ID_EXPORT_VIEWASIMAGE, OnExportViewAsImage)
    ON_COMMAND_RANGE(ID_RESOLUTION_320X200, ID_RESOLUTION_640X480, OnSetResolution)
    ON_UPDATE_COMMAND_UI_RANGE(ID_RESOLUTION_320X200, ID_RESOLUTION_640X480, OnUpdateResolution)
END_MESSAGE_MAP()

UINT NativeResolutionToID(NativeResolution resolution)
{
    return ID_RESOLUTION_320X200 + (UINT)resolution;
}
NativeResolution IDToNativeResolution(UINT id)
{
    return (NativeResolution)(id - ID_RESOLUTION_320X200);
}

void CNewRasterResourceDocument::OnUpdateResolution(CCmdUI *pCmdID)
{
    const RasterComponent &raster = GetComponent<RasterComponent>();
    if (appState->GetVersion().DefaultResolution == NativeResolution::Res320x200)
    {
        pCmdID->Enable(FALSE);
    }
    else
    {
        pCmdID->Enable(TRUE);
    }
    pCmdID->SetRadio(pCmdID->m_nID == NativeResolutionToID(raster.Resolution));
}

void CNewRasterResourceDocument::OnSetResolution(UINT nID)
{
    NativeResolution newResolution = IDToNativeResolution(nID);
    ApplyChanges<RasterComponent>(
        [newResolution](RasterComponent &raster)
    {
        RasterChange chnage;
        if (raster.Resolution != newResolution)
        {
            raster.Resolution = newResolution;
            chnage.hint |= RasterChangeHint::NewView;
        }
        return WrapRasterChange(chnage);
    });
}

CNewRasterResourceDocument::CNewRasterResourceDocument()
{
    // Add ourselves as a sync
    CResourceMap &map = appState->GetResourceMap();
    map.AddSync(this);

    _currentPaletteIndex = 0;
    _fLocked = false;
    _fApplyToAllCels = FALSE;
    _nCel = 0;
    _nLoop = 0;

    // These get set in SetResource
    _color = 0;
    _alternateColor = 0x0;
}

void CNewRasterResourceDocument::v_OnUndoRedo()
{
    _ValidateCelIndex();
    PostApplyChanges(&WrapRasterChange(RasterChangeHint::NewView));
    _UpdateHelper(RasterChange(RasterChangeHint::NewView));
}

void CNewRasterResourceDocument::SetNewResource(std::unique_ptr<ResourceEntity> pResource)
{
    SetResource(move(pResource));
}

void CNewRasterResourceDocument::SetResource(std::unique_ptr<ResourceEntity> pResource, int id)
{
    _checksum = id;
    const RasterComponent &raster = pResource->GetComponent<RasterComponent>();
    _color = raster.Traits.DefaultEditColor;
    _alternateColor = raster.Traits.DefaultEditAltColor;

    AddFirstResource(move(pResource));
    RefreshPaletteOptions();
    _ValidateCelIndex();
    _UpdateHelper(RasterChange(RasterChangeHint::NewView));
    _UpdateTitle();
}

void CNewRasterResourceDocument::PostApplyChanges(CObject *pObj)
{ 
    // If palette choice changed or view changed, then we need to update palette choice (if it's currently an embedded palette)
    // This has to be done before we calls views to update them, since they will ask us for the current palette to render with.
    RasterChangeHint hint = GetHint<RasterChangeHint>(pObj);
    if (IsFlagSet(hint, RasterChangeHint::NewView))
    {
        RefreshPaletteOptions();
    }
}

void CNewRasterResourceDocument::SetPenStyle(PenStyle penStyle)
{
    if (penStyle != _penStyle)
    {
        _penStyle = penStyle;
        _UpdateHelper(RasterChange(RasterChangeHint::PenStyle));
    }
}

void CNewRasterResourceDocument::SetViewColor(uint8_t color)
{
    _color = color;
    UpdateAllViewsAndNonViews(nullptr, 0, &WrapHint(RasterChangeHint::Color));
}

void CNewRasterResourceDocument::SetAlternateViewColor(uint8_t color)
{
    _alternateColor = color;
    UpdateAllViewsAndNonViews(nullptr, 0, &WrapHint(RasterChangeHint::Color));
}

COLORREF CNewRasterResourceDocument::SCIColorToCOLORREF(uint8_t color)
{
    // The correct thing to do is to check our traits
    COLORREF colorRef = RGB(255, 0, 255);
    const ResourceEntity *pResource = GetResource();
    if (pResource)
    {
        RGBQUAD rgb;
        const RasterComponent &raster = GetComponent<RasterComponent>();
        if (raster.Traits.PaletteType == PaletteType::VGA_256)
        {
            const PaletteComponent *palette = GetCurrentPaletteComponent();
            if (palette)
            {
                rgb = palette->Colors[color];
            }
            else
            {
                rgb = RGBQUAD();
            }
        }
        else
        {
            rgb = g_egaColors[color % 16];
        }
        colorRef = RGB(rgb.rgbRed, rgb.rgbGreen, rgb.rgbBlue);
    }
    return colorRef;
}

const RGBQUAD *CNewRasterResourceDocument::GetPaletteVGA() const
{
    if (_currentPaletteComponent)
    {
        return _currentPaletteComponent->Colors;
    }
    else
    {
        return nullptr;
    }
}

int CNewRasterResourceDocument::GetSelectedGroup(CelIndex *rgGroups, size_t ceGroup)
{
    int iRet = 0;
    if (_fApplyToAllCels)
    {
        const RasterComponent &raster = GetComponent<RasterComponent>();
        size_t iCelCount = raster.Loops[_nLoop].Cels.size();
        if (iCelCount <= ceGroup)
        {
            // The starting position for groups is relevant when paste multiple selections into a loop (when _fApplyToAllCels).
            int startAt = GetSelectedIndex().cel;
            iRet = iCelCount;
            for (size_t i = 0; i < iCelCount; i++)
            {
                rgGroups[i] = CelIndex(_nLoop, (int)((i + startAt) % iCelCount));
            }
        }
    }
    else
    {
        // Gets the  group indices of the selected cels in this resource. (e.g. all the cels of one view loop, if
        // drawing is applied to all cels).  By default, this is just the single selected cel.
        assert(ceGroup > 0);
        if (ceGroup > 0)
        {
            *rgGroups = GetSelectedIndex();
            iRet = 1;
        }
        else
        {
            iRet = 0;
        }
    }
    return iRet;
}

//
// Ensures the select loop/cel exist in the current resource.
//
void CNewRasterResourceDocument::_ValidateCelIndex(bool wrap)
{
    const RasterComponent &raster = GetComponent<RasterComponent>();
    raster.ValidateCelIndex(_nLoop, _nCel, wrap);
}

void CNewRasterResourceDocument::SetSelectedLoop(int nLoop, bool wrap)
{
    if (nLoop != _nLoop)
    {
        _nLoop = nLoop;
        _ValidateCelIndex(wrap);
        _UpdateHelper(RasterChange(RasterChangeHint::LoopSelection));
    }
}

void CNewRasterResourceDocument::_UpdateHelper(RasterChange change)
{
    UpdateAllViewsAndNonViews(nullptr, 0, &WrapObject(change.hint, change.index));
}

void CNewRasterResourceDocument::SetSelectedCel(int nCel, bool wrap)
{
    // It is essential to check this, otherwise we'll end up in an infinite loop!
    if (nCel != _nCel)
    {
        _nCel = nCel;
        _ValidateCelIndex(wrap);
        _UpdateHelper(RasterChange(RasterChangeHint::CelSelection));
    }
}

//
// Allows the callers to move "selection"
//
void CNewRasterResourceDocument::MoveSelectedCel(CPoint point, bool wrap)
{
    // if the y changed, that indicates a loop change.
    if (point.y != 0)
    {
        int nLoop = _nLoop + point.y;
        SetSelectedLoop(nLoop, wrap);
    }
    if (point.x != 0)
    {
        int nCel = _nCel + point.x;
        SetSelectedCel(nCel, wrap);
    }
}

int CNewRasterResourceDocument::GetDefaultZoom() const
{
    return GetComponent<RasterComponent>().Settings.DefaultZoom;
}
void CNewRasterResourceDocument::SetDefaultZoom(int iZoom) const
{
    GetComponent<RasterComponent>().Settings.DefaultZoom = iZoom;
}

// We need collect all the info first, then make a decision.
// We have: palette/cel combinations and gidplus images.
struct ImageSequenceItem
{
    ImageSequenceItem() {}
    ImageSequenceItem(ImageSequenceItem&& other)
    {
        Palette = other.Palette;
        Cels = other.Cels;
        Bitmap = move(other.Bitmap);
    }

    ImageSequenceItem& operator=(ImageSequenceItem&& other)
    {
        if (this != &other)
        {
            Palette = other.Palette;
            Cels = other.Cels;
            Bitmap = move(other.Bitmap);
        }
        return *this;
    }

    // Either we have:
    PaletteComponent Palette;
    std::vector<Cel> Cels;

    // Or we have a 24 bit bitmap in this form:
    std::unique_ptr<Bitmap> Bitmap;
};

const float IdealAspectRatio = 1.7f;

unique_ptr<Bitmap> _DrawImageSequenceToLargeBitmap(vector<ImageSequenceItem> &items, vector<CRect> &rectsOut)
{
    // Rules:
    // 1) If we have any Bitmaps, then we'll lose the palette.
    // 2) If we have more than one Bitmap, we'll lose the palette.
    vector<unique_ptr<Bitmap>> bitmaps;
    PixelFormat pixelFormat = PixelFormat8bppIndexed;       // We consider this ideal.
    PaletteComponent palette;
    int totalWidth = 0;
    int totalHeight = 0;
    int maxHeight = 0;
    int maxWidth = 0;
    for (auto &item : items)
    {
        if (item.Bitmap)
        {
            if (item.Bitmap->GetPixelFormat() != pixelFormat)
            {
                pixelFormat = PixelFormat32bppARGB;
            }
            totalWidth += item.Bitmap->GetWidth();
            totalHeight += item.Bitmap->GetHeight();
            maxHeight = max((int)item.Bitmap->GetHeight(), maxHeight);
            maxWidth = max((int)item.Bitmap->GetWidth(), maxWidth);
            bitmaps.push_back(move(item.Bitmap));
        }
        else
        {
            palette = item.Palette;
            for (auto &cel : item.Cels)
            {
                bitmaps.push_back(move(CelAndPaletteToBitmap(cel, palette, false)));
                totalWidth += cel.size.cx;
                totalHeight += cel.size.cy;
                maxHeight = max((int)cel.size.cy, maxHeight);
                maxWidth = max((int)cel.size.cx, maxWidth);
            }
        }
    }

    // Use the average aspect ratio to determine how to lay things out. This should really be handled
    // by the UI, but we're compiling all the images into a single bitmap prior to sending it to the dialog.
    float averageAspectRatio = (float)totalWidth / (float)totalHeight;
    // IGNORE THIS FOR NOW until we get stuff working.
    float avgToIdealAspectRatio = IdealAspectRatio / averageAspectRatio;
    // If this is close to 1, then in general we want the same number of rows as columns
    float rowsF = sqrt((float)bitmaps.size() / avgToIdealAspectRatio);
    int rows = max(1, (int)rowsF);
    int columns = max(1, (int)bitmaps.size() / rows);
    // Now figure out our rects before we make the big bitmap
    UINT width = max(maxWidth, columns * totalWidth / (int)bitmaps.size());
    int x = 0;
    int y = 0;
    int maxRowHeight = 0;
    for (auto &bitmap : bitmaps)
    {
        if ((x + bitmap->GetWidth()) > width)
        {
            y += maxRowHeight;
            maxRowHeight = 0;
            x = 0;
        }
        int right = x + bitmap->GetWidth();
        maxRowHeight = max(maxRowHeight, (int)bitmap->GetHeight());
        rectsOut.emplace_back(x, y, right, y + bitmap->GetHeight());
        x = right;
    }

    // Now we're ready to create the big bitmap
    UINT height = y + maxRowHeight;
    unique_ptr<Bitmap> bigBitmap(new Bitmap(width, height, pixelFormat));
    int currentCol = 0;
    if (bigBitmap)
    {
        // WOW! This is crucial. Our "non-scaled" image will be scaled if we don't set these to whatever
        // the default screen resolution is.
        REAL verticalDPI = bigBitmap->GetVerticalResolution();
        REAL horizontalDPI = bigBitmap->GetHorizontalResolution();

        std::unique_ptr<uint8_t[]> cp;
        if (pixelFormat == PixelFormat8bppIndexed)
        {
            // Each bitmap must have a palette in this case, so grab the one from the first one.
            INT paletteSize = bitmaps[0]->GetPaletteSize();
            cp = std::make_unique<uint8_t[]>(paletteSize);
            ColorPalette *colorPalette = reinterpret_cast<ColorPalette*>(cp.get());
            bitmaps[0]->GetPalette(colorPalette, paletteSize);
            bigBitmap->SetPalette(colorPalette);
        }

        Gdiplus::Graphics g(bigBitmap.get());

        if (pixelFormat == PixelFormat8bppIndexed)
        {
            // 8bpp won't get drawn to an 8bpp Graphics unless this is set:
            g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            // (despite Gdiplus still returning Ok from the DrawImage call)
        }

        int index = 0;
        for (auto &bitmap : bitmaps)
        {
            CRect &rect = rectsOut[index];
            bitmap->SetResolution(horizontalDPI, verticalDPI);
            int result = g.DrawImage(bitmap.get(), rect.left, rect.top, 0, 0, bitmap->GetWidth(), bitmap->GetHeight(), UnitPixel);
            index++;
        }
    }
    return bigBitmap;
}

bool CNewRasterResourceDocument::_GetColors(const RasterComponent &raster, const PaletteComponent *optionalNewPalette,
    const uint8_t **paletteMapping,
    int *colorCount,
    const RGBQUAD **colors
    )
{
    // TODO: Handle transparent colors?
    bool isEGA16 = false;
    *paletteMapping = optionalNewPalette ? optionalNewPalette->Mapping : raster.Traits.PaletteMapping;
    *colorCount = optionalNewPalette ? 256 : 0;
    *colors = optionalNewPalette ? optionalNewPalette->Colors : raster.Traits.Palette;
    switch (raster.Traits.PaletteType)
    {
        case PaletteType::VGA_256:
        {
            *colorCount = 256;
            const PaletteComponent *embedded = GetResource()->TryGetComponent<PaletteComponent>();
            if (embedded)
            {
                *colors = embedded->Colors;
            }
            else
            {
                const PaletteComponent *global = this->GetCurrentPaletteComponent();
                if (global)
                {
                    *colors = global->Colors;
                }
            }
            break;
        }
        case PaletteType::EGA_Four:
            *colorCount = 4;
            break;
        case PaletteType::EGA_Sixteen:
            isEGA16 = true;
            *colorCount = 16;
            break;
        case PaletteType::EGA_Two:
            *colorCount = 2;
            break;
    }
    return isEGA16;
}

void CNewRasterResourceDocument::_ApplyImageSequenceNew(uint8_t transparentColor, const PaletteComponent *currentPalette, std::vector<ImageSequenceItem> &items, bool fixedPalette, int paletteSize, bool replaceEntireLoop)
{
    vector<CRect> bitmapRects;
    unique_ptr<Bitmap> bigBitmap = _DrawImageSequenceToLargeBitmap(items, bitmapRects);
    size16 size((uint16_t)bigBitmap->GetWidth(), (uint16_t)bigBitmap->GetHeight());
    CBitmapToVGADialog dialog(
        move(bigBitmap),
        nullptr,
        currentPalette,
        fixedPalette,
        paletteSize,
        false,
        size,
        transparentColor,
        PaletteAlgorithm::MatchExisting,    // REQUIRED for fixed palette. So be careful if changing this.
        DefaultPaletteUsage::UsedColors,
        "Import images");
    
    if (dialog.DoModal() == IDOK)
    {
        int nLoop = GetSelectedLoop();
        int nCel = GetSelectedCel();
        auto &bigCel = dialog.GetFinalResult();
        transparentColor = bigCel->TransparentColor;

        auto finalPalette = dialog.GetFinalResultPalette();
        const PaletteComponent *optionalNewPalette = nullptr;
        if (!fixedPalette && (*finalPalette != *currentPalette))
        {
            // Only apply a palette if it changed...
            optionalNewPalette = finalPalette.get();
        }

        // Restore our big bitmap to the individual cels.
        vector<Cel> finalCels;
        for (auto &rect : bitmapRects)
        {
            finalCels.emplace_back(size16((uint16_t)rect.Width(), (uint16_t)rect.Height()), point16(0, 0), transparentColor);
            Cel &cel = finalCels.back();
            cel.Data.allocate(cel.GetDataSize());
            for (int y = 0; y < cel.size.cy; y++)
            {
                int sourceLine = bigCel->size.cy - 1 - y - rect.top;
                int destLine = cel.size.cy - 1 - y;// -rect.top;
                memcpy(&cel.Data[destLine * CX_ACTUAL(cel.size.cx)], &bigCel->Data[sourceLine * CX_ACTUAL(bigCel->size.cx) + rect.left], cel.size.cx);
            }
        }

        assert(replaceEntireLoop || finalCels.size() == 1);
            
        ApplyChanges<RasterComponent>(
            [replaceEntireLoop, nLoop, nCel, &finalCels](RasterComponent &raster)
        {
            RasterChange chnage;
            if (replaceEntireLoop)
            {
                chnage = ApplyCelsToLoop(raster, nLoop, finalCels);
            }
            else
            {
                chnage = ReplaceCel(raster, CelIndex(nLoop, nCel), finalCels.back());
            }
            chnage.hint |= RasterChangeHint::PaletteChoice;
            return WrapRasterChange(chnage);
        },

            // Apply the new palette if needed.
            [optionalNewPalette](ResourceEntity &resource)
        {
            if (optionalNewPalette)
            {
                if (resource.TryGetComponent<PaletteComponent>())
                {
                    PaletteComponent &existingPalette = resource.GetComponent<PaletteComponent>();
                    existingPalette = *optionalNewPalette;
                }
                else
                {
                    resource.AddComponent<PaletteComponent>(std::make_unique<PaletteComponent>(*optionalNewPalette));
                }
            }
        }
        );
    }
}

void CNewRasterResourceDocument::PrepareEGAOrVGAPalette(PaletteComponent &egaPalette, int &colorCount, bool &fixedPalette)
{

    const RasterComponent &raster = GetComponent<RasterComponent>();
    bool isVGA = (raster.Traits.PaletteType == PaletteType::VGA_256);
    if (isVGA)
    {
        if (GetCurrentPaletteComponent())
        {
            egaPalette = *GetCurrentPaletteComponent();
        }
        // else we can't do anything...
        colorCount = 256;
        fixedPalette = false;
    }
    else
    {
        const RGBQUAD *colors;
        const uint8_t *paletteMapping;
        _GetColors(raster, nullptr, &paletteMapping, &colorCount, &colors);
        memcpy(egaPalette.Mapping, paletteMapping, sizeof(*paletteMapping) * colorCount);
        memcpy(egaPalette.Colors, colors, sizeof(*colors) * colorCount);
        fixedPalette = true;
    }
}

void CNewRasterResourceDocument::_InsertFiles(const vector<string> &files, bool replaceEntireLoop)
{
    assert(replaceEntireLoop || (files.size() == 1));
    const RasterComponent &raster = GetComponent<RasterComponent>();
    bool isVGA = (raster.Traits.PaletteType == PaletteType::VGA_256);

    // Use the transparent color of the current cel.
    // Another option would be to set the transparent color automatically by detecting the least-used color in the
    // imported image.
    uint8_t transparentColor = raster.GetCel(GetSelectedIndex()).TransparentColor;

    // First, get a collection of usable images
    std::vector<ImageSequenceItem> imageSequenceItems;
    for (const string &file : files)
    {
        imageSequenceItems.emplace_back();
        ImageSequenceItem &item = imageSequenceItems.back();
        bool success = false;
        if (0 == lstrcmpi(".gif", PathFindExtension(file.c_str())))
        {
            // Try our gif loader, which understands palettes better than gdip
            success = GetCelsAndPaletteFromGIFFile(file.c_str(), item.Cels, item.Palette);
        }
        if (!success)
        {
            // Use gdiplus to load the image.
#ifdef UNICODE
            item.Bitmap.reset(Bitmap::FromFile(pszFileName));
#else
            // GDI+ only deals with unicode.
            BSTR unicodestr = SysAllocStringLen(NULL, file.length());
            MultiByteToWideChar(CP_ACP, 0, file.c_str(), file.length(), unicodestr, file.length());
            item.Bitmap.reset(Bitmap::FromFile(unicodestr));
            //... when done, free the BSTR
            SysFreeString(unicodestr);
#endif    
            success = item.Bitmap->GetLastStatus() == Gdiplus::Ok;
        }
        if (!success)
        {
            imageSequenceItems.pop_back();
        }
    }

    // If we got some, then figure out what to do
    if (!imageSequenceItems.empty())
    {
        // If we're only replacing a cel, just take the first image.
        // (We might have opened just a single file, but it might have been a gif with multiple frames)
        if (!replaceEntireLoop)
        {
            assert(imageSequenceItems.size() == 1);
            vector<Cel> &canOnlyBeOne = imageSequenceItems.back().Cels;
            if (!canOnlyBeOne.empty())
            {
                canOnlyBeOne.erase(canOnlyBeOne.begin() + 1, canOnlyBeOne.end());
            }
        }

        // At this point, if onePalette is not null, we should apply it to the view and directly copy any cel data (gdiplus bitmaps will still be remapped)
        // Otherwise, we should remap everything based on RGB.
        PaletteComponent tempPalette;
        int colorCount;
        bool fixedPalette;
        PrepareEGAOrVGAPalette(tempPalette, colorCount, fixedPalette);
        _ApplyImageSequenceNew(transparentColor, &tempPalette, imageSequenceItems, fixedPalette, colorCount, replaceEntireLoop);
    }

    // Reset the selection.
    _nCel = 0;
}

void CNewRasterResourceDocument::OnImportImage()
{
    // Create a file dialog.
    CFileDialog fileDialog(TRUE, nullptr, nullptr, OFN_ENABLESIZING | OFN_EXPLORER | OFN_NOCHANGEDIR, g_szGdiplusFilter);
    fileDialog.m_ofn.lpstrTitle = TEXT("Import image");

    if (IDOK == fileDialog.DoModal())
    {
        vector<string> fileList = { (PCSTR)fileDialog.GetPathName() };
        _InsertFiles(fileList, false);
    }
}

void CNewRasterResourceDocument::OnImportImageSequence()
{
    // Create a file dialog.
    CFileDialog fileDialog(TRUE, nullptr, nullptr, OFN_ENABLESIZING | OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR, g_szGdiplusFilter);
    fileDialog.m_ofn.lpstrTitle = TEXT("Import image sequence");

    // set a buffer to keep at least 100 full path and file names
    int bufferSize = 100 * (MAX_PATH + 1) + 1;
    unique_ptr<char[]> buffer = make_unique<char[]>(bufferSize);
    fileDialog.GetOFN().lpstrFile = buffer.get();
    fileDialog.GetOFN().nMaxFile = bufferSize;

    if (IDOK == fileDialog.DoModal())
    {
        vector<string> fileList;
        POSITION pos = fileDialog.GetStartPosition();
        while (pos != nullptr)
        {
            fileList.push_back((PCTSTR)fileDialog.GetNextPathName(pos));
        }
        sort(fileList.begin(), fileList.end());
        _InsertFiles(fileList, true);
    }
}

void CNewRasterResourceDocument::_OnExportAsImageWorker(CelIndex celIndex)
{
    const ResourceEntity *resource = GetResource();
    if (resource)
    {
        PaletteComponent *palette = _currentPaletteComponent.get();
        if (!palette)
        {
            palette = &g_egaDummyPalette;
        }
        CBitmap bitmap;
        SCIBitmapInfo bmi;
        BYTE *pBitsDest;
        bitmap.Attach(CreateBitmapFromResource(*resource, celIndex, palette, &bmi, &pBitsDest));
        if ((HBITMAP)bitmap)
        {
            // Construct a cel based on the bitmap. Throw away the HBITMAP.
            Cel celEntire(size16((uint16_t)bmi.bmiHeader.biWidth, (uint16_t)bmi.bmiHeader.biHeight), point16(), 0);
            celEntire.Data.allocate(celEntire.GetDataSize());
            celEntire.Data.assign(pBitsDest, pBitsDest + celEntire.GetDataSize());
            // Default extension should be the first one in the list for g_szGdiplus8BitSaveFilter
            CFileDialog fileDialog(FALSE, ".bmp", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, g_szGdiplus8BitSaveFilter);
            if (IDOK == fileDialog.DoModal())
            {
                CString strFileName = fileDialog.GetPathName();
                Save8BitBmpGdiP(strFileName, celEntire, *palette);
            }
        }
    }
}

void CNewRasterResourceDocument::OnExportCelAsImage()
{
    _OnExportAsImageWorker(GetSelectedIndex());
}
void CNewRasterResourceDocument::OnExportLoopAsImage()
{
    CelIndex celIndex = GetSelectedIndex();
    celIndex.cel = -1;
    _OnExportAsImageWorker(celIndex);
}
void CNewRasterResourceDocument::OnExportViewAsImage()
{
    _OnExportAsImageWorker(CelIndex(-1, -1));
}
void CNewRasterResourceDocument::OnUpdateEGA(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(GetComponent<RasterComponent>().Traits.PaletteType != PaletteType::VGA_256);
}

void CNewRasterResourceDocument::OnUpdateImportImage(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        !appState->_fNoGdiPlus &&
        ((GetComponent<RasterComponent>().Traits.Caps & RasterCaps::Animate) != RasterCaps::None)
        );
}

void CNewRasterResourceDocument::OnUpdateAnimate(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        ((GetComponent<RasterComponent>().Traits.Caps & RasterCaps::Animate) != RasterCaps::None)
        );
}

void CNewRasterResourceDocument::OnUpdateFont(CCmdUI *pCmdUI)
{
    const ResourceEntity *resource = GetResource();
    if (resource && resource->TryGetComponent<FontComponent>())
    {
        pCmdUI->Enable(TRUE);
    }
    else
    {
        pCmdUI->Enable(FALSE);
    }
}

void CNewRasterResourceDocument::OnAnimate()
{
    if (HasUndos() && !_fLocked)
    {
        CAnimateDialog dialog(nullptr, GetResource(), _currentPaletteComponent.get());
        dialog.SetTitle(GetTitle());
        dialog.SetLoop(_nLoop);
        dialog.DoModal();
    }
}

const TCHAR g_rgszHeaderFilter[] = TEXT("GIF Files (*.gif)|*.gif|All Files (*.*)|*.*");

void CNewRasterResourceDocument::ExportAsGif()
{
    SCI_RESOURCE_INFO &ri = GetResourceInfo(GetResource()->GetType());
    _ValidateCelIndex();
    // Set up the save dialog.
    std::string filespec = fmt::format("{0}{1}-{2}", ri.pszTitleDefault, GetResource()->ResourceNumber, _nLoop);
    CFileDialog fileDialog(FALSE,
        ".gif",
        filespec.c_str(),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR,
        g_rgszHeaderFilter);
    if (IDOK == fileDialog.DoModal())
    {
        CString strFileName = fileDialog.GetPathName();
        const RasterComponent &raster = GetComponent<RasterComponent>();

        int colorCount;
        const RGBQUAD *colors;
        const uint8_t *paletteMapping;
        if (_currentPaletteComponent)
        {
            colorCount = ARRAYSIZE(_currentPaletteComponent->Colors);
            colors = _currentPaletteComponent->Colors;
            paletteMapping = _currentPaletteComponent->Mapping;
        }
        else
        {
            _GetColors(raster, nullptr, &paletteMapping, &colorCount, &colors);
        }
        SaveCelsAndPaletteToGIFFile(strFileName, raster.Loops[_nLoop].Cels, colorCount, colors, paletteMapping, raster.Loops[_nLoop].Cels[0].TransparentColor);
    }
}

void CNewRasterResourceDocument::MakeFont()
{
    CFontDialog fontDialog;
    if (IDOK == fontDialog.DoModal())
    {
        LOGFONT *pLogFont = fontDialog.m_cf.lpLogFont;

        // Trying this
        pLogFont->lfQuality = PROOF_QUALITY;
        pLogFont->lfOutPrecision = OUT_RASTER_PRECIS;

        if (pLogFont)
        {
            ApplyChanges<RasterComponent, FontComponent>(
                [&](RasterComponent &raster, FontComponent &font)
            {
                InitFontFromLOGFONT(raster, font, pLogFont);
                return WrapHint(RasterChangeHint::Cel | RasterChangeHint::NewView);
            }
            );
        }
    }
}

void CNewRasterResourceDocument::OnCloseDocument()
{
    // Remove ourselves as a sync
    CResourceMap &map = appState->GetResourceMap();
    map.RemoveSync((IResourceMapEvents*)this);
    __super::OnCloseDocument();
}

void CNewRasterResourceDocument::OnResourceAdded(const ResourceBlob *pData, AppendBehavior appendBehavior)
{
    if (pData->GetType() == ResourceType::Palette)
    {
        RefreshPaletteOptions();
        UpdateAllViewsAndNonViews(nullptr, 0, &WrapHint(RasterChangeHint::NewView));
    }
}

bool CNewRasterResourceDocument::v_IsVGA()
{
    const RasterComponent &raster = GetComponent<RasterComponent>();
    return (raster.Traits.PaletteType == PaletteType::VGA_256);
}

void CNewRasterResourceDocument::v_OnUpdatePaletteOptions()
{
    _UpdateHelper(RasterChange(RasterChangeHint::NewView));
}
const ResourceEntity *CNewRasterResourceDocument::v_GetResource()
{
    return GetResource();
}
void CNewRasterResourceDocument::v_OnUpdatePaletteChoice()
{
    _UpdateHelper(RasterChange(RasterChangeHint::PaletteChoice));
}

OnionSkinFrameOptions::OnionSkinFrameOptions() : Enabled(false) {}
