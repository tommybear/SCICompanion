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
// SaveResourceDialog.cpp : implementation file
//

#include "stdafx.h"
#include "PicDimensionsDialog.h"
#include "format.h"

using namespace std;

// SaveResourceDialog dialog

PicDimensionsDialog::PicDimensionsDialog(size16 picSize, CWnd* pParent /*=NULL*/)
    : CExtResizableDialog(PicDimensionsDialog::IDD, pParent), _size(picSize)
{
}

void PicDimensionsDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
    ShowSizeGrip(FALSE);

    DDX_Control(pDX, IDC_EDITWIDTH, m_wndEditWidth);
    DDX_Control(pDX, IDC_EDITHEIGHT, m_wndEditHeight);
    m_wndEditHeight.LimitText(3);
    m_wndEditWidth.LimitText(3);

    m_wndEditWidth.SetWindowText(fmt::format("{0}", _size.cx).c_str());
    m_wndEditHeight.SetWindowText(fmt::format("{0}", _size.cy).c_str());
    
    // Visuals
    DDX_Control(pDX, IDOK, m_wndOk);
    DDX_Control(pDX, IDCANCEL, m_wndCancel);
    DDX_Control(pDX, IDC_STATIC1, m_wndLabel1);
    DDX_Control(pDX, IDC_STATIC2, m_wndLabel2);
    DDX_Control(pDX, IDC_STATIC3, m_wndLabel3);
}


BEGIN_MESSAGE_MAP(PicDimensionsDialog, CDialog)
END_MESSAGE_MAP()

void PicDimensionsDialog::OnOK()
{
    CString str;
    m_wndEditHeight.GetWindowText(str);
    _size.cy = (uint16_t)StrToInt(str);
    _size.cy = max(DEFAULT_PIC_HEIGHT, _size.cy);
    _size.cy = min(sPIC_HEIGHT_MAX, _size.cy);

    __super::OnOK();
}