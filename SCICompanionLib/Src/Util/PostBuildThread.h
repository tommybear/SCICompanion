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

class PostBuildThread
{
public:
    PostBuildThread(const std::string &gameFolder);

    void Abort();

    friend std::shared_ptr<PostBuildThread> CreatePostBuildThread(const std::string &gameFolder);

private:
    void _Start(std::shared_ptr<PostBuildThread> myself);
    static UINT s_PostBuildThreadWorker(void *pParam);
    void _Main();

    HWND _hwndUI;
    std::shared_ptr<PostBuildThread> _myself;
    std::string _gameFolder;
    ScopedHandle _hAbort;
};

std::shared_ptr<PostBuildThread> CreatePostBuildThread(const std::string &gameFolder);
