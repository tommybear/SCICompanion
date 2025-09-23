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

class GameFolderHelper;
class CResourceMap;
class ResourceBlob;
class ResourceSource;
enum class ResourceSourceAccessFlags;
enum class ResourceTypeFlags;
void DeleteResource(CResourceMap &resourceMap, const ResourceBlob &data);
std::unique_ptr<ResourceSource> CreateResourceSource(ResourceTypeFlags flagsHint, const GameFolderHelper &helper, ResourceSourceFlags source, ResourceSourceAccessFlags access = ResourceSourceAccessFlags::Read, int mapContext = -1);