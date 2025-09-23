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

#include "ResourceEntityDocument.h"
#include "MessageSource.h"
#include "Message.h"

struct SyncComponent;
class MessageSource;
class MessageHeaderFile;
struct TextEntry;

class CMessageDoc : public ResourceEntityDocument
{
    DECLARE_DYNCREATE(CMessageDoc)

public:
    CMessageDoc();
    virtual void Serialize(CArchive& ar);   // overridden for document i/o

    // Takes ownership:
    void SetMessageResource(std::unique_ptr<ResourceEntity> pText, int id = -1);

    // CUndoResource
    void v_OnUndoRedo();

#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    void SetSelectedIndex(int index, bool force = false);
    int GetSelectedIndex() const { return _selectedIndex; }
    ResourceEntity *GetAudioResource();             // Can return null_ptr
    ResourceEntity *GetAudioResource(int index);    // Can return null_ptr
    const TextEntry *GetEntry();

    void SetAudioResource(std::unique_ptr<ResourceEntity> audioResource);
    template<typename _TFunc>
    void ModifyCurrentAudioResource(_TFunc f)
    {
        if ((_selectedIndex != -1) && _audioResources[_selectedIndex])
        {
            if (f(*_audioResources[_selectedIndex]))
            {
                _audioModified[_selectedIndex] = true;
                SetModifiedFlag(TRUE);
                _RecalcAudioSize();
                UpdateAllViewsAndNonViews(nullptr, 0, &WrapHint(MessageChangeHint::ItemChanged));
            }
        }
    }
    uint32_t GetEstimatedAudioSize() const { return _estimatedAudioSize; }

    bool v_PreventUndos() const override;

    // All modifications to text entries should go through CMessageDoc instead of the caller calling ApplyChanges directly,
    // since it needs to track sidecar audio resources. I wish I could enforce this somehow.
    void DeleteCurrentEntry();
    void AddEntry(const TextEntry &entry);
    void SetEntry(const TextEntry &entry);
    void ImportMessage();
    void ExportMessage();

protected:
    bool v_DoPreResourceSave() override;
    void PostSuccessfulSave(const ResourceEntity *pResource) override;

    ResourceType _GetType() const override
    {
        const ResourceEntity *pResource = static_cast<const ResourceEntity*>(GetResource());
        return pResource->GetType();
    }

private:
    void _PreloadAudio();
    void _RecalcAudioSize();

    int _selectedIndex;

    // Audio sidecare stuff. These vector match the list of texts, and can be indexed with _selectedIndex
    std::vector<std::unique_ptr<ResourceEntity>> _audioResources;
    std::vector<bool> _audioModified;
    // We keep track of this so we know which audio files we can delete:
    std::set<uint32_t> _originalTuplesWithAudio;
    int _originalResourceNumber;
    uint32_t _estimatedAudioSize;

    DECLARE_MESSAGE_MAP()
};

const MessageSource *GetMessageSourceFromType(CMessageDoc *pDoc, MessageSourceType sourceType, bool reload);