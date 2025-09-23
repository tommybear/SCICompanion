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

#include "resource.h"       // main symbols
#include "ResourceMap.h"
#include "ResourceRecency.h"
#include "IntellisenseListBox.h"
#include "ColoredToolTip.h"
#include "CompileInterfaces.h"
#include "RunLogic.h"

class AutoCompleteThread2;
class CScriptView;

struct HoverTipPayload;
struct HoverTipResponse;

template<typename _TPayload, typename _TResponse>
class BackgroundScheduler;

// FWD decl
class CScriptDoc;
class ResourceEntity;
class CResourceListDoc;
class AppState;
class SCIClassBrowser;
class DependencyTracker;
struct AudioProcessingSettings;

template<typename _TPayload, typename _TResponse>
class BackgroundScheduler;

//
// This allows us to hook up the slider bar, celdialogbar, and other "non views"
//
class CMultiDocTemplateWithNonViews : public CMultiDocTemplate
{
public:
    CMultiDocTemplateWithNonViews(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);
    virtual void InitialUpdateFrame(CFrameWnd *pFrame, CDocument *pDoc, BOOL bMakeVisible);
};

class AppState : public ISCIAppServices
{
public:
    AppState(CWinApp *pApp = nullptr);
    ~AppState();

private:
    CWinApp *_pApp;

    // Overrides
public:
    BOOL InitInstance();
    int ExitInstance();

    CMultiDocTemplate *GetPicTemplate() { return _pPicTemplate; }
    CMultiDocTemplate *GetVocabTemplate() { return _pVocabTemplate; }
    CMultiDocTemplate *GetViewTemplate() { return _pViewTemplate; }
    CMultiDocTemplate *GetTextTemplate() { return _pTextTemplate; }
    CMultiDocTemplate *GetSoundTemplate() { return _pSoundTemplate; }
    CMultiDocTemplate *GetFontTemplate() { return _pFontTemplate; }
    CMultiDocTemplate *GetCursorTemplate() { return _pCursorTemplate; }
    CMultiDocTemplate *GetScriptTemplate() { return _pScriptTemplate; }
    CMultiDocTemplate *GetMessageTemplate() { return _pMessageTemplate; }
    CMultiDocTemplate *GetPaletteTemplate() { return _pPaletteTemplate; }
    
    void OpenScript(std::string strName, const ResourceBlob *pData = NULL, WORD wScriptNum = InvalidResourceNumber);
    void OpenScript(WORD w);
    void OpenScriptHeader(std::string strName);
    void OpenScriptAtLine(ScriptId script, int iLine);
    void OpenMostRecentResource(ResourceType type, uint16_t wNum);
    void ReopenScriptDocument(uint16_t wNum);
    void OpenMostRecentResourceAt(ResourceType type, uint16_t number, int index);
    void SetScriptFrame(CFrameWnd *pScriptFrame) { _pScriptFrame = pScriptFrame; }
    CResourceMap &GetResourceMap() { return _resourceMap; }
    const SCIVersion &GetVersion() const { return _resourceMap.GetSCIVersion(); }
    UINT GetCommandClipboardFormat() { return _uClipboardFormat; }
    CDocument* OpenDocumentFile(PCTSTR lpszFileName);
    int GetSelectedViewResourceNumber();
    std::vector<int> &GetRecentViews();
    void SetExportFolder(LPITEMIDLIST pidl); // Takes ownership
    const LPITEMIDLIST GetExportFolder() { return _pidlFolder; }
    CIntellisenseListBox &GetIntellisense() { return m_wndIntel; }
    CColoredToolTip &GetToolTipCtrl() { return m_wndToolTip; }
    bool IsBrowseInfoEnabled() { return _fBrowseInfo != 0; }
    BOOL IsCodeCompletionEnabled() { return _fBrowseInfo && _fCodeCompletion; }
    BOOL AreHoverTipsEnabled() { return _fBrowseInfo && _fHoverTips; }
    BOOL IsScriptNavEnabled() { return _fBrowseInfo && _fScriptNav; }
    void GenerateBrowseInfo();
    void ResetClassBrowser();
    void ClearResourceManagerDoc() { _pResourceDoc = NULL; }
    void NotifyChangeAspectRatio();
    void NotifyChangeShowTabs();
    void TellScriptsToSave();

    void TerminateDebuggedProcess();
    bool IsProcessBeingDebugged();

    int AspectRatioY(int value) const;
    int InverseAspectRatioY(int value) const;

    void GiveMeAutoComplete(CScriptView *pSV);
    void HideTipWindows();

    // Output pane
    void OutputResults(OutputPaneType type, std::vector<CompileResult> &compileResults);
    // For finer control
    void ShowOutputPane(OutputPaneType type);
    void OutputClearResults(OutputPaneType type);
    void OutputAddBatch(OutputPaneType type, std::vector<CompileResult> &compileResults);
    void OutputFinishAdd(OutputPaneType type);

    UINT GetMidiDeviceId();

    // Game explorer
    void ShowResourceType(ResourceType iType);
    ResourceType GetShownResourceType();
    void SetExplorerFrame(CFrameWnd *pFrame) { _pExplorerFrame = pFrame; }

    DependencyTracker &GetDependencyTracker();
    SCIClassBrowser &GetClassBrowser();

    // Game properties
    std::string GetGameName();
    void SetGameName(PCTSTR pszName);

    void RunGame(bool debug, int optionalResourceNumber);

    // ISCIAppServices
    void OnGameFolderUpdate() override;
    void SetRecentlyInteractedView(int resourceNumber) override;

    void LogInfo(const TCHAR *pszFormat, ...);

    // Global settings:
    int _cxFakeEgo;
    int _cyFakeEgo;
    BOOL _fUseBoxEgo;
    int _fScaleTracingImages;
    BOOL _fDontShowTraceScaleWarning;
    BOOL _fUseAutoSuggest;
    bool _fAllowBraceSyntax;
    BOOL _fAutoLoadGame;
    BOOL _fDupeNewCels;
    BOOL _fShowPolyDotted;
    BOOL _fBrowseInfo;
    BOOL _fScriptNav;
    BOOL _fCodeCompletion;
    BOOL _fHoverTips;
    BOOL _fPlayCompileErrorSound;
    BOOL _fUseOriginalAspectRatioDefault;
    BOOL _fShowTabs;
    BOOL _fShowToolTips;
    BOOL _fSaveScriptsBeforeRun;
    BOOL _fTrackHeaderFiles;
    BOOL _fCompileDirtyScriptsBeforeRun;
    DWORD _onionLeftTint;
    DWORD _onionRightTint;
    BOOL _onionLeftOnTop;
    BOOL _onionRightOnTop;
    BOOL _onionWrap;

    // This is a hack, but we're making this as a spot fix to allow
    // for per-game aspect ratio.
    bool _fUseOriginalAspectRatioCached;

    std::string _midiDeviceName;
    std::unique_ptr<AudioProcessingSettings> _audioProcessing;
    std::string _docGenFolder;
    std::string _docGenCommand;

    BOOL _fNoGdiPlus;   // GDI+ is not available

    // This setting is not persisted across instances of the app:
    BOOL _fDontCheckPic;

    // Last known position of a fake ego:
    CPoint _ptFakeEgo;
    // Last selected views
    std::vector<int> _recentViews;
    bool _fObserveControlLines; // Does fake ego observe control lines?
    bool _fObservePolygons;     // Does it observe polygons?

    // Prof-UIS command profile
    PCSTR _pszCommandProfile;

    ResourceRecency _resourceRecency;

    UINT CelDataClipboardFormat;
    UINT ViewAttributesClipboardFormat;
    UINT PaletteColorsClipboardFormat;
    UINT EGAPaletteColorsClipboardFormat;

public: // TODO for now
    HRESULT _GetGameStringProperty(PCTSTR pszProp, PTSTR pszValue, size_t cchValue);
    HRESULT _SetGameStringProperty(PCTSTR pszProp, PCTSTR pszValue);
    HRESULT _GetGameIni(PTSTR pszValue, size_t cchValue);

    Gdiplus::GdiplusStartupInput _gdiplusStartupInput;
    ULONG_PTR _gdiplusToken;

    // Do not free these member.  It is just here for convenience.
    CMultiDocTemplate *_pPicTemplate;
    CMultiDocTemplate *_pVocabTemplate;
    CMultiDocTemplate *_pViewTemplate;
    CMultiDocTemplate *_pResourceTemplate;
    CMultiDocTemplate *_pScriptTemplate;
    CMultiDocTemplate *_pTextTemplate;
    CMultiDocTemplate *_pSoundTemplate;
    CMultiDocTemplate *_pFontTemplate;
    CMultiDocTemplate *_pCursorTemplate;
    CMultiDocTemplate *_pRoomExplorerTemplate;
    CMultiDocTemplate *_pMessageTemplate;
    CMultiDocTemplate *_pPaletteTemplate;

    CFrameWnd *_pScriptFrame;

    // Game explorer
    CResourceListDoc *_pResourceDoc;
    ResourceType _shownType;
    CFrameWnd *_pExplorerFrame;

    CResourceMap _resourceMap;

    UINT _uClipboardFormat;

    CFile _logFile;

    // Last folder for exporting resources
    LPITEMIDLIST _pidlFolder;

    CIntellisenseListBox m_wndIntel;
    CColoredToolTip m_wndToolTip;
    AutoCompleteThread2 *_pACThread;

    std::unique_ptr<BackgroundScheduler<HoverTipPayload, HoverTipResponse>> _pHoverTipScheduler;

    ScopedHandle _hProcessDebugged;

    RunLogic _runLogic;

    std::unique_ptr<DependencyTracker> _dependencyTracker;
    std::unique_ptr<SCIClassBrowser> _classBrowser;
};

extern AppState *appState;