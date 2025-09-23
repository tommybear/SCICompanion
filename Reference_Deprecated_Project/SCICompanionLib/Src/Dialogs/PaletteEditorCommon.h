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

#include "resource.h"
#include "ChooseColorStatic.h"
#include "ResourceEntityDocument.h"
#include "PaletteOperations.h"
#include "AppState.h"
#include "format.h"
#include "GradientDialog.h"
#include "ColorAdjustDialog.h"
#include "PaletteDefinitionCallback.h"
#include "ClipboardUtil.h"

template<class T>
class PaletteEditorCommon : public T, public IColorDialogCallback, public IVGAPaletteDefinitionCallback
{
public:
    // TODO: Set the used value as 0x3 or 0x1, depending.

    PaletteEditorCommon<T>(IVGAPaletteDefinitionCallback *callback, UINT id, CWnd *pwnd) : T(id, pwnd), _callback(callback) {}

    void UpdateCommon(CObject *pObject)
    {
        PaletteChangeHint hint = GetHint<PaletteChangeHint>(pObject);
        if (IsFlagSet(hint, PaletteChangeHint::Changed))
        {
            if (_pDoc && _pDoc->GetResource())
            {
                // _pDoc will be set for non-dialog guys (e.g. PaletteView)
                _paletteOwned = std::make_unique<PaletteComponent>(_pDoc->GetResource()->GetComponent<PaletteComponent>());
                _palette = _paletteOwned.get();
                _SyncPalette();
            }
        }
    }

    void Init(PaletteComponent &palette, const std::vector<const Cel*> &cels)
    {
        // This is the path used for dialog-based guys.
        _hasActualUsedColors = !cels.empty();
        if (_hasActualUsedColors)
        {
            CountActualUsedColors(cels, _actualUsedColors);
        }
        _mainSelectedIndex = -1;
        _palette = &palette;
        memcpy(_originalColors, palette.Colors, sizeof(_originalColors));
    }

    // IVGAPaletteDefinitionCallback
    void OnVGAPaletteChanged() override
    {
        _SyncPalette();
    }

    // IColorDialogCallback
    void OnColorClick(BYTE bIndex, int nID, BOOL fLeftClick)
    {
        _mainSelectedIndex = bIndex;
        _SyncUI();
        _SyncSelection();

        bool multipleSelection[256];
        m_wndStatic.GetMultipleSelection(multipleSelection);
        int countUsed = 0;
        int countUnused = 0;
        int countSelected = 0;
        int countFixed = 0;
        for (int i = 0; i < ARRAYSIZE(multipleSelection); i++)
        {
            if (multipleSelection[i])
            {
                countSelected++;
                if (_palette->Colors[i].rgbReserved)
                {
                    if (_palette->Colors[i].rgbReserved == 0x3)
                    {
                        countFixed++;
                    }
                    countUsed++;
                }
                else
                {
                    countUnused++;
                }
            }
        }

        m_wndUsedCheck.EnableWindow(countSelected > 0);
        m_wndFixedCheck.EnableWindow(countSelected > 0);
        if (countUsed && countUnused)
        {
            m_wndUsedCheck.SetCheck(BST_INDETERMINATE);
        }
        else if (countUsed)
        {
            m_wndUsedCheck.SetCheck(BST_CHECKED);
        }
        else
        {
            m_wndUsedCheck.SetCheck(BST_UNCHECKED);
        }

        if (countFixed && (countFixed == countSelected))
        {
            m_wndFixedCheck.SetCheck(BST_CHECKED);
        }
        else if (countFixed)
        {
            m_wndFixedCheck.SetCheck(BST_INDETERMINATE);
        }
        else
        {
            m_wndFixedCheck.SetCheck(BST_UNCHECKED);
        }

        if (!fLeftClick)
        {
            OnBnClickedButtoneditcolor();
        }
    }

    void SetDocument(CDocument *pDoc)
    {
        _pDoc = static_cast<ResourceEntityDocument*>(pDoc);
        if (_pDoc)
        {
            UpdateCommon(&WrapHint(PaletteChangeHint::Changed));
        }
    }

    PaletteComponent GetPalette() { return *_palette; }

    void DoDataExchange(CDataExchange* pDX) override
    {
        __super::DoDataExchange(pDX);
        DDX_Control(pDX, IDC_STATIC1, m_wndStatic);

        DDX_Control(pDX, IDC_EDITRANGE, m_wndEditRange);

        _SyncPalette();
        bool initialSelection[256] = {};
        m_wndStatic.SetShowHover(false);
        m_wndStatic.SetSelection(initialSelection);
        m_wndStatic.ShowSelection(TRUE);
        m_wndStatic.SetAutoHandleSelection(true);
        m_wndStatic.ShowUnused(true);
        m_wndStatic.SetCallback(this);
        m_wndStatic.SetActualUsedColors(_actualUsedColors);

        DDX_Control(pDX, IDC_CHECK1, m_wndUsedCheck);
        m_wndUsedCheck.EnableWindow(FALSE);

        DDX_Control(pDX, IDC_CHECK2, m_wndFixedCheck);
        m_wndFixedCheck.EnableWindow(FALSE);
        m_wndFixedCheck.ShowWindow(appState->GetVersion().sci11Palettes ? SW_SHOW : SW_HIDE);

        DDX_Control(pDX, IDC_BUTTONEDITCOLOR, m_wndButtonChooseColor);
        m_wndButtonChooseColor.EnableWindow(FALSE);

        DDX_Control(pDX, IDC_BUTTONRANGEGRADIENT, m_wndButtonGradients);
        m_wndButtonGradients.EnableWindow(FALSE);

        DDX_Control(pDX, IDC_BUTTONADJUSTMENTS, m_wndButtonAdjust);
        m_wndButtonAdjust.EnableWindow(FALSE);

        // Visuals
        if (GetDlgItem(IDCANCEL))
        {
            DDX_Control(pDX, IDCANCEL, m_wndCancel);
        }
        if (GetDlgItem(IDC_BUTTONREVERT))
        {
            DDX_Control(pDX, IDC_BUTTONREVERT, m_wndRevert);
        }
        if (GetDlgItem(IDOK))
        {
            DDX_Control(pDX, IDOK, m_wndOk);
        }
        DDX_Control(pDX, IDC_EDIT1, m_wndEditDescription);
        DDX_Control(pDX, IDC_BUTTON_SAVE, m_wndSave);
        DDX_Control(pDX, IDC_BUTTON_LOAD, m_wndLoad);
        DDX_Control(pDX, IDC_BUTTON_SAVERANGE, m_wndSaveRange);
        m_wndSaveRange.EnableWindow(FALSE);
        DDX_Control(pDX, IDC_BUTTON_LOADAT, m_wndLoadAt);
        m_wndLoadAt.EnableWindow(FALSE);

        m_wndEditDescription.SetWindowTextA(
            "Shift-click selects a range of colors. Ctrl-click toggles selection. Right-click opens the color chooser."
            );
    }

protected:

    bool _IsSingleRangeSelected(const std::vector<std::pair<uint8_t, uint8_t>> &ranges)
    {
        if (ranges.size() == 1)
        {
            int start = ranges[0].first;
            int end = ranges[0].second;
            return (end > (start + 1)); // At least 3
        }
        return false;
    }

    void _SyncSelection()
    {
        std::vector<std::pair<uint8_t, uint8_t>> ranges = GetSelectedRanges(m_wndStatic);
        std::string rangeText = GetRangeText(ranges);
        m_wndEditRange.SetWindowTextA(rangeText.c_str());

        // The range button only works with a single range3.
        bool isSingleRangeSelected = _IsSingleRangeSelected(ranges);
        m_wndButtonGradients.EnableWindow(isSingleRangeSelected);
        m_wndSaveRange.EnableWindow(isSingleRangeSelected);
    }

    void _UpdateDocument()
    {
        if (_pDoc)
        {
            _pDoc->ApplyChanges<PaletteComponent>(
                [this](PaletteComponent &palette)
            {
                palette = *this->_palette;
                return WrapHint(PaletteChangeHint::Changed);
            }
                );
        }
    }


    void _SyncPalette()
    {
        if (_palette && m_wndStatic.GetSafeHwnd())
        {
            m_wndStatic.SetPalette(16, 16, reinterpret_cast<const EGACOLOR *>(_palette->Mapping), 256, _palette->Colors, false);
            m_wndStatic.Invalidate(FALSE);
        }
        if (_callback)
        {
            _callback->OnVGAPaletteChanged();
        }
    }

    void _SyncUI()
    {
        m_wndButtonChooseColor.EnableWindow(_mainSelectedIndex != -1);
        m_wndLoadAt.EnableWindow(_mainSelectedIndex != -1);
        m_wndButtonAdjust.EnableWindow(_mainSelectedIndex != -1);
    }

    DECLARE_MESSAGE_MAP()

    CChooseColorStatic m_wndStatic;
    CExtEdit m_wndEditRange;
    CExtCheckBox m_wndUsedCheck;
    CExtCheckBox m_wndFixedCheck;
    CExtButton m_wndButtonChooseColor;
    CExtButton m_wndButtonGradients;
    CExtButton m_wndButtonAdjust;

    PaletteComponent *_palette;
    std::unique_ptr<PaletteComponent> _paletteOwned;   // For the _pDoc scenario where we need a copy of the paletter to work on.
    bool _hasActualUsedColors;
    bool _actualUsedColors[256];
    int _mainSelectedIndex;

    RGBQUAD _originalColors[256];

    // Visuals
    CExtButton m_wndCancel;
    CExtButton m_wndOk;
    CExtButton m_wndRevert;
    CExtButton m_wndSave;
    CExtButton m_wndSaveRange;
    CExtButton m_wndLoad;
    CExtButton m_wndLoadAt;
    CExtEdit m_wndEditDescription;

    ResourceEntityDocument *_pDoc;
    IVGAPaletteDefinitionCallback *_callback;

public:
    afx_msg void OnBnClickedButtonrevert();
    afx_msg void OnBnClickedCheck1();
    afx_msg void OnBnClickedCheck2();
    afx_msg void OnBnClickedButtoneditcolor();
    afx_msg void OnBnClickedButtonGradient();
    afx_msg void OnBnClickedButtonAdjust();
    afx_msg void OnEnChangeEdit1();
    afx_msg void OnImportPalette();
    afx_msg void OnExportPalette();
    afx_msg void OnImportPaletteAt();
    afx_msg void OnExportPaletteRange();
    afx_msg void OnCopy();
    afx_msg void OnPaste();

    void OnUpdateCopyPaste(CCmdUI* pCmdUI);
};

BEGIN_TEMPLATE_MESSAGE_MAP(PaletteEditorCommon, T, T)
    ON_BN_CLICKED(IDC_BUTTONREVERT, OnBnClickedButtonrevert)
    ON_BN_CLICKED(IDC_CHECK1, OnBnClickedCheck1)
    ON_BN_CLICKED(IDC_CHECK2, OnBnClickedCheck2)
    ON_BN_CLICKED(IDC_BUTTONEDITCOLOR, OnBnClickedButtoneditcolor)
    ON_BN_CLICKED(IDC_BUTTONRANGEGRADIENT, OnBnClickedButtonGradient)
    ON_BN_CLICKED(IDC_BUTTONADJUSTMENTS, OnBnClickedButtonAdjust)
    ON_BN_CLICKED(IDC_BUTTON_LOAD, OnImportPalette)
    ON_BN_CLICKED(IDC_BUTTON_SAVE, OnExportPalette)
    ON_BN_CLICKED(IDC_BUTTON_LOADAT, OnImportPaletteAt)
    ON_BN_CLICKED(IDC_BUTTON_SAVERANGE, OnExportPaletteRange)
    ON_COMMAND(ID_EDIT_COPY, OnCopy)
    ON_COMMAND(ID_EDIT_PASTE, OnPaste)
    ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateCopyPaste)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateCopyPaste)
    END_MESSAGE_MAP()

template<class T>
void PaletteEditorCommon<T>::OnBnClickedButtonrevert()
{
    memcpy(_palette->Colors, _originalColors, sizeof(_originalColors));
    _SyncPalette();
    m_wndStatic.Invalidate(FALSE);
}

template<class T>
void PaletteEditorCommon<T>::OnUpdateCopyPaste(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

template<class T>
void PaletteEditorCommon<T>::OnBnClickedCheck1()
{
    bool multipleSelection[256];
    m_wndStatic.GetMultipleSelection(multipleSelection);

    int checkValue = m_wndUsedCheck.GetCheck();

    if ((checkValue == BST_CHECKED) || (checkValue == BST_UNCHECKED))
    {
        for (int i = 0; i < ARRAYSIZE(multipleSelection); i++)
        {
            if (multipleSelection[i])
            {
                if (checkValue == BST_CHECKED)
                {
                    _palette->Colors[i].rgbReserved |= 0x1;
                }
                else
                {
                    _palette->Colors[i].rgbReserved = 0;
                }
                
            }
        }
    }
    _SyncPalette();
    _UpdateDocument();
    m_wndStatic.Invalidate(FALSE);
}

template<class T>
void PaletteEditorCommon<T>::OnBnClickedCheck2()
{
    bool multipleSelection[256];
    m_wndStatic.GetMultipleSelection(multipleSelection);

    int checkValue = m_wndFixedCheck.GetCheck();

    if ((checkValue == BST_CHECKED) || (checkValue == BST_UNCHECKED))
    {
        for (int i = 0; i < ARRAYSIZE(multipleSelection); i++)
        {
            if (multipleSelection[i])
            {
                if (checkValue == BST_CHECKED)
                {
                    _palette->Colors[i].rgbReserved = 0x3;
                }
                else
                {
                    _palette->Colors[i].rgbReserved &= ~0x2;
                }
            }
        }
    }
    _SyncPalette();
    _UpdateDocument();
    m_wndStatic.Invalidate(FALSE);
}

template<class T>
void PaletteEditorCommon<T>::OnBnClickedButtoneditcolor()
{
    if (_mainSelectedIndex != -1)
    {
        COLORREF color = RGB_TO_COLORREF(_palette->Colors[_mainSelectedIndex]);
        CExtColorDlg colorDialog(color, color);
        colorDialog.m_strCaption = fmt::format("Edit color {0}", _mainSelectedIndex).c_str();
        if (IDOK == colorDialog.DoModal())
        {
            bool multipleSelection[256];
            m_wndStatic.GetMultipleSelection(multipleSelection);
            for (int i = 0; i < ARRAYSIZE(multipleSelection); i++)
            {
                if (multipleSelection[i])
                {
                    uint8_t usedValue = _palette->Colors[i].rgbReserved;
                    _palette->Colors[i] = RGBQUADFromCOLORREF(colorDialog.m_clrNew);
                    _palette->Colors[i].rgbReserved = usedValue;
                }
            }
            _UpdateDocument();
            _SyncPalette();
        }
    }
}

template<class T>
void PaletteEditorCommon<T>::OnBnClickedButtonGradient()
{
    std::vector<std::pair<uint8_t, uint8_t>> ranges = GetSelectedRanges(m_wndStatic);
    if (_IsSingleRangeSelected(ranges))
    {
        PaletteComponent paletteBackup = *_palette;
        GradientDialog dialog(*_palette, this, ranges[0].first, ranges[0].second);
        if (IDOK == dialog.DoModal())
        {
            _UpdateDocument();
            _SyncPalette();
        }
        else
        {
            *_palette = paletteBackup;
            _SyncPalette();
        }
    }
}

template<class T>
void PaletteEditorCommon<T>::OnBnClickedButtonAdjust()
{
    bool multipleSelection[256];
    m_wndStatic.GetMultipleSelection(multipleSelection);
    PaletteComponent paletteBackup = *_palette;
    ColorAdjustDialog dialog(*_palette, this, multipleSelection);
    if (IDOK == dialog.DoModal())
    {
        _UpdateDocument();
        _SyncPalette();
    }
    else
    {
        *_palette = paletteBackup;
        _SyncPalette();
    }
}

bool _GetPaletteFilename(bool open, const std::string &dialogTitle, std::string &filename);

template<class T>
void PaletteEditorCommon<T>::OnImportPalette()
{
    std::string filename;
    if (_GetPaletteFilename(true, "Import palette", filename))
    {
        try
        {
            LoadPALFile(filename, *_palette, 0);
            _SyncPalette();
            _UpdateDocument();
        }
        catch (std::exception &e)
        {
            AfxMessageBox(e.what(), MB_OK | MB_ICONWARNING);
        }
    }
}

template<class T>
void PaletteEditorCommon<T>::OnExportPalette()
{
    std::string filename;
    if (_GetPaletteFilename(false, "Export palette", filename))
    {
        try
        {
            SavePALFile(filename, *_palette, 0, 256);
        }
        catch (std::exception &e)
        {
            AfxMessageBox(e.what(), MB_OK | MB_ICONWARNING);
        }
    }
}


template<class T>
void PaletteEditorCommon<T>::OnImportPaletteAt()
{
    if (_mainSelectedIndex != -1)
    {
        std::string filename;
        if (_GetPaletteFilename(true, fmt::format("Import palette at index {0}", _mainSelectedIndex), filename))
        {
            try
            {
                LoadPALFile(filename, *_palette, _mainSelectedIndex);
                _SyncPalette();
                _UpdateDocument();
            }
            catch (std::exception &e)
            {
                AfxMessageBox(e.what(), MB_OK | MB_ICONWARNING);
            }
        }
    }
}


template<class T>
void PaletteEditorCommon<T>::OnExportPaletteRange()
{
    std::vector<std::pair<uint8_t, uint8_t>> ranges = GetSelectedRanges(m_wndStatic);
    if (_IsSingleRangeSelected(ranges))
    {
        std::string filename;
        if (_GetPaletteFilename(false, fmt::format("Export palette range {0}-{1}", (int)ranges[0].first, (int)ranges[0].second), filename))
        {
            try
            {
                SavePALFile(filename, *_palette, ranges[0].first, ranges[0].second - ranges[0].first + 1);
            }
            catch (std::exception &e)
            {
                AfxMessageBox(e.what(), MB_OK | MB_ICONWARNING);
            }
        }
    }
}

template<class T>
void PaletteEditorCommon<T>::OnCopy()
{
    sci::ostream stream;
    bool multipleSelection[256];
    m_wndStatic.GetMultipleSelection(multipleSelection);
    stream << multipleSelection;
    for (int i = 0; i < ARRAYSIZE(multipleSelection); i++)
    {
        stream << _palette->Colors[i];
    }
    OpenAndSetClipboardDataFromStream(this, appState->PaletteColorsClipboardFormat, stream);
}

template<class T>
void PaletteEditorCommon<T>::OnPaste()
{
    std::vector<std::pair<uint8_t, uint8_t>> ranges = GetSelectedRanges(m_wndStatic);
    int startIndex = 0;
    if (!ranges.empty())
    {
        startIndex = ranges[0].first;
    }

    ProcessClipboardDataIfAvailable(appState->PaletteColorsClipboardFormat, this,
        [this, startIndex](sci::istream &stream)
    {
        bool multipleSelection[256];
        stream >> multipleSelection;
        RGBQUAD colors[ARRAYSIZE(multipleSelection)];
        stream >> colors;

        // Find the first selected guy
        int firstSelected = 0;
        while ((firstSelected < ARRAYSIZE(multipleSelection)) && !multipleSelection[firstSelected])
        {
            firstSelected++;
        }

        for (int i = 0; ((i + firstSelected) < ARRAYSIZE(multipleSelection)) && ((i + startIndex) < ARRAYSIZE(multipleSelection)); i++)
        {
            if (multipleSelection[i + firstSelected])
            {
                _palette->Colors[i + startIndex] = colors[i + firstSelected];
            }
        }
        _UpdateDocument();
        _SyncPalette();
    }
    );
}
