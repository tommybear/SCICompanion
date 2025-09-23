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
// GotoDialog.cpp : implementation file
//

#include "stdafx.h"
#include "AppState.h"
#include "GotoDialog.h"

// CGotoDialog dialog

CGotoDialog::CGotoDialog(CWnd* pParent /*=NULL*/)
	: CExtResizableDialog(CGotoDialog::IDD, pParent)
{
    _iLineNumber = 0;
}

CGotoDialog::~CGotoDialog()
{
}

void CGotoDialog::DoDataExchange(CDataExchange* pDX)
{
    if (!Title.empty())
    {
        SetWindowText(Title.c_str());
    }

    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT1, m_LineNumber);

    CString strLineNumber;
    m_LineNumber.GetWindowText(strLineNumber);
    _iLineNumber = StrToInt(strLineNumber);

    DDX_Control(pDX, IDOK, m_wndOK);
    DDX_Control(pDX, IDCANCEL, m_wndCancel);
    DDX_Control(pDX, IDC_STATIC1, m_wndStatic1);

    if (!Label.empty())
    {
        m_wndStatic1.SetWindowText(Label.c_str());
    }
}


BEGIN_MESSAGE_MAP(CGotoDialog, CExtResizableDialog)
END_MESSAGE_MAP()


// CGotoDialog message handlers
