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

class MessageHeaderFile;
typedef std::pair<std::string, uint16_t> MessageDefine;

enum class MessageSourceType
{
    Talkers,
    Verbs,
    Nouns,
    Conditions,
};

class MessageSource
{
public:
    MessageSource(MessageHeaderFile *file);
    MessageSource() { assert(false); }

    const std::vector<MessageDefine> &GetDefines() const { return _defines; }
    void SetName(size_t index, const std::string &newName);
    void SetValue(size_t index, uint16_t newValue);
    int IndexOf(const std::string &name) const;
    int IndexOf(uint16_t value) const;
    size_t AddDefine(const std::string &newName, uint16_t newValue);
    void DeleteDefine(size_t index);
    void Commit();
    std::string ValueToName(uint16_t value) const;
    uint16_t NameToValue(const std::string &name) const;

    std::string MandatoryPrefix;
    std::string Name;   // Includes the prefix

    // Returns empty string if not backed by file.
    std::string GetBackingFile() const;

    bool IsDirty() const { return _dirty; }

private:
    std::vector<MessageDefine> _defines;
    MessageHeaderFile *_file;
    bool _dirty;
};

std::unique_ptr<MessageHeaderFile> GetMessageFile(const std::string &messageFolder, int scriptNumber);