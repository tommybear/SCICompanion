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
#include "GradientDialog.h"
#include "PaletteOperations.h"

GradientDialog::GradientDialog(PaletteComponent &palette, IVGAPaletteDefinitionCallback *callback, uint8_t start, uint8_t end, CWnd* pParent)
    : CExtResizableDialog(GradientDialog::IDD, pParent), _palette(palette), _callback(callback), _start(start), _endInclusive(end), _gradientType(0), _initialized(false)
{
    // 256 things here.
    memset(&_edge, 0, sizeof(_edge));
    memset(&_center, 255, sizeof(_center));
}

void GradientDialog::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);

    if (!_initialized)
    {
        DDX_Control(pDX, IDC_STATIC2, m_wndGroupGradientType);
        DDX_Control(pDX, IDC_RADIOLINEAR, m_wndRadioGradientLinear);
        DDX_Control(pDX, IDC_RADIOCENTER, m_wndRadioGradientCenter);

        // Visuals
        DDX_Control(pDX, IDOK, m_wndOk);
        DDX_Control(pDX, IDCANCEL, m_wndCancel);
        DDX_Control(pDX, IDC_BUTTONCENTER, m_wndCenter);
        DDX_Control(pDX, IDC_BUTTONEDGES, m_wndEdges);

        _SyncPalette();

        _initialized = true;
    }

    DDX_Radio(pDX, IDC_RADIOLINEAR, _gradientType);
}

BEGIN_MESSAGE_MAP(GradientDialog, CExtResizableDialog)
    ON_BN_CLICKED(IDC_BUTTONCENTER, &GradientDialog::OnBnClickedButtoncenter)
    ON_BN_CLICKED(IDC_BUTTONEDGES, &GradientDialog::OnBnClickedButtonedges)
    ON_BN_CLICKED(IDC_RADIOLINEAR, &GradientDialog::OnBnClickedRadiolinear)
    ON_BN_CLICKED(IDC_RADIOCENTER, &GradientDialog::OnBnClickedRadiocenter)
END_MESSAGE_MAP()

void _ApplyProgression(int progression1000, RGBQUAD &destination, RGBQUAD one, RGBQUAD two)
{
    progression1000 = min(1000, progression1000);
    int invProgression1000 = 1000 - progression1000;
    // 1 at edges, 0 at center. 
    destination.rgbBlue = (progression1000 * one.rgbBlue + invProgression1000 * two.rgbBlue) / 1000;
    destination.rgbRed = (progression1000 * one.rgbRed + invProgression1000 * two.rgbRed) / 1000;
    destination.rgbGreen = (progression1000 * one.rgbGreen + invProgression1000 * two.rgbGreen) / 1000;
}

void GradientDialog::_SyncPalette()
{
    if (_gradientType == 1)
    {
        // Centered
        int centerIndex = ((int)_endInclusive + (int)_start) / 2;
        RGBQUAD centerRGB = RGBQUADFromCOLORREF(_center);
        RGBQUAD edgeRGB = RGBQUADFromCOLORREF(_edge);
        for (int i = _start; i <= (int)_endInclusive; i++)
        {
            int progression1000 = abs(centerIndex - i) * 1000 / (centerIndex - _start);
            _ApplyProgression(progression1000, _palette.Colors[i], edgeRGB, centerRGB);
        }
    }
    else if (_gradientType == 0)
    {
        // Linear
        RGBQUAD startRGB = RGBQUADFromCOLORREF(_edge);
        RGBQUAD endRGB = RGBQUADFromCOLORREF(_center);
        for (int i = _start; i <= (int)_endInclusive; i++)
        {
            int progression1000 = (_endInclusive - i) * 1000 / (_endInclusive - _start);
            _ApplyProgression(progression1000, _palette.Colors[i], endRGB, startRGB);
        }
    }

    if (_callback)
    {
        _callback->OnVGAPaletteChanged();
    }
}

void GradientDialog::OnBnClickedButtoncenter()
{
    CExtColorDlg colorDialog(_center, _center);
    colorDialog.m_strCaption = "Edit color A";
    if (IDOK == colorDialog.DoModal())
    {
        _center = colorDialog.m_clrNew;
    }
    _SyncPalette();
}

void GradientDialog::OnBnClickedButtonedges()
{
    CExtColorDlg colorDialog(_edge, _edge);
    colorDialog.m_strCaption = "Edit color B";
    if (IDOK == colorDialog.DoModal())
    {
        _edge = colorDialog.m_clrNew;
    }
    _SyncPalette();
}


void GradientDialog::OnBnClickedRadiolinear()
{
    _gradientType = 0;
    _SyncPalette();
}


void GradientDialog::OnBnClickedRadiocenter()
{
    _gradientType = 1;
    _SyncPalette();
}
