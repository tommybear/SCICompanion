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

class CCrystalScriptStream;
class CCrystalTextBuffer;
class CScriptStreamLimiter;
class SyntaxContext;;
enum class AutoCompleteIconIndex;

class AutoCompleteChoice
{
public:
    AutoCompleteChoice();
    AutoCompleteChoice(const std::string &text, AutoCompleteIconIndex iIcon);
    AutoCompleteChoice(const std::string &text, const std::string &lower, AutoCompleteIconIndex iIcon);
    const std::string &GetText() const;
    const std::string &GetLower() const;
    AutoCompleteIconIndex GetIcon() const;

private:
    std::string _strText;
    std::string _strLower;
    AutoCompleteIconIndex _iIcon;
};

bool operator<(const AutoCompleteChoice &one, const AutoCompleteChoice &two);

enum ACType
{
    AC_Normal,
    AC_ShowOnFirst_Empty,
    AC_ShowOnFirst_Shown,
};


class AutoCompleteResult
{
public:
    AutoCompleteResult() { Reset(); }
    ~AutoCompleteResult() {}
    void Add(PCTSTR psz, AutoCompleteIconIndex iIcon) { choices.push_back(AutoCompleteChoice(psz, iIcon)); }
    void AddOnFirstLetter(PCTSTR pszLetters, PCTSTR pszText, AutoCompleteIconIndex iIcon)
    {
        fACType = pszLetters[0] ? AC_ShowOnFirst_Shown : AC_ShowOnFirst_Empty;
        Add(pszText, iIcon);
    }
    void Clear() { choices.clear(); strMethod.clear(); strParams.clear(); fACType = AC_Normal; }
    void Reset() { dwFailedPos = 0; fResultsChanged = FALSE; Clear(); fACType = AC_Normal; }
    BOOL HasMethodTip() { return !strMethod.empty(); }

    DWORD dwFailedPos;
    BOOL fResultsChanged;
    std::vector<AutoCompleteChoice> choices;
    ACType fACType;
    CPoint OriginalLimit;
    
    // method info tips
    std::string strMethod;
    std::vector<std::string> strParams;
};

class SyntaxContext; // fwd decl

class AutoCompleteThread2
{
public:
    // TODO: Crit sec to synchronize access to things
    AutoCompleteThread2();
    ~AutoCompleteThread2();

    void InitializeForScript(CCrystalTextBuffer *buffer, LangSyntax lang);
    void StartAutoComplete(CPoint pt, HWND hwnd, UINT message, uint16_t scriptNumber);
    std::unique_ptr<AutoCompleteResult> GetResult(int id);
    CPoint GetCompletedPosition();
    void ResetPosition();
    void Exit();

private:
    static UINT s_ThreadWorker(void *pParam);
    void _DoWork();

    std::thread _thread;

    struct AutoCompleteId
    {
        int id;
        HWND hwnd;
        UINT message;
    };

    enum class AutoCompleteInstruction
    {
        None,
        Continue,   // Keep going from where we were
        Restart,    // Start anew
        Abort
    };

    enum class AutoCompleteStatus
    {
        None,
        Pending,
        Parsing,
        WaitingForMore,
    };

    void _SetResult(std::unique_ptr<AutoCompleteResult> result, AutoCompleteId id);

    // Both
    AutoCompleteId _id;
    std::unique_ptr<CScriptStreamLimiter> _limiterPending;
    std::unique_ptr<CCrystalScriptStream> _streamPending;
    uint16_t _scriptNumberPending;
    std::mutex _mutex;
    std::condition_variable _condition;
    AutoCompleteInstruction _instruction;
    AutoCompleteStatus _bgStatus;

    std::string _additionalCharacters;
    int _idUpdate;

    // Both
    std::unique_ptr<AutoCompleteResult> _result;
    int _resultId;
    std::mutex _mutexResult;

    // UI
    int _nextId;
    HWND _lastHWND;
    CPoint _lastPoint;
    CCrystalTextBuffer *_bufferUI;
    LangSyntax _lang;

    // background
};


