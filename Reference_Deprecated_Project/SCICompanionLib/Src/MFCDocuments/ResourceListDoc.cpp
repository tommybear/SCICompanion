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

// ResourceListDoc.cpp : implementation file
//

#include "stdafx.h"
#include "AppState.h"
#include "ResourceListDoc.h"
#include "CObjectWrap.h"
#include "ResourceContainer.h"
#include "ResourceBlob.h"
#include "DependencyTracker.h"

// ResourceListDoc

IMPLEMENT_DYNCREATE(CResourceListDoc, CDocument)

CResourceListDoc::CResourceListDoc() : _shownResourceType(ResourceType::None)
{
    // Add ourselves as a sync
    CResourceMap &map = appState->GetResourceMap();
    map.AddSync(this);
}

BOOL CResourceListDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    BOOL fRet =  __super::OnOpenDocument(lpszPathName);
    appState->GenerateBrowseInfo();
    return fRet;
}

BOOL CResourceListDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

    appState->GenerateBrowseInfo();

	return TRUE;
}

void CResourceListDoc::OnCloseDocument()
{
    std::string strEmpty;
    appState->GetDependencyTracker().Clear();
    appState->GetResourceMap().SetGameFolder(strEmpty);

    // Remove ourselves as a sync
    CResourceMap &map = appState->GetResourceMap();
    map.RemoveSync((IResourceMapEvents*)this);

    appState->ResetClassBrowser();

    appState->ClearResourceManagerDoc();

    __super::OnCloseDocument();
}

CResourceListDoc::~CResourceListDoc()
{
}


BEGIN_MESSAGE_MAP(CResourceListDoc, CDocument)
    ON_UPDATE_COMMAND_UI(ID_FILE_CLOSE, OnDisableCommand)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE, OnDisableCommand)
    ON_UPDATE_COMMAND_UI(ID_FILE_SAVE_AS, OnDisableCommand)
END_MESSAGE_MAP()


// Always disable certain commands when this doc has focus.
// You can't save/saveas or close a resource doc
void CResourceListDoc::OnDisableCommand(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(FALSE);
}

void CResourceListDoc::OnResourceAdded(const ResourceBlob *pData, AppendBehavior appendBehavior)
{
    // Add this as the most recent resource of this type/number/package
    appState->_resourceRecency.AddResourceToRecency(pData);
    UpdateAllViews(nullptr, 0, &WrapObject((appendBehavior == AppendBehavior::Replace) ? ResourceMapChangeHint::Replaced : ResourceMapChangeHint::Added, pData));
}

void CResourceListDoc::OnResourceDeleted(const ResourceBlob *pDataDeleted)
{
    // Delete this from the recency
    // It is crucial that this come before the update below.  We will check the recency list
    // for the ResourceBlob passed to UpdateAllViews, and at this point, we want to not find
    // it in the list.
    appState->_resourceRecency.DeleteResourceFromRecency(pDataDeleted);

    // Cast away constness...
    UpdateAllViews(nullptr, 0, &WrapObject(ResourceMapChangeHint::Deleted, pDataDeleted));
}

void CResourceListDoc::OnResourceMapReloaded(bool isInitialLoad)
{
    // Initial load is handled elsewhere (archive)
    if (!isInitialLoad)
    {
        UpdateAllViews(nullptr, 0, &WrapHint(ResourceMapChangeHint::Change));
    }
}

void CResourceListDoc::OnResourceTypeReloaded(ResourceType iType)
{
    UpdateAllViews(nullptr, 0, &WrapObject(ResourceMapChangeHint::Type, iType));
}

void CResourceListDoc::OnImagesInvalidated()
{
    UpdateAllViews(nullptr, 0, &WrapHint(ResourceMapChangeHint::Image));
}

//
// This class is just a CObject class that wraps a resource type number
//
class CResourceTypeWrap : public CObject
{
public:
    CResourceTypeWrap(ResourceType iType) { _iType = iType; }
    ResourceType GetType() { return _iType; }

private:
    ResourceType _iType;
};

void CResourceListDoc::ShowResourceType(ResourceType iType)
{
    if (iType != _shownResourceType)
    {
        _shownResourceType = ValidateResourceType(iType);
        CResourceTypeWrap resourceType(iType);
        UpdateAllViewsAndNonViews(nullptr, 0, &WrapObject(ResourceMapChangeHint::ShowType, iType));
    }
}

// ResourceListDoc diagnostics

#ifdef _DEBUG
void CResourceListDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CResourceListDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// ResourceListDoc serialization

void CResourceListDoc::Serialize(CArchive& ar)
{
    if (ar.IsStoring())
    {
        // There is nothing to save.
    }
    else
    {
        CFile *pFile = ar.GetFile();

        // Set the current directory, so we know where to look for the resource files.
        // If not, clicking on an item in the recent documents list won't work
        CString path = pFile->GetFilePath();
        path.MakeLower();
        int iFileOffset = path.Find(TEXT("\\resource.map"));
        if (iFileOffset > 0)
        {
            path.SetAt(iFileOffset, 0); // Null terminate it

            // Set this folder as our new game folder
            CResourceMap &map = appState->GetResourceMap();
            appState->GetDependencyTracker().Clear();
            map.SetGameFolder((PCSTR)path);
            appState->_fUseOriginalAspectRatioCached = map.Helper().GetUseSierraAspectRatio(!!appState->_fUseOriginalAspectRatioDefault);

            appState->LogInfo(TEXT("Open game: %s"), (PCTSTR)path);

            UpdateAllViews(nullptr, 0, &WrapHint(ResourceMapChangeHint::Change));
        }
        else
        {
            AfxMessageBox(TEXT("SCI game resources must be called resource.map"), MB_OK | MB_ICONEXCLAMATION);
        }
    }
}


// ResourceListDoc commands
