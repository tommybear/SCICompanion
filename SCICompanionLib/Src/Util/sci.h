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

//
// General stuff 
//
//
#pragma once

#ifdef DEBUG
#define DOCSUPPORT
#endif

//
// Resource types
// The numbers here are important, they map to the
// actual values that SCI uses, and are used for array lookups.
//
enum class ResourceType
{
    None = -1,
    View = 0,
    Pic = 1,
    Script = 2,
    Text = 3,
    Sound = 4,
    Memory = 5,
    Vocab = 6,
    Font = 7,
    Cursor = 8,
    Patch = 9,
    // SCI1 and beyond....
    Bitmap = 10,
    Palette = 11,
    CDAudio = 12,
    Audio = 13,
    Sync = 14,          // Included with Audio resources, not a separate type
    Message = 15,
    AudioMap = 16,
    Heap = 17,

    Max = 18
};

// Used to color and identify tabs.
enum MDITabType
{
    TAB_NONE           =  0x00000000,
    TAB_GAME           =  0x00000001,   // The game explorer
    TAB_VIEW           =  0x00000002,
    TAB_PIC            =  0x00000004,
    TAB_SCRIPT         =  0x00000008,
    TAB_VOCAB          =  0x00000010,
    TAB_FONT           =  0x00000020,
    TAB_CURSOR         =  0x00000040,
    TAB_TEXT           =  0x00000080,   // Shared with message
    TAB_SOUND          =  0x00000100,
    TAB_ROOMEXPLORER   =  0x00000200,
    TAB_PALETTE        =  0x00000400,
    TAB_MESSAGE        =  0x00000800
};

#define DEFINE_ENUM_FLAGS(_enumName_ , _enumType_) \
inline _enumName_ operator | (_enumName_ lhs, _enumName_ rhs) \
        { \
    return static_cast<_enumName_>((static_cast<_enumType_>(lhs) | static_cast<_enumType_>(rhs))); \
        } \
inline _enumName_& operator |= (_enumName_& lhs, _enumName_ rhs) \
        { \
    lhs = static_cast<_enumName_>((static_cast<_enumType_>(lhs) | static_cast<_enumType_>(rhs))); \
    return lhs; \
        } \
inline _enumName_ operator & (_enumName_ lhs, _enumName_ rhs) \
        { \
return static_cast<_enumName_>((static_cast<_enumType_>(lhs) & static_cast<_enumType_>(rhs))); \
        } \
inline _enumName_& operator &= (_enumName_& lhs, _enumName_ rhs) \
        { \
lhs = static_cast<_enumName_>((static_cast<_enumType_>(lhs) & static_cast<_enumType_>(rhs))); \
return lhs; \
        }  \
inline _enumName_ operator ~(_enumName_ hint) \
    { \
    return static_cast<_enumName_>(~static_cast<_enumType_>(hint)); \
    } \
inline bool IsFlagSet(_enumName_ hint, _enumName_ test) \
    { \
    return (static_cast<_enumType_>(hint)& static_cast<_enumType_>(test)) != 0; \
    } \
inline bool AreAllFlagsSet(_enumName_ hint, _enumName_ test) \
    { \
    return (static_cast<_enumType_>(hint)& static_cast<_enumType_>(test)) == static_cast<_enumType_>(test); \
    } \
inline void ClearFlag(_enumName_ &hint, _enumName_ clear) \
    { \
    hint &= ~clear; \
    }


                                                        
enum class ResourceTypeFlags
{
    None = 0,
    View = 1 << (int)ResourceType::View,
    Pic = 1 << (int)ResourceType::Pic,
    Script = 1 << (int)ResourceType::Script,
    Text = 1 << (int)ResourceType::Text,
    Sound = 1 << (int)ResourceType::Sound,
    Memory = 1 << (int)ResourceType::Memory,
    Vocab = 1 << (int)ResourceType::Vocab,
    Font = 1 << (int)ResourceType::Font,
    Cursor = 1 << (int)ResourceType::Cursor,
    Patch = 1 << (int)ResourceType::Patch,
    Bitmap = 1 << (int)ResourceType::Bitmap,
    Palette = 1 << (int)ResourceType::Palette,
    CDAudio = 1 << (int)ResourceType::CDAudio,
    Audio = 1 << (int)ResourceType::Audio,
    // NOTE: Sync resources are included in Audio resources.
    // Sync = 1 << (int)ResourceType::Sync,
    Message = 1 << (int)ResourceType::Message,
    AudioMap = 1 << (int)ResourceType::AudioMap,
    Heap = 1 << (int)ResourceType::Heap,

    All = 0x3fffffff,

    AllCreatable = View | Font | Cursor | Text | Sound | Vocab | Pic | Palette | Message | Audio | AudioMap,
};
DEFINE_ENUM_FLAGS(ResourceTypeFlags, uint32_t)

static const int NumResourceTypesSCI0 = 10;
static const int NumResourceTypes = 18;

// Map ID_SHOW_VIEWS to ResourceType::View, for example.  -1 if no match.
ResourceType ResourceCommandToType(UINT nId);

BOOL IsValidResourceType(int iResourceType);
BOOL IsValidResourceNumber(int iResourceNum);

static const uint32_t NoBase36 = 0xffffffff;

std::string GetGameIniFileName(const std::string &gameFolder);
std::string FigureOutResourceName(const std::string &iniFileName, ResourceType type, int iNumber, uint32_t base36Number);

const TCHAR *g_rgszTypeToSectionName[];

ResourceTypeFlags ResourceTypeToFlag(ResourceType dwType);
ResourceType ResourceFlagToType(ResourceTypeFlags dwType);
struct sPOINT
{
    __int16 x;
    __int16 y;
};

struct ScopedHandle
{
    ScopedHandle() : hFile(INVALID_HANDLE_VALUE) {}
    void Close()
    {
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }
    virtual ~ScopedHandle()
    {
        Close();
    }

    HANDLE hFile;
};

bool DirectoryExists(LPCTSTR szPath);
void AdvancePastWhitespace(const std::string &line, size_t &offset);

struct ScopedFile : public ScopedHandle
{
    ScopedFile(const std::string &filename, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition);
    void Write(const uint8_t *data, uint32_t length);
    uint32_t GetLength();
    uint32_t SeekToEnd();

    std::string filename;
};

#define DEFAULT_PIC_WIDTH      320
#define DEFAULT_PIC_HEIGHT     190  
#define sPIC_WIDTH_MAX          320
#define sPIC_HEIGHT_MAX         200
#define SCITicksPerSecond 60

// 320 x 190 pixels, each a byte. (For our drawing buffers)
#define BMPSIZE (DEFAULT_PIC_WIDTH * DEFAULT_PIC_HEIGHT)
#define BMPSIZE_MAX (sPIC_WIDTH_MAX * sPIC_HEIGHT_MAX)

struct EGACOLOR
{
    // Color2 comes before color 1, as it occupies the low bits.
    BYTE color2:4;      // 0000 xxxx
    BYTE color1:4;      // xxxx 0000

    BYTE ToByte() const
    {
        return color2 | (color1 << 4);
    }
};

#define EGACOLOR_TO_BYTE(x) ((x).color2 | ((x).color1 << 4))
#define EGACOLOR_EQUAL(a, b) (EGACOLOR_TO_BYTE(a) == EGACOLOR_TO_BYTE(b))

TCHAR g_szGdiplusFilter[];

extern RGBQUAD g_egaColors[16];
extern RGBQUAD g_egaColorsPlusOne[17];
extern RGBQUAD g_egaColorsExtended[256];	// 256, 16 repeated 16 times.
extern RGBQUAD g_egaColorsMixed[256];		// 256, 16x16 mixed (136 unique colors)
extern RGBQUAD g_continuousPriorityColors[256];
extern COLORREF g_egaColorsCR[16];
#define EGA_TO_RGBQUAD(x)  g_egaColors[(x)]
#define EGA_TO_COLORREF(x) g_egaColorsCR[(x)]
RGBQUAD _Combine(RGBQUAD color1, RGBQUAD color2);
RGBQUAD _Darker(RGBQUAD color);
RGBQUAD _Lighter(RGBQUAD color);
RGBQUAD _RGBQuadFromColorRef(COLORREF color);
COLORREF _ColorRefFromRGBQuad(RGBQUAD color);
RGBQUAD EgaColorToRGBQuad(EGACOLOR ega);
extern const int VocabKernelNames;

EGACOLOR g_egaColorChooserPalette[];

EGACOLOR EGAColorFromByte(BYTE b);

int GetColorDistance(COLORREF color1, COLORREF color2);
int GetColorDistanceRGB(RGBQUAD color1, RGBQUAD color2);
EGACOLOR GetClosestEGAColor(int iAlgorithm, bool gammaCorrected, int iPalette, COLORREF color);
EGACOLOR GetClosestEGAColorFromSet(int iAlgorithm, bool gammaCorrected, COLORREF color, EGACOLOR *rgColors, int cColors);

// EGA:
#define PALETTE_SIZE 40
#define GET_PALDEX(s)               ((s)/PALETTE_SIZE)
#define GET_PALCOL(s)               ((s)%PALETTE_SIZE)

// TODO: use for logging errors
#define REPORTERROR(x) 

// These values are important.  Pattern codes look like this:
// 87654321
// bits 6 an 5 are the flags below. Bits 3, 2 and 1 are the pattern size.
#define PATTERN_FLAG_RECTANGLE 0x10
#define PATTERN_FLAG_USE_PATTERN 0x20

#define RECTWIDTH(rc) ((rc).right - (rc).left)
#define RECTHEIGHT(rc) ((rc).bottom - (rc).top)

//
// BITMAPINFO for standard SCI0 raster resources.  We use 8bit
// even though we may only use 16 (or less) of the 256 colours.
//
struct SCIBitmapInfo : public BITMAPINFO
{
    SCIBitmapInfo() {}
    SCIBitmapInfo(int cx, int cy, const RGBQUAD *pPalette = nullptr, int count = 0);
    RGBQUAD _colors[256];
};

// Has an extended palette with duplicate colours, so we can hide information
// within.
struct SCIBitmapInfoEx : public BITMAPINFO
{
    SCIBitmapInfoEx(int cx, int cy);
    RGBQUAD _colors[256];
};

class ViewPort;
BYTE PriorityFromY(WORD y, const ViewPort &picState);

// The actual width of a cel, used in the bitmaps (multiple of 32 bits)
#define CX_ACTUAL(cx) (((cx) + 3) / 4 * 4)

inline HRESULT ResultFromLastError() { return HRESULT_FROM_WIN32(GetLastError()); }

void DisplayFileError(HRESULT hr, BOOL fOpen, LPCTSTR pszFileName);

//
// Represents a position in a script.
//
class LineCol
{
public:
    LineCol() : _dwPos(0) {}
    LineCol(int line, int column) : _dwPos(((((DWORD)(line)) << 16)+ (DWORD)(column))) {}
    int Line() const { return static_cast<int>((((_dwPos) >> 16) & 0xffff)); }
    int Column() const { return static_cast<int>(((_dwPos) & 0x0000ffff)); }
    bool operator<(const LineCol& _Right) const
    {
        return _dwPos < _Right._dwPos;
    }
    bool operator<=(const LineCol& _Right) const
    {
        return _dwPos <= _Right._dwPos;
    }
private:
    DWORD _dwPos;
};

struct GlobalAllocGuard
{
    GlobalAllocGuard(HGLOBAL hGlobal)
    {
        Global = hGlobal;
    }

    GlobalAllocGuard(UINT uFlags, SIZE_T dwBytes)
    {
        Global = GlobalAlloc(uFlags, dwBytes);
    }

    void Free()
    {
        if (Global)
        {
            GlobalFree(Global);
        }
        Global = nullptr;
    }

    HGLOBAL RelinquishOwnership()
    {
        HGLOBAL temp = Global;
        Global = nullptr;
        return temp;
    }

    ~GlobalAllocGuard()
    {
        Free();
    }

    HGLOBAL Global;
};

template<typename _T>
struct GlobalLockGuard
{
    GlobalLockGuard(HGLOBAL mem)
    {
        Object = reinterpret_cast<_T>(GlobalLock(mem));
    }

    GlobalLockGuard(GlobalAllocGuard &allocGuard)
    {
        Object = reinterpret_cast<_T>(GlobalLock(allocGuard.Global));
    }

    void Unlock()
    {
        if (Object)
        {
            GlobalUnlock(Object);
        }
        Object = nullptr;
    }

    ~GlobalLockGuard()
    {
        Unlock();
    }

    _T Object;
};


// Common combination of flags.
#define MB_ERRORFLAGS (MB_OK | MB_ICONEXCLAMATION | MB_APPLMODAL)

WORD _HexToWord(PCTSTR psz);

enum LangSyntax
{
    // Don't change these values, they are used to index into comboboxes.
    LangSyntaxUnknown = 2,
    LangSyntaxStudio = 1,
    LangSyntaxSCI = 0,
};

bool IsSCIKeyword(LangSyntax lang, const std::string &word);
bool IsTopLevelKeyword(LangSyntax lang, const std::string &word);
const std::vector<std::string> &GetTopLevelKeywords(LangSyntax lang);
bool IsCodeLevelKeyword(LangSyntax lang, const std::string &word);
bool IsClassLevelKeyword(LangSyntax lang, const std::string &word);
const std::vector<std::string> &GetCodeLevelKeywords(LangSyntax lang);
const std::vector<std::string> &GetClassLevelKeywords(LangSyntax lang);
const std::vector<std::string> &GetValueKeywords(LangSyntax lang);
bool IsValueKeyword(const std::string &word);
int string_to_int(const std::string &word);
static const WORD InvalidResourceNumber = 0xffff;

//
// A script description consists of:
// 1) full folder name
// 2) script title
// 2) script file name
//
class ScriptId
{
public:
    ScriptId();
    ScriptId(const std::string &fullPath);
    ScriptId(PCTSTR pszFullFileName);
    ScriptId(PCTSTR pszFileName, PCTSTR pszFolder);
    ScriptId(const ScriptId &src);
    ScriptId& operator=(const ScriptId& src);

    void SetLanguage(LangSyntax lang);

    BOOL IsNone() const;
    const std::string &GetFileName() const;
    const std::string &GetFolder() const;
    const std::string &GetFileNameOrig() const;

    // e.g. for Main.sc, it returns Main.  For keys.sh, it returns keys
    std::string GetTitle() const;
    // e.g. for Main.sc, it returns main.  For keys.sh, it returns keys
    std::string GetTitleLower() const;
    
    // Returns the complete path, for loading/saving, etc...
    std::string GetFullPath() const;

    // Set the path w/o changing the resource number.
    void SetFullPath(const std::string &fullPath);

    // Script resource number
    WORD GetResourceNumber() const { return _wScriptNum; }
    void SetResourceNumber(WORD wScriptNum);

    // Is this a header, or a script file?
    bool IsHeader() const;
    LangSyntax Language() const;

    friend bool operator<(const ScriptId& script1, const ScriptId& script2);

private:
    void _MakeLower();
    void _Init(PCTSTR pszFullFileName, WORD wScriptNum = InvalidResourceNumber);
    void _DetermineLanguage();

    std::string _strFolder;
    std::string _strFileName;
    std::string _strFileNameOrig;   // Not lower-cased
    WORD _wScriptNum;
    LangSyntax _language;
};

LangSyntax _DetermineLanguage(const std::string &firstLine);

extern const std::string SCILanguageMarker;

//
// equality operator for ScriptId
//
bool operator==(const ScriptId& script1, const ScriptId& script2);

bool operator<(const ScriptId& script1, const ScriptId& script2);


//
// Hash function for ScriptId
//
//template<> UINT AFXAPI HashKey<ScriptId&>( ScriptId& key );

#define PACKVERSION(major,minor) MAKELONG(minor,major)
DWORD GetDllVersion(LPCTSTR lpszDllName);

class CPrecisionTimer
{
  LARGE_INTEGER lFreq, lStart;

public:
  CPrecisionTimer()
  {
    QueryPerformanceFrequency(&lFreq);
  }

  inline void Start()
  {
    //SetThreadAffinityMask( GetCurrentThread(), 1 );
    QueryPerformanceCounter(&lStart);
  }
  
  inline double Stop()
  {
    // Return duration in seconds...
    LARGE_INTEGER lEnd;
    QueryPerformanceCounter(&lEnd);
    return (static_cast<double>(lEnd.QuadPart - lStart.QuadPart) / static_cast<double>(lFreq.QuadPart));
  }
};

const std::string MakeFile(PCSTR pszContent, const std::string &filename);
void ShowTextFile(PCSTR pszContent, const std::string &filename);
void ShowFile(const std::string &actualPath);
std::string MakeTextFile(PCSTR pszContent, const std::string &filename);
std::string GetBinaryDataVisualization(const uint8_t *data, size_t length, int columns = 16);

#define RGB_TO_COLORREF(x) RGB(x.rgbRed, x.rgbGreen, x.rgbBlue)

void throw_if(bool value, const char *message);

class IClassBrowserEvents
{
public:
    enum BrowseInfoStatus
    {
        Ok,
        Errors,
        InProgress
    };
    virtual void NotifyClassBrowserStatus(BrowseInfoStatus status, int iPercent) = 0;
};

class ResourceBlob;
BOOL OpenResource(const ResourceBlob *pData, bool setModifier = false);
int ResourceNumberFromFileName(PCTSTR pszFileName);
void deletefile(const std::string &filename);
void movefile(const std::string &from, const std::string &to);
void testopenforwrite(const std::string &filename);
uint32_t GetResourceOffsetInFile(uint8_t secondHeaderByte);
extern const TCHAR g_szResourceSpec[];
extern const TCHAR* g_szResourceSpecByType[18];
std::string GetMessageFromLastError(const std::string &details);
void ToUpper(std::string &aString);
bool IsCodeFile(const std::string &text);

class ResourceEntity;
struct Cel;
const Cel &GetCel(const ResourceEntity *pvr, int &nLoop, int &nCel);

enum class PicScreen
{
    Visual = 0,
    Priority = 1,
    Control = 2,
    Aux = 3,
};

enum class PicScreenFlags
{
    None = 0x0000,
    Visual = 0x0001,
    Priority = 0x0002,
    Control = 0x0004,
    Aux = 0x0008,
    All = 0x0007,
    VGA = 0x0080,       // A hack, to pass information through to our draw functions that we should write vga
};
DEFINE_ENUM_FLAGS(PicScreenFlags, uint32_t)

std::string GetExeSubFolder(const char *subFolder);
bool CopyFilesOver(HWND hwnd, const std::string &from, const std::string &to);
bool DeleteDirectory(HWND hwnd, const std::string &folder);
std::string GetRandomTempFolder();
bool EnsureFolderExists(const std::string &folderName, bool throwException = true);

enum class OutputPaneType
{
    Compile = 0,
    Find = 1,
    Debug = 2
};

bool TerminateProcessTree(HANDLE hProcess, DWORD retCode);

// Returns "n004" for 4.
std::string default_reskey(int iNumber, uint32_t base36Number);


struct size16
{
    size16() : size16(0, 0) {}
    size16(uint16_t width, uint16_t height) : cx(width), cy(height) {}

    uint16_t cx;
    uint16_t cy;

    bool operator==(const size16 &other) const
    {
        return cx == other.cx && cy == other.cy;
    }
    bool operator!=(const size16 &other) const
    {
        return cx != other.cx || cy != other.cy;
    }
    size16 operator+(size16 other) const
    {
        return size16(cx + other.cx, cy + other.cy);
    }
    size16 operator-(size16 other) const
    {
        return size16(cx - other.cx, cy - other.cy);
    }
};

int PaddedSize(size16 size);

struct point16
{
    point16() : point16(0, 0) {}
    point16(int16_t xIn, int16_t yIn) : x(xIn), y(yIn) {}

    int16_t x;
    int16_t y;

    bool operator==(const point16 &other) const
    {
        return x == other.x && y == other.y;
    }
    bool operator!=(const point16 &other) const
    {
        return x != other.x || y != other.y;
    }
    point16 operator+(point16 point) const
    {
        return point16(x + point.x, y + point.y);
    }
    point16 operator-(point16 point) const
    {
        return point16(x - point.x, y - point.y);
    }
};

