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
#include "ControlFlowNode.h"
#include "ControlFlowGraphViz.h"
#include "format.h"
#include "PMachine.h"

using namespace std;

const char *colors[] = 
{
    "black",
    "red",
    "blue",
    "green",
    "purple",
    "orange",
};

class GraphVisualizer
{
    string _CreateGraphNodeId(const ControlFlowNode *node, int index, const ControlFlowNode *head = nullptr)
    {
        string name;
        switch (node->Type)
        {
            case CFGNodeType::RawCode:
            {
                const RawCodeNode *rawCodeNode = static_cast<const RawCodeNode *>(node);
                name = fmt::format("n_{:04x}_{}", (rawCodeNode->start->get_final_offset_dontcare()), index);
                break;
            }

            case CFGNodeType::Loop:
                name = fmt::format("loop_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::Case:
                name = fmt::format("case_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::Switch:
                name = fmt::format("switch_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::CompoundCondition:
            {
                const CompoundConditionNode *ccNode = static_cast<const CompoundConditionNode *>(node);
                name = fmt::format((ccNode->condition == ConditionType::And) ? "and_{}" : "or_{}", node->ArbitraryDebugIndex);
                break;
            }
            case CFGNodeType::Exit:
                name = fmt::format("exit_{}", node->ArbitraryDebugIndex);
                break;
               
            case CFGNodeType::Invert:
                name = fmt::format("inv_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::If:
                name = fmt::format("if_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::CommonLatch:
                name = fmt::format("commonlatch_{}", node->ArbitraryDebugIndex);
                break;

            case CFGNodeType::FakeBreakOrContinue:
                name = fmt::format("fakebreakorcont_{}", node->ArbitraryDebugIndex);
                break;

            default:
                assert(false);
                break;
        }

        return name;
    }

    void _CreateHumanReadableLabel(stringstream &ss, const ControlFlowNode *node)
    {
        ss << " [label =\"";

        switch (node->Type)
        {
            case CFGNodeType::RawCode:
            {
                const RawCodeNode *rawCodeNode = static_cast<const RawCodeNode *>(node);
                code_pos cur = rawCodeNode->start;
                ss << fmt::format("{:04x}:\\n", cur->get_final_offset_dontcare());
                while (cur != rawCodeNode->end)
                {
                    Opcode opcode = cur->get_opcode();
                    const char *pszOpcode = OpcodeToName(opcode, cur->get_first_operand());
                    ss << pszOpcode << "\\n";
                    ++cur;
                }
            }
            break;

            case CFGNodeType::Loop:
            {
                ss << "Loop " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::Case:
            {
                ss << "Case " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::Switch:
            {
                ss << "Switch " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::Invert:
            {
                ss << "Invert " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::CommonLatch:
            {
                ss << "CommonLatch " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::FakeBreakOrContinue:
            {
                ss << "FakeBreakOrCont " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::Exit:
            {
                ss << "Exit:" << fmt::format("{:04x}", static_cast<const ExitNode*>(node)->startingAddressForExit);
            }
            break;

            case CFGNodeType::CompoundCondition:
            {
                const CompoundConditionNode *ccNode = static_cast<const CompoundConditionNode *>(node);
                ss << (ccNode->isFirstTermNegated ? "!" : "") << "X " << ((ccNode->condition == ConditionType::And) ? "and" : "or") << " Y " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::Main:
            {
                ss << "Main " << node->ArbitraryDebugIndex;
            }
            break;

            case CFGNodeType::If:
            {
                ss << "If " << node->ArbitraryDebugIndex;
            }
            break;


            default:
                assert(false);
                break;

        }

        ss << "\"]";
    }

    // For reading purposes
    void _CreateLabel(stringstream &ss, const ControlFlowNode *node, int index, int colorIndex, const ControlFlowNode *head = nullptr)
    {
        colorIndex %= ARRAYSIZE(colors);

        ss << "\t" << _CreateGraphNodeId(node, index, head);

        _CreateHumanReadableLabel(ss, node);

        ss << " [color=" << colors[colorIndex] << "];\n";
    }

public:
    void CFGVisualize(const std::string &name, NodeSet &discoveredControlStructures)
    {
        // Visualize it, until we know it's right
        std::stringstream ss;
        ss << "digraph code {\n";

        int graphIndex = 0;
        for (ControlFlowNode *currentParent : discoveredControlStructures)
        {
            int colorIndex = currentParent->ArbitraryDebugIndex;
            for (auto node : currentParent->Children())
            {
                if (discoveredControlStructures.contains(node))
                {
                    // This is a "parent", so color it like its children.
                    _CreateLabel(ss, node, graphIndex, node->ArbitraryDebugIndex);
                }
                else
                {
                    _CreateLabel(ss, node, graphIndex, colorIndex);
                }

                // Construct the graph from the predecessors
                for (auto &pred : node->Predecessors())
                {
                    ss << "\t" << _CreateGraphNodeId(pred, graphIndex) << " -> " << _CreateGraphNodeId(node, graphIndex);
                    ss << " [color=" << colors[colorIndex % ARRAYSIZE(colors)] << "];\n";
                }

                // If it's the head, but a fake entry point
                if (node == currentParent->MaybeGet(SemId::Head))
                {
                    // Special case, no predecessor. This is an entry point.
                    string nodeLabel = fmt::format("ENTRY_{}", graphIndex);
                    ss << "\t" << nodeLabel << " -> " << _CreateGraphNodeId(node, graphIndex);
                    ss << ";\n";

                    // Name it
                    ss << "\t" << nodeLabel;
                    _CreateHumanReadableLabel(ss, currentParent);
                    ss << ";\n";
                }
            }
            graphIndex++;
        }
        ss << "}\n";
        ShowTextFile(ss.str().c_str(), name + ".txt");
    }
};


void CFGVisualize(const std::string &name, NodeSet &discoveredControlStructures)
{
    GraphVisualizer viz;
    viz.CFGVisualize(name, discoveredControlStructures);
}