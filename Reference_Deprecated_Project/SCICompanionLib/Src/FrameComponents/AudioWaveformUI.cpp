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
#include "AudioWaveformUI.h"
#include "Audio.h"
#include "Sync.h"
#include "ResourceEntity.h"
#include <sphelper.h>
#include "sapi_lipsync.h"
#include <locale>
#include <codecvt>
#include "LipSyncUtil.h"

using namespace std;

IMPLEMENT_DYNAMIC(AudioWaveformUI, CStatic)

// AudioWaveformUI control

AudioWaveformUI::~AudioWaveformUI() {}

AudioWaveformUI::AudioWaveformUI() : AudioWaveformUI("") {}

AudioWaveformUI::AudioWaveformUI(const std::string &label)
    : CStatic(), _streamPosition(0), _audioComponent(nullptr), _label(label), _selected(true)
{
}

BEGIN_MESSAGE_MAP(AudioWaveformUI, CStatic)
    ON_WM_DRAWITEM_REFLECT()
END_MESSAGE_MAP()

COLORREF g_darkPhonemeColors[] =
{
    RGB(144, 144, 255),
    RGB(144, 255, 144),
    RGB(144, 255, 255),
    RGB(255, 144, 255),
    RGB(255, 255, 144),
    RGB(255, 255, 255),
};
COLORREF g_phonemeColors[] =
{
    RGB(196, 196, 255),
    RGB(196, 255, 196),
    RGB(196, 255, 255),
    RGB(255, 196, 255),
    RGB(255, 255, 196),
    RGB(255, 255, 255),
};

void AudioWaveformUI::SetResource(const AudioComponent *audio)
{
    _audioComponent = audio;
    _pbitmapDoubleBuf.reset(nullptr);
    Invalidate(FALSE);
}

void AudioWaveformUI::SetSelected(bool selected)
{
    _selected = selected;
    _pbitmapDoubleBuf.reset(nullptr);
    Invalidate(FALSE);
}

void AudioWaveformUI::SetStreamPosition(uint32_t streamPosition)
{
    _streamPosition = streamPosition;
    Invalidate(FALSE);
}

void AudioWaveformUI::SetRawLipSyncData(const SyncComponent &sync)
{
    std::string sierraRawLipSyncData = sync.RawData;
    // KQ6: Consists of a string of "phoneme [space] tickdelta [space]" pairs
    // LB2: Consists of {{}}, in between which are 2 chars and some sort of absolute time delta.
    bool isBinary = false;
    if (sierraRawLipSyncData.size() >= 2)
    {
        isBinary = sierraRawLipSyncData[0] == '{' && sierraRawLipSyncData[1] == '{';
    }

    vector<string> phonemes;
    vector<int> ticks;

    if (isBinary)
    {
        const int MagicNumber = 5;
        sci::istream stream(&sync.RawDataBinary[2], sync.RawDataBinary.size());
        uint16_t binaryPhoneme;
        stream >> binaryPhoneme;
        uint16_t previousAccTick = 0;
        while (stream.has_more_data() && binaryPhoneme != 0x7d7d) // }}
        {
            uint16_t accumulatedTicks;
            stream >> accumulatedTicks;
            std::string phonemeText;
            phonemeText.push_back((char)binaryPhoneme & 0xff);
            if ((binaryPhoneme >> 8) != 0x20) // space
            {
                phonemeText.push_back((char)(binaryPhoneme >> 8));
            }

            phonemes.push_back(phonemeText);
            ticks.push_back(SCITicksToMilliseconds((accumulatedTicks - previousAccTick) * MagicNumber));
            previousAccTick = accumulatedTicks;

            stream >> binaryPhoneme;
        }
    }
    else
    {
        stringstream ss(sierraRawLipSyncData);
        string token;
        bool isPhoneme = true;

        while (getline(ss, token, ' '))
        {
            if (isPhoneme)
            {
                phonemes.push_back(token);
            }
            else
            {
                ticks.push_back(SCITicksToMilliseconds(string_to_int(token)));
            }
            isPhoneme = !isPhoneme;
        }
    }

    _pbitmapDoubleBuf = nullptr;    // Trigger redraw.
    _rawLipSyncData.clear();

    uint32_t previousTime = 0;
    size_t count = min(phonemes.size(), ticks.size());
    for (size_t i = 0; i < count; i++)
    {
        uint32_t endTime = previousTime + ticks[i];
        UILipSyncData uiData = { previousTime, endTime, phonemes[i] };
        _rawLipSyncData.push_back(uiData);
        previousTime = endTime;
    }
    Invalidate(FALSE);
}

void AudioWaveformUI::SetRawLipSyncData(std::vector<alignment_result> &rawLipSyncData)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    _pbitmapDoubleBuf = nullptr;    // Trigger redraw.
    _rawLipSyncData.clear();
    for (auto &result : rawLipSyncData)
    {
        long prevTime = result.m_msStart;
        for (size_t i = 0; i < result.m_phonemes.size(); i++)
        {

            UILipSyncData uiData = { prevTime, result.m_phonemeEndTimes[i], converter.to_bytes(result.m_phonemes[i]) };
            _rawLipSyncData.push_back(uiData);
            prevTime = result.m_phonemeEndTimes[i];
        }

    }
    Invalidate(FALSE);
}

void AudioWaveformUI::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    LPRECT prc = &lpDrawItemStruct->rcItem;
    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);

    _GenerateDoubleBuffer(&dc, prc);

    CDC dcMem;
    if (dcMem.CreateCompatibleDC(&dc))
    {
        if (_pbitmapDoubleBuf)
        {
            HGDIOBJ hOldBmp = dcMem.SelectObject(_pbitmapDoubleBuf.get());

            // Copy into the final buffer:
            dc.StretchBlt(0, 0, RECTWIDTH(*prc), RECTHEIGHT(*prc), &dcMem, 0, 0, RECTWIDTH(*prc), RECTHEIGHT(*prc), SRCCOPY);

            dcMem.SelectObject(hOldBmp);
        }

        if (_audioComponent)
        {
            if (_audioComponent->GetLength() > 0)
            {
                uint32_t bar = _streamPosition * RECTWIDTH(*prc) / _audioComponent->GetLength();

                CPen pen(PS_SOLID, 1, RGB(0, 128, 0));
                HGDIOBJ hOld = dc.SelectObject(pen);
                dc.MoveTo(bar, prc->top);
                dc.LineTo(bar, prc->bottom);
                dc.SelectObject(hOld);
            }
        }
    }

    dc.Detach();
}

void AudioWaveformUI::_DrawWaveform(CDC *pDC, LPRECT prc)
{
    CDC dcMem;
    if (dcMem.CreateCompatibleDC(pDC))
    {
        HGDIOBJ hOldBmp = dcMem.SelectObject(_pbitmapDoubleBuf.get());

        dcMem.FillSolidRect(prc, RGB(196, 196, 196));

        int blockAlign = IsFlagSet(_audioComponent->Flags, AudioFlags::SixteenBit) ? 2 : 1;
        int scale = (blockAlign == 2) ? 32768 : 128;
        int sampleCount = _audioComponent->GetLength() / blockAlign;
        std::vector<CPoint> points;
        std::vector<DWORD> pointCounts;
        points.reserve(200);
        pointCounts.reserve(100);
        std::vector<CPoint> pointsClipped;
        std::vector<DWORD> pointCountsClipped;
        int yMiddle = (prc->top + prc->bottom) / 2;
        int height = RECTHEIGHT(*prc);
        int width = RECTWIDTH(*prc);
        std::unique_ptr<int[]> waveFormMax = std::make_unique<int[]>(width);
        std::unique_ptr<int[]> waveFormMin = std::make_unique<int[]>(width);
        int binSize = (sampleCount + (width - 1)) / width;  // "round up"
        for (int i = 0; i < sampleCount; i++)
        {
            int value;
            if (blockAlign == 1)
            { 
                value = (int)_audioComponent->DigitalSamplePCM[i] - scale; // Normalize
            }
            else
            {
                // 16 bit is signed already? (usually)
                value = *reinterpret_cast<const int16_t*>(&_audioComponent->DigitalSamplePCM[i * 2]);
            }

            waveFormMax[i / binSize] = max(waveFormMax[i / binSize], value);
            waveFormMin[i / binSize] = min(waveFormMin[i / binSize], value);
        }

        for (int x = 0; x < width; x++)
        {
            // Add a line from y to -y the middle.
            int yTop = waveFormMax[x] * height / scale / 2;
            int yBottom = waveFormMin[x] * height / scale / 2;

            bool clipped = false;
            if (blockAlign == 2)
            {
                clipped = (waveFormMax[x] == 32767) || (waveFormMin[x] == -32768);
            }
            else
            {
                clipped = (waveFormMax[x] == 127) || (waveFormMin[x] == -128);
            }

            if (clipped)
            {
                pointsClipped.emplace_back(x, yMiddle + yTop);
                pointsClipped.emplace_back(x, yMiddle + yBottom);
                pointCountsClipped.push_back(2);
            }
            else
            {
                points.emplace_back(x, yMiddle + yTop);
                points.emplace_back(x, yMiddle + yBottom);
                pointCounts.push_back(2);
            }
        }

        // Finally a horizontal line
        points.emplace_back(0, yMiddle);
        points.emplace_back(width, yMiddle);
        pointCounts.push_back(2);

        if (!points.empty())
        {
            CPen pen(PS_SOLID, 1, _selected ? RGB(0, 0, 128) : RGB(64, 64, 128));
            HGDIOBJ hOld = dcMem.SelectObject(pen);
            dcMem.PolyPolyline(&points[0], &pointCounts[0], (int)pointCounts.size());
            dcMem.SelectObject(hOld);
        }
        if (!pointsClipped.empty())
        {
            CPen pen(PS_SOLID, 1, _selected ? RGB(255, 0, 0) : RGB(255, 64, 64));
            HGDIOBJ hOld = dcMem.SelectObject(pen);
            dcMem.PolyPolyline(&pointsClipped[0], &pointCountsClipped[0], (int)pointCountsClipped.size());
            dcMem.SelectObject(hOld);
        }

        // If we have lipsync data, draw those phonemes now.
        long ms = _audioComponent->GetLength() * 1000 / _audioComponent->Frequency / blockAlign;
        int colorIndex = 0;
        dcMem.SetTextColor(RGB(0, 0, 0));
        dcMem.SetBkMode(TRANSPARENT);
        CRect rcMeasure = {};
        dcMem.DrawText("M", &rcMeasure, DT_SINGLELINE | DT_CALCRECT);
        int lineHeight = RECTHEIGHT(rcMeasure);
        int xLastEnds[4] = { 0, 0, 0, 0 };
        for (auto &phoneme : _rawLipSyncData)
        {
            // To start with, just draw them.
            int xStart = phoneme.start * width / ms;
            int xEnd = phoneme.stop * width / ms;
            if (xStart == xEnd)
            {
                xEnd++; // Ensure we have at least one pixel.
            }
            dcMem.SetBkColor(g_phonemeColors[colorIndex]);
            colorIndex++;
            colorIndex %= ARRAYSIZE(g_phonemeColors);

            // Find a "line" on which to draw it
            int line = 0;
            while (line < ARRAYSIZE(xLastEnds))
            {
                if (xStart >= xLastEnds[line])
                {
                    break;
                }
                line++;
            }
            line %= ARRAYSIZE(xLastEnds);
            int yTop = line * lineHeight;

            CRect rcMeasureP = { xStart, 0, 0, 0 };
            dcMem.DrawText(phoneme.phoneme.c_str(), &rcMeasureP, DT_SINGLELINE | DT_CALCRECT);
            
            // Background...
            CRect rcBackgroundDark = { xStart, yTop, xEnd, yTop + lineHeight };
            dcMem.FillSolidRect(&rcBackgroundDark, g_darkPhonemeColors[colorIndex]);
            if (xEnd < rcMeasureP.right)
            {
                // Text is longer than timespan... extend the background under it
                CRect rcBackground = { xEnd, yTop, rcMeasureP.right, yTop + lineHeight };
                dcMem.FillSolidRect(&rcBackground, g_phonemeColors[colorIndex]);
            }

            CRect rcText = { xStart, yTop, rcMeasureP.right, yTop + lineHeight };
            dcMem.DrawText(phoneme.phoneme.c_str(), &rcText, DT_SINGLELINE);

            // Two widths to consider... text string length and phoneme length.
            xLastEnds[line] = max(xEnd, rcMeasureP.right); 
        }

        if (!_label.empty())
        {
            dcMem.SetTextColor(RGB(92, 92, 92));
            CRect rc = {};
            dcMem.DrawText(_label.c_str(), -1, &rc, DT_SINGLELINE | DT_CALCRECT);
            rc.left = prc->right - rc.Width();
            rc.right = prc->right;
            rc.top = prc->bottom - rc.Height();
            rc.bottom = prc->bottom;
            dcMem.DrawText(_label.c_str(), -1, &rc, DT_SINGLELINE);
        }

        dcMem.SelectObject(hOldBmp);
    }
}

void AudioWaveformUI::_GenerateDoubleBuffer(CDC *pDC, LPRECT prc)
{
    CSize sizeCurrent = CSize(RECTWIDTH(*prc), RECTHEIGHT(*prc));
    if (!_pbitmapDoubleBuf || (_sizeDoubleBuf != sizeCurrent))
    {
        if (_audioComponent)
        {
            _sizeDoubleBuf = sizeCurrent;
            _pbitmapDoubleBuf = make_unique<CBitmap>();
            // Hope this succeededs...
            if (_pbitmapDoubleBuf->CreateCompatibleBitmap(pDC, _sizeDoubleBuf.cx, _sizeDoubleBuf.cy))
            {
                _DrawWaveform(pDC, prc);
            }
        }
    }
}

