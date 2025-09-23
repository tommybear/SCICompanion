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
// MainFrm.h : interface of the CMainFrame class
//

#pragma once

#include "MDITabsDialogBar.h"
#include "RasterSidePane.h"
#include "MDITabChildWnd.h"
#include "PicCommandSidePane.h"
#include "QuickScripts.h"
#include "ExtControlBarNoX.h"
#include "BarContainerDialog.h"
#include "NewOutputPane.h"
#include "ScriptComboBox.h"
#include "SamplesDialogBar.h"
#include "SoundToolboxSidePane.h"
#include "MessageSidePane.h"

class CNewScriptDialog;
class GenerateDocsDialog;
class DependencyTracker;

// wparam is the type, lparam is a vector<CompileResults> that needs to be deleted.
#define UWM_RESULTS (WM_APP + 2)

void PurgeUnnecessaryResources();

class CExtMenuControlBarHideShow : public CExtMenuControlBar
{
protected:
    BOOL _UpdateMenuBar(BOOL bDoRecalcLayout = 1);
    bool _ExcludeMenu(PCSTR pszName);
};

class CBrowseInfoStatusPane : public CExtLabel, public IClassBrowserEvents
{
public:
    CBrowseInfoStatusPane();
    ~CBrowseInfoStatusPane();

    // IClassBrowserEvents
    // Note: this method may be called on a different thread.
    void NotifyClassBrowserStatus(BrowseInfoStatus status, int iPercent);

    void OnLButtonDblClk();

private:
    DECLARE_MESSAGE_MAP()
	void OnDrawLabelText(CDC &dc, const RECT &rcText, __EXT_MFC_SAFE_LPCTSTR strText, DWORD dwDrawTextFlags, bool bEnabled);
    LRESULT _OnStatusReady(WPARAM wParam, LPARAM lParam);
    BrowseInfoStatus _status;
    int _lastPercent;

    // Text to post back to the UI thread.
    std::string _textToPost;
    mutable std::mutex _csTextPosting;
};

class CMainFrame : public CExtNCW<CMDIFrameWnd>
{
    DECLARE_DYNAMIC(CMainFrame)    

public:
    CMainFrame();


// Attributes
public:

// Operations
public:

// Overrides
public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    void AddTab(CFrameWnd *pNewFrame, MDITabType iType);
    void RemoveTab(CFrameWnd *pNewFrame);
    void OnActivateTab(CFrameWnd *pWnd);
    void OnDeactivateTab(CFrameWnd *pWnd);
    CMDITabsDialogBar &Tabs() { return m_wndTab; }
    CBrowseInfoStatusPane &BrowseInfoStatus() { return m_BrowseInfoStatus; }

    NewOutputPane& GetOutputPane() { return m_wndNewOutput; }
    void ShowOutputPane(OutputPaneType type);
    BOOL DestroyWindow();

    void RefreshExplorerTools();

// Implementation
public:
    virtual ~CMainFrame();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

private:  // control bar embedded members
    CExtStatusControlBar m_wndStatusBar;
    CBrowseInfoStatusPane m_BrowseInfoStatus;

    CExtToolControlBar m_wndToolBar;
    CExtMenuControlBarHideShow m_wndMenuBar;

    // The tabs across the top
    CMDITabsDialogBar  m_wndTab;

    // The toolbar under the tab
    CExtToolControlBar m_wndPicTools;
    CExtToolControlBar m_wndScriptTools;
    CClassComboBox m_wndScriptToolComboBoxClass;
    //CMethodComboBox m_wndScriptToolComboBoxFunction;
    CExtToolControlBar m_wndTextTools;
    CExtToolControlBar m_wndSoundTools;
    CExtToolControlBar m_wndVocabTools;
    CExtToolControlBar m_wndViewTools;
    CExtToolControlBar m_wndExplorerTools;
    CExtToolControlBar m_wndMessageTools;

    // These combine to form a control bar that contains an empty dialog
    CExtControlBarNoX m_wndResizableBarGeneral;
    CBarContainerDialog m_dlgEmpty;

    // We don't actually show this, but we use its m_nMenuMarkerID member variable.
    CExtThemeSwitcherToolControlBar m_ThemeSwitcher;

    // The empty dialog can contain these subdialogs:
    RasterSidePane m_dlgForPanelDialogFont;
    RasterSidePane m_dlgForPanelDialogView;
    RasterSidePane m_dlgForPanelDialogCursor;
    PicCommandSidePane m_dlgForPanelDialogPic;
    PicCommandSidePane m_dlgForPanelDialogPicVGA;
    PicCommandSidePane m_dlgForPanelDialogPicEGAPoly;
    SoundToolboxSidePane m_dlgForPanelDialogSound;
    QuickScriptsSidePane m_dlgForPanelDialogScript;
    CSamplesDialogBar m_dlgForPanelDialogGame;
    MessageSidePane m_dlgForPanelDialogMessage;

    // The output window at the bottom (find and compile results)
    CExtControlBar m_wndResizableBarOutput;
    NewOutputPane m_wndNewOutput;

    // Our cache of the currently active document.
    CMDITabChildWnd *_pActiveFrame;
    bool _fDidntGetDocYet;

// Generated message map functions
private:
    void ActivateFrame(int nCmdShow);

    void _RefreshToolboxPanel(CFrameWnd *pWnd);
    void _ApplyIndicators(MDITabType tabType);
    void _RefreshToolboxPanelOnDeactivate(CFrameWnd *pWnd);
    void _OnNewScriptDialog(CNewScriptDialog &dialog);
    void _HideTabIfNot(MDITabType iTabTypeCurrent, MDITabType iTabTypeCompare, CExtControlBar &bar);
    void _FindInFilesOfType(ICompileLog &log, const std::string &srcFolder, PCTSTR pszWildcard, PCTSTR pszWhat, BOOL fMatchCase, BOOL fWholeWord);
    void _FindInTexts(ICompileLog &log, PCTSTR pszWhat, BOOL fMatchCase, BOOL fWholeWord);
    void _FindInVocab000(ICompileLog &log, PCTSTR pszWhat, BOOL fMatchCase, BOOL fWholeWord);
    void _AddFindResults(std::vector<char> &buffer, ICompileLog &log, PCTSTR pszFullPath, PCTSTR pszFileName, PCTSTR pszWhat, BOOL fMatchCase, BOOL fWholeWord);
    void _PrepareExplorerCommands();
    void _PrepareRasterCommands();
    void _PrepareScriptCommands();
    void _PrepareMainCommands();
    void _PreparePicCommands();
    void _PrepareTextCommands();
    void _PrepareMessageCommands();
    void _PrepareVocabCommands();
    void _PrepareSoundCommands();
    BOOL PreTranslateMessage(MSG* pMsg);
    LRESULT OnConstructPopupMenuCB(WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT OnOutputPaneResults(WPARAM wParam, LPARAM lParam);
    LRESULT OnExtMenuPrepare(WPARAM wParam, LPARAM);
    void _EnumeratePlugins(CExtPopupMenuWnd &menu);

    afx_msg void OnWindowPosChanged(WINDOWPOS *wp);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnFileNewPic();
    afx_msg void OnFileNewGame();
    afx_msg void OnFileNewView();
    afx_msg void OnFileNewFont();
    afx_msg void OnFileNewCursor();
    afx_msg void OnFileNewText();
    afx_msg void OnFileNewMessage();
    afx_msg void OnFileNewPalette();
    afx_msg void OnFileNewSound();
    afx_msg void OnFileOpenResource();
    afx_msg void OnFileAddResource();
    afx_msg void OnUpdateNewPic(CCmdUI *pCmdUI);
    afx_msg void OnUpdateNewMessage(CCmdUI *pCmdUI);
    afx_msg void OnUpdateShowIfGameLoaded(CCmdUI *pCmdUI);
    afx_msg void OnUpdateShowIfSupportsAudio(CCmdUI *pCmdUI);
    afx_msg void OnUpdateStopDebugging(CCmdUI *pCmdUI);
    afx_msg void OnShowPreferences();
    afx_msg void OnShowAudioPreferences();
    afx_msg void OnStopDebugging();
    afx_msg void OnRebuildResources();
    afx_msg void OnRepackageAudio();
    afx_msg void OnRebuildClassTable();
    afx_msg void OnExtractAllResources();
    afx_msg BOOL OnShowResource(UINT nId);
    afx_msg void OnIdleUpdateCmdUI();
    afx_msg void OnNewRoom();
    afx_msg void OnNewScript();
    afx_msg void OnCompileAll();
    afx_msg void OnUpdateNewPalette(CCmdUI *pCmdUI);
    afx_msg void OnUpdateAlwaysEnabled(CCmdUI *pCmdUI) { pCmdUI->Enable(); }
    afx_msg void OnFindInFiles();
    afx_msg void OnUpdateBackForward(CCmdUI *pCmdUI);
    afx_msg void OnUpdateScriptCombo(CCmdUI *pCmdUI);
    afx_msg void OnGoForward();
    afx_msg void OnGoBack();
    afx_msg void OnClassBrowser();
    afx_msg void OnManageDecompilation();
    afx_msg void OnUpdateClassBrowser(CCmdUI *pCmdUI);
    afx_msg void OnRunPlugin(UINT nID);
    afx_msg void OnValidateAllSaids();
    afx_msg void OnUpdateValidateAllSaids(CCmdUI *pCmdUI);
    afx_msg void ExtractAllScriptStrings();

#ifdef DOCSUPPORT
    afx_msg void OnGenerateDocs();
#endif
    // afx_msg void OnTimer(UINT_PTR nIDEvent);
    // afx_msg void OnSelfTest();

    DECLARE_MESSAGE_MAP()

    int _fMatchWholeWord;
    int _fMatchCase;
    int _fFindInAll;
    bool _fSelfTest;

    WINDOWPLACEMENT m_dataFrameWP;

#ifdef DOCSUPPORT
    std::unique_ptr<GenerateDocsDialog> _docsDialog;
#endif

    std::vector<std::string> _pluginExes;
};

bool CompileABunchOfScripts(AppState *appState, DependencyTracker *dependencyTracker);
