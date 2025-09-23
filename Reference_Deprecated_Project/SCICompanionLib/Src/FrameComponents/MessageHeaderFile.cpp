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
#include "MessageHeaderFile.h"
#include "format.h"
#include "AppState.h"

using namespace std;

// REVIEW: Currently message header files do their own parsing. As long as the performance
// is ok, it would be better to leverage the script parser to do loading and saving (at least saving), as then we don't
// need to bother with language differences (SCI Script vs Studio) here.

MessageHeaderFile::MessageHeaderFile(const std::string &filepath, const std::string &title, const std::vector<std::string> &sourcesOptional) : _filePath(filepath), _title(title), _resourceNumber(-1)
{
    _Load(sourcesOptional);
}

MessageHeaderFile::MessageHeaderFile(const std::string &folderPath, int resourceNumber, const std::vector<std::string> &sourcesOptional) : _folderPath(folderPath), _resourceNumber(resourceNumber)
{
    _title = fmt::format("{0}.shm", _resourceNumber);
    _filePath = fmt::format("{0}\\{1}", _folderPath, _title);
    _Load(sourcesOptional);
}

const char c_szCommentStudio[] = "//";
const char c_szCommentSCI[] = ";;;";
const char c_szDefine[] = "(define";

void AdvancePastWhitespace(const std::string &line, size_t &offset)
{
    while (offset < line.size())
    {
        if (line[offset] == '\t' || line[offset] == ' ')
        {
            offset++;
        }
        else
        {
            break;
        }
    }
}

void AdvanceToWhitespace(const std::string &line, size_t &offset)
{
    while (offset < line.size())
    {
        if (line[offset] == '\t' || line[offset] == ' ')
        {
            break;
        }
        offset++;
    }
}

void MessageHeaderFile::_Load(const std::vector<std::string> &sourcesOptional)
{
    for (const auto &source : sourcesOptional)
    {
        auto itPair = _sources.emplace(std::make_pair(source, MessageSource(this)));
        auto messageSource = &itPair.first->second;
        messageSource->Name = source;
        messageSource->MandatoryPrefix = source.substr(0, 1) + "_";
    }

    MessageSource *currentSource = nullptr;
    // For speed, we won't bother with our standard parser here. Also, we need comments.
    ifstream file;
    file.open(_filePath);
    string line;
    LangSyntax lang = LangSyntaxUnknown;
    while (std::getline(file, line))
    {
        if (lang == LangSyntaxUnknown)
        {
            lang = _DetermineLanguage(line);
        }
        char commentChar = lang == LangSyntaxSCI ? ';' : '/';
        int minChars = lang == LangSyntaxSCI ? 1 : 2;
        size_t offset = 0;
        AdvancePastWhitespace(line, offset);
        bool wasComment = false;
        for (; offset < line.length(); offset++)
        {
            if (line[offset] != commentChar)
            {
                break;
            }
            wasComment = true;
        }

        if (wasComment)
        {
            // Read tokens like CASES and NOUNS
            AdvancePastWhitespace(line, offset);
            size_t afterName = offset;
            AdvanceToWhitespace(line, afterName);
            string token = line.substr(offset, afterName - offset);
            auto itFind = _sources.find(token);
            if (itFind != _sources.end())
            {
                currentSource = &itFind->second;
            }
            else
            {
                currentSource = nullptr;
            }
        }
        else
        {
            AdvancePastWhitespace(line, offset);
            if (line.compare(offset, ARRAYSIZE(c_szDefine) - 1, c_szDefine) == 0)
            {
                if (currentSource == nullptr)
                {
                    auto itPair = _sources.emplace(std::make_pair("", MessageSource(this)));
                    currentSource = &itPair.first->second;
                    currentSource->Name = "";
                }

                size_t offset = ARRAYSIZE(c_szDefine);
                AdvancePastWhitespace(line, offset);
                size_t afterName = offset;
                AdvanceToWhitespace(line, afterName);
                string name = line.substr(offset, afterName - offset);
                AdvancePastWhitespace(line, afterName);
                // This should be a number
                string number = line.substr(afterName);
                size_t afterNumber;
                uint16_t value = (uint16_t)stoi(number, &afterNumber);
                // TODO: We could assert we find a closing bracket.

                currentSource->AddDefine(name, value);
            }
        }
    }
}

const MessageSource *MessageHeaderFile::GetMessageSource(const std::string &name) const
{
    return &(_sources.find(name)->second);
}

MessageSource *MessageHeaderFile::GetMessageSource(const std::string &name)
{
    if (_sources.find(name) == _sources.end())
    {
        // We hit this when we don't have a file to begin with.
        _sources.emplace(std::make_pair(name, MessageSource(this)));
        _sources[name].Name = name;
        _sources[name].MandatoryPrefix = name.substr(0, 1) + "_";
    }
    return &_sources[name];
}

MessageSource *MessageHeaderFile::GetMessageSource()
{
    if (_sources.size() == 0)
    {
        _sources.emplace(std::make_pair("", MessageSource(this)));
    }
    return &_sources.begin()->second;
}

std::string MessageHeaderFile::GetBackingFile() const
{
    return _filePath;
}

void MessageHeaderFile::Commit(int resourceNumber)
{
    if ((resourceNumber != -1) && !_folderPath.empty())
    {
        _resourceNumber = resourceNumber;
        _title = fmt::format("{0}.shm", _resourceNumber);
        _filePath = fmt::format("{0}\\{1}", _folderPath, _title);
    }

    // Ensure _folderPath exists.
    EnsureFolderExists(_folderPath, false);

    LangSyntax lang = appState->GetResourceMap().Helper().GetDefaultGameLanguage();
    PCSTR commentText = (lang == LangSyntaxSCI) ? c_szCommentSCI : c_szCommentStudio;

    ofstream file;
    string bakFile = _filePath + ".bak";
    file.open(bakFile, ios_base::out | ios_base::trunc);
    if (file.is_open())
    {
        if (lang == LangSyntaxSCI)
        {
            file << commentText << " " << SCILanguageMarker << "\n";
        }

        file << commentText << " " << _title << " -- Produced by SCI Companion\n";
        file << commentText << " This file should only be edited with the SCI Companion message editor\n";
        file << "\n";
        for (auto &source : _sources)
        {
            file << commentText << " " << source.first << "\n";
            file << "\n";
            for (auto &define : source.second.GetDefines())
            {
                file << "(define " << define.first << " " << define.second << ")\n";
            }
            file << "\n";
        }

        file.close();
        try
        {
            deletefile(_filePath);
            movefile(bakFile, _filePath);
        }
        catch (std::exception &e)
        {
            AfxMessageBox(e.what(), MB_ICONWARNING | MB_OK);
        }
    }
    else
    {
        AfxMessageBox(fmt::format("Unable to open {0} for writing.", bakFile).c_str(), MB_ICONWARNING | MB_OK);
    }
}