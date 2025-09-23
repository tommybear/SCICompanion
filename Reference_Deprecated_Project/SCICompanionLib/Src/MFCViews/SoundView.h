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

#include "Sound.h"
#include "ColoredToolTip.h"

class CSoundDoc;
class CSoundView;
struct AudioComponent;
class AudioPlayback;

class DragEntry
{
public:
    DragEntry(CSoundView *pView, CPoint pt, int iconIndex) : _pView(pView), _ptOrig(pt), _ptDragged(pt), _iconIndex(iconIndex), _ptDeltaOrig(0) {}
    virtual void OnDragStart(CPoint pt) = 0;
    virtual void OnDragOver() = 0;
    virtual CRect GetRect() = 0;
    void UpdateToolTip(bool fShow);
    virtual std::string TipText() = 0;
    void OnDragMove(CPoint pt);
    virtual int GetIconIndex() { return _iconIndex; }
    virtual bool IsEnabled() { return true; }

protected:
    CSoundView *_pView;
    CPoint _ptDragged;  // The current drag point
    CPoint _ptOrig;     // Where the DragEntry started
    CPoint _ptDeltaOrig;// The difference between _ptOrig and the *initial* _ptDragged
    int _iconIndex;
};

// CSoundView view

class CSoundView : public CView
{
	DECLARE_DYNCREATE(CSoundView)

protected:
	CSoundView();           // protected constructor used by dynamic creation

public:
    ~CSoundView();
    CSoundDoc* GetDocument() const;
    const SoundComponent* GetSoundComponent() const;
    const AudioComponent* GetAudioComponent() const;
    virtual void OnDraw(CDC *);
    virtual BOOL OnEraseBkgnd(CDC *pDC);
    void SetTempo(WORD wTempo, bool setModified);
    WORD GetTempo() const;
    int XToTicks(int cx);
    int TicksToX(int ticks);
    CRect GetLoopMarkerRect(DWORD dwTicks);
    CRect GetCueMarkerRect(DWORD dwTicks);
    CColoredToolTip &ToolTip() { return _toolTip; }

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
    virtual void OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint);

protected:

    void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) override;

    afx_msg void OnPlay();
    afx_msg void OnPause();
    afx_msg void OnStop();
    afx_msg void OnUpdatePlay(CCmdUI *pCmdUI);
    afx_msg void OnUpdatePause(CCmdUI *pCmdUI);
    afx_msg void OnUpdateStop(CCmdUI *pCmdUI);
    afx_msg void OnUpdateTempoSlider(CCmdUI *pCmdUI);
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSize(UINT nType, int cx, int cy);
    void OnTimer(UINT_PTR nIDEvent);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonUp(UINT nFlags, CPoint point);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
    void OnMouseMove(UINT nFlags, CPoint point);

    bool _CanEditChannelMask();

    DECLARE_MESSAGE_MAP()
private:
    void _UpdateMidiPlayer();
    void _RecalculateChannelData(const SoundComponent *pSound);
    void _RecalculateChannelBitmaps();
    size_t _GetNumberOfChannelBitmaps();
    bool _PrepChannelBitmaps(int width, int height, size_t count);
    void _SetCursorPos();
    int _HitTestChannelHeader(CPoint pt, bool &hitHeader);
    void _InvalidateChannelHeaders();
    void _OnDrawTrackHeader(CDC *pDC, int channelId, int channel, bool fChannelOn);
    void _OnButtonDown(CPoint point);
    CRect _GetBottomMargin();
    CRect _GetLeftMargin();
    CRect _GetTrackArea();
    int _GetChannelHeight(int index);
    int _GetChannelY(int track);
    CPoint _GetLoopIconLocation();
    bool _IsInLoopTrack(CPoint pt);
    bool _IsInCueTrack(CPoint pt);
    int _HitTestDraggableThing(CPoint pt);
    bool _LoopPointInViewCoordinates(int &x);
    int _CuePointInViewCoordinates(const CuePoint &cue);
    void _ClearDrags();
    void _RedoDrags();

    DragEntry *_CurrentDrag()
    {
        if (_currentDragIndex == -1)
        {
            return nullptr;
        }
        else
        {
            return _drags[_currentDragIndex].get();
        }
    }
    void _InvalidateCurrentDragCursor();

    std::vector<std::unique_ptr<CBitmap>> _channelBitmaps;
    std::vector<int> _channelNumbers;
    int _loopBitmapIndex;
    int _cueBitmapIndex;
    int _activeCueBitmapIndex;
    CImageList _imageList;
    int _cursorPos;
    int _channelIdHover;

    std::vector<std::unique_ptr<DragEntry>> _drags;
    int _currentDragIndex;

    //
    uint16_t _tempoToSet;

    const static COLORREF ColorActiveTrack = RGB(64, 64, 255);
    const static COLORREF ColorTrackEvent = RGB(128, 128, 255);
    const static COLORREF ColorSilentTrack = RGB(196, 196, 196);
    const static COLORREF ColorTrackBackground = RGB(220, 220, 220);
    const static COLORREF ColorSelectedTrackBackgroundMask = RGB(255, 196, 196);
    const static COLORREF ColorSelectedTrackBackground = RGB(220, 196, 196);    // Combo of two above
    const static COLORREF ColorCursor = RGB(255, 0, 0);

    CColoredToolTip _toolTip;

    std::unique_ptr<AudioPlayback> _playback;
    std::unique_ptr<AudioComponent> _playbackAudio;
};

#ifndef _DEBUG  // debug version in TextView.cpp
inline CSoundDoc* CSoundView::GetDocument() const
   { return reinterpret_cast<CSoundDoc*>(m_pDocument); }
#endif


//
// Slider for trace image alpha
//
class CExtTempoSlider : public CExtBarSliderButton
{
public:
    CExtTempoSlider::CExtTempoSlider(CExtToolControlBar *pBar, UINT nCmdId) :
        CExtBarSliderButton(pBar, nCmdId, 0,
    		// scroll total/pos/page
            200, 120, 0,
		    // button extent horz(left/right)/vert(up/down) in pixels
		    // (if zeros - use slider-like layout instead of scrollbar-like)
		    0, 0,
		    // total slider control extent horz/vert in pixels
		    100, 100)
    {
        _pView = NULL;
    }

    void OnClick(CPoint point, bool down) override
    {
        if (!down)
        {
            int x = 0;
        }
        __super::OnClick(point, down);
    }

    void SetView(CSoundView *pView)
    {
        _pView = pView;
        // Update from view
        // Set range to 200, or 150% of current tempo, which ever is higher.
        //ScrollTotalRangeSet(max((_pView->GetTempo() * 3 / 2), 200));
        ScrollTotalRangeSet(400);   // Just some max value
        ULONG oldValue = ScrollPosSet((ULONG)_pView->GetTempo());
        if (oldValue != (ULONG)_pView->GetTempo())
        {
            // Why does it update by itself???
            this->RedrawButton(true);
        }
    }
    ULONG ScrollPosSet(ULONG nScrollPos)
    {
        // Push to view.
        ULONG nOldPos = __super::ScrollPosSet(nScrollPos);
        if (_pView)
        {
            // Note: It's possible we hold a ref to a _pView that gets
            // deleted, but this should be ok, since we'll never get here again unless
            // we are assigned a new view (since the scroll pos should only change if
            // 1) we are assigned a new view, or
            // 2) the user moves it, which means our view is still valid.
            _pView->SetTempo((WORD)max(1, nScrollPos), false);
        }
        return nOldPos;
    }

private:
    CSoundView *_pView;
};

extern CExtTempoSlider *g_pSoundTempoSlider;
