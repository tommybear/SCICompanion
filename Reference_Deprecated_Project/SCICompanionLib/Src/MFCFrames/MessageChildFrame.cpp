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
#include "AppState.h"
#include "MessageChildFrame.h"

IMPLEMENT_DYNCREATE(CMessageChildFrame, CMDITabChildWnd)

CMessageChildFrame::CMessageChildFrame()
{
}

CMessageChildFrame::~CMessageChildFrame()
{
}

void CMessageChildFrame::HookUpNonViews(CDocument *pDoc)
{
    m_wndNounConditionPane.SetDocument(pDoc);
    m_wndMessageEditPane.SetDocument(pDoc);
}

BEGIN_MESSAGE_MAP(CMessageChildFrame, CMDITabChildWnd)
    ON_COMMAND(ID_PLAY, OnAudioPlay)
    ON_COMMAND(ID_RECORD, OnAudioRecord)
    ON_COMMAND(ID_STOP, OnAudioStop)
END_MESSAGE_MAP()

void CMessageChildFrame::OnAudioPlay()
{
    m_wndMessageEditPane.OnPlay();
}
void CMessageChildFrame::OnAudioRecord()
{
    m_wndMessageEditPane.OnRecord();
}
void CMessageChildFrame::OnAudioStop()
{
    m_wndMessageEditPane.OnStop();
}

int CMessageChildFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (__super::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_wndResizableBarNounCondition.SetInitDesiredSizeVertical(CSize(240, 100));
    if (!m_wndResizableBarNounCondition.Create("Nouns and conditions", this, ID_BAR_PICHELPER))
    {
        TRACE0("Failed to create m_wndResizableBarNounCondition\n");
        return -1;		// fail to create
    }
    m_wndResizableBarNounCondition.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
    if (!m_wndNounConditionPane.Create(MessageNounConditionPane::IDD, &m_wndResizableBarNounCondition))
    {
        TRACE0("Failed to create empty dialog\n");
        return -1;		// fail to create
    }

    m_wndResizableBarMessageEdit.SetInitDesiredSizeHorizontal(CSize(320, 210));
    if (!m_wndResizableBarMessageEdit.Create("Message details", this, ID_BAR_MESSAGEEDIT))
    {
        TRACE0("Failed to create m_wndResizableBarMessageEdit\n");
        return -1;		// fail to create
    }
    m_wndResizableBarMessageEdit.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
    if (!m_wndMessageEditPane.Create(MessageEditPane::IDD, &m_wndResizableBarMessageEdit))
    {
        TRACE0("Failed to create empty dialog\n");
        return -1;		// fail to create
    }

    if (!CExtControlBar::FrameEnableDocking(this))
    {
        ASSERT(FALSE);
        return -1;
    }

    m_wndResizableBarNounCondition.DockControlBar(AFX_IDW_DOCKBAR_RIGHT, 1, this, false);
    m_wndResizableBarMessageEdit.DockControlBar(AFX_IDW_DOCKBAR_BOTTOM, 2, this, false);

    return 0;
}
