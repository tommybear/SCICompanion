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
// SCIPicEditor.cpp : Defines the class behaviors for the application.
//

//
// SCICompanion:
// Major architectural issues:
//  - ScriptOM: assignment operators and copy constructors modify the source object
//  - ScriptOM: getting methods/properties/etc from these objects exposes too much of the internals.
//


#include "stdafx.h"

#include "AppState.h"
#include "MainFrm.h"
#include "PicChildFrame.h"


#include "PicDoc.h"
#include "PicView.h"

#include "VocabDoc.h"
#include "VocabChildFrame.h"
#include "VocabView.h"

#include "ResourceListDoc.h"
#include "GameExplorerFrame.h"
#include "GameExplorerView.h"

#include "ScriptFrame.h"
#include "ScriptDocument.h"
#include "ScriptView.h"

#include "ViewChildFrame.h"
#include "RasterView.h"

#include "TextChildFrame.h"
#include "MessageChildFrame.h"
#include "TextDoc.h"
#include "TextView.h"

#include "MessageDoc.h"
#include "MessageView.h"

#include "SoundChildFrame.h"
#include "SoundDoc.h"
#include "SoundView.h"

#include "CursorChildFrame.h"

#include "FontChildFrame.h"

#include "RoomExplorerFrame.h"
#include "RoomExplorerDoc.h"
#include "RoomExplorerView.h"

#include "PaletteChildFrame.h"
#include "PaletteView.h"
#include "PaletteDoc.h"

#include "ResourceEntity.h"

#include "GamePropertiesDialog.h"

#include "ColoredToolTip.h"

#include "CrystalScriptStream.h"
#include "CodeAutoComplete.h"

#include "View.h"
#include "NewRasterResourceDocument.h"

#include "crc.h"

#include "SCICompanion.h"
#include "AppState.h"

#include "GameVersionDialog.h"

#include "AudioProcessingSettings.h"

#include "PicClipsDialog.h"

// REVIEW temp
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const char c_szExecutableString[] = "Executable";
static const char c_szExeParametersString[] = "ExeCmdLineParameters";
static const char c_szDefaultExe[] = "sciv.exe";


//
// Handle parsing /log log.txt
//
class CSCICommandLineInfo : public CCommandLineInfo
{
public:
    CSCICommandLineInfo()
    {
        m_bLogFlag = FALSE;
    }
    void ParseParam(const TCHAR *pszParam, BOOL bFlag, BOOL bLast)
    {
        if (m_bLogFlag && !bFlag)
        {
            m_strLogFile = pszParam;
        }
        m_bLogFlag = (0 == lstrcmpi(pszParam, TEXT("log")));
        __super::ParseParam(pszParam, bFlag, bLast);
    }

    CString m_strLogFile;

private:
    BOOL m_bLogFlag;
};

// SCICompanionApp

BEGIN_MESSAGE_MAP(SCICompanionApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_HELP_ONLINETUTORIALS, OnTutorials)
    ON_COMMAND(ID_HELP_FORUMS, OnForums)
    ON_COMMAND(ID_HELP_ONLINESCIDOCUMENTATION, OnDocumentation)
    ON_COMMAND(ID_HELP_GETLATESTUPDATES, OnUpdates)
    ON_COMMAND(ID_HELP_EMAIL, OnEmail)
    ON_COMMAND(ID_HELP_GETGAMES, GetGames)
    ON_COMMAND(ID_HELP_SCICOMPANION, OnSCICompHelp)
    // Standard file based document commands
    ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
    ON_COMMAND(ID_RUNGAME, OnRunGame)
    ON_COMMAND(ID_DEBUGGAME, OnDebugGame)
    ON_COMMAND(ID_ROOM_EXPLORER, OnRoomExplorer)
    ON_COMMAND(ID_FILE_CLOSEGAME, OnCloseGame)
    ON_COMMAND(ID_GAME_PROPERTIES, OnGameProperties)
    ON_COMMAND(ID_GAME_VERSIONDETECTION, OnGameVersionDetection)
    ON_COMMAND(ID_OPEN_EXPLORERWINDOW, OnWindowsExplorer)
    ON_COMMAND(ID_PLUGINS_OPENPLUGINSFOLDER, OnOpenPluginsFolder)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSEGAME, OnUpdateGameLoaded)
    ON_UPDATE_COMMAND_UI(ID_OPEN_EXPLORERWINDOW, OnUpdateGameLoaded)
    ON_UPDATE_COMMAND_UI(ID_GAME_PROPERTIES, OnUpdateGameLoaded)
    ON_UPDATE_COMMAND_UI(ID_GAME_VERSIONDETECTION, OnUpdateGameLoaded)
    ON_UPDATE_COMMAND_UI(ID_ROOM_EXPLORER, OnUpdateGameLoaded)
END_MESSAGE_MAP()


// SCICompanionApp construction

SCICompanionApp::SCICompanionApp()
{
    m_eHelpType = afxHTMLHelp;
}

SCICompanionApp::~SCICompanionApp()
{
}


void _DestroyPicClipsDialog()
{
    if (g_PicClipsDialog)
    {
        g_PicClipsDialog->DestroyWindow();
        g_PicClipsDialog = nullptr;
    }
}


//
// LoadGame - this is where we load a new game.
//
// If we are opening a folder, append resource.map
//
CDocument* SCICompanionApp::OpenDocumentFile(PCTSTR lpszFileName)
{
    TCHAR szBuffer[MAX_PATH];
    if (SUCCEEDED(StringCchCopy(szBuffer, ARRAYSIZE(szBuffer), lpszFileName)))
    {
        PTSTR pszFileName = StrStrI(szBuffer, TEXT("resource.map"));
        if (pszFileName == NULL)
        {
            PathCombine(szBuffer, lpszFileName, TEXT("resource.map"));
        }

        // FEATURE - before closing everything, ask the user if they want to just close the resource
        // map.

        // Save any modified documents:
        if (SaveAllModified())
        {
            // Then close them all.
            CloseAllDocuments(FALSE);
            _DestroyPicClipsDialog();

            appState->_pResourceDoc = static_cast<CResourceListDoc*>(__super::OpenDocumentFile(szBuffer));
            if (appState->_pResourceDoc)
            {
                // Override the title - get the game name
                PCTSTR pszName = NULL;
                std::string strGameName = appState->GetGameName();
                std::string gameFolder = appState->GetResourceMap().GetGameFolder(); // Keep gameFolder in scope...
                if (!strGameName.empty())
                {
                    pszName = strGameName.c_str();
                }
                else
                {
                    pszName = gameFolder.c_str(); // Keep gameFolder in scope.
                    pszName = PathFindFileName(pszName);
                }
                if (pszName)
                {
                    TCHAR szTitle[MAX_PATH];
                    StringCchPrintf(szTitle, ARRAYSIZE(szTitle), TEXT("Explorer (%s)"), pszName);
                    appState->_pResourceDoc->SetTitle(szTitle);
                }
            }
            return appState->_pResourceDoc;
        }
    }
    return nullptr;
}

// The one and only SCICompanionApp object
SCICompanionApp theApp;

// SCICompanionApp initialization
BOOL SCICompanionApp::InitInstance()
{
    bool resetSettings = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    appState = new AppState(this);
    appState->InitInstance();
    CWinApp::InitInstance();

    // Initialize OLE libraries
    if (!AfxOleInit())
    {
        AfxMessageBox(IDP_OLE_INIT_FAILED);
        return FALSE;
    }
    AfxEnableControlContainer();

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    // of your final executable, you should remove from the following
    // the specific initialization routines you do not need
    // Change the registry key under which our settings are stored
    SetRegistryKey(_T("mtnPhilms"));
    LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)
    // Register the application's document templates.  Document templates
    //  serve as the connection between documents, frame windows and views

    BOOL fLoadRecentFile = FALSE;
    if (resetSettings)
    {
        // Reset settings when the shift key is down
        _LoadSettings(TRUE);
        _SaveSettings();
        for (int i = 0; i < m_pRecentFileList->GetSize(); i++)
        {
            m_pRecentFileList->Remove(0);
        }
    }
    else
    {
        fLoadRecentFile = TRUE;
        _LoadSettings();
    }

    CMultiDocTemplate* pDocTemplate;
    pDocTemplate = new CMultiDocTemplate(IDR_MAINFRAME,
        RUNTIME_CLASS(CResourceListDoc),
        RUNTIME_CLASS(CGameExplorerFrame), // custom MDI child frame
        RUNTIME_CLASS(CGameExplorerView));
    if (!pDocTemplate)
        return FALSE;
    AddDocTemplate(pDocTemplate);

    appState->_pPicTemplate = new CMultiDocTemplateWithNonViews(IDR_TEXTFRAME,
        RUNTIME_CLASS(CPicDoc),
        RUNTIME_CLASS(CPicChildFrame),        // standard MDI child frame
        RUNTIME_CLASS(CPicView));
    if (!appState->_pPicTemplate)
        return FALSE;
    AddDocTemplate(appState->_pPicTemplate);

    appState->_pVocabTemplate = new CMultiDocTemplate(IDR_VOCABFRAME,
        RUNTIME_CLASS(CVocabDoc),
        RUNTIME_CLASS(CVocabChildFrame),
        RUNTIME_CLASS(CVocabView));
    if (!appState->_pVocabTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pVocabTemplate);

    appState->_pTextTemplate = new CMultiDocTemplate(IDR_TEXTFRAME,
        RUNTIME_CLASS(CTextDoc),
        RUNTIME_CLASS(CTextChildFrame),
        RUNTIME_CLASS(CTextView));
    if (!appState->_pTextTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pTextTemplate);

    appState->_pSoundTemplate = new CMultiDocTemplate(IDR_TEXTFRAME,
        RUNTIME_CLASS(CSoundDoc),
        RUNTIME_CLASS(CSoundChildFrame),
        RUNTIME_CLASS(CSoundView));
    if (!appState->_pSoundTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pSoundTemplate);

    appState->_pViewTemplate = new CMultiDocTemplateWithNonViews(IDR_TEXTFRAME,
        RUNTIME_CLASS(CNewRasterResourceDocument),
        RUNTIME_CLASS(CEditViewChildFrame),
        RUNTIME_CLASS(CRasterView));
    if (!appState->_pViewTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pViewTemplate);

    appState->_pFontTemplate = new CMultiDocTemplateWithNonViews(IDR_TEXTFRAME,
        RUNTIME_CLASS(CNewRasterResourceDocument),
        RUNTIME_CLASS(CEditFontChildFrame),
        RUNTIME_CLASS(CRasterView));
    if (!appState->_pFontTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pFontTemplate);

    appState->_pCursorTemplate = new CMultiDocTemplateWithNonViews(IDR_TEXTFRAME,
        RUNTIME_CLASS(CNewRasterResourceDocument),
        RUNTIME_CLASS(CCursorChildFrame),
        RUNTIME_CLASS(CRasterView));
    if (!appState->_pCursorTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pCursorTemplate);

    appState->_pScriptTemplate = new CMultiDocTemplate(IDR_SCRIPTFRAME,
        RUNTIME_CLASS(CScriptDocument),
        RUNTIME_CLASS(CScriptFrame),
        RUNTIME_CLASS(CScriptView));
    if (!appState->_pScriptTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pScriptTemplate);

    appState->_pMessageTemplate = new CMultiDocTemplateWithNonViews(IDR_TEXTFRAME,
        RUNTIME_CLASS(CMessageDoc),
        RUNTIME_CLASS(CMessageChildFrame),
        RUNTIME_CLASS(CMessageView));
    if (!appState->_pMessageTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pMessageTemplate);

    appState->_pPaletteTemplate = new CMultiDocTemplate(IDR_TEXTFRAME,
        RUNTIME_CLASS(CPaletteDoc),
        RUNTIME_CLASS(CPaletteChildFrame),
        RUNTIME_CLASS(CPaletteView));
    if (!appState->_pMessageTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pPaletteTemplate);

    appState->_pRoomExplorerTemplate = new CMultiDocTemplate(IDR_TEXTFRAME,
        RUNTIME_CLASS(CRoomExplorerDoc),
        RUNTIME_CLASS(CRoomExplorerFrame),
        RUNTIME_CLASS(CRoomExplorerView));
    if (!appState->_pRoomExplorerTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(appState->_pRoomExplorerTemplate);

    // Prof-UIS stuff
    CWinApp *pApp = ::AfxGetApp();
    if (resetSettings)
    {
        g_PaintManager.InstallPaintManager(new CExtPaintManagerOffice2007_R2_LunaBlue);
        g_PaintManager.PaintManagerStateSave(pApp->m_pszRegistryKey, pApp->m_pszProfileName, pApp->m_pszProfileName);
    }
    else
    {
        if (!g_PaintManager.PaintManagerStateLoad(pApp->m_pszRegistryKey, pApp->m_pszProfileName, pApp->m_pszProfileName))
        {
            g_PaintManager.InstallPaintManager(new CExtPaintManagerOffice2007_R2_LunaBlue);
        }
    }

    // create main MDI Frame window
    CMainFrame* pMainFrame = new CMainFrame;
    if (!pMainFrame || !pMainFrame->LoadFrame(IDR_MAINFRAME))
        return FALSE;
    m_pMainWnd = pMainFrame;

    // Register a clipboard format for pic commands
    appState->_uClipboardFormat = RegisterClipboardFormat(TEXT("SCIPicCommands-V1"));

    CSCICommandLineInfo cmdLine;
    ParseCommandLine(cmdLine);
    if (!cmdLine.m_strLogFile.IsEmpty())
    {
        appState->_logFile.Open(cmdLine.m_strLogFile, CFile::modeWrite | CFile::modeCreate | CFile::shareExclusive);
    }

    if (!_RegisterWindowClasses())
    {
        appState->LogInfo(TEXT("Couldn't register window classes."));
        return FALSE;
    }

    // The one and only window has been initialized, so show and update it
    // The main window has been initialized, so show and update it
    pMainFrame->ShowWindow(m_nCmdShow);
    pMainFrame->UpdateWindow();

    if (appState->_fAutoLoadGame && fLoadRecentFile && m_pRecentFileList && (m_pRecentFileList->GetSize() > 0) &&
        !m_pRecentFileList->m_arrNames[0].IsEmpty())
    {
        OnOpenRecentFile(ID_FILE_MRU_FILE1);
    }

    // Change extension for help file
    CString strHelpFile = m_pszHelpFilePath;
    strHelpFile.Replace(".HLP", ".chm");
    free((void*)m_pszHelpFilePath);
    m_pszHelpFilePath = _tcsdup(strHelpFile);

    // Create an intellisense window.
    // Note: even though it's a top-level window, we have to give it a parent, so it can ownerdraw.
    // Give the frame as the "owner"
    SystemParametersInfo(SPI_SETLISTBOXSMOOTHSCROLLING, FALSE, NULL, 0); // turn off smoothscrolling, it makes autocomplete suck
    //appState->m_wndIntel.CreateEx(0, _T("LISTBOX"), NULL, WS_THICKFRAME | WS_VSCROLL | WS_POPUP | LBS_SORT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_WANTKEYBOARDINPUT | LBS_NOTIFY, 0, 0, 100, 100, pMainFrame->GetSafeHwnd(), NULL);
    appState->m_wndIntel.CreateEx(0, _T("LISTBOX"), NULL, WS_DLGFRAME | WS_VSCROLL | WS_POPUP | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_WANTKEYBOARDINPUT | LBS_NOTIFY, 0, 0, 100, 100, pMainFrame->GetSafeHwnd(), NULL);
    // And the code tooltip
    appState->m_wndToolTip.CreateEx(0, _T("STATIC"), NULL, SS_NOTIFY | SS_OWNERDRAW | WS_POPUP | WS_CLIPSIBLINGS | WS_BORDER, 0, 0, 100, 100, pMainFrame->GetSafeHwnd(), NULL);

    AfxInitRichEdit();

    return TRUE;
}

BOOL SCICompanionApp::_RegisterWindowClasses()
{
    WNDCLASS wndcls;
    memset(&wndcls, 0, sizeof(WNDCLASS));   // start with NULL
    // defaults
    // CS_DROPSHADOW fails on win2k... we need to put in selectively based on runtime os info (GetVersionEx). CS_DROPSHADOW is XP and above
    //wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc = ::DefWindowProc;
    wndcls.hInstance = AfxGetInstanceHandle();
    wndcls.hIcon = LoadIcon(IDI_APPICON); // or load a different icon
    wndcls.hCursor = LoadCursor(IDC_ARROW);
    wndcls.hbrBackground = NULL;
    wndcls.lpszMenuName = NULL;
    wndcls.lpszClassName = COLOREDTOOLTIP_CLASSNAME;

    // Register the new class and exit if it fails
    if (!AfxRegisterClass(&wndcls))
    {
        TRACE("Class Registration Failed\n");
        return FALSE;
    }
    return TRUE;
}

#ifndef CS_DROPSHADOW
#define CS_DROPSHADOW 0x00020000
#endif

int SCICompanionApp::ExitInstance()
{
    _SaveSettings();
    CWinApp *pApp = ::AfxGetApp();
    g_PaintManager.PaintManagerStateSave(pApp->m_pszRegistryKey, pApp->m_pszProfileName, pApp->m_pszProfileName);
    appState->ExitInstance();
    int iRet = __super::ExitInstance();
    delete appState;
    return iRet;
}


void SCICompanionApp::_LoadSettings(BOOL fReset)
{
    PCTSTR pszRegName = fReset ? TEXT("SomethingThatIsntThere") : m_pszAppName;
    appState->_cyFakeEgo = GetProfileInt(pszRegName, TEXT("FakeEgoY"), 48);
    appState->_cxFakeEgo = GetProfileInt(pszRegName, TEXT("FakeEgoX"), 30);

    appState->_audioProcessing->TrimLeftMS = GetProfileInt(pszRegName, TEXT("AudioTrimLeft"), appState->_audioProcessing->TrimLeftMS);
    appState->_audioProcessing->TrimLeftMS = GetProfileInt(pszRegName, TEXT("AudioTrimRight"), appState->_audioProcessing->TrimLeftMS);
    appState->_audioProcessing->AutoGain = GetProfileInt(pszRegName, TEXT("AutoGain"), appState->_audioProcessing->AutoGain);
    appState->_audioProcessing->AudioDither = GetProfileInt(pszRegName, TEXT("AudioDither"), appState->_audioProcessing->AudioDither);
    appState->_audioProcessing->Compression = GetProfileInt(pszRegName, TEXT("Compression"), appState->_audioProcessing->Compression);
    appState->_audioProcessing->Noise.AttackTimeMS = GetProfileInt(pszRegName, TEXT("NoiseAttackTimeMS"), appState->_audioProcessing->Noise.AttackTimeMS);
    appState->_audioProcessing->Noise.ReleaseTimeMS = GetProfileInt(pszRegName, TEXT("NoiseReleaseTimeMS"), appState->_audioProcessing->Noise.ReleaseTimeMS);
    appState->_audioProcessing->Noise.HoldTimeMS = GetProfileInt(pszRegName, TEXT("NoiseHoldTimeMS"), appState->_audioProcessing->Noise.HoldTimeMS);
    appState->_audioProcessing->Noise.OpenThresholdDB = GetProfileInt(pszRegName, TEXT("NoiseOpenThresholdDB"), appState->_audioProcessing->Noise.OpenThresholdDB);
    appState->_audioProcessing->Noise.CloseThresholdDB = GetProfileInt(pszRegName, TEXT("NoiseCloseThresholdDB"), appState->_audioProcessing->Noise.CloseThresholdDB);

    appState->_fUseBoxEgo = GetProfileInt(pszRegName, TEXT("UseBoxEgo"), FALSE);
    appState->_fScaleTracingImages = GetProfileInt(pszRegName, TEXT("ScaleTracingImages"), TRUE);
    appState->_fDontShowTraceScaleWarning = GetProfileInt(pszRegName, TEXT("DontShowTraceScaleWarning"), FALSE);
    appState->_fUseAutoSuggest = GetProfileInt(pszRegName, TEXT("UseAutoSuggest"), FALSE);
    appState->_fAllowBraceSyntax = GetProfileInt(pszRegName, TEXT("AllowBraceSyntax"), FALSE) != 0;
    appState->_fAutoLoadGame = GetProfileInt(pszRegName, TEXT("AutoLoadGame"), FALSE);
    appState->_fDupeNewCels = GetProfileInt(pszRegName, TEXT("DupeNewCels"), TRUE);
    appState->_fShowPolyDotted = GetProfileInt(pszRegName, TEXT("ShowPolyDotted"), FALSE);
    appState->_fBrowseInfo = GetProfileInt(pszRegName, TEXT("BrowseInfo"), TRUE);
    appState->_fScriptNav = GetProfileInt(pszRegName, TEXT("ScriptNav"), TRUE);
    appState->_fCodeCompletion = GetProfileInt(pszRegName, TEXT("CodeCompletion"), TRUE);
    appState->_fHoverTips = GetProfileInt(pszRegName, TEXT("HoverTips"), TRUE);
    appState->_fPlayCompileErrorSound = GetProfileInt(pszRegName, TEXT("CompileErrorSound"), TRUE);
    appState->_midiDeviceName = (PCSTR)GetProfileString(pszRegName, TEXT("DefaultMidiDevice"), "");
    appState->_fUseOriginalAspectRatioDefault = GetProfileInt(pszRegName, TEXT("OriginalAspectRatio"), FALSE);
    appState->_fShowTabs = GetProfileInt(pszRegName, TEXT("ShowTabs"), FALSE);
    appState->_docGenFolder = (PCSTR)GetProfileString(pszRegName, TEXT("DocGenFolder"), "");
    appState->_docGenCommand = (PCSTR)GetProfileString(pszRegName, TEXT("DocGenCommand"), "make html");
    appState->_fSaveScriptsBeforeRun = GetProfileInt(pszRegName, TEXT("SaveScriptsBeforeRun"), TRUE);
    appState->_fTrackHeaderFiles = GetProfileInt(pszRegName, TEXT("TrackHeaderFiles"), TRUE);
    appState->_fCompileDirtyScriptsBeforeRun = GetProfileInt(pszRegName, TEXT("CompileModifiedScriptsBeforeRun"), TRUE);
    appState->_onionLeftTint = GetProfileInt(pszRegName, TEXT("OnionLeftTint"), 0x80FF8080);
    appState->_onionRightTint = GetProfileInt(pszRegName, TEXT("OnionRightTint"), 0x808080FF);
    appState->_onionLeftOnTop = GetProfileInt(pszRegName, TEXT("OnionLeftOnTop"), FALSE);
    appState->_onionRightOnTop = GetProfileInt(pszRegName, TEXT("OnionRightOnTop"), FALSE);
    appState->_onionWrap = GetProfileInt(pszRegName, TEXT("OnionWrap"), TRUE);
}

void SCICompanionApp::_SaveSettings()
{
    WriteProfileInt(m_pszAppName, TEXT("FakeEgoY"), appState->_cyFakeEgo);
    WriteProfileInt(m_pszAppName, TEXT("FakeEgoX"), appState->_cxFakeEgo);

    WriteProfileInt(m_pszAppName, TEXT("TrimLeftMS"), appState->_audioProcessing->TrimLeftMS);
    WriteProfileInt(m_pszAppName, TEXT("TrimRightMS"), appState->_audioProcessing->TrimRightMS);
    WriteProfileInt(m_pszAppName, TEXT("AudioDither"), appState->_audioProcessing->AudioDither);
    WriteProfileInt(m_pszAppName, TEXT("AutoGain"), appState->_audioProcessing->AutoGain);
    WriteProfileInt(m_pszAppName, TEXT("DetectStartEnd"), appState->_audioProcessing->DetectStartEnd);
    WriteProfileInt(m_pszAppName, TEXT("Compression"), appState->_audioProcessing->Compression);
    WriteProfileInt(m_pszAppName, TEXT("NoiseAttackTimeMS"), appState->_audioProcessing->Noise.AttackTimeMS);
    WriteProfileInt(m_pszAppName, TEXT("NoiseReleaseTimeMS"), appState->_audioProcessing->Noise.ReleaseTimeMS);
    WriteProfileInt(m_pszAppName, TEXT("NoiseHoldTimeMS"), appState->_audioProcessing->Noise.HoldTimeMS);
    WriteProfileInt(m_pszAppName, TEXT("NoiseOpenThresholdDB"), appState->_audioProcessing->Noise.OpenThresholdDB);
    WriteProfileInt(m_pszAppName, TEXT("NoiseCloseThresholdDB"), appState->_audioProcessing->Noise.CloseThresholdDB);

    WriteProfileInt(m_pszAppName, TEXT("UseBoxEgo"), appState->_fUseBoxEgo);
    WriteProfileInt(m_pszAppName, TEXT("ScaleTracingImages"), appState->_fScaleTracingImages);
    WriteProfileInt(m_pszAppName, TEXT("DontShowTraceScaleWarning"), appState->_fDontShowTraceScaleWarning);
    WriteProfileInt(m_pszAppName, TEXT("UseAutoSuggest"), appState->_fUseAutoSuggest);
    WriteProfileInt(m_pszAppName, TEXT("AllowBraceSyntax"), appState->_fAllowBraceSyntax);
    WriteProfileInt(m_pszAppName, TEXT("AutoLoadGame"), appState->_fAutoLoadGame);
    WriteProfileInt(m_pszAppName, TEXT("DupeNewCels"), appState->_fDupeNewCels);
    WriteProfileInt(m_pszAppName, TEXT("ShowPolyDotted"), appState->_fShowPolyDotted);
    WriteProfileInt(m_pszAppName, TEXT("BrowseInfo"), appState->_fBrowseInfo);
    WriteProfileInt(m_pszAppName, TEXT("ScriptNav"), appState->_fScriptNav);
    WriteProfileInt(m_pszAppName, TEXT("CodeCompletion"), appState->_fCodeCompletion);
    WriteProfileInt(m_pszAppName, TEXT("HoverTips"), appState->_fHoverTips);
    WriteProfileInt(m_pszAppName, TEXT("CompileErrorSound"), appState->_fPlayCompileErrorSound);
    WriteProfileString(m_pszAppName, TEXT("DefaultMidiDevice"), appState->_midiDeviceName.c_str());
    WriteProfileInt(m_pszAppName, TEXT("OriginalAspectRatio"), appState->_fUseOriginalAspectRatioDefault);
    WriteProfileInt(m_pszAppName, TEXT("ShowTabs"), appState->_fShowTabs);
    WriteProfileString(m_pszAppName, TEXT("DocGenFolder"), appState->_docGenFolder.c_str());
    WriteProfileString(m_pszAppName, TEXT("DocGenCommand"), appState->_docGenCommand.c_str());

    WriteProfileInt(m_pszAppName, TEXT("SaveScriptsBeforeRun"), appState->_fSaveScriptsBeforeRun);
    WriteProfileInt(m_pszAppName, TEXT("TrackHeaderFiles"), appState->_fTrackHeaderFiles);
    WriteProfileInt(m_pszAppName, TEXT("CompileModifiedScriptsBeforeRun"), appState->_fCompileDirtyScriptsBeforeRun);

    WriteProfileInt(m_pszAppName, TEXT("OnionLeftTint"), appState->_onionLeftTint);
    WriteProfileInt(m_pszAppName, TEXT("OnionRightTint"), appState->_onionRightTint);
    WriteProfileInt(m_pszAppName, TEXT("OnionLeftOnTop"), appState->_onionLeftOnTop);
    WriteProfileInt(m_pszAppName, TEXT("OnionRightOnTop"), appState->_onionRightOnTop);
    WriteProfileInt(m_pszAppName, TEXT("OnionWrap"), appState->_onionWrap);
}

// CAboutDlg dialog used for App About

class CAboutDlg : public CExtResizableDialog
//class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

    // Dialog Data
    enum { IDD = IDD_ABOUTBOX };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

    // Implementation
protected:
    DECLARE_MESSAGE_MAP()

    // Visuals
    CExtButton m_wndOk;
    CExtLabel m_wndLabel1;
    CExtLabel m_wndLabel2;
    CExtEdit m_wndEdit;
    CExtLabel m_wndLabel4;
};

CAboutDlg::CAboutDlg() : CExtResizableDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //ShowSizeGrip(FALSE);
    DDX_Control(pDX, IDOK, m_wndOk);
    DDX_Control(pDX, IDC_STATIC1, m_wndLabel1);
    DDX_Control(pDX, IDC_STATIC2, m_wndLabel2);
    DDX_Control(pDX, IDC_EDIT1, m_wndEdit);
    DDX_Control(pDX, IDC_STATIC4, m_wndLabel4);

    m_wndEdit.SetWindowText(
        "By Phil Fortier (IceFall Games)\r\n\r\n\r\nParts of code (""Crystal Edit"" text editor) by Andrei Stcherbatchenko\r\n\r\nContains decompression routines from the ScummVM project.\r\n\r\ncppformat library  Copyright (c) 2012 - 2015, Victor Zverovich\r\n\r\nSample rate converter designed by Aleksey Vaneev of Voxengo"
        );
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// App command to run the dialog
void SCICompanionApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

void SCICompanionApp::OnOpenPluginsFolder()
{
    std::string pluginsFolder = GetExeSubFolder("Plugins");
    if (!pluginsFolder.empty())
    {
        ShellExecute(NULL, "open", pluginsFolder.c_str(), "", "", SW_SHOWNORMAL);
    }
}

void SCICompanionApp::OnWindowsExplorer()
{
    std::string gameFolder = appState->GetResourceMap().GetGameFolder();
    if (!gameFolder.empty())
    {
        ShellExecute(NULL, "open", gameFolder.c_str(), "", "", SW_SHOWNORMAL);
    }
}

void SCICompanionApp::OnForums()
{
    ShellExecute(NULL, "open", "http://sciprogramming.com/", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnTutorials()
{
    ShellExecute(NULL, "open", "http://sciprogramming.com/tutorial.php?entry=6693", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnDocumentation()
{
    ShellExecute(NULL, "open", "http://sciwiki.sierrahelp.com/", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnUpdates()
{
    ShellExecute(NULL, "open", "http://www.icefallgames.com/SCICompanion", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnEmail()
{
    ShellExecute(NULL, "open", "https://github.com/icefallgames/SCICompanion", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::GetGames()
{
    ShellExecute(NULL, "open", "http://www.gog.com/games##search=Sierra", "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnSCICompHelp()
{
    std::string helpFolder = GetExeSubFolder("Help");
    helpFolder += "\\index.html";
    ShellExecute(NULL, "open", helpFolder.c_str(), "", "", SW_SHOWNORMAL);
}

void SCICompanionApp::OnRunGame()
{
    appState->RunGame(false, -1);
}

void SCICompanionApp::OnDebugGame()
{
    appState->RunGame(true, -1);
}

void SCICompanionApp::OnRoomExplorer()
{
    if (appState->_pRoomExplorerTemplate)
    {
        // Create the one and only script doc.
        CRoomExplorerDoc *pDoc = (CRoomExplorerDoc*)appState->_pRoomExplorerTemplate->OpenDocumentFile(NULL, TRUE);
        pDoc->SetTitle("Room explorer");
    }
}

//
// Ensure that the folder name appears in the recent docs list, instead of the file name (resource.map)
//
void SCICompanionApp::AddToRecentFileList(PCTSTR lpszPathName)
{
    TCHAR szBuffer[MAX_PATH];
    if (SUCCEEDED(StringCchCopy(szBuffer, ARRAYSIZE(szBuffer), lpszPathName)))
    {
        PTSTR pszFileName = StrStrI(szBuffer, TEXT("resource.map"));
        if (pszFileName)
        {
            *pszFileName = 0;
            __super::AddToRecentFileList(szBuffer);
        }
    }
}

void SCICompanionApp::OnCloseGame()
{
    if (SaveAllModified())
    {
        // Then close them all.
        CloseAllDocuments(FALSE);
        _DestroyPicClipsDialog();
        appState->_resourceRecency.ClearAllResourceTypes();
    }
}

void SCICompanionApp::OnGameVersionDetection()
{
    SCIVersion version = appState->GetVersion();
    CGameVersionDialog dialog(version);
    if (IDOK == dialog.DoModal())
    {
        appState->GetResourceMap().SetVersion(version);
        // Close and re-open
        std::string gameFolder = appState->GetResourceMap().GetGameFolder();
        OnCloseGame();
        appState->GetResourceMap().SkipNextVersionSniff();
        OpenDocumentFile(gameFolder.c_str());
    }
}

void SCICompanionApp::OnGameProperties()
{
    CGamePropertiesDialog dialog(appState->GetResourceMap().GetRunLogic());
    dialog._strGameName = appState->GetGameName().c_str();
    LangSyntax lang = appState->_resourceMap.Helper().GetDefaultGameLanguage();
    if (IDOK == dialog.DoModal())
    {
        appState->SetGameName(dialog._strGameName);
        if (dialog.NeedsReload())
        {
            // Close and re-open the game
            std::string gameFolder = appState->GetResourceMap().GetGameFolder();
            OnCloseGame();
            OpenDocumentFile(gameFolder.c_str());
        }
    }
}

void SCICompanionApp::OnUpdateGameLoaded(CCmdUI *pCmdUI)
{
    // If we have a resource map loaded, then we can close game.
    pCmdUI->Enable(appState->GetResourceMap().IsGameLoaded());
}

