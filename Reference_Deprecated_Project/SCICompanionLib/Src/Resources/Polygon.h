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

#include "Components.h"

enum class PolygonType
{
    // These values must match those in sci.h
    TotalAccess = 0,
    NearestAccess = 1,
    BarredAccess = 2,
    ContainedAccess = 3
};

namespace sci
{
    class ProcedureCall;
}

class PolygonComponent;

class SCIPolygon
{
public:
    SCIPolygon();
    const std::vector<point16> &Points() const { return _points; }
    std::vector<point16> &Points() { return _points; }
    void AppendPoint(point16 point);
    void DeletePoint(size_t index);
    void SetPoint(size_t index, point16 point);
    void InsertPoint(size_t index, point16 point);
    PolygonType Type;
    std::string Name;

private:
    std::vector<point16> _points;
};

bool operator==(const SCIPolygon &one, const SCIPolygon &two);
bool operator!=(const SCIPolygon &one, const SCIPolygon &two);

class PolygonComponent : public ResourceComponent
{
public:
    PolygonComponent(const std::string &polyFolder, int picNumber);
    void Commit(int picNumber);

    PolygonComponent *Clone() const override
    {
        return new PolygonComponent(*this);
    }

    const std::vector<SCIPolygon> &Polygons() const { return _polygons; }
    SCIPolygon *GetAt(size_t index);
    const SCIPolygon *GetAt(size_t index) const;
    SCIPolygon *GetBack();
    void AppendPolygon(const SCIPolygon &polygon);
    void DeletePolygon(size_t index);
    std::string GetPolyFilePath() const;
    std::string GetPolyFilename() const;

private:

    std::vector<SCIPolygon> _polygons;
    std::string _polyFolder;
    int _picNumber;
};

bool operator==(const PolygonComponent &one, const PolygonComponent &two);
bool operator!=(const PolygonComponent &one, const PolygonComponent &two);

std::unique_ptr<PolygonComponent> CreatePolygonComponent(const std::string &polyFolder, int picNumber);
std::unique_ptr<sci::ProcedureCall> GetSetUpPolyProcedureCall(int picResource);
void FixupPolygon(SCIPolygon &polygon);