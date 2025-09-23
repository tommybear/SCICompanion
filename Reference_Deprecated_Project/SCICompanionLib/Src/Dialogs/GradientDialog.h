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

#include "BaseColorDialog.h"
#include "ChooseColorStatic.h"
#include "ColorDialogCallback.h"
#include "PaletteDefinitionCallback.h"
#include "resource.h"

struct PaletteComponent;

class GradientDialog : public CExtResizableDialog
{
public:
    GradientDialog(PaletteComponent &palette, IVGAPaletteDefinitionCallback *callback, uint8_t start, uint8_t end, CWnd* pParent = NULL);  // standard constructor

    // Dialog Data
    enum { IDD = IDD_DIALOGGRADIENTS };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    void _SyncPalette();

    DECLARE_MESSAGE_MAP()

    // Visuals
    CExtButton m_wndOk;
    CExtButton m_wndCancel;
    CExtButton m_wndCenter;
    CExtButton m_wndEdges;

    CExtGroupBox m_wndGroupGradientType;
    CExtRadioButton m_wndRadioGradientLinear;
    CExtRadioButton m_wndRadioGradientCenter;

    bool _initialized;
    int _gradientType;
    PaletteComponent &_palette;
    uint8_t _start;
    uint8_t _endInclusive;
    COLORREF _edge;
    COLORREF _center;
    IVGAPaletteDefinitionCallback *_callback;
public:
    afx_msg void OnBnClickedButtoncenter();
    afx_msg void OnBnClickedButtonedges();
    afx_msg void OnBnClickedRadiolinear();
    afx_msg void OnBnClickedRadiocenter();
};
