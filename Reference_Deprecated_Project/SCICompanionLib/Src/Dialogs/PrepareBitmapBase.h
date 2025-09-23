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
#pragma once

struct PaletteComponent;

class PrepareBitmapBase
{
public:
    PrepareBitmapBase(int convertButtonId, int staticOriginalImageId, size16 picResourceDimensions);
    virtual ~PrepareBitmapBase();

protected:
    void _UpdateOrigBitmap(CWnd *pwnd);
    void _PrepareBitmapForConversion();
    BOOL _ReallocateCRBitmap(CSize size);
    BOOL _Init(std::unique_ptr<Gdiplus::Bitmap> pImage, IStream *imageStreamKeepAlive, CWnd *pwnd);
    void _ApplySettingsToCurrentBitmap();
    void _CalculateContrastCenter(Gdiplus::Bitmap *pBitmap, BYTE *pbContrastCenter);
    void _OnPasteFromClipboard(CWnd *pwnd);
    void _OnBrowse(CWnd *pwnd);

    int _convertButtonId;
    int _staticOriginalImageId;

    int _iScaleImage;

    int _nBrightness;
    int _nContrast;
    int _nSaturation;
    int _hue;
    bool _gammaCorrect;

    // Straight through
    COLORREF *_pCRBitmap;
    CSize _size;

    Gdiplus::Bitmap *_pbmpNormal;
    int _nContrastCenter;
    Gdiplus::Bitmap *_pbmpScaled;
    Gdiplus::Bitmap *_pbmpCurrent;
    std::unique_ptr<Gdiplus::Bitmap> _pbmpOrig;
    IStream *_imageStreamKeepAlive;
    std::unique_ptr<PaletteComponent> _originalPalette;
    int _numberOfPaletteEntries;
    uint8_t _bContrastCenterNormal;
    uint8_t _bContrastCenterScaled;

    bool _disableAllEffects;

    size16 _picResourceDimensions;
};