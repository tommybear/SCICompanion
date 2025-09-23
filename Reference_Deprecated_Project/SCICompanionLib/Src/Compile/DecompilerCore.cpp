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
#include "DecompilerCore.h"
#include "PMachine.h"
#include "ScriptOMAll.h"
#include "scii.h"
#include "DisassembleHelper.h"
#include "ControlFlowGraph.h"
#include "DecompilerNew.h"
#include "DecompilerFallback.h"
#include "format.h"
#include "DecompilerConfig.h"
#include <iterator>
#include "GameFolderHelper.h"
#include "Operators.h"

#define DEBUG_DECOMPILER 1

using namespace sci;
using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BinaryOperator GetBinaryOpFromAssignment(AssignmentOperator assignment);

const char InvalidLookupError[] = "LOOKUP_ERROR";
const char RestParamName[] = "params";
const char SelfToken[] = "self";

ValueType _ScriptObjectTypeToPropertyValueType(ICompiledScriptSpecificLookups::ObjectType type)
{
    switch (type)
    {
    case ICompiledScriptSpecificLookups::ObjectTypeSaid:
        return ValueType::Said;
        break;
    case ICompiledScriptSpecificLookups::ObjectTypeString:
        return ValueType::String;
        break;
    case ICompiledScriptSpecificLookups::ObjectTypeClass:
        return ValueType::Token;
        break;
    }
    return ValueType::Token;
}


bool _IsVOIndexed(Opcode bOpcode)
{
    return !!(static_cast<BYTE>(bOpcode) & 0x08);
}
bool _IsVOPureStack(Opcode bOpcode)
{
    return !!(static_cast<BYTE>(bOpcode) & 0x04);
}
bool _IsVOStack(Opcode bOpcode)
{
    // It's a stack operation if it says it's a stack operation,
    // or if the accumulator is being used as an index.
    // WARNING PHIL: true for store, but maybe no load???
    return _IsVOPureStack(bOpcode) || _IsVOIndexed(bOpcode);
}
bool _IsVOStoreOperation(Opcode bOpcode)
{
    return ((static_cast<BYTE>(bOpcode) & 0x30) == 0x10);
}
bool _IsVOIncremented(Opcode bOpcode)
{
    return ((static_cast<BYTE>(bOpcode) & 0x30) == VO_INC_AND_LOAD);
}
bool _IsVODecremented(Opcode bOpcode)
{
    return ((static_cast<BYTE>(bOpcode) & 0x30) == VO_DEC_AND_LOAD);
}

std::string _GetPublicProcedureName(WORD wScript, WORD wIndex)
{
    std::stringstream ss;
    ss << "proc" << wScript << "_" << wIndex;
    return ss.str();
}

std::string _GetBaseProcedureName(WORD wIndex)
{
    return _GetPublicProcedureName(0, wIndex);
}

typedef std::list<scii>::reverse_iterator rcode_pos;

struct Fixup
{
    code_pos branchInstruction;
    WORD wTarget;
    bool fForward;
};

code_pos get_cur_pos(std::list<scii> &code)
{
    code_pos pos = code.end();
    --pos;
    return pos;
}

// 
// pBegin/pEnd - bounding pointers for the raw byte code
// wBaseOffset - byte offset in script file where pBegin is (used to calculate absolute code offsets)
// code        - (out) list of sci instructions.
//
// Returns the end.
const BYTE *_ConvertToInstructions(DecompileLookups &lookups, std::list<scii> &code, const BYTE *pBegin, const BYTE *pEnd, WORD wBaseOffset, bool abortOnError)
{
    std::unordered_map<WORD, code_pos> referenceToCodePos;
    std::vector<Fixup> branchTargetsToFixup;
    std::set<uint16_t> branchTargets;

    SCIVersion sciVersion = lookups.GetVersion();

    code_pos undetermined = code.end();

    uint16_t codeLength = (uint16_t)(pEnd - pBegin);
    uint16_t wReferencePosition = 0;
    const BYTE *pCur = pBegin;
    while (pCur < pEnd)
    {
        const BYTE *pThisInstruction = pCur;
        BYTE bRawOpcode = *pCur;
        bool bByte = (*pCur) & 1;
        Opcode bOpcode = RawToOpcode(sciVersion, bRawOpcode);
        assert(bOpcode <= Opcode::LastOne);
        ++pCur; // Advance past opcode.
        uint16_t wOperands[3];
        ZeroMemory(wOperands, sizeof(wOperands));
        int cIncr = 0;

        for (int i = 0; i < 3; i++)
        {
            OperandType opType = GetOperandTypes(sciVersion, bOpcode)[i];
            cIncr = GetOperandSize(bRawOpcode, opType, pCur);
            switch (cIncr)
            {
            case 1:
                // We may need to sign-extend this.
                if ((opType == otINT) || (opType == otINT8) || (opType == otLABEL))
                {
                    wOperands[i] = (uint16_t)(int16_t)(int8_t)*pCur;
                }
                else
                {
                    wOperands[i] = (uint16_t)*pCur;
                }
                break;
            case 2:
                wOperands[i] = *((uint16_t*)pCur); // REVIEW
                break;
            default:
                break;
            }
            pCur += cIncr;
            if (cIncr == 0) // No more operands
            {
                break;
            }
        }
        // Add the instruction - use the constructor that takes all arguments, even if
        // not all are valid.
        if ((bOpcode == Opcode::BNT) || (bOpcode == Opcode::BT) || (bOpcode == Opcode::JMP))
        {
            // +1 because its the operand start pos.
            uint16_t wTarget = CalcOffset(lookups.GetVersion(), wReferencePosition + 1, wOperands[0], bByte, bRawOpcode);

            if (wTarget > codeLength)
            {
                if (abortOnError)
                {
                    return nullptr;
                }
                else
                {
                    // This goes out of bounds. Some code is corrupt, like SmoothLooper in script 968 in Hero's Quest.
                    // We can't intelligently reason about where the branch is supposed to point, so just replace it with a load operation.
                    // We need to make sure it's the same size though.
                    code.push_back(scii(sciVersion, Opcode::LDI, 0xbaad, -1));
                    lookups.DecompileResults().AddResult(DecompilerResultType::Warning, fmt::format("Bad branch at 0x{0:4x}, replaced with -17747.", (wReferencePosition + wBaseOffset)));
                }
            }
            else
            {
                code.push_back(scii(sciVersion, bOpcode, undetermined, true, -1));
                bool fForward = (wTarget > wReferencePosition);
                Fixup fixup = { get_cur_pos(code), wTarget, fForward };
                branchTargetsToFixup.push_back(fixup);
                branchTargets.insert(wTarget);
            }
        }
        else
        {
            code.push_back(scii(sciVersion, bOpcode, wOperands[0], wOperands[1], wOperands[2], -1));
        }

        // Store the position of the instruction we just added:
        referenceToCodePos[wReferencePosition] = get_cur_pos(code);
        // Store the actual offset in the instruction itself:
        uint16_t wSize = (uint16_t)(pCur - pThisInstruction);
        get_cur_pos(code)->set_offset_and_size(wReferencePosition + wBaseOffset, wSize);

        // Attempt to detect the end of the function. If we encounter a return staetment and it's equal to or beyond anything in branchTargetsToFixup,
        // then we have reached the end.
        if (bOpcode == Opcode::RET)
        {
            if (branchTargets.empty() || ((*branchTargets.rbegin()) <= wReferencePosition))
            {
                // We've reached the end
                break;
            }
        }

        wReferencePosition += wSize;
    }

    // Now fixup any branches.
    for (size_t i = 0; i < branchTargetsToFixup.size(); i++)
    {
        Fixup &fixup = branchTargetsToFixup[i];
        std::unordered_map<WORD, code_pos>::iterator it = referenceToCodePos.find(fixup.wTarget);
        if (it != referenceToCodePos.end())
        {
            fixup.branchInstruction->set_branch_target(it->second, fixup.fForward);
        }
        else
        {
            lookups.DecompileResults().AddResult(DecompilerResultType::Error, "Invalid branch target.");
            return nullptr;
        }
    }

    // Special hack... function code is placed at even intervals.  That means there might be an extra bogus
    // instruction at the end, just after a ret statement (often it is Opcode::BNOT, which is 0).  If the instruction
    // before the end is a ret instruction, remove the last instruction (unless it's also a ret - since sometimes
    // functions will end with two RETs, both of which are jump targets).
    if (code.size() > 1)
    {
        code_pos theEnd = code.end();
        --theEnd; // Last instruction
        code_pos maybeRET = theEnd;
        --maybeRET;
        if ((maybeRET->get_opcode() == Opcode::RET) && (theEnd->get_opcode() != Opcode::RET))
        {
            code.erase(theEnd);
        }
    }

    return pCur;
}

bool _IsVariableUse(SCIVersion version, code_pos pos, VarScope varScope, WORD &wIndex)
{
    bool fRet = false;
    Opcode bOpcode = pos->get_opcode();
    if (GetOperandTypes(version, bOpcode)[0] == otVAR)
    {
        // It's a variable opcode.
        wIndex = pos->get_first_operand();
        fRet = (static_cast<VarScope>(static_cast<BYTE>(bOpcode) & 0x03) == varScope);
    }
    else if (bOpcode == Opcode::LEA)
    {
        // The "load effective address" instruction
        uint16_t wType = pos->get_first_operand();
        fRet = (static_cast<VarScope>(static_cast<BYTE>((wType >> 1))& 0x03) == varScope);
        if (fRet)
        {
            wIndex = pos->get_second_operand();
        }
    }
    return fRet;
}

void _FigureOutParameters(SCIVersion sciVersion, FunctionBase &function, FunctionSignature &signature, std::list<scii> &code)
{
    WORD wBiggest = 0;
    for (code_pos pos = code.begin(); pos != code.end(); ++pos)
    {
        WORD wIndex;
        if (_IsVariableUse(sciVersion, pos, VarScope::Param, wIndex))
        {
            wBiggest = max(wIndex, wBiggest);
        }
        else if (pos->get_opcode() == Opcode::REST)
        {
            // REST usage also indicates params.
            uint16_t restIndex = pos->get_first_operand();
            wBiggest = max(restIndex, wBiggest);
        }
    }
    if (wBiggest) // Parameters start at index 1, so 0 means no parameters
    {
        for (WORD i = 1; i <= wBiggest; i++)
        {
            std::unique_ptr<FunctionParameter> pParam = std::make_unique<FunctionParameter>();
            pParam->SetName(_GetParamVariableName(i));
            pParam->SetDataType("var"); // Generic for now... we could try to detect things in the future.
            signature.AddParam(std::move(pParam), false);   // false -> hard to detect if optional or not... serious code analysis req'd
        }
    }
}

//
// Scans the code for local variable usage, and adds "temp0, temp1, etc..." variables to function.
//
template<typename _TVarHolder>
void _FigureOutTempVariables(DecompileLookups &lookups, _TVarHolder &function, VarScope varScope, std::list<scii> &code)
{
    // Look for the link instruction.
    WORD cTotalVariableRoom = 0;
    for (code_pos pos = code.begin(); pos != code.end(); ++pos)
    {
        if (pos->get_opcode() == Opcode::LINK)
        {
            cTotalVariableRoom = pos->get_first_operand();
            break;
        }
    }

    if (cTotalVariableRoom)
    {
        // So, I don't know if I can put the things in initializers. That would involve extra work.
        // So I won't bother (I'll just leave the assignment ops)
        std::string methodTrackingName = GetMethodTrackingName(function.GetOwnerClass(), function);
        auto &localUsage = lookups.GetTempUsage(methodTrackingName);
        vector<VariableRange> varRanges;
        CalculateVariableRanges(localUsage, cTotalVariableRoom, varRanges);

        // Ok, now I have the ranges. Without doing extra work to find assignment opcodes that come at
        // the begining of the function, I'll just avoid putting any initializers in the variable decls.
        for (VariableRange &varRange : varRanges)
        {
            unique_ptr<VariableDecl> tempVar = std::make_unique<VariableDecl>();
            tempVar->SetDataType("var"); // For now...
            tempVar->SetName(_GetTempVariableName(varRange.index));
            tempVar->SetSize(varRange.arraySize);
            function.AddVariable(move(tempVar));
        }
    }
}

std::string _indent(int iIndent)
{
    std::string theFill;
    theFill.insert(theFill.begin(), iIndent, ' ');
    return theFill;
}

Consumption _GetInstructionConsumption(scii &inst, DecompileLookups *lookups)
{
    Opcode bOpcode = inst.get_opcode();
    assert(bOpcode <= Opcode::LastOne);
    int cEatStack = 0;
    bool fChangesAcc = false;
    bool fEatsAcc = false;
    bool fPutsOnStack = false;
    bool fEatsPrev = false;
    bool fChangesPrev = false;

    switch (bOpcode)
    {
    case Opcode::SELF:
        cEatStack = inst.get_first_operand() / 2;
        fChangesAcc = true;
        break;

    case Opcode::SEND:
        cEatStack = inst.get_first_operand() / 2;
        fChangesAcc = true;
        fEatsAcc = true;
        break;

    case Opcode::SUPER:
        cEatStack = inst.get_second_operand() / 2;
        fChangesAcc = true;
        break;

    case Opcode::BNOT:
    case Opcode::NOT:
    case Opcode::NEG:
        fChangesAcc = true;
        fEatsAcc = true;
        break;

    case Opcode::SUB:
    case Opcode::MUL:
    case Opcode::DIV:
    case Opcode::MOD:
    case Opcode::SHR:
    case Opcode::SHL:
    case Opcode::XOR:
    case Opcode::AND:
    case Opcode::OR:
    case Opcode::ADD:
        fChangesAcc = true;
        fEatsAcc = true;
        cEatStack = 1;
        break;

    case Opcode::EQ:
    case Opcode::GT:
    case Opcode::LT:
    case Opcode::LE:
    case Opcode::NE:
    case Opcode::GE:
    case Opcode::UGT:
    case Opcode::UGE:
    case Opcode::ULT:
    case Opcode::ULE:
        fChangesPrev = true;
        fChangesAcc = true;
        fEatsAcc = true;
        cEatStack = 1;
        break;

    case Opcode::BT:
    case Opcode::BNT:
        // These leave the accumulator intact. So we might want to put fChangesAcc here.
        // Some stuff decompiles incorrect (see MergePoly usage in SQ5)
        fEatsAcc = true;
        break;

    case Opcode::RET:
        if (lookups && lookups->FunctionDecompileHints.ReturnsValue)
        {
            fEatsAcc = true;
        }
        // Wreaks havoc
        // fEatsAcc = true; // But not always intentional...
        // Because we don't know the intent of the code, we can't really do anything here.
        // The acc could be valid, or garbage. So we can just do the "doesn't affect anything"
        // statement before the return here and call it good. SCISTudio and SICompanion both compile ok with that
        // (though SCICompanion gives warnings).
        // e.g.
        //	return (5)
        // will become
        //	5				// SCICompanion gives warning here
        //	return
        break;

    case Opcode::JMP:
    case Opcode::LINK:
        break;

    case Opcode::LDI:
        fChangesAcc = true;
        break;

    case Opcode::PUSH:
        fEatsAcc = true;
        fPutsOnStack = true;
        break;

    case Opcode::PUSHI:
    case Opcode::PUSH0:
    case Opcode::PUSH1:
    case Opcode::PUSH2:
        fPutsOnStack = true;
        break;

    case Opcode::PUSHSELF:
        fPutsOnStack = true;
        break;

    case Opcode::TOSS:
        //fChangesAcc = true; // doesn't touch the acc, by definition (toss)
        cEatStack = 1;
        break;

    case Opcode::DUP:
        fPutsOnStack = true;
        break;

    case Opcode::CALL:
    case Opcode::CALLK:
    case Opcode::CALLB:
        cEatStack = inst.get_second_operand() / 2;
        cEatStack++; // the number didn't include the # of instructions push.
        fChangesAcc = true;
        break;

    case Opcode::CALLE:
        //fEatsAcc = true;
        cEatStack = inst.get_third_operand() / 2;
        cEatStack++; // Also a parameter count.
        fChangesAcc = true;
        break;

    case Opcode::CLASS:
    case Opcode::SELFID:
        fChangesAcc = true;
        break;

    case Opcode::PPREV:
        fPutsOnStack = true;
        fEatsPrev = true;
        break;

    case Opcode::REST:
        // Doesn't really affect anything
        break;

    case Opcode::LEA:
        fEatsAcc = !!(static_cast<BYTE>(inst.get_first_operand() >> 1) & LEA_ACC_AS_INDEX_MOD);
        fChangesAcc = true;
        break;

    case Opcode::PTOA:
    case Opcode::IPTOA:
    case Opcode::DPTOA:
        fChangesAcc = true;
        break;

    case Opcode::ATOP:
        fEatsAcc = true;
        fChangesAcc = true; 
        // Not technically, but it leaves the value in the accumulator, so
        // it's what people should look at.
        break;

    case Opcode::PTOS:
    case Opcode::IPTOS:
    case Opcode::DPTOS:
        fPutsOnStack = true;
        break;

    case Opcode::STOP:
        cEatStack = 1;
        break;

    case Opcode::LOFSA:
        fChangesAcc = true;
        break;

    case Opcode::LOFSS:
        fPutsOnStack = true;
        break;

    case Opcode::LineNumber:
    case Opcode::Filename:
        // SCI2+ only.
        // File and line number info. Doesn't do anything.
        break;

    default:
        assert((bOpcode >= Opcode::FirstLoadStore) && (bOpcode <= Opcode::LastLoadStore));
        // TODO: use our defines/consts
        if (_IsVOStoreOperation(bOpcode))
        {
            // Store operation
            if (_IsVOPureStack(bOpcode))
            {
                cEatStack = 1;
            }
            else
            {
                if (_IsVOStack(bOpcode))
                {
                    cEatStack = 1;
                }
                fEatsAcc = true;
                fChangesAcc = true; // Not really, but leaves a valid thing in the acc.
            }
        }
        else
        {
            // Load operation
            if (_IsVOPureStack(bOpcode))
            {
                fPutsOnStack = true;
            }
            else
            {
                fChangesAcc = true;
            }
        }
        if (_IsVOIndexed(bOpcode))
        {
            fEatsAcc = true; // index is in acc
        }
        break;

    }

    Consumption cons;
    if (fEatsAcc)
    {
        cons.cAccConsume++;
    }
    if (fChangesAcc)
    {
        cons.cAccGenerate++;
    }
    cons.cStackConsume = cEatStack;
    if (fPutsOnStack)
    {
        cons.cStackGenerate++;
    }
    if (fEatsPrev)
    {
        cons.cPrevConsume++;
    }
    if (fChangesPrev)
    {
        cons.cPrevGenerate++;
    }

    return cons;
}

// Works backward from branchInstruction to beginning (inclusive) until the sequence of instructions leading up to the branch
// has been satisfied. If it fails, it returns false.
//
// This fails in situations where it shouldn't need to. For instance, in SQ5, script 801, offset 0x34a
// sat	temp[$4]                // This is the acc we needed
// pushi	$b4; 180, check     // this was for a compare operation after callk
// pushi	$4; cel             // first stack push for callk
// lst	temp[$5]
// lst	temp[$6]
// lst	temp[$3]                // Oh... no acc, it's another stack... don't need a stack, so I bail
// push                         // Here's a stack, but this needs acc
// callk	kernel_63, $8       // I need 5 stacks
//
// We'd need to implement some kind of instruction borrowing system to get this to work.
bool _ObtainInstructionSequence(code_pos endingInstruction, code_pos beginning, code_pos &beginningOfBranchInstructionSequence, bool includeDebugOpcodes)
{
    bool success = true;

    code_pos beforeBeginning = beginning;
    --beforeBeginning; // Since beginning is inclusive.
    code_pos cur = endingInstruction;
    beginningOfBranchInstructionSequence = cur;
    Consumption consTemp = _GetInstructionConsumption(*endingInstruction);
    int cStackConsume = consTemp.cStackConsume;
    int cAccConsume = consTemp.cAccConsume;
    --cur;
    while (success && (cur != beforeBeginning) && (cStackConsume || cAccConsume))
    {
        Consumption consTemp = _GetInstructionConsumption(*cur);
        if (consTemp.cAccGenerate)
        {
            if (cAccConsume)
            {
                cAccConsume -= consTemp.cAccGenerate;
            }
            // else... is acc generate ok even if we're not consuming?
        }
        if (consTemp.cStackGenerate)
        {
            if (cStackConsume)
            {
                cStackConsume -= consTemp.cStackGenerate;
            }
            else
            {
                // Something put something on the stack and we didn't need it.
                // We can't do anything useful here.
                success = false;
            }
        }

        if (consTemp.cAccConsume || consTemp.cStackConsume)
        {
            // This instruction we just got consumes stuff. We need to recurse here and
            // have it eat up its stuff.
            // Afterwards, if success is true, cur will point to the beginning of its thing
            // and we can keep going (if necessary)
            code_pos save = cur;
            success = _ObtainInstructionSequence(cur, beginning, cur, false);
            if (!success && !consTemp.cStackConsume)
            {
                // It's a "needs acc". Try to muddle through anyway... SQ5, localproc_0beb needs this.
                success = true;
                cur = save;
            }
        }

        beginningOfBranchInstructionSequence = cur;

        --cur;
    }
    
    if (includeDebugOpcodes)
    {
        // Add on any opcodes that don't do anything (debug opcodes) to complete our sequences.
        while (success && (cur != beforeBeginning))
        {
            if ((cur->get_opcode() != Opcode::Filename) && (cur->get_opcode() != Opcode::LineNumber))
            {
                break;
            }
            beginningOfBranchInstructionSequence = cur;
            --cur;
        }
    }

    return success;
}

ControlFlowNode *GetFirstSuccessorOrNull(ControlFlowNode *node)
{
    assert(node->Successors().size() <= 1);
    return node->Successors().empty() ? nullptr : *node->Successors().begin();
}

class DetermineHexValues : public IExploreNode
{
public:
    DetermineHexValues(FunctionBase &func)
    {
        useHex.push(true);
        func.Traverse(*this);
    }

    void ExploreNode(SyntaxNode &node, ExploreNodeState state) override
    {
        // Set property values to hex if we should:
        if (state == ExploreNodeState::Pre)
        {
            PropertyValue *propValue = SafeSyntaxNode<PropertyValue>(&node);
            if (propValue && useHex.top())
            {
                propValue->_fHex = true;
                propValue->_fNegate = false;
            }
        }

        // Push a "hex context" if we are in a bitwise operation.
        string operation;
        BinaryOperator binaryoperation = BinaryOperator::None;
        UnaryOperator unaryOperator = UnaryOperator::None;
        BinaryOp *binaryOp = SafeSyntaxNode<BinaryOp>(&node);
        if (binaryOp)
        {
            binaryoperation = binaryOp->Operator;
        }
        UnaryOp *unaryOp = SafeSyntaxNode<UnaryOp>(&node);
        if (unaryOp)
        {
            unaryOperator = unaryOp->Operator;
        }
        Assignment *assignment = SafeSyntaxNode<Assignment>(&node);
        if (assignment)
        {
            binaryoperation = GetBinaryOpFromAssignment(assignment->Operator);
        }

        if (binaryoperation == BinaryOperator::BinaryOr ||
            binaryoperation == BinaryOperator::BinaryAnd ||
            binaryoperation == BinaryOperator::ExclusiveOr ||
            unaryOperator == UnaryOperator::BinaryNot ||
            binaryoperation == BinaryOperator::ShiftRight ||
            binaryoperation == BinaryOperator::ShiftLeft)
        {
            if (state == ExploreNodeState::Pre)
            {
                useHex.push(true);
            }
            else if (state == ExploreNodeState::Post)
            {
                useHex.pop();
            }
        }
        else
        {
            // Ignore certain "insignificant" syntax nodes
            if (state == ExploreNodeState::Pre)
            {
                useHex.push(false);
            }
            else if (state == ExploreNodeState::Post)
            {
                useHex.pop();
            }
        }
    }

private:
    stack<bool> useHex;
};

class CollapseNots : public IExploreNode
{
public:
    CollapseNots(FunctionBase &func)
    {
        func.Traverse(*this);
    }

    void ExploreNode(SyntaxNode &node, ExploreNodeState state) override
    {
        if (state == ExploreNodeState::Pre)
        {
            ConditionalExpression *condExp = SafeSyntaxNode<ConditionalExpression>(&node);
            if (condExp)
            {
                _Process(condExp->GetStatements()[0]);
            }
        }
    }

private:
    unique_ptr<SyntaxNode> _PutInNot(unique_ptr<SyntaxNode> other)
    {
        unique_ptr<UnaryOp> unaryOp = make_unique<UnaryOp>();
        unaryOp->Operator = UnaryOperator::LogicalNot;
        unaryOp->SetStatement1(move(other));
        return unique_ptr<SyntaxNode>(move(unaryOp));
    }

    void _Process(unique_ptr<SyntaxNode> &statement)
    {
        // If this is a binary op of and or or, then procede onward with each one
        // See if we have a binary operation underneath us for an and/or
        BinaryOp *binOp = SafeSyntaxNode<BinaryOp>(statement.get());
        if (binOp && ((binOp->Operator == BinaryOperator::LogicalAnd) || (binOp->Operator == BinaryOperator::LogicalOr)))
        {
            _Process(binOp->GetStatement1Internal());
            _Process(binOp->GetStatement2Internal());
        }
        else
        {
            // If this is a not
            UnaryOp *unary = SafeSyntaxNode<UnaryOp>(statement.get());
            if (unary && (unary->Operator == UnaryOperator::LogicalNot))
            {
                // Then let's see if it contains a binary op, in which case, we'll apply DeMorgan's theorem.
                BinaryOp *child = SafeSyntaxNode<BinaryOp>(unary->GetStatement1());
                if (child && ((child->Operator == BinaryOperator::LogicalAnd) || (child->Operator == BinaryOperator::LogicalOr)))
                {
                    // Ok. We need to replace the unary op with its child.
                    unique_ptr<SyntaxNode> binaryOpStatement = move(unary->GetStatement1Internal());
                    statement = move(binaryOpStatement);
                    // That should do it.
                    // Now we need to switch the operator
                    if (child->Operator == BinaryOperator::LogicalAnd)
                    {
                        child->Operator = BinaryOperator::LogicalOr;
                    }
                    else
                    {
                        child->Operator = BinaryOperator::LogicalAnd;
                    }
                    
                    // Then we need to go insert unary nots in front of both statements of the binary operator.
                    unique_ptr<SyntaxNode> binOpStatement1 = move(child->GetStatement1Internal());
                    child->SetStatement1(_PutInNot(move(binOpStatement1)));
                    unique_ptr<SyntaxNode> binOpStatement2 = move(child->GetStatement2Internal());
                    child->SetStatement2(_PutInNot(move(binOpStatement2)));
                }
            }
        }
    }
};

class ResolveCallSiteParameters : public IExploreNode
{
public:
    ResolveCallSiteParameters(DecompileLookups &decompileLookups, FunctionBase &func) : _lookups(decompileLookups)
    {
        func.Traverse(*this);
    }

    void ExploreNode(SyntaxNode &node, ExploreNodeState state) override
    {
        if (state == ExploreNodeState::Pre)
        {
            SendParam *send = SafeSyntaxNode<SendParam>(&node);
            if (send)
            {
                _lookups.GetDecompilerConfig()->ResolveMethodCallParameterTypes(*send);
            }
            ProcedureCall *procCall = SafeSyntaxNode<ProcedureCall>(&node);
            if (procCall)
            {
                _lookups.GetDecompilerConfig()->ResolveProcedureCallParameterTypes(*procCall);
            }
        }
    }

private:
    DecompileLookups &_lookups;
};

class DetermineNegativeValues : public IExploreNode
{
public:
    DetermineNegativeValues(FunctionBase &func)
    {
        useNeg.push(true);
        func.Traverse(*this);
    }

    void ExploreNode(SyntaxNode &node, ExploreNodeState state) override
    {
        // Set property values to hex if we should:
        if (state == ExploreNodeState::Pre)
        {
            PropertyValue *propValue = SafeSyntaxNode<PropertyValue>(&node);
            if (propValue && useNeg.top())
            {
                if ((propValue->GetType() == ValueType::Number) && (propValue->GetNumberValue() > 32768))
                {
                    propValue->_fNegate = true;
                }
            }
        }

        // Push a "neg context" if we are in a signed comparison

        BinaryOperator binaryOperation = BinaryOperator::None;
        BinaryOp *binaryOp = SafeSyntaxNode<BinaryOp>(&node);
        if (binaryOp)
        {
            binaryOperation = binaryOp->Operator;
        }
        Assignment *assignment = SafeSyntaxNode<Assignment>(&node);
        if (assignment)
        {
            binaryOperation = GetBinaryOpFromAssignment(assignment->Operator);
        }

        if (binaryOperation == BinaryOperator::Add ||
            binaryOperation == BinaryOperator::Subtract ||
            binaryOperation == BinaryOperator::LessThan ||
            binaryOperation == BinaryOperator::GreaterThan ||
            binaryOperation == BinaryOperator::LessEqual ||
            binaryOperation == BinaryOperator::GreaterEqual)
        {
            if (state == ExploreNodeState::Pre)
            {
                useNeg.push(true);
            }
            else if (state == ExploreNodeState::Post)
            {
                useNeg.pop();
            }
        }
        else
        {
            if (state == ExploreNodeState::Pre)
            {
                useNeg.push(false);
            }
            else if (state == ExploreNodeState::Post)
            {
                useNeg.pop();
            }
        }
    }

private:
    stack<bool> useNeg;
};

void _DetermineIfFunctionReturnsValue(std::list<scii> code, DecompileLookups &lookups)
{
    // Look for return statements and see if they have any statements without side effects before them.
    code_pos cur = code.end();
    --cur;
    while (!lookups.FunctionDecompileHints.ReturnsValue && (cur != code.begin()))
    {
        if (cur->get_opcode() == Opcode::RET)
        {
            --cur;
            Opcode opcode = cur->get_opcode();
            switch (opcode)
            {
                // These instructions don't really have any effect other than
                // to put something in the accumulator. If they came right before a ret,
                // then it's safe to assume this function returns a value.
                // To be more complete, we could follow branches, as this is a common pattern that
                // won't be caught by us:
                // (if (blah)
                //     5
                // (else 4)
                // return

                case Opcode::LDI:
                case Opcode::SELFID:
                case Opcode::BNOT:
                case Opcode::NOT:
                case Opcode::NEG:
                case Opcode::SUB:
                case Opcode::MUL:
                case Opcode::DIV:
                case Opcode::MOD:
                case Opcode::SHR:
                case Opcode::SHL:
                case Opcode::XOR:
                case Opcode::AND:
                case Opcode::OR:
                case Opcode::ADD:
                case Opcode::EQ:
                case Opcode::GT:
                case Opcode::LT:
                case Opcode::LE:
                case Opcode::NE:
                case Opcode::GE:
                case Opcode::UGT:
                case Opcode::UGE:
                case Opcode::ULT:
                case Opcode::ULE:
                case Opcode::PTOA:
                case Opcode::LOFSA:
                    lookups.FunctionDecompileHints.ReturnsValue = true;
                    break;

                default:
                    if ((opcode >= Opcode::LAG) && (opcode <= Opcode::LastLoadStore))
                    {
                        if (!_IsVOStoreOperation(opcode) && !_IsVOPureStack(opcode))
                        {
                            lookups.FunctionDecompileHints.ReturnsValue = true;
                        }
                    }
                    // TODO: We could also check for zero parameter sends for known property selectors.
            }
        }
        else
        {
            --cur;
        }
    }
}

void _TrackExternalScriptUsage(std::list<scii> code, DecompileLookups &lookups)
{
    code_pos cur = code.end();
    --cur;
    while (cur != code.begin())
    {
        Opcode opcode = cur->get_opcode();
        switch (opcode)
        {
            case Opcode::CALLB:
                lookups.TrackUsingScript(0);
                break;

            case Opcode::CALLE:
                lookups.TrackUsingScript(cur->get_first_operand());
                break;

            case Opcode::CLASS:
            {
                uint16_t classIndex = cur->get_first_operand();
                uint16_t scriptNumber;
                if (lookups.GetSpeciesScriptNumber(classIndex, scriptNumber))
                {
                    lookups.TrackUsingScript(scriptNumber);
                }
                break;
            }

            case Opcode::LEA:
            {
                VarScope scope;
                _GetVariableNameFromCodePos(*cur, lookups, &scope);
                if (scope == VarScope::Global)
                {
                    lookups.TrackUsingScript(0);
                }
                break;
            }

            default:
            {
                if ((opcode >= Opcode::FirstLoadStore) && (opcode <= Opcode::LastLoadStore))
                {
                    VarScope scope;
                    _GetVariableNameFromCodePos(*cur, lookups, &scope);
                    if (scope == VarScope::Global)
                    {
                        lookups.TrackUsingScript(0);
                    }
                }
            }
                break;
        }

        --cur;
    }
}

// pEnd can be teh end of script data. I have added autodetection support.
void DecompileRaw(FunctionBase &func, DecompileLookups &lookups, const BYTE *pBegin, const BYTE *pEstimatedMaxEnd, const BYTE *pScriptResourceEnd, WORD wBaseOffset)
{
    bool allowContinues = func.GetOwnerScript()->GetScriptId().Language() == LangSyntaxSCI;

    lookups.EndowWithFunction(&func);

    // Take the raw data, and turn it into a list of scii instructions, and make sure the branch targets point to code_pos's
    std::list<scii> code;
    const BYTE *discoveredEnd = _ConvertToInstructions(lookups, code, pBegin, pScriptResourceEnd, wBaseOffset, true);
    if (discoveredEnd == nullptr)
    {
        // If there were problems with that (say bogus branches that go somewhere incorrect), try a tighter bound.
        // We don't want to try the tight bound right away, because it might have been determined using bogus
        // exports in the export table (e.g. SQ5 does this in script 243).
        code.clear();
        discoveredEnd = _ConvertToInstructions(lookups, code, pBegin, pEstimatedMaxEnd, wBaseOffset, false);
    }

    bool success = (discoveredEnd != nullptr);
    if (success)
    {
        success = false;
        // Insert a no-op at the beginning of code (so we can get an iterator to point to a spot before code)
        code.insert(code.begin(), scii(lookups.GetVersion(), Opcode::INDETERMINATE, -1));

        // Do some early things
        _DetermineIfFunctionReturnsValue(code, lookups);

        // Construct the function -> for now use procedure, but really should be method or proc
        unique_ptr<FunctionSignature> pSignature = std::make_unique<FunctionSignature>();
        _FigureOutParameters(lookups.GetVersion(), func, *pSignature, code);
        func.AddSignature(std::move(pSignature));

        _TrackExternalScriptUsage(code, lookups);

        if (!lookups.DecompileAsm)
        {
            string className = func.GetOwnerClass() ? func.GetOwnerClass()->GetName() : "";
            string messageDescription = fmt::format("{0} {1}::{2}: Analyzing control flow", func.GetOwnerScript()->GetName(), className, func.GetName());
            lookups.DecompileResults().AddResult(DecompilerResultType::Update, messageDescription);

            ControlFlowGraph cfg(messageDescription, lookups.DecompileResults(), GetMethodTrackingName(func.GetOwnerClass(), func, true), allowContinues, lookups.DebugControlFlow, lookups.pszDebugFilter);
            success = cfg.Generate(code.begin(), code.end());
            if (success && !lookups.DecompileResults().IsAborted())
            {
                const NodeSet &controlStructures = cfg.ControlStructures();
                MainNode *mainNode = cfg.GetMain();
                lookups.DecompileResults().AddResult(DecompilerResultType::Update, fmt::format("{0} {1}::{2}: Generating code", func.GetOwnerScript()->GetName(), className, func.GetName()));
                messageDescription = fmt::format("{0} {1}::{2}: Instruction consumption", func.GetOwnerScript()->GetName(), className, func.GetName());
                success = OutputNewStructure(messageDescription, func, *mainNode, lookups);
            }
        }
    }
    else
    {
        func.AddSignature(std::make_unique<FunctionSignature>());
        func.AddStatement(std::make_unique<sci::PropertyValue>("CorruptFunction_CantDetermineCodeBounds", ValueType::Token));
    }
    
    if (!success && !lookups.DecompileResults().IsAborted() && discoveredEnd)
    {
        // Disassemble the function instead.
        // First though, we need to remove all code:
        func.GetStatements().clear();
        lookups.ResetOnFailure();

        lookups.DecompileResults().AddResult(DecompilerResultType::Important, fmt::format("Falling back to disassembly for {0}", func.GetName()));
        DisassembleFallback(func, code.begin(), code.end(), lookups);
    }

    // Give some statistics.
    if (discoveredEnd)
    {
        lookups.DecompileResults().InformStats(success, discoveredEnd - pBegin);
    }

    if (!lookups.DecompileResults().IsAborted())
    {
        CollapseNots collapseNots(func);
        ResolveCallSiteParameters resolveCallSiteParameters(lookups, func);
        DetermineHexValues determineHexValues(func);
        DetermineNegativeValues determinedNegValues(func);

        _FigureOutTempVariables(lookups, func, VarScope::Temp, code);

        lookups.ResolveRestStatements();

        lookups.EndowWithFunction(nullptr);

        func.PruneExtraneousReturn();
    }
}

// Both for variable opcodes, and Opcode::LEA
std::string _GetVariableNameFromCodePos(const scii &inst, DecompileLookups &lookups, VarScope *pVarType, WORD *pwIndexOut)
{
    std::string name;
    BYTE bThing;
    WORD wIndex;
    if (inst.get_opcode() == Opcode::LEA)
    {
        bThing = (BYTE)inst.get_first_operand();
        bThing >>= 1;
        wIndex = inst.get_second_operand();
    }
    else
    {
        bThing = (BYTE)inst.get_opcode();
        wIndex = inst.get_first_operand();
        assert(bThing >= 64);
    }
    std::stringstream ss;
    switch (bThing & 0x3)
    {
    case VO_GLOBAL:
        {
            // global
            name = lookups.ReverseLookupGlobalVariableName(wIndex);
            if (name.empty())
            {
                ss << _GetGlobalVariableName(wIndex);
                name = ss.str();
            }
        }
        break;
    case VO_LOCAL:
        // local
        ss << _GetLocalVariableName(wIndex, lookups.GetScriptNumber());
        name = ss.str();
        break;
    case VO_TEMP:
        // temp
        ss << _GetTempVariableName(wIndex);
        name = ss.str();
        break;
    case VO_PARAM:
        // param
        name = lookups.LookupParameterName(wIndex);
        break;
    }

    if (pwIndexOut)
    {
        *pwIndexOut = wIndex;
    }
    if (pVarType)
    {
        *pVarType = static_cast<VarScope>(bThing & 0x3);
    }

    return name;
}

DecompileLookups::DecompileLookups(const IDecompilerConfig *config, const GameFolderHelper &helper, uint16_t wScript, GlobalCompiledScriptLookups *pLookups, IObjectFileScriptLookups *pOFLookups, ICompiledScriptSpecificLookups *pScriptThings, ILookupNames *pTextResource, IPrivateSpeciesLookups *pPrivateSpecies, IDecompilerResults &results) :
_wScript(wScript), _pLookups(pLookups), _pOFLookups(pOFLookups), _pScriptThings(pScriptThings), _pTextResource(pTextResource), _pPrivateSpecies(pPrivateSpecies), PreferLValue(false), _results(results), Helper(helper), DebugControlFlow(false), DebugInstructionConsumption(false), _config(config)
{
    _CategorizeSelectors();

    // Track all the valid script/export combos, so we know when someone is calling an invalid one.
    for (CompiledScript *script : _pLookups->GetGlobalClassTable().GetAllScripts())
    {
        _scriptExistance.insert(script->GetScriptNumber());
        uint32_t scriptNumber = script->GetScriptNumber();
        // In the high word we'll put the script number.
        uint32_t index = 0;
        for (uint16_t theExport : script->GetExports())
        {
            if (theExport != 0)
            {
                uint32_t scriptAndExport = (scriptNumber << 16) | index;
                _scriptExportExistance.insert(scriptAndExport);
            }
            index++;
        }
    }
}

std::string DecompileLookups::LookupSelectorName(WORD wIndex)
{
    return _pLookups->LookupSelectorName(wIndex);
}
std::string DecompileLookups::LookupKernelName(WORD wIndex)
{
    return _pLookups->LookupKernelName(wIndex);
}
std::string DecompileLookups::LookupClassName(WORD wIndex)
{
    std::string ret = _pLookups->LookupClassName(wIndex);
    if (ret.empty())
    {
        ret = _pPrivateSpecies->LookupClassName(wIndex);
    }
    return ret;
}
bool DecompileLookups::LookupSpeciesPropertyList(WORD wIndex, std::vector<WORD> &props)
{
    bool fRet = _pLookups->LookupSpeciesPropertyList(wIndex, props);
    if (!fRet)
    {
        fRet = _pPrivateSpecies->LookupSpeciesPropertyList(wIndex, props);
    }
    return fRet;
}
bool DecompileLookups::LookupSpeciesPropertyListAndValues(WORD wIndex, std::vector<WORD> &props, std::vector<CompiledVarValue> &values)
{
    bool fRet = _pLookups->LookupSpeciesPropertyListAndValues(wIndex, props, values);
    if (!fRet)
    {
        fRet = _pPrivateSpecies->LookupSpeciesPropertyListAndValues(wIndex, props, values);
    }
    return fRet;
}
std::string DecompileLookups::ReverseLookupGlobalVariableName(WORD wIndex)
{
    static std::string s_defaults[] = {
        "gEgo",
        "gGame",
        "gRoom",
        "gSpeed",
        "gQuitGame",
        "gCast",
        "gRegions",
        "gLocales",
        "gTimers",
        "gSounds",  // gInv, SQ
        "gInv",     // gAddToPics...
        "gAddToPics",   // something passed to OwnedBy
        "gFeatures",
        "gSFeatures",
        "gRoomNumberExit",
        "gPreviousRoomNumber",
        "gRoomNumber",
        "gDebugOnExit",
        "gScore",
        "gMaxScore",
    };


    std::string result = _pOFLookups->ReverseLookupGlobalVariableName(wIndex);
    if (result.empty())
    {
        // Disable this and come up with a better solution that does this automatically
        /*
        // Supply some defaults.  These may be different for different games.
        if (wIndex < ARRAYSIZE(s_defaults))
        {
            result = s_defaults[wIndex];
        }*/
    }
    return result;
}
std::string DecompileLookups::ReverseLookupPublicExportName(WORD wScript, WORD wIndex)
{
    std::string ret = _pOFLookups->ReverseLookupPublicExportName(wScript, wIndex);
    if (ret.empty())
    {
        // This will be true if there are no .sco files.
        ret = _GetPublicProcedureName(wScript, wIndex);
    }
    return ret;
}
std::string DecompileLookups::LookupPropertyName(WORD wPropertyIndex)
{
    if (_pPropertyNames)
    {
        _requestedProperty = true;
        return _pPropertyNames->LookupPropertyName(this, wPropertyIndex);
    }
    else
    {
        return "PROPERTY-ACCESS-IN-NON-METHOD";
    }
}
bool DecompileLookups::LookupPropertyName(uint16_t wPropertyIndex, std::string &name)
{
    if (_pPropertyNames)
    {
        _requestedProperty = true;
        name = _pPropertyNames->LookupPropertyName(this, wPropertyIndex);
        return true;
    }
    else
    {
        return false;
    }
}
bool DecompileLookups::LookupScriptThing(WORD wName, ICompiledScriptSpecificLookups::ObjectType &type, std::string &name) const
{
    return _pScriptThings->LookupObjectName(wName, type, name);
}

bool DecompileLookups::GetSpeciesScriptNumber(uint16_t species, uint16_t &scriptNumber)
{
    return _pLookups->GetGlobalClassTable().GetSpeciesScriptNumber(species, scriptNumber);
}

std::string DecompileLookups::LookupParameterName(WORD wIndex)
{
    if (wIndex)
    {
        const FunctionSignature &signature = *_pFunc->GetSignatures()[0];
        size_t iRealIndex = (wIndex - 1);
        ASSERT(iRealIndex < signature.GetParams().size()); // Since it was us who analyzed the code and added the right # of params
        return signature.GetParams()[iRealIndex]->GetName();
    }
    else
    {
        return "paramTotal"; // parameter 0 is the count of params
    }
}

std::string DecompileLookups::LookupTextResource(WORD wIndex) const
{
    std::string ret;
    if (_pTextResource)
    {
        ret = _pTextResource->Lookup(wIndex);
    }
    return ret;
}

void DecompileLookups::_CategorizeSelectors()
{
    for (auto &script : _pLookups->GetGlobalClassTable().GetAllScripts())
    {
        for (auto &object : script->GetObjects())
        {
            for (uint16_t propSelector : object->GetProperties())
            {
                _propertySelectors.insert(propSelector);
            }
            for (uint16_t methodSelector : object->GetMethods())
            {
                _methodSelectors.insert(methodSelector);
            }
        }
    }
}

bool DecompileLookups::IsPropertySelectorOnly(uint16_t selector) const
{
    return (_propertySelectors.find(selector) != _propertySelectors.end()) &&
        (_methodSelectors.find(selector) == _methodSelectors.end());
}

const SelectorTable& DecompileLookups::GetSelectorTable() const
{
    return _pLookups->GetSelectorTable();
}

bool DecompileLookups::DoesExportExist(uint16_t script, uint16_t theExport) const
{
    uint32_t scriptAndExport = (((uint32_t)script) << 16) | theExport;
    auto it = _scriptExportExistance.find(scriptAndExport);
    return (it != _scriptExportExistance.end());
}

uint16_t DecompileLookups::GetNameSelector() const
{
    uint16_t nameSelector = 0;
    _pLookups->GetSelectorTable().ReverseLookup("name", nameSelector);
    return nameSelector;
}

void DecompileLookups::SetPosition(sci::SyntaxNode *pNode)
{
    pNode->SetPosition(_fakePosition);
    _fakePosition = LineCol(_fakePosition.Line() + 1, 0);
}

std::set<uint16_t> DecompileLookups::GetValidUsings()
{
    // Only return usings for scripts that actually exist.
    std::set<uint16_t> results;
    for (uint16_t script : _usings)
    {
        if (_scriptExistance.find(script) != _scriptExistance.end())
        {
            results.insert(script);
        }
    }
    return results;
}

const ILookupPropertyName *DecompileLookups::GetPossiblePropertiesForProc(uint16_t localProcOffset)
{
    auto it = _localProcToPropLookups.find(localProcOffset);
    if (it != _localProcToPropLookups.end())
    {
        return it->second;
    }
    return nullptr;
}

const SCIVersion &DecompileLookups::GetVersion()
{
    return Helper.Version;
}

void DecompileLookups::EndowWithFunction(sci::FunctionBase *pFunc)
{
    _pFunc = pFunc;
    FunctionDecompileHints.Reset();
    if (_pFunc)
    {
        _functionTrackingName = GetMethodTrackingName(_pFunc->GetOwnerClass(), *_pFunc);
    }
    else
    {
        _functionTrackingName.clear();
        _restStatementTrack.clear();
    }
}

const ClassDefinition *DecompileLookups::GetClassContext() const
{
    const MethodDefinition *method = SafeSyntaxNode<MethodDefinition>(_pFunc);
    if (method)
    {
        return method->GetOwnerClass();
    }
    return nullptr;
}

void DecompileLookups::TrackVariableUsage(VarScope varScope, WORD wIndex, bool isIndexed)
{
    map<WORD, bool> *pFunctionVarUsage = nullptr;
    if ((varScope == VarScope::Local) || ((GetScriptNumber() == 0) && (varScope == VarScope::Global)))
    {
        pFunctionVarUsage = &_localVarUsage;
    }
    else if (varScope == VarScope::Temp)
    {
        auto &it = _tempVarUsage.find(_functionTrackingName);
        if (it != _tempVarUsage.end())
        {
            pFunctionVarUsage = &(*it).second;
        }
        else
        {
            _tempVarUsage[_functionTrackingName] = map<WORD, bool>();
            pFunctionVarUsage = &_tempVarUsage[_functionTrackingName];
        }
    }

    if (pFunctionVarUsage)
    {
        // We can't just blindly set isIndexed. If a variable is used in both manners, we'll treat
        // it as indexed. So check if something is already there.
        auto &findIt = pFunctionVarUsage->find(wIndex);
        if (findIt != pFunctionVarUsage->end())
        {
            (*pFunctionVarUsage)[wIndex] = isIndexed || findIt->second;
        }
        else
        {
            (*pFunctionVarUsage)[wIndex] = isIndexed;
        }
    }
}

// REVIEW: Stuff that is tracked while decompiling should really be put elsewhere
// (i.e. not in DEcompileLookups, which should be read-only)
void DecompileLookups::ResetOnFailure()
{
    // This holds weak pointers to objects in the script, so clear those out.
    _restStatementTrack.clear();
}

void DecompileLookups::TrackRestStatement(sci::RestStatement *rest, uint16_t index)
{
    _restStatementTrack.emplace_back(rest, index);
}

void DecompileLookups::ResolveRestStatements()
{
    for (auto &pair : _restStatementTrack)
    {
        uint16_t varIndex = pair.second;
        FunctionSignature &signature = *_pFunc->GetSignaturesNC()[0];
        size_t iRealIndex = (varIndex - 1);
        if (iRealIndex < signature.GetParams().size())
        {
            // There is already a named parameter for this guy. Use this name for the rest statement.
            pair.first->SetName(signature.GetParams()[iRealIndex]->GetName());
        }
        else
        {
            // The more common case. Use "params"
            pair.first->SetName(RestParamName);
            // Now, we need this in the function signature too. 
            // We shouldn't ever need to add more than one parameter.
            // REVIEW: This probably shouldn't be an assert, since it could theoretically happen.
            // But I'd like to leave it in for now until I know the code is correct
            assert(iRealIndex == signature.GetParams().size());
            signature.AddParam(RestParamName);
        }
    }
}

void DecompileLookups::TrackProcedureCall(uint16_t offset)
{
    auto it = _localProcToPropLookups.find(offset);
    if (it == _localProcToPropLookups.end())
    {
        _localProcToPropLookups[offset] = _pPropertyNames;
    }
    else
    {
        // This procedure is called from multiple contexts (e.g. multiple
        // objects or other procedures) so we can't say it belongs to one object.
        if (it->second != _pPropertyNames)
        {
            _localProcToPropLookups[offset] = nullptr;
        }
    }
}

void CalculateVariableRanges(const std::map<WORD, bool> &usage, WORD variableCount, vector<VariableRange> &varRanges)
{
    // (1) This first part of the code attempts to figure out which variables are arrays and which are not.
    // For the global script, we assume none of the variables are arrays. We can't determine their usage from just
    // main itself, because all scripts use them.
    VariableRange currentVarRange = { 0, 0 };
    bool hasVariableInProcess = false;
    bool isCurrentIndexed = false;
    auto &usageIterator = usage.cbegin();
    auto &endUsage = usage.cend();
    for (WORD i = 0; i < variableCount; i++)
    {
        if ((usageIterator != endUsage) && (usageIterator->first == i))
        {
            // We start a new variable here
            if (hasVariableInProcess)
            {
                // If we have something in process, add it now
                currentVarRange.arraySize = (i - currentVarRange.index);
                ASSERT(isCurrentIndexed || (currentVarRange.arraySize == 1));
                varRanges.push_back(currentVarRange);
            }

            // Now start the new one
            currentVarRange.index = i;
            isCurrentIndexed = usageIterator->second;
            hasVariableInProcess = true;

            ++usageIterator;
        }
        else
        {
            // This var index when never directly used.
            if (hasVariableInProcess)
            {
                // This is a new one, unless we're indexed
                if (!isCurrentIndexed)
                {
                    currentVarRange.arraySize = 1;
                    varRanges.push_back(currentVarRange);

                    // But start a new one. isCurrentIndexed stays false.
                    // Actually, let it be indexed. Otherwise unused arrays take
                    // up tons of noise.
                    isCurrentIndexed = true;
                    currentVarRange.index = i;
                    hasVariableInProcess = true;
                }
                // else if indexed, just continue
            }
            else
            {
                // Just make a single one
                // Actually, let's make an indexed one.
                isCurrentIndexed = true;
                currentVarRange.index = i;
                hasVariableInProcess = true;
                //varRanges.push_back(VariableRange() = { i, 1 });
            }
        }
    }

    // Need one at the end too
    if (hasVariableInProcess)
    {
        currentVarRange.arraySize = variableCount - currentVarRange.index;
        varRanges.push_back(currentVarRange);
    }
}

void AddLocalVariablesToScript(sci::Script &script, const CompiledScript &compiledScript, DecompileLookups &lookups, const std::vector<CompiledVarValue> &localVarValues)
{
    // Based on what we find in lookups, we should be able to deduce what is an array and what is not.
    // And we should be able to initialize things too. Default values are zero.
    vector<VariableRange> varRanges;
    if (script.GetScriptNumber() == 0)
    {
        for (size_t i = 0; i < localVarValues.size(); i++)
        {
            varRanges.push_back({ (uint16_t)i, 1 });
        }

    }
    else
    {
        CalculateVariableRanges(lookups.GetLocalUsage(), static_cast<WORD>(localVarValues.size()), varRanges);
    }

    // The next step is to supply values and initializers
    // Script local variables are zero-initialized by default. So we don't need to assign anything to them if the value is zero.
    for (VariableRange &varRange : varRanges)
    {
        unique_ptr<VariableDecl> localVar = std::make_unique<VariableDecl>();
        localVar->SetDataType("var"); // For now...
        localVar->SetName(_GetLocalVariableName(varRange.index, script.GetScriptNumber()));
        localVar->SetSize(varRange.arraySize);

        int wStart = varRange.index;
        int wEnd = varRange.index + varRange.arraySize;
        // Now we need to determine if we are setting any initializers.
        // We need to provide initializers all the way up to the last non-zero value.
        WORD firstAllZeroesFromHereIndex = wStart;
        for (int i = wEnd - 1; i >= wStart; i--)
        {
            if (localVarValues[i].value != 0)
            {
                firstAllZeroesFromHereIndex = static_cast<WORD>(i + 1);
                break;
            }
        }

        // We need to supply values from varRange.index to firstAllZeroesFromHereIndex
        for (WORD w = varRange.index; w < firstAllZeroesFromHereIndex; w++)
        {
            PropertyValue value;
            uint16_t propValue = localVarValues[w].value;
            if (localVarValues[w].isObjectOrString)
            {
                ValueType typeStringOrSaid;
                std::string theString = compiledScript.GetStringOrSaidFromOffset(propValue, typeStringOrSaid);
                value.SetValue(theString, typeStringOrSaid);
            }
            else
            {
                value.SetValue(propValue);
                if (propValue >= 32768)
                {
                    // Probably intended to be negative.
                    value.Negate();
                }
            }
            localVar->AddSimpleInitializer(value);
        }

        script.AddVariable(move(localVar));
    }
}
