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

struct ResourceComponent
{
    virtual ResourceComponent* Clone() const = 0;

    // This is necessary, or else lists of ResourceComponents won't be properly destroyed.
    virtual ~ResourceComponent() {}
};

typedef void(*GetItemLabelFuncPtr)(PTSTR  pszLabel, size_t cch, int nCel);

extern uint8_t g_egaPaletteMapping[16];
