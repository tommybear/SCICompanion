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

void OutputSourceCode_SCI(const sci::Script &script, sci::SourceCodeWriter &out);
void OutputSourceCode_SCI(const sci::ClassDefinition &classDef, sci::SourceCodeWriter &out);
void OutputSourceCode_SCI(const sci::MethodDefinition &script, sci::SourceCodeWriter &out);
void OutputSourceCode_SCI(const sci::ClassProperty &classDef, sci::SourceCodeWriter &out);
