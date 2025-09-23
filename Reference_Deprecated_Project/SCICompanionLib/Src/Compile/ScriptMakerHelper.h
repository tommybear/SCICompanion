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

template<typename _T>
void _AddStatement(_T &method, std::unique_ptr<sci::SyntaxNode> pNode)
{
    method.AddStatement(std::move(pNode));
}

std::unique_ptr<sci::SyntaxNode> _MakeTokenStatement(const std::string &token);

void _AddAssignment(sci::MethodDefinition &method, const std::string &lvalueName, const std::string &assigned);

template<typename _T>
void _AddComment(_T &method, const std::string &comment, sci::CommentType type)
{
    _AddStatement(method, std::make_unique<sci::Comment>(comment, type));
}

void _AddBasicSwitch(sci::MethodDefinition &method, const std::string &switchValue, const std::string &case0Comments);

void _AddSendCall(sci::MethodDefinition &method, const std::string &objectName, const std::string &methodName, const std::string &parameter, bool isVariable = false);

void _SetSendVariableTarget(sci::SendCall &send, const std::string &target);

std::unique_ptr<sci::SyntaxNode> _MakeNumberStatement(int16_t w);