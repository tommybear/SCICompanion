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
// Compile.cpp : implementation file
// This file contains the code generation.
//

#include "stdafx.h"
#include "ScriptOMAll.h"
#include "CompileInterfaces.h"
#include "CompileContext.h"
#include "PMachine.h"
#include "Operators.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
// Potential optimizations
// 1) Switch statements where the cases are sequential.  We can save 2 bytes per case if we
//      use a temporary variable as an incrementer.
// 2) having "not" followed by "bnt" is a common pattern (e.g. 17 hits in Controls.sc).
//      We could do this optimization (to bt) after
//      the main compile.  We would have to ensure that no one branches to the bnt instruction.
//      When we do out branch tracking, we could mark each instruction as being the target of a branch or not.
//      So it would be a simple case of scanning the code afterwards, and coaslescening the not/bnt if there
//      was no branch to the bnt.  Actually, we'd have to do this scan before we finalize the size of the code,
//      since removing instructions would change that.  So, we could do a pass where we mark branch targets.
//      Then another pass where we coalesce.
// 3) Using Opcode::DUP when you have two of the same number in a row.  e.g.  pushi 5, dup.  Saves 1 byte.

#define WORD_OP(x) ((x)<<1)
#define BYTE_OP(x) (((x)<<1) | 1)
#define _OP(x) ((x)<<1)

using namespace sci;
using namespace std;

#ifdef DEBUG
#define DEBUG_IF_STATEMENT      1
#define DEBUG_LOCAL_PROC_CALL   2
#define DEBUG_CONDITIONAL_FAIL  3
#define DEBUG_CONDITIONAL_SUCCESS  4
#define DEBUG_FOR_LOOP_TEST     5
#define DEBUG_FOR_LOOP_LOOP     6
#define DEBUG_WHILE_LOOP_TEST   7
#define DEBUG_WHILE_LOOP_LOOP   8
#define DEBUG_DO_LOOP           9
#define DEBUG_IF_END            10
#define DEBUG_SWITCH_NEXT       11
#define DEBUG_SWITCH_END        12
#define DEBUG_BREAK             13
#define DEBUG_CONTINUE          14
#define DEBUG_BRANCH(code, info) (code).set_debug_info(info)
#else
#define DEBUG_BRANCH(code, info)
#endif

static const WORD NumberOfSendPushesSentinel = 0x5390; // 'send'

void ErrorHelper(CompileContext &context, const ISourceCodePosition *pPos, const string &text, const string &identifier, bool checkUse)
{
    string strError = text;
    strError += " '";
    strError += identifier;
    strError += "' ";
    // Do a little more error checking... suggest a file to "use"
    string strUse = checkUse ? context.ScanForIdentifiersScriptName(identifier) : "";
    if (strUse.empty())
    {
        strError += ".";
    }
    else
    {
        strError += ". Did you forget to use \"";
        strError += strUse;
        strError += "\"?";
    }
    context.ReportError(pPos, strError.c_str(), identifier.c_str());
}

void ReportKeywordError(CompileContext &context, const ISourceCodePosition *pPos, const string &text, const string &use)
{
    string strError = "'";
    strError += text;
    strError += "' is a reserved keyword and cannot be used as a ";
    strError += use;
    strError += ".";
    context.ReportError(pPos, strError.c_str());
}

std::vector<std::pair<BinaryOperator, Opcode>> c_rgOperatorToOpcode =
{
    { BinaryOperator::Equal, Opcode::EQ },
    { BinaryOperator::NotEqual, Opcode::NE },
    { BinaryOperator::UnsignedGreaterEqual, Opcode::UGE },
    { BinaryOperator::GreaterEqual, Opcode::GE },
    { BinaryOperator::UnsignedGreaterThan, Opcode::UGT },
    { BinaryOperator::ShiftRight, Opcode::SHR },
    { BinaryOperator::GreaterThan, Opcode::GT },
    { BinaryOperator::UnsignedLessEqual, Opcode::ULE },
    { BinaryOperator::LessEqual, Opcode::LE },
    { BinaryOperator::UnsignedLessThan, Opcode::ULT },
    { BinaryOperator::ShiftLeft, Opcode::SHL },
    { BinaryOperator::LessThan, Opcode::LT },
    { BinaryOperator::Add, Opcode::ADD },
    { BinaryOperator::Subtract, Opcode::SUB },
    { BinaryOperator::Multiply, Opcode::MUL },
    { BinaryOperator::Divide, Opcode::DIV },
    { BinaryOperator::Mod, Opcode::MOD },
    { BinaryOperator::BinaryAnd, Opcode::AND },
    { BinaryOperator::BinaryOr, Opcode::OR },
    { BinaryOperator::ExclusiveOr, Opcode::XOR },
};
Opcode GetInstructionForBinaryOperator(BinaryOperator op)
{
    auto findIt = std::find_if(c_rgOperatorToOpcode.begin(), c_rgOperatorToOpcode.end(),
        [op](std::pair<BinaryOperator, Opcode> &entry) { return entry.first == op; }
        );
    if (findIt != c_rgOperatorToOpcode.end())
    {
        return findIt->second;
    }
    assert(false);
    return Opcode::INDETERMINATE;
}
BinaryOperator GetBinaryOperatorForInstruction(Opcode opcode)
{
    auto findIt = std::find_if(c_rgOperatorToOpcode.begin(), c_rgOperatorToOpcode.end(),
        [opcode](std::pair<BinaryOperator, Opcode> &entry) { return entry.second == opcode; }
    );
    if (findIt != c_rgOperatorToOpcode.end())
    {
        return findIt->first;
    }
    return BinaryOperator::None;
}

void WriteSimple(CompileContext &context, Opcode bOpcode, int lineNumber)
{
    context.code().inst(lineNumber, bOpcode);
}

//
// The value is in the accumulator. If our output context is the stack, then
// insert a push instruction.
//
WORD PushToStackIfAppropriate(CompileContext &context, int lineNumber)
{
    WORD wBytes = 0;
    if (context.GetOutputContext() == OC_Stack)
    {
        WriteSimple(context, Opcode::PUSH, lineNumber);
        wBytes = 2;
    }
    return wBytes;
}

class PerformPreScan : public std::binary_function<IOutputByteCode*, CompileContext, void>
{
public:
    PerformPreScan(CompileContext &context) : _context(context) {}
    void operator()(IOutputByteCode* proc) const
    {
        proc->PreScan(_context);
    }
private:
    CompileContext &_context;
};

template<typename T>
void ForwardPreScan(const T &container, CompileContext &context)
{
    for_each(container.begin(), container.end(), PerformPreScan(context));
}

template<typename T>
class PerformPreScan2
{
public:
    PerformPreScan2(CompileContext &context) : _context(context) {}
    void operator()(const unique_ptr<T> &proc) const
    {
        proc->PreScan(_context);
    }
private:
    CompileContext &_context;
};

template<typename T>
void ForwardPreScan2(const vector<unique_ptr<T>> &container, CompileContext &context)
{
    for_each(container.begin(), container.end(), PerformPreScan2<T>(context));
}

//
// Helper class to ensure we leave a branch block that we enter.
//
class branch_block_base
{
public:
	branch_block_base(CompileContext &context, BranchBlockIndex index = BranchBlockIndex::Default) : _code(context.code())
    {
        _fActive = false;
        _index = index;
    }
    ~branch_block_base()
    {
        leave();
    }
    void enter()
    {
        assert(!_fActive);
        _code.enter_branch_block(_index);
        _fActive = true;
   }
    void leave()
    {
        if (_fActive)
        {
            _code.leave_branch_block(_index);
            _fActive = false;
        }
    }
    bool active() { return _fActive; }
	BranchBlockIndex index() { return _index; }
private:
    scicode &_code;
    bool _fActive;
	BranchBlockIndex _index;
};

//
// Helper class to ensure we leave a branch block that we enter.
// This automatically enters the block on construction.
//
class branch_block : public branch_block_base
{
public:
	branch_block(CompileContext &context, BranchBlockIndex index = BranchBlockIndex::Default) : branch_block_base(context, index)
    {
        enter();
    }
};

// Helper class to ensure we leave a continue frame.
// A continue frame with an undetermine code_pos means we should have a branch_block for it too.
// This is confusing, so let's list out the loops:
//  - while loop: continue jumps to start of condition. Always jumping back.
//  - for loop: continue jumps to the post-loop code. Always jumping forward to an unknown spot (needs branch_block)
//  - do loop: continue jumps to the start of the code. Always jumping back, but to an unknown spot. Don't currently
//             have the infrastructure to handle this, but that's ok, because there are no syntaxes where both continue
//             and do-loops are supported.
class continue_frame_guard
{
public:
    // For when we already know the continue target.
    continue_frame_guard(CompileContext &context, code_pos knownContinuePos) : _code(context.code()), _active(true)
    {
        _code.enter_continue_frame(knownContinuePos);
    }
    // For when the continue target will be written later.
    continue_frame_guard(CompileContext &context) : _code(context.code()), _active(true)
    {
        _code.enter_continue_frame(_code.get_undetermined());
        _branchBlock = std::make_unique<branch_block>(context, BranchBlockIndex::Continue);
    }
    ~continue_frame_guard() { leave(); }
    void leave()
    {
        if (_active)
        {
            _code.leave_continue_frame();
            if (_branchBlock)
            {
                _branchBlock->leave();
            }
            _active = false;
        }
    }
private:
    scicode &_code;
    bool _active;
    // Used in case when we don't know the target.
    std::unique_ptr<branch_block> _branchBlock;
};


//
// RAII for variable lookups
// (e.g. a function tells the compile context about its variables, so code inside can reference them)
//
class variable_lookup_context
{
public:
    variable_lookup_context(CompileContext &context, const IVariableLookupContext *pVarContext) : _context(context)
    {
        _context.PushVariableLookupContext(pVarContext);
    }
    ~variable_lookup_context()
    {
        _context.PopVariableLookupContext();
    }
private:
    CompileContext &_context;
};


class code_insertion_point
{
public:
    code_insertion_point(CompileContext &context, code_pos insertion_point) : _context(context)
    {
        context.code().push_code_insertion_point(insertion_point);
    }
    ~code_insertion_point()
    {
        _context.code().pop_code_insertion_point();
    }
private:
    CompileContext &_context;
};

//
// It is possible to revoke or give meaning, and also to not affect meaning at all (fDoIt == false)
// "meaning" indicates whether or not the code has any effect.
//
class change_meaning
{
public:
    change_meaning(CompileContext &context, bool fMeaning, bool fDoIt = true) : _context(context), _fDidIt(fDoIt)
    {
        if (_fDidIt)
        {
            _context.PushMeaning(fMeaning);
        }
    }
    ~change_meaning()
    {
        if (_fDidIt)
        {
            _context.PopMeaning();
        }
    }
private:
    CompileContext &_context;
    bool _fDidIt;
};

class declare_conditional
{
public:
    declare_conditional(CompileContext &context, bool fCondition) : _context(context)
    {
        _context.PushConditional(fCondition);
    }
    ~declare_conditional()
    {
        _context.PopConditional();
    }
private:
    CompileContext &_context;
};

class declare_compileQuery
{
public:
    declare_compileQuery(CompileContext &context) : _context(context)
    {
        _context.PushQuery(&query);
    }
    ~declare_compileQuery()
    {
        _context.PopQuery();
    }

    CompileQuery query;

private:
    CompileContext &_context;
};

//
// RAII for telling the impending value that it is being modified (incremented or decremented)
//
class variable_opcode_modifier
{
public:
    variable_opcode_modifier(const SyntaxNode *pNode, CompileContext &context, VariableModifier modifier) : _context(context)
    {
        if (_context.GetVariableModifier() != VM_None)
        {
            // We don't know when a variable modifier has been "used up", and the context doesn't
            // support a context of variable modifiers, so this code is not possible:
            // ++myArray[++x];
            // TODO: support this.  For now, we'll generate an error.
            _context.ReportError(pNode, "Multiple decrement (--) or increment (++) operators are not allowed.");
        }
        _context.SetVariableModifier(modifier);
    }
    ~variable_opcode_modifier()
    {
        _context.SetVariableModifier(VM_None);
    }
private:
    CompileContext &_context;
};

//
// RAII for indicating whether values go on the stack or accumulator
//
class COutputContext
{
public:
    COutputContext(CompileContext &context, OutputContext outputContext) : _context(context)
    {
        _context.PushOutputContext(outputContext);
    }
    ~COutputContext()
    {
        _context.PopOutputContext();
    }
private:
    CompileContext &_context;
};

//
// RAII for indicating the species of an object of a send call.
//
class SendTargetSpeciesIndex
{
public:
    SendTargetSpeciesIndex(CompileContext &context, SpeciesIndex wSpecies, const std::string &typeName) : _context(context)
    {
        _context.PushSendCallType(wSpecies, typeName);
    }
    ~SendTargetSpeciesIndex()
    {
        _context.PopSendCallType();
    }
private:
    CompileContext &_context;
};

// Helper
void OutputByteCodeToAccumulator(CompileContext &context, const SyntaxNode &statement)
{
    COutputContext accContext(context, OC_Accumulator);
    statement.OutputByteCode(context);
}

//
// RAII for telling the compile context that we're in a class.
//
class ClassContext
{
public:
    ClassContext(CompileContext &context, const ClassDefinition *pClass) : _context(context)
    {
        if (pClass)
        {
            _context.SetClassContext(pClass->GetName(), pClass->GetSuperClass(), pClass->IsInstance());
            _context.SetClassPropertyLookupContext(pClass);
        }
    }
    ~ClassContext()
    {
        _context.SetClassContext("", "", false);
        _context.SetClassPropertyLookupContext(nullptr);
    }
private:
    CompileContext &_context;
};

//
// Use the most efficient instruction.
//
void PushImmediate(CompileContext &context, WORD wValue, int lineNumber)
{
    switch (wValue)
    {
    case 0:
        context.code().inst(lineNumber, Opcode::PUSH0);
        break;
    case 1:
        context.code().inst(lineNumber, Opcode::PUSH1);
        break;
    case 2:
        context.code().inst(lineNumber, Opcode::PUSH2);
        break;
    default:
        context.code().inst(lineNumber, Opcode::PUSHI, wValue);
        break;
    }
}

void WriteImmediate(CompileContext &context, WORD wValue, int lineNumber)
{
    if (context.GetOutputContext() == OC_Stack)
    {
        PushImmediate(context, wValue, lineNumber);
    }
    else
    {
        // acc
        context.code().inst(lineNumber, Opcode::LDI, wValue);
    }
}

//
// Like the above, but *replace* the instruction at code_pos with the push.
//
void PushImmediateAt(CompileContext &context, WORD wValue, code_pos pos, int lineNumber)
{
    switch (wValue)
    {
    case 0:
        (*pos) = scii(context.GetVersion(), Opcode::PUSH0, lineNumber);
        break;
    case 1:
        (*pos) = scii(context.GetVersion(), Opcode::PUSH1, lineNumber);
        break;
    case 2:
        (*pos) = scii(context.GetVersion(), Opcode::PUSH2, lineNumber);
        break;
    default:
        (*pos) = scii(context.GetVersion(), Opcode::PUSHI, wValue, lineNumber);
        break;
    }
}

//
// We can do optimizations if the indexer is actually just an immediate value
//
bool CanDoIndexOptimization(const SyntaxNode *pIndexer, WORD &wIndex)
{
    bool fIndexerOptimize = false;
    if (pIndexer->GetNodeType() == ComplexPropertyValue::MyNodeType)
    {
        const ComplexPropertyValue *pValue = static_cast<const ComplexPropertyValue*>(pIndexer);
        if (pValue->GetType() == ValueType::Number)
        {
            // Instead of using a fancy instruction, just add to the index of the thing we have.
            wIndex += pValue->GetNumberValue();
            fIndexerOptimize = true;
        }
    }
    return fIndexerOptimize;
}


BYTE TokenTypeToVOType(ResolvedToken tokenType)
{
    switch(tokenType)
    {
    case ResolvedToken::GlobalVariable:
        // e.g. gEgo
        return VO_GLOBAL;
    case ResolvedToken::ScriptVariable:
        // e.g. the foo in (local foo = 0)
        return VO_LOCAL;
    case ResolvedToken::Parameter:
        // e.g. the pEvent in (method (handleEvent pEvent)
        return VO_PARAM;
    case ResolvedToken::TempVariable:
        // e.g. the foo in (var foo)
        return VO_TEMP;
    default:
        assert(false); // Compiler error
        return 0;
    }
}

PCSTR VOTypeToString(uint8_t type)
{
    switch (type)
    {
        case VO_GLOBAL:
            return "global";
        case VO_LOCAL:
            return "local";
        case VO_PARAM:
            return "param";
        case VO_TEMP:
            return "temp";
    }
    return "";
}

BYTE GetVarOpcodeModifier(VariableModifier modifier)
{
    switch (modifier)
    {
    case VM_PreIncrement:
        return VO_INC_AND_LOAD;
        break;
    case VM_PreDecrement:
        return VO_DEC_AND_LOAD;
        break;
    }
    return 0;
}

void VariableOperand(CompileContext &context, WORD wIndex, BYTE bOpcode, int lineNumber, const SyntaxNode *pIndexer = nullptr)
{
    if (pIndexer)
    {
        // Put the indexer value in the accumulator.
        // First check if we can optimize this though... is the indexer just an immediate?
        if (!CanDoIndexOptimization(pIndexer, wIndex))
        {
            // No optimization :-(
            // Ask the indexer to put its value in the accumulator, and use this as an index for our operation.
            COutputContext accContext(context, OC_Accumulator);
            pIndexer->OutputByteCode(context);
            bOpcode |= VO_ACC_AS_INDEX_MOD;
        }
    }
    // REVIEW: we might be able to do extra error checking here, if an index is out of bounds
    // (could happen with index optimization for immediate array indicies)
    // We could do this for script vars and temp vars at least (not params)
    // The SCI interpreter doesn't crash instanly if you access these out of bounds, so I think it may
    // silently corrupt.
    bOpcode |= 0x40; // bit6 is always 1  (bit 7 in the final result)
    bOpcode <<= 1; // Since we're calling RawToOpcode, need to turn it back to Raw.
    context.code().inst(lineNumber, RawToOpcode(context.GetVersion(), bOpcode), wIndex);
}

void LoadEffectiveAddress(CompileContext &context, WORD wIndex, BYTE bVarType, const SyntaxNode *pIndexer, int lineNumber)
{
    if (pIndexer)
    {
        // Put the indexer value in the accumulator
        // First check if we can optimize this though... is the indexer just an immediate?
        if (!CanDoIndexOptimization(pIndexer, wIndex))
        {
            // No optimization :-(
            // Ask the indexer to put its value in teh accumulator, and use this as an index for our operation.
            COutputContext accContext(context, OC_Accumulator);
            pIndexer->OutputByteCode(context);
            bVarType |= LEA_ACC_AS_INDEX_MOD;
        }
    }
    context.code().inst(lineNumber, Opcode::LEA, bVarType << 1, wIndex);
}

//
// Given a string (e.g. "gEgo"), check to see what type of token it is, and what its index is, and
// generate the appropriate instruction that will load it in to the accumulator.
//
// e.g. the "gEgo" in the following:
// (= someVar gEgo)
//
//
void LookupTokenAndPlacePtrInAccumulator(const std::string &objectName, CompileContext &context, const SyntaxNode *pIndexer, const SyntaxNode *pNode, SpeciesIndex &si)
{
    WORD wIndex;
    ResolvedToken tokenType = context.LookupToken(pNode, objectName, wIndex, si);
    switch (tokenType)
    {
    case ResolvedToken::GlobalVariable:
        // e.g. gEgo
        LoadEffectiveAddress(context, wIndex, LEA_GLOBAL, pIndexer, pNode->GetLineNumber());
        break;
    case ResolvedToken::ScriptVariable:
        // e.g. the foo in (local foo = 0)
        LoadEffectiveAddress(context, wIndex, LEA_LOCAL, pIndexer, pNode->GetLineNumber());
        break;
    case ResolvedToken::Parameter:
        // e.g. the pEvent in (method (handleEvent pEvent)
        LoadEffectiveAddress(context, wIndex, LEA_PARAM, pIndexer, pNode->GetLineNumber());
        break;
    case ResolvedToken::TempVariable:
        // e.g. the foo in (var foo)
        LoadEffectiveAddress(context, wIndex, LEA_TEMP, pIndexer, pNode->GetLineNumber());
        break;
    default:
        context.ReportError(pNode, "Expected something to which we could get a pointer: %s", objectName.c_str());
        break;
    }
}

//
// Load property to stack or acc
//
void LoadProperty(CompileContext &context, WORD wIndex, bool bStack, int lineNumber)
{
    Opcode bOpcode;
    switch (context.GetVariableModifier())
    {
    case VM_PreIncrement:
        bOpcode = bStack ? Opcode::IPTOS : Opcode::IPTOA;
        break;
    case VM_PreDecrement:
        bOpcode = bStack ? Opcode::DPTOS : Opcode::DPTOA;
        break;
    default:
        bOpcode = bStack ? Opcode::PTOS : Opcode::PTOA;
        break;
    }
    context.code().inst(lineNumber, bOpcode, wIndex);
}

//
// Store stack or acc in property.
//
void StoreProperty(CompileContext &context, WORD wIndex, bool bStack, int lineNumber)
{
    context.code().inst(lineNumber, bStack ? Opcode::STOP : Opcode::ATOP, wIndex);
}

void WriteInstanceOrClass(CompileContext &context, ResolvedToken tokenType, WORD wNumber, const string &name, int lineNumber)
{
    OutputContext oc = context.GetOutputContext();
    switch (tokenType)
    {
    case ResolvedToken::Instance:
        context.code().inst(lineNumber, (oc == OC_Stack) ? Opcode::LOFSS : Opcode::LOFSA, wNumber);
        break;
    case ResolvedToken::Class:
        // Load the class address into the accumulator
        context.code().inst(lineNumber, Opcode::CLASS, wNumber);
        PushToStackIfAppropriate(context, lineNumber);
        break;
    default:
        assert(FALSE);
        break;
    }
}

void _AddParameter(ProcedureCall &proc, WORD wValue)
{
    proc.AddStatement(std::make_unique<PropertyValue>(wValue));
}

void WriteScriptID(CompileContext &context, WORD wInstanceScript, WORD wIndex)
{
    // Fake up a procedure call to ScriptID
    ProcedureCall proc;
    proc.SetName("ScriptID");

    // Add two parameters.
    _AddParameter(proc, wInstanceScript);
    _AddParameter(proc, wIndex);

    // Write the code.  We don't care about the result.
    proc.OutputByteCode(context);
}

void ValidateVariableDeclaration(CompileContext &context, const ISourceCodePosition *pPos, std::string &name)
{
    if (IsSCIKeyword(context.GetLanguage(), name))
    {
        context.ReportError(pPos, "'%s' is a keyword and cannot be used as a variable name.", name.c_str());
    }
    uint16_t dummy;
    if (context.LookupDefine(name, dummy))
    {
        context.ReportError(pPos, "'%s' is a define and cannot be used as a variable name.", name.c_str());
    }
}

//
// Required for if's and else's to compile properly
//
// An if/else looks like this:
// 
//      bnt A
//      ... if code ...
//      jmp B
// A:   ... else code...
// B:   ... rest of the function
//
// The number of bytes returned in CodeResult, is equal to the total number of bytes returned by all the statements.
// pwBytesLastStatement returns the number of bytes returned by the final statement.
// The CodeResult returns the type of the last thing.
//
CodeResult SingleStatementVectorOutputHelper(const SyntaxNodeVector &statements, CompileContext &context, WORD *pwBytesLastStatement = nullptr)
{
    WORD wBytes = 0;
    WORD wBytesLastStatement = 0;
    SpeciesIndex returnType = DataTypeVoid;

    // Handle ifs
    SyntaxNodeVector::const_iterator statementIt = statements.begin();
    while (statementIt != statements.end())
    {
        CodeResult result = (*statementIt)->OutputByteCode(context);
        wBytesLastStatement = result.GetBytes();
        returnType = result.GetType();
        wBytes += wBytesLastStatement;
        statementIt++;
    }
    if (pwBytesLastStatement)
    {
        *pwBytesLastStatement = wBytesLastStatement;
    }
    return CodeResult(wBytes, returnType);
}

// We pull the code out, because we need to use the exact same code in another case
// pFailure is optional.
CodeResult _OutputCodeForIfStatement(CompileContext &context, const SyntaxNode &condition, const SyntaxNode &success, const SyntaxNode *pFailure, bool fMeaning = false)
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, fMeaning);
    // Begin two branch blocks.
    branch_block blockTrue(context, BranchBlockIndex::Success);
    branch_block blockFalse(context, BranchBlockIndex::Failure);
    // Apply the condition, with result in accumulator.
    {
        declare_conditional isCondition(context, true);
        COutputContext accContext(context, OC_Accumulator);
        condition.OutputByteCode(context);
    }
    
    // Output the 'true' statements
    blockTrue.leave();
    success.OutputByteCode(context);

    // Now the false.
    if (pFailure)
    {
        // There is an else clause.  Before writing it, we need to write in a jump instruction here so the if clause
        // jumps to after the else.
        branch_block blockPostElse(context, BranchBlockIndex::PostElse); // -> this is where we'll jump to
        context.code().inst(condition.GetLineNumber(), Opcode::JMP, context.code().get_undetermined(), BranchBlockIndex::PostElse);
        // Leave the 'false' block, so the false will jump right here...
        blockFalse.leave();
        pFailure->OutputByteCode(context);
    }
    return CodeResult(0, DataTypeAny);
}

// Returns true if wValue is filled in.
// Reports an error if a keyword is used.
bool _PreScanPropertyTokenToNumber(CompileContext &context, SyntaxNode *pNode, const std::string &token, WORD &wValue)
{
    bool fRet = true;
    // Resolve it if it's a define.
    if (context.LookupDefine(token, wValue))
    {
        // Convert it to a number value.
    }
    // Handle some built-in types.
    else if (token == "true")
    {
        wValue = 1;
    }
    else if ((token == "false") || (token == "nullptr"))
    {
        wValue = 0;
    }
    else if (token == "self")
    {
        // Special case
        fRet = false;
    }
    // Finally, process any errors.
    else if (IsCodeLevelKeyword(context.GetLanguage(), token))
    {
        fRet = false;
        // Can't use code-level keywords as values.
        ReportKeywordError(context, pNode, token, "value");
    }
    else
    {
        fRet = false;
    }
    return fRet;
}

void Define::PreScan(CompileContext &context)
{
    if (!_strValue.empty())
    {
        if (_PreScanPropertyTokenToNumber(context, this, _strValue, _wValue))
        {
            _strValue.clear(); // Clear this out - we're an integer now.
        }
        else
        {
            context.ReportError(this, "Unknown token '%s'.", _strValue.c_str());
        }
    }
}

void ClassProperty::PreScan(CompileContext &context)
{
    if (_statement1)
    {
        _statement1->PreScan(context);
    }
}

void SanitizeSaid(string &said)
{
    // Remove all whitespace
    // 'green towel' becomes 'greentowel'
    // This comforms with SCIStudio's behavior.
    said.erase(std::remove_if(said.begin(), said.end(), ::isspace), said.end());
}

void PropertyValueBase::PreScan(CompileContext &context)
{
    switch(_type)
    {
    case ValueType::String:
        context.GetTempToken(ValueType::String, _stringValue);
        break;
    case ValueType::ResourceString:
        // Generating a token here means that the string will be added to the
        // script. But we don't want that if it is used as a resource string only.
        // In some contexts (e.g. class properties, assignments, comparisons, etc..),
        // quoted resource strings are treated as regular strings. So any part of the script that does that
        // needs to be output prior to the string section. Or, the parser needs to ensure that script part
        // never has ValueType::ResourceString. This is a bit fragile, should think how to rework or enforce this.
        //context.GetTempToken(ValueType::String, _stringValue);
        break;
    case ValueType::Said:
        SanitizeSaid(_stringValue);
        context.PreScanSaid(_stringValue, this);
        break;
    case ValueType::Token:
        if (_PreScanPropertyTokenToNumber(context, this, _stringValue, _numberValue))
        {
            assert(_fNegate == false); // REVIEW: SCIStudio compiler doesn't allow it - should we?
            _type = ValueType::Number;
        }
        break;

    case ValueType::ArraySize:
        {
            uint16_t arraySize = 1;
            auto it = context.ScriptArraySizes.find(_stringValue);
            if (it == context.ScriptArraySizes.end())
            {
                // Maybe it's a temp var.
                bool found = false;
                if (context.FunctionBaseForPrescan)
                {
                    for (const auto &var : context.FunctionBaseForPrescan->GetVariables())
                    {
                        found = (var->GetName() == _stringValue);
                        if (found)
                        {
                            // Temp vars must have specified size
                            assert(!var->IsUnspecifiedSize());
                            arraySize = var->GetSize();
                            break;
                        }
                    }
                }

                if (!found)
                {
                    context.ReportError(this, "Unknown array: '%s'.", _stringValue.c_str());
                }
            }
            else
            {
                arraySize = it->second;
            }
            // It's now a number...
            _type = ValueType::Number;
            _numberValue = arraySize;
    }
        break;
    }
    // Also pre-scan the indexer
    if (GetIndexer())
    {
        GetIndexer()->PreScan(context);
    }
}

CodeResult PropertyValueBase::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    if (!context.HasMeaning())
    {
        // This indicates that we have a value that is "sitting by itself", that can have no possible
        // effect on code.  e.g. "hello" in the following snippet:
        // (procedure (foo)
        //     (= gEgo 1)
        //     "hello"
        // )
        // context.ReportWarning(this, "'%s' has no effect on code.", ToString().c_str());
        // REVIEW: Got rid of this warning because
        // 1) it's not a big source of errors
        // 2) official Sierra (e.g. decompiler) scripts often do this for perf
    }

    BYTE wNumSendParams = 1;
    BYTE bOpcodeMod = VO_LOAD;
    OutputContext oc = context.GetOutputContext();
    bool fArrayError = false;
    bool fVarModifierError = (context.GetVariableModifier() != VM_None); // Assume failure, and we'll set to false where approp.
    switch (oc)
    {
    case OC_Stack:
        bOpcodeMod |= VO_STACK;
        break;
    case OC_Accumulator:
        bOpcodeMod |= VO_ACC;
        break;
    }

    // Some assertions
    // Negation is handled by the syntax parser - it will tell the
    // property value when it was the result of negation, but it should not allow negation of
    // non numeric values - even defines.
    assert(!_fNegate || (_type == ValueType::Number));
    // The syntax parser should also not allow array indexers on non-alphanumeric tokens
    assert((GetIndexer() == nullptr) || (_type == ValueType::Token) || (_type == ValueType::Pointer));
    // We'll check some more invalid cases of indexers below too (e.g. self[4])

    WORD wNumber;
    SpeciesIndex wType = DataTypeInvalid;
    switch(_type)
    {
    case ValueType::Number:
        {
            // That was easy.
            // e.g. 7, or $2000
            WriteImmediate(context, _numberValue, GetLineNumber());
            if (_fHex)
            {
                wType = DataTypeUInt;
            }
            else
            {
                wType = DataTypeInt;
            }
        }
        break;

    case ValueType::ResourceString:
        // Not implemented yet.
        // TODO: We need to know we're in a procedure call. This isn't supported for selector calls (per Sierra compat)
        uint16_t autoTextResourceNumber;
        if ((oc != OC_Accumulator) && context.IsAutoText(autoTextResourceNumber))
        {
            // Add ourselves as a resource tuple, and get the number.
            wNumber = context.AddStringResourceTuple(_stringValue);
            // We have two number to put - script number and index
            PushImmediate(context, autoTextResourceNumber, GetLineNumber());
            PushImmediate(context, wNumber, GetLineNumber());
            wNumSendParams++; // One more than normal.
            wType = DataTypeResourceString;
            break;
        }
        else
        {
            // Report error, since this is a tuple - can't go in acc
            // context.ReportError(this, "'%s': A resource string is not valid here.  Use an internal string instead.", _stringValue.c_str());
            // Just fall through to String, actually.
        }

    case ValueType::String:
        context.code().inst(GetLineNumber(), (oc == OC_Stack) ? Opcode::LOFSS : Opcode::LOFSA, context.GetTempToken(ValueType::String, _stringValue));
        wType = DataTypeString;
        break;

    case ValueType::Said:
        context.code().inst(GetLineNumber(), (oc == OC_Stack) ? Opcode::LOFSS : Opcode::LOFSA, context.GetTempToken(ValueType::Said, _stringValue));
        wType = DataTypeSaidString;
        break;

    case ValueType::Selector:
        // Look up the selector in the global selector table, and use that.
        if (context.LookupSelector(_stringValue, wNumber))
        {
            WriteImmediate(context, wNumber, GetLineNumber());
        }
        else
        {
            context.ReportError(this, "Unknown property or method: '%s'.", _stringValue.c_str());
        }
        wType = DataTypeSelector;
        break;

    case ValueType::Pointer:
        {
            // We can only apply the pointer operator to variables.
            LookupTokenAndPlacePtrInAccumulator(_stringValue, context, GetIndexer(), this, wType);
            PushToStackIfAppropriate(context, GetLineNumber());
            // ...and only of certain types.
            if (IsPointerType(wType))
            {
                context.ReportError(this, "Can't apply '@' to type '%s'.", context.SpeciesIndexToDataTypeString(wType).c_str());
            }
        }
        wType = DataTypePointer;
        break;

    case ValueType::Token:
        {
            WORD wInstanceScript;
            ResolvedToken tokenType = context.LookupToken(this, _stringValue, wNumber, wType, &wInstanceScript);
            if (_type == ValueType::Token)
            {
                switch (tokenType)
                {
                case ResolvedToken::Class:
                case ResolvedToken::Instance:
                case ResolvedToken::Self:
                    fArrayError = (GetIndexer() != nullptr);
                }

                switch (tokenType)
                {
                case ResolvedToken::ImmediateValue:
                    assert(false); // Handled by define resolution now
                    break;
                case ResolvedToken::GlobalVariable:
                case ResolvedToken::ScriptVariable:
                case ResolvedToken::Parameter:
                case ResolvedToken::TempVariable:
                    {
                        fVarModifierError = false;
                        bOpcodeMod |= GetVarOpcodeModifier(context.GetVariableModifier());
                        // Set the modifier (-- or ++) to none now, since we just used it, and we don't want it applying to the indexer.
                        // This properly handles codes like this:
                        // (-- [global5 temp0]) ; decrement "global5 indexed by temp0"
                        context.SetVariableModifier(VM_None);
                        VariableOperand(context, wNumber, TokenTypeToVOType(tokenType) | bOpcodeMod, GetLineNumber(), GetIndexer());
                    }
                    break;
                case ResolvedToken::ScriptString:
                    {
                        context.code().inst(GetLineNumber(), (oc == OC_Stack) ? Opcode::LOFSS : Opcode::LOFSA, context.GetTempToken(ValueType::String, context.GetScriptStringFromToken(_stringValue)));
                        WORD wImmediateIndex = 0;
                        if (GetIndexer())
                        {
                            if (!CanDoIndexOptimization(GetIndexer(), wImmediateIndex))
                            {
                                context.ReportError(GetIndexer(), "Expected an integer for the index.");
                            }
                        }
                    }
                    break;
                case ResolvedToken::ClassProperty:
                    fVarModifierError = false;
                    fArrayError = (GetIndexer() != nullptr);
                    // e.g. a property on the current object
                    LoadProperty(context, wNumber, (oc == OC_Stack), GetLineNumber());
                    if (context.GetClassName().empty())
                    {
                        context.ReportError(this, "'%s' can only be used within an object method.", _stringValue.c_str());
                    }
                    break;
                case ResolvedToken::Class:
                case ResolvedToken::Instance:
                    fArrayError = (GetIndexer() != nullptr);
                    WriteInstanceOrClass(context, tokenType, wNumber, _stringValue, GetLineNumber());
                    break;
                case ResolvedToken::ExportInstance:
                    fArrayError = (GetIndexer() != nullptr);
                    WriteScriptID(context, wInstanceScript, wNumber);
                    break;
                case ResolvedToken::Self:
                    fArrayError = (GetIndexer() != nullptr);
                    if (oc == OC_Stack)
                    {
                        WriteSimple(context, Opcode::PUSHSELF, GetLineNumber());
                    }
                    else
                    {
                        WriteSimple(context, Opcode::SELFID, GetLineNumber());
                    }
                    break;
                case ResolvedToken::Unknown:
                    if (IsSCIKeyword(context.GetLanguage(), _stringValue))
                    {
                        context.ReportError(this, "'%s' cannot be used here.", _stringValue.c_str());
                    }
                    else
                    {
                        // NOTE
                        // The SCIStudio language is not a context-free grammar, and so following statement:
                        // FooBar (+ a b)
                        // Could be interpreted as either a function call to FooBar with one parameters (+ a b), or
                        // a sequence of two values (FooBar and (+ a b)).  There is no way to tell without first looking
                        // up the meaning of Foobar (e.g. is it a function?).  Unfortunately we construct the syntax tree
                        // prior to doing that, and it is now too late.  To compensate, in SCICompanion we treat function
                        // calls as those where the open paren immediately follows the function name, with no whitespace.
                        // e.g. FooBar(+ a b).
                        // This is a difference with the SCIStudio compiler.  To aid the user in fixing this issue,
                        // we'll look up the "undeclared identifier" here, and see if it's a function name.
                        WORD wScript, wIndex;
                        string classOwner;
                        if (ProcedureUnknown != context.LookupProc(_stringValue, wScript, wIndex, classOwner))
                        {
                            context.ReportError(this, "The '(' character must immediately follow the function call '%s'.", _stringValue.c_str());
                        }
                        else
                        {
                            // "Normal" case
                            // Also check if this is a keyword, and provide a better error.
                            if (IsSCIKeyword(context.GetLanguage(), _stringValue))
                            {
                                context.ReportError(this, "'%s' cannot be used here.", _stringValue.c_str());
                            }
                            else
                            {
                                ErrorHelper(context, this, "Undeclared identifier", _stringValue);
                            }
                        }
                    }
                    break;
                }
            }
        }
        break;
    }

    if (fArrayError)
    {
        context.ReportError(this, "Can't use array index on this type.");
    }
    if (fVarModifierError)
    {
        context.ReportError(this, "++ and -- may only be applied to variables or properties.");
    }
    return CodeResult(wNumSendParams * 2, wType);
}

typedef void(*UpdateOperandFn)(code_pos, WORD);
void _UpdateFirstOperand(code_pos instruction, WORD wValue)
{
    instruction->update_first_operand(wValue);
}
void _UpdateSecondOperand(code_pos instruction, WORD wValue)
{
    instruction->update_second_operand(wValue);
}

CodeResult SendCall::OutputByteCode(CompileContext &context) const
{
    context.NotifySendOrProcCall();
    declare_conditional isCondition(context, false);
    if (_params.empty())
    {
        context.ReportWarning(this, "empty send call.");
    }
    Opcode bOpSend = Opcode::SEND;
    SpeciesIndex wObjectSpecies = DataTypeInvalid;

    // Before we start writing out the code that puts the target object into the accumulator,
    // we need to store the current position of the code.  We will use this as an insertion point
    // for the code that places the parameters on the stack.  The parameter code depends on the
    // results of the target object code, so it must be written out afterwards, even though it 
    // precedes the target object code.
    bool fFirstCode = context.code().empty();
    code_pos putParameterCodeBeforeHere;
    if (!fFirstCode)
    {
        putParameterCodeBeforeHere = context.code().get_cur_pos();
    }

    if (GetTargetName() == "super")
    {
        if (context.GetSuperClassName().empty())
        {
            context.ReportError(this, "'super' can only be used within an object method.");
        }
        bOpSend = Opcode::SUPER;
        // SCI byte code permits sending this to any class (or super class?) but our source code only
        // permits sending it to the direct superclass. This corresponds to SCIStudio, except SCIStudio
        // also lets this compile outside of a class, albeit with garbage.
        wObjectSpecies = context.LookupTypeSpeciesIndex(context.GetSuperClassName(), this);
    }
    else if (GetTargetName() == "self")
    {
        // We don't need wObjectSpecies to output actual byte code in the case of self.
        string classNameToLookup = context.IsInstance() ? context.GetSuperClassName() : context.GetClassName();
        if (!classNameToLookup.empty())
        {
            wObjectSpecies = context.LookupTypeSpeciesIndex(classNameToLookup, this);
        }
        else
        {
            // It is ok to call this outside of a class (e.g. in a procedure called *from* a class)
            // So let's not produce an error here. Instead, we'll change species to "any". Since this is
            // not used to generate bytecode for "self", this is fine. It's only used for errors/validation
            wObjectSpecies = DataTypeAny;
        }
        bOpSend = Opcode::SELF;
    }

    UpdateOperandFn updateOperand = nullptr;
    code_pos sendPushInstruction = context.code().get_undetermined();

    {
        declare_compileQuery queryWrap(context);

        // Imbue meaning from here on out
        change_meaning meaning(context, true);
        // If this is a true send (as opposed to a self or super), then we need to load the object
        // to which we're sending into the accumulator.
        if (bOpSend == Opcode::SEND)
        {
            if (!GetTargetName().empty())
            {
                // A straightforward send (e.g. aMan:init())
                // This can only be an instance or class
                WORD wNumber;
                WORD wInstanceScript;
                ResolvedToken tokenType = context.LookupToken(this, GetTargetName(), wNumber, wObjectSpecies, &wInstanceScript);
                switch (tokenType)
                {
                case ResolvedToken::Instance:
                case ResolvedToken::Class:
                    {
                        COutputContext accContext(context, OC_Accumulator);
                        WriteInstanceOrClass(context, tokenType, wNumber, GetTargetName(), GetLineNumber());
                    }
                    break;
                case ResolvedToken::ExportInstance:
                    WriteScriptID(context, wInstanceScript, wNumber);
                    break;
                default:
                    // Studio requires a "send" keyword for these things, so it won't hit this path.
                    if (context.GetLanguage() != LangSyntaxStudio)
                    {
                        BYTE bOpcodeMod = VO_LOAD | VO_ACC;
                        switch (tokenType)
                        {
                        case ResolvedToken::GlobalVariable:
                        case ResolvedToken::ScriptVariable:
                        case ResolvedToken::Parameter:
                        case ResolvedToken::TempVariable:
                            VariableOperand(context, wNumber, TokenTypeToVOType(tokenType) | bOpcodeMod, GetLineNumber(), nullptr);
                            break;
                        case ResolvedToken::ClassProperty:
                            // Load the property into the accumulator
                            LoadProperty(context, wNumber, false, GetLineNumber());
                            break;
                        default:
                            wObjectSpecies = DataTypeAny; // Just so we can continue
                            ErrorHelper(context, this, "Unknown identifier:", GetTargetName());
                            break;
                        }
                    }
                    else
                    {
                        wObjectSpecies = DataTypeAny; // Just so we can continue
                        context.ReportError(this, "%s must be an instance or class.", GetTargetName().c_str());
                    }
                    break;
                }
            }
            else if (_object3 && !_object3->GetName().empty())
            {
                // An LValue send (e.g. (send flakes[i]:x(4))
                // This is really just here for SCIStudio compatibility, for having array indexers.  Otherwise,
                // we could just always use _object or _object2.  We'll get here for any simple send statement that has
                // "send" though.  So it's possible the indexer is not there.
                const SyntaxNode *pIndexer = _object3->GetIndexer();

                BYTE bOpcodeMod = VO_LOAD | VO_ACC;
                WORD wNumber;
                ResolvedToken tokenType = context.LookupToken(this, _object3->GetName(), wNumber, wObjectSpecies);
                switch (tokenType)
                {
                case ResolvedToken::GlobalVariable:
                case ResolvedToken::ScriptVariable:
                case ResolvedToken::Parameter:
                case ResolvedToken::TempVariable:
                    VariableOperand(context, wNumber, TokenTypeToVOType(tokenType) | bOpcodeMod, GetLineNumber(), pIndexer);
                    break;
                case ResolvedToken::ClassProperty:
                    // Load the property into the accumulator
                    LoadProperty(context, wNumber, false, GetLineNumber());
                    if (pIndexer)
                    {
                        context.ReportError(this, "Properties cannot be indexed: %s.", _object3->GetName().c_str());
                    }
                    break;
                default:
                    wObjectSpecies = DataTypeAny; // Just so we can continue
                    ErrorHelper(context, this, "Unknown identifier: can not send to", _object3->GetName());
                    break;
                }
                // Good to go...
            }
            else
            {
                // The object we're sending is the result of some more complex expression
                // enclosed in ()
                // e.g. (send (myMotion:client):x(5))  -> we're sending to "myMotion:client"
                COutputContext accContext(context, OC_Accumulator);
                wObjectSpecies = _statement1->OutputByteCode(context).GetType();
            }
        }

        // write the instruction
        // The opcode
        if (bOpSend == Opcode::SUPER)
        {
            // Write the class (in our case, always the superclass)
            SpeciesIndex wClassIndex;
            if (context.LookupSpeciesIndex(context.GetSuperClassName(), wClassIndex))
            {
                context.code().inst(GetLineNumber(), Opcode::SUPER, wClassIndex.Type(), NumberOfSendPushesSentinel);
                sendPushInstruction = context.code().get_cur_pos();
                updateOperand = _UpdateSecondOperand;
            }
            else
            {
                context.ReportError(this, "Internal compiler error - unable to find class index of '%s'", context.GetSuperClassName().c_str());
            }
        }
        else
        {
            assert((bOpSend == Opcode::SELF) || (bOpSend == Opcode::SEND));
            context.code().inst(GetLineNumber(), bOpSend, NumberOfSendPushesSentinel);
            sendPushInstruction = context.code().get_cur_pos();
            updateOperand = _UpdateFirstOperand;
        }

        if (queryWrap.query.SendOrProcCallWasOutput)
        {
            for (const auto &param : _params)
            {
                if (param->ContainsRest())
                {
                    // If the target made a proc or send call, then we can't use &rest in our params, since it would affect the target's code
                    // (which is executed after the params are pushed to the stack)
                    // REVIEW: Maybe I could just output put &rest after? I dunno.
                    // REVIEW: We probably also need to guard against multi-sends using rest in multiple places.
                    context.ReportError(param.get(), "&rest cannot be used if the send target itself contains nested procedure calls or sends. Assign the result of the procedure call or send to a temporary variable and use that instead.");
                    break;
                }
            }
        }
    }

    WORD wSendPushes = 0;
    SpeciesIndex returnType = DataTypeAny;

    {
        if (fFirstCode)
        {
            putParameterCodeBeforeHere = context.code().get_beginning();
        }
        else
        {
            // Increment this so it points to the first instruction of the target object code.
            // We will tell the code() to insert all future instructions just *before* this position.
            ++putParameterCodeBeforeHere;
        }
        code_insertion_point insertionPoint(context, putParameterCodeBeforeHere);

        assert(wObjectSpecies != DataTypeInvalid);
        // Let the sendparams know whom they're calling. However, use "DataTypeAny" in the case of self, because a call to self
        // might be a call on a subclass of ourselves. In that case, we can't know which properties or methods it supports.
        SendTargetSpeciesIndex objectSpecies(context, (bOpSend == Opcode::SELF) ? DataTypeAny : wObjectSpecies, GetTargetName());
        COutputContext stackContext(context, OC_Stack); // Ensure parameters are pushed on the stack.

        GenericOutputByteCode2<SendParam> gobcResult = for_each(_params.begin(), _params.end(), GenericOutputByteCode2<SendParam>(context));
        wSendPushes = gobcResult.GetByteCount();
        returnType = gobcResult.GetLastType();

        // Also the rest statement.  This is a hack for the SCIStudio syntax, which allows the following:
        // (send gEgo:init() rest params)
        // when it really should be
        // (send gEgo:init(rest params))
        if (_fRestHack)
        {
            assert(context.GetLanguage() == LangSyntaxStudio);
            wSendPushes += _rest->OutputByteCode(context).GetBytes();
        }
    }

    if (updateOperand && (sendPushInstruction != context.code().get_undetermined()))
    {
        (*updateOperand)(sendPushInstruction, wSendPushes);
    }

    return CodeResult(PushToStackIfAppropriate(context, GetLineNumber()), returnType);
}

CodeResult Cast::OutputByteCode(CompileContext &context) const
{
    CodeResult result = _statement1->OutputByteCode(context);
    SpeciesIndex si = context.LookupTypeSpeciesIndex(_innerType, this);
    return CodeResult(result.GetBytes(), si);
}

bool SendParam::ContainsRest() const
{
    for (const auto &param : _segments)
    {
        if (param->GetNodeType() == sci::NodeType::NodeTypeRest)
        {
            return true;
        }
    }
    return false;
}

void SendParam::PreScan(CompileContext &context) 
{
    ForwardPreScan2(_segments, context);
}

CodeResult SendParam::OutputByteCode(CompileContext &context) const
{
    // We force "meaning" only if we are a method call (not just a property retrieval)
    change_meaning meaning(context, true, IsMethod());

    // A send param consists of:
    // W    - the selector
    // W    - number of params for selector
    // Wxn  - each param.
    WORD wBytes = 0;
    WORD wSelector;
    bool fApplyTypeChecking = false;
    if (context.LookupSelector(GetSelectorName(), wSelector))
    {
        // The selector
        PushImmediate(context, wSelector, GetLineNumber());
        fApplyTypeChecking = true;
    }
    else
    {
        // Maybe this is a variable that is a selector.
        // e.g. 
        // void (selector foo)
        // {
        //     myObject.foo(5);
        // }
        //
        // Quick and dirty approach is to make a temporary property value to do the code generation.
        {
            PropertyValue value;
            value.SetValue(GetSelectorName(), ValueType::Token);
            value.SetPosition(GetPosition()); // For error reporting.
            COutputContext stackContext(context, OC_Stack); // We want the value pushed to the stack
            SpeciesIndex si = value.OutputByteCode(context).GetType(); // We'll rely on it to generate errors...

            // We can't do any type checking of the function/property call here,
            // since the selector is only known at run-time.  However, we can verify that the variable
            // is indeed a selector (or var)
            if (si == DataTypeInvalid)
            {
                context.ReportError(this, "Unknown property or method: '%s'.", GetSelectorName().c_str());
            }
            else if (!DoesTypeMatch(context, DataTypeSelector, si, nullptr, nullptr))
            {
                context.ReportError(this, "'%s' must be of type 'selector' to be used here.", GetSelectorName().c_str());
            }
        }
    }

    // The number of parameters - put in a fake opcode to start with, since we don't yet know
    // how many parameters.
    context.code().inst(GetLineNumber(), Opcode::INDETERMINATE);
    code_pos pushNumberOfParams = context.code().get_cur_pos();

    // Indicate how many bytes were pushed to the stack so far.
    wBytes = 4;

    // Note: the parameters will indicate to themselves how many (since it could vary - one parameter could end up pushing two things)
    COutputContext stackContext(context, OC_Stack);
    GenericOutputByteCode2<SyntaxNode> gobc = for_each(_segments.begin(), _segments.end(), GenericOutputByteCode2<SyntaxNode>(context));
    WORD wBytesOfParams = gobc.GetByteCount();
    std::vector<SpeciesIndex> parameterTypes = gobc.GetTypes();
   
    // Go back to our fake instruction and fill in how many parameters were passed.
    // The number of params is the number of bytes divided by 2.
    PushImmediateAt(context, wBytesOfParams / 2, pushNumberOfParams, GetLineNumber());

    if (!context.HasMeaning())
    {
        // If this is not a method call, but just a property retrieval, then generate 
        // a warning if no one is listening to it.
        // (if this was a method call, we would already imbue ourselves with meaning at the top
        // of the function)
        // context.ReportWarning(this, "'%s' has no effect on code.", _selector.c_str());
        // REVIEW: we can't emit this warning unless we know that a selector is a property
        // and not a method.  e.g. (send gEgo:init)  does have meaning.
    }

    SpeciesIndex returnType = DataTypeAny;

    if (fApplyTypeChecking)
    {
        std::string typeName;
        SpeciesIndex calleeSpecies = context.GetSendCalleeType(typeName);
        // Get the signatures for this method (or property)

        SpeciesIndex propertyType;
        bool fMethod;

        if (context.LookupSpeciesMethodOrProperty(calleeSpecies, wSelector, propertyType, fMethod))
        {
            if (fMethod)
            {
                returnType = DataTypeAny;
            }
            else
            {
                if (parameterTypes.size() > 1)
                {
                    string selName = GetSelectorName();
                    context.ReportError(this, "%s is a property.  Only one parameter may be supplied.", selName.c_str());
                }
                if (parameterTypes.empty())
                {
                    // Reading the property
                    returnType = propertyType;
                }
                else
                {
                    // Produce an error if the property type does not match what the caller is passing.
                    if (!DoesTypeMatch(context, propertyType, parameterTypes[0], nullptr, _segments[0].get()))
                    {
                        std::stringstream ss;
                        ss << "Type '%s' does not match property '" << GetSelectorName() << "' of type '%s'.";
                        context.ReportTypeError(this, parameterTypes[0], propertyType, ss.str().c_str());
                    }
                    // Assigning a property has no data type.
                    returnType = DataTypeVoid;
                }
            }
        }
        else if ((calleeSpecies != DataTypeAny) && (calleeSpecies != DataTypeNone))
        {
            // We'll make a decision not to generate an error if the callee is of type 'var'.
            // We need some way for code to call specific methods on something, so this will be it
            // (e.g. by casting to var).
            // REVIEW: a strongly-typed alternative would be support for interfaces.

            // Before generating an error here, see if typeName is an instance in the current script. Instances can define
            // their own methods.
            if (!context.DoesScriptObjectHaveMethod(typeName, GetSelectorName()))
            {
                std::string objectTypeString = context.SpeciesIndexToDataTypeString(calleeSpecies);
                context.ReportError(this, "%s is not a property or method on type '%s'.", GetSelectorName().c_str(), objectTypeString.c_str());
            }
        }
    }

    // + 4 for the selector and param count.
    // We return DataTypeNone here, because it is the responsibility of the SendCall to determine
    // the type (based on method matching)
    return CodeResult(wBytesOfParams + 4, returnType);
}

CodeResult CodeBlock::OutputByteCode(CompileContext &context) const
{
    WORD wBytes = 0;
    // Don't sum all the bytes here - only return the # of bytes of the final one.

    // If there are more than two statements, we can consider this a "list" of statements that probably
    // has no value - even though technically, the last statement will generate the value for this - and generate
    // a warning if someone is trying to use the value that this list of statements represents.
    // We do so by removing "meaning".
    // change_meaning meaning(context, false, _code.size() > 1);
    // I've decided this is a bad thing.  We should not remove meaning.
    // e.g. switch (a, b)

    CodeResult result = SingleStatementVectorOutputHelper(_segments, context, &wBytes);
    return CodeResult(wBytes, result.GetType());
}

CodeResult ProcedureCall::OutputByteCode(CompileContext &context) const
{
    context.NotifySendOrProcCall();

    declare_conditional isCondition(context, false);
    change_meaning meaning(context, true);
    // callb or callk or calle
    // callb: we need to write the "dispatch index" here... this would be gleaned from the .sco file I suppose.
    // It's the index of the public procedure in a script (section 7, the dispatch table)
    // But how do we specify the script???
    // callb: it's for main scripts.
    // calle: it includes a script numbers... but the # of bytes is confusing in the docs.  WW or BB
    // callk: call a kernel -> specify the kernel func index.
    // call: internal call -> use a relative position

    // Put in a fake opcode for the time being - we'll add our push later (we need to know
    // how many parameters we're pushing, before we know which instruction to use).
    context.code().inst(GetLineNumber(), Opcode::INDETERMINATE);
    code_pos parameterCountInstruction = context.code().get_cur_pos();

    // Create a send frame and stack context, and party on.
    WORD wCallBytes = 0;
    std::vector<SpeciesIndex> parameterTypes;
    {
        COutputContext stackContext(context, OC_Stack);
        // Collect parameter type information here, in addition to the number of bytes pushed.
        GenericOutputByteCode2<SyntaxNode> gobc = for_each(_segments.begin(), _segments.end(), GenericOutputByteCode2<SyntaxNode>(context));
        wCallBytes = gobc.GetByteCount();
        parameterTypes = gobc.GetTypes();
    }

    // Now go back and put in our parameter count (note: we don't include the parameter count in the number of parameters used)
    PushImmediateAt(context, wCallBytes / 2, parameterCountInstruction, GetLineNumber());

    WORD wScript, wIndex;
    string classOwner;
    ProcedureType procType = context.LookupProc(_innerName, wScript, wIndex, classOwner);
    switch (procType)
    {
    case ProcedureMain:
        // We're calling something in the main script. "callb"
        context.code().inst(GetLineNumber(), Opcode::CALLB, wIndex, wCallBytes);
        break;

    case ProcedureExternal:
        context.code().inst(GetLineNumber(), Opcode::CALLE, wScript, wIndex, wCallBytes);
        break;

    case ProcedureLocal:
        // Something in the current script "call"
        if (!classOwner.empty() && (context.GetClassName() != classOwner))
        {
            context.ReportError(this, "'%s' can only be called from class '%s'.", _innerName.c_str(), classOwner.c_str());
        }
        context.code().inst(GetLineNumber(), Opcode::CALL, wIndex, wCallBytes); // wIndex is temporary
        DEBUG_BRANCH(context.code(), DEBUG_LOCAL_PROC_CALL);
        context.TrackLocalProcCall(_innerName);
        break;
    case ProcedureKernel:
        // A kernel function
        context.code().inst(GetLineNumber(), Opcode::CALLK, wIndex, wCallBytes);
        break;

    case ProcedureUnknown:
        {
            std::string message = "Unknown procedure";
            bool checkUse = true;
            if (context.GetLanguage() == LangSyntaxSCI)
            {
                // It could have been accidentally enclosed in parentheses.
                uint16_t index;
                SpeciesIndex dataType;
                if ((ResolvedToken::Unknown != context.LookupToken(nullptr, _innerName, index, dataType)) ||
                    context.LookupDefine(_innerName, index))
                {
                    message += " -- (solitary values don't need to be enclosed in parentheses). ";
                    checkUse = false;
                }
            }
            ErrorHelper(context, this, message, _innerName, checkUse);
            break;
        }
    }

    return CodeResult(PushToStackIfAppropriate(context, GetLineNumber()), DataTypeAny);
}

//
// This function assumes that two branch block frames have been declared:
// BranchBlockIndex::Success and BranchBlockIndex::Failure
//
CodeResult ConditionalExpression::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, true);
    change_meaning meaning(context, true);
    {
        assert(_segments.size() >= 1);
        // Put things into the accumulator, in general
        COutputContext ocContext(context, OC_Accumulator);

        // We should never have been created if we don't have at least one term
        assert(!_segments.empty());

        // A, B, C
        // Output the first 0 to n-2 terms as if they were not in a condition.  In the above
        // example, this would be A and B.
        auto it = _segments.begin();
        auto itEnd = _segments.end();
        --itEnd;
        CodeResult result;
        while (it != itEnd)
        {
            (*it)->OutputByteCode(context); // Ignore the result
            ++it;
        }

        // We care about the result of the final term.
        // Enclose the final one in an OR branch block (above example, this would be C)
        {
            branch_block_base orBlock(context, BranchBlockIndex::Or);
            result = (*it)->OutputByteCode(context);
            context.code().inst(GetLineNumber(), Opcode::BNT, context.code().get_undetermined(), BranchBlockIndex::Failure);
            assert(!orBlock.active());
        }

        // Do some type-checking
        // Whatever the result was, it must be convertable to bool (basically only void should fail)
        if (!DoesTypeMatch(context, DataTypeBool, result.GetType(), nullptr, (*it).get()))
        {
            context.ReportError((*it).get(), "Conditional expression of type '%s' is illegal.", context.SpeciesIndexToDataTypeString(result.GetType()).c_str());
        }
    }
    return 0; // void
}


CodeResult ReturnStatement::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, true);
    SpeciesIndex returnValueType = DataTypeVoid;
    // Put the result from the statement (if we have one) into the accumulator
    if (_statement1)
    {
        COutputContext accContext(context, OC_Accumulator);
        returnValueType = _statement1->OutputByteCode(context).GetType();
    }

    // Type checking
    std::vector<SpeciesIndex> returnTypes = context.GetReturnValueTypes();
    bool fMatch = false;
    for (SpeciesIndex si : returnTypes)
    {
        if (DoesTypeMatch(context, si, returnValueType, nullptr, _statement1.get()))
        {
            fMatch = true;
        }
    }

    // Then return
    WriteSimple(context, Opcode::RET, GetLineNumber());
    return 0; // void
}

vector<pair<BinaryOperator, AssignmentOperator>> binaryAssignmentMapping =
{
    { BinaryOperator::None, AssignmentOperator::Assign },
    { BinaryOperator::Add, AssignmentOperator::Add },
    { BinaryOperator::Subtract, AssignmentOperator::Subtract },
    { BinaryOperator::Multiply, AssignmentOperator::Multiply },
    { BinaryOperator::Divide, AssignmentOperator::Divide },
    { BinaryOperator::Mod, AssignmentOperator::Mod },
    { BinaryOperator::BinaryAnd, AssignmentOperator::BinaryAnd },
    { BinaryOperator::BinaryOr, AssignmentOperator::BinaryOr },
    { BinaryOperator::ExclusiveOr, AssignmentOperator::ExclusiveOr },
    { BinaryOperator::ShiftRight, AssignmentOperator::ShiftRight },
    { BinaryOperator::ShiftLeft, AssignmentOperator::ShiftLeft },
};

//
// Turns "+=" into "+".
// Returns BinaryOperator::None if there was no equivalent.
BinaryOperator GetBinaryOpFromAssignment(AssignmentOperator assignment)
{
    auto itFind = find_if(binaryAssignmentMapping.begin(), binaryAssignmentMapping.end(),
        [assignment](pair<BinaryOperator, AssignmentOperator> &entry)
    {
        return entry.second == assignment;
    }
        );
    if (itFind != binaryAssignmentMapping.end())
    {
        return itFind->first;
    }
    return BinaryOperator::None;
}

CodeResult Assignment::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, true);
    OutputContext ocWhereWePutValue = OC_Unknown; // Whether we put the value on the stack or not.

    // Look up what the variable is.
    std::string strVarName = _variable->GetName();
    WORD wIndex;

    SpeciesIndex wType = DataTypeInvalid;
    ResolvedToken tokenType = context.LookupToken(this, strVarName, wIndex, wType);

    // Catch some errors
    const SyntaxNode *pIndexer = _variable->HasIndexer() ? _variable->GetIndexer() : nullptr;
    switch(tokenType)
    {
    case ResolvedToken::GlobalVariable:
    case ResolvedToken::ScriptVariable:
    case ResolvedToken::Parameter:
    case ResolvedToken::TempVariable:
        break;
    case ResolvedToken::ClassProperty:
        if (pIndexer)
        {
            context.ReportError(this, "Property '%s' cannot be indexed like an array.", strVarName.c_str());
        }
        if (context.GetClassName().empty())
        {
            context.ReportError(this, "'%s' can only be used within an object method.", strVarName.c_str());
        }
        break;
    default:
        ErrorHelper(context, this, "Unknown variable", strVarName);
        break;
    }

    BinaryOperator theBinaryOperator = GetBinaryOpFromAssignment(Operator);
    if (theBinaryOperator != BinaryOperator::None)
    {
        // This is something like +=, *=, etc...

        // If we have an indexer, check here if it's an immediate value (this optimization is in
        // VariableOperand, but things are much simple for the += type operations when the indexer is
        // immediate).
        if (pIndexer && CanDoIndexOptimization(pIndexer, wIndex))
        {
            // Get rid of this... we'll just use the adjusted the index.
            pIndexer = nullptr;
        }

        BYTE bOpcodeMod = VO_LOAD | VO_STACK; // ????
        switch (tokenType)
        {
        case ResolvedToken::GlobalVariable:
        case ResolvedToken::ScriptVariable:
        case ResolvedToken::Parameter:
        case ResolvedToken::TempVariable:
            if (pIndexer)
            {
                // A little bit more complicated:  a[10] += 5;
                // Let's evaluate the indexer, and put it on the accumulator
                OutputByteCodeToAccumulator(context, *pIndexer);
                // Put a copy on the stack too, since we'll need it later.
                WriteSimple(context, Opcode::PUSH, GetLineNumber());
                // Now don't pass an indexer to VariableOperand, because we've already taken care of that.
                // We'll load the variable onto the stack, and say to use the acc as an index:
                VariableOperand(context, wIndex, TokenTypeToVOType(tokenType) | VO_STACK | VO_LOAD | VO_ACC_AS_INDEX_MOD, GetLineNumber());
                // And put the value into the accumulator
                OutputByteCodeToAccumulator(context, *_statement1);
                // Perform the operation (adding the value to the thing on the stack)
                WriteSimple(context, GetInstructionForBinaryOperator(theBinaryOperator), GetLineNumber());
                // The result is now in the accumulator.  We actually want it in the stack, since
                // we want to use the variable index (currently on the stack) in the accumulator
                // Do a little trick:
                WriteSimple(context, Opcode::EQ, GetLineNumber());        // -> eq?... the value in the accumulator will now be on the prev register
                WriteSimple(context, Opcode::TOSS, GetLineNumber());      // Now the saved index will be in the accumulator
                WriteSimple(context, Opcode::PPREV, GetLineNumber());     // And now... the value will be on the stack!
                VariableOperand(context, wIndex, TokenTypeToVOType(tokenType) | VO_STACK | VO_STORE | VO_ACC_AS_INDEX_MOD, GetLineNumber());

                // REVIEW: the value was on the stack, but now it's gone!  Technically, we'll need a system where
                // we can know if the caller *really* needs the result of this assignment.  But that's probably rare,
                // and we can live without it.
            }
            else
            {
                // Easy!  Load the variable onto the stack.
                VariableOperand(context, wIndex, TokenTypeToVOType(tokenType) | VO_STACK | VO_LOAD, GetLineNumber());
                // And the value into the accumulator
                OutputByteCodeToAccumulator(context, *_statement1);
                // Perform the operation
                WriteSimple(context, GetInstructionForBinaryOperator(theBinaryOperator), GetLineNumber());
                // And now it goes back in the variable
                VariableOperand(context, wIndex, TokenTypeToVOType(tokenType) | VO_ACC | VO_STORE, GetLineNumber());
                ocWhereWePutValue = OC_Accumulator; // It's sitting in the accumulator
            }
            break;
        case ResolvedToken::ClassProperty:
            // Load the property onto the stack
            LoadProperty(context, wIndex, true, GetLineNumber());
            // And the value into the accumulator
            OutputByteCodeToAccumulator(context, *_statement1);
            // Perform the operation
            WriteSimple(context, GetInstructionForBinaryOperator(theBinaryOperator), GetLineNumber());
            // And put the value (now in the accumulator) back into the property
            StoreProperty(context, wIndex, false, GetLineNumber());
            ocWhereWePutValue = OC_Accumulator; // It's sitting in the accumulator
            break;
        }
    }
    else
    {
        SpeciesIndex wValueType;
        // Regular = assignment
        // The indexer will use the accumulator, so that means we'll need to use the stack for the value in that case
        if (pIndexer && CanDoIndexOptimization(pIndexer, wIndex))
        {
            // Get rid of this... we'll just use the adjusted the index.
            pIndexer = nullptr;
        }
        ocWhereWePutValue = OC_Accumulator; // Always the accumulator.
        bool fValueWasOnStack;
        // Write the value
        {
            // Scope the context. If we're indexed, we'll ask to put the value on the stack, since the
            // accumulator will be used for the index.
            fValueWasOnStack = (pIndexer != nullptr);
            COutputContext accContext(context, fValueWasOnStack ? OC_Stack : OC_Accumulator);
            wValueType = _statement1->OutputByteCode(context).GetType();
        }

        // Now actually write the assignment

        // Now figure out the correct instruction to use and write it.
        // Get information about the variable descriptor
        switch (tokenType)
        {
        case ResolvedToken::GlobalVariable:
        case ResolvedToken::ScriptVariable:
        case ResolvedToken::Parameter:
        case ResolvedToken::TempVariable:
        {
            // We always use the accumulator version of the store opcodes. The value being stored is still
            // put on the stack in the increment case, even if VO_ACC is used. The difference is that in the indexed
            // case, we want the value put on the accumulator after the accumulator is used for indexing. The
            // stack versions of the store opcodes don't do that.
            VariableOperand(context, wIndex, TokenTypeToVOType(tokenType) | VO_STORE | VO_ACC, GetLineNumber(), pIndexer);
        }
            break;
        case ResolvedToken::ClassProperty:
            assert(pIndexer == nullptr || context.HasErrors());
            StoreProperty(context, wIndex, false, GetLineNumber());  // false -> accumulator
            break;
        }

        // Ensure the types are the same.
        if (!DoesTypeMatch(context, wType, wValueType, nullptr, _statement1.get()))
        {
            context.ReportTypeError(this, wValueType, wType, "type '%s' cannot be assigned to type '%s'.");
        }
    }

    // We're almost done.  The value needs to be returned too.
    if (context.GetOutputContext() != ocWhereWePutValue)
    {
        if (ocWhereWePutValue == OC_Accumulator)
        {
            // It's in the accumulator, but needs to be on the stack.
            WriteSimple(context, Opcode::PUSH, GetLineNumber());
        }
        else
        {
            assert(ocWhereWePutValue != OC_Stack);
            // OC_Unknown.  A rare case, not worth getting to work.
        }
    }
    WORD wBytesUsed = (context.GetOutputContext() == OC_Stack) ? 2 : 0;
    return CodeResult(wBytesUsed, wType);
}

CodeResult LValue::OutputByteCode(CompileContext &context) const
{
    // No one needs to call this.  The caller should instead glean information off of this object.
    assert(FALSE);
    return 0;
}
void LValue::PreScan(CompileContext &context)
{
    if (_indexer) _indexer->PreScan(context);
}

vector<BinaryOperator> BooleanOps =
{
    BinaryOperator::UnsignedGreaterEqual,
    BinaryOperator::GreaterEqual,
    BinaryOperator::UnsignedGreaterThan,
    BinaryOperator::GreaterThan,
    BinaryOperator::UnsignedLessEqual,
    BinaryOperator::LessEqual,
    BinaryOperator::UnsignedLessThan,
    BinaryOperator::LessThan,
    BinaryOperator::NotEqual,
    BinaryOperator::Equal,
    BinaryOperator::LogicalAnd,
    BinaryOperator::LogicalOr,
};

CodeResult BinaryOp::_OutputByteCodeAnd(CompileContext &context) const
{
    branch_block blockSuccess(context, BranchBlockIndex::Success);
    // Output the left operand.
    {
        COutputContext stackContext(context, OC_Accumulator);
        _statement1->OutputByteCode(context).GetType();
    }

    context.code().inst(GetLineNumber(), Opcode::BNT, context.code().get_undetermined(), BranchBlockIndex::Failure);

    blockSuccess.leave(); // Successes in the left operand, branch to here.

    // Then the right operand.
    {
        COutputContext stackContext(context, OC_Accumulator);
        _statement2->OutputByteCode(context);
    }

    WORD wBytesUsed = 0;
    return CodeResult(wBytesUsed, DataTypeBool);
}

CodeResult BinaryOp::_OutputByteCodeOr(CompileContext &context) const
{
    branch_block blockFailure(context, BranchBlockIndex::Failure);

    // Output the left operand.
    {
        COutputContext stackContext(context, OC_Accumulator);
        _statement1->OutputByteCode(context);
    }

    context.code().inst(GetLineNumber(), Opcode::BT, context.code().get_undetermined(), BranchBlockIndex::Success);

    blockFailure.leave(); // failure in the left operand, branch to here.

    // Then the right operand.
    {
        COutputContext stackContext(context, OC_Accumulator);
        _statement2->OutputByteCode(context);
    }

    WORD wBytesUsed = 0;
    return CodeResult(wBytesUsed, DataTypeBool);
}

// Mapping of signed to unsigned equivalents.
vector<pair<BinaryOperator, BinaryOperator>> c_unsignedOps =
{
    { BinaryOperator::GreaterEqual, BinaryOperator::UnsignedGreaterEqual },
    { BinaryOperator::LessEqual, BinaryOperator::UnsignedLessEqual },
    { BinaryOperator::GreaterThan, BinaryOperator::UnsignedGreaterThan },
    { BinaryOperator::LessThan, BinaryOperator::UnsignedLessThan },
};

CodeResult _WriteFakeIfStatement(CompileContext &context, const BinaryOp &binary)
{
    // When the user writes:
    // x = (a && b)
    // then for (a && b) we do
    // if (a && b)
    // {
    //     1;
    // } // else being 0 is implicit.
    PropertyValue success;
    success.SetValue(1);

    {
        // Put the result of the if into the accumulator - we'll push to stack as necessary.
        COutputContext accContext(context, OC_Accumulator);
        // We need to put this inside a ConditionalExpression for the code output to work.
        // We can't move the BinaryOp into another syntax node, so we'll use WeakSyntaxNode as
        // the bridge.
        ConditionalExpression expression(make_unique<WeakSyntaxNode>(&binary));
        // true -> give this meaning, otherwise the compiler will complain that the '1' value
        // has no effect on code.  It actually does, because we're playing tricks.
        _OutputCodeForIfStatement(context, expression, success, nullptr, true);
    }
    return CodeResult(PushToStackIfAppropriate(context, binary.GetLineNumber()), DataTypeBool);
}

// An n-ary operation is any operator that takes n operands.
// Technically +, *, |, ^ and & all do this, but those are converted to nested
// BinaryOp's before compilation. So all we deal with here are the comparison operators
CodeResult NaryOp::OutputByteCode(CompileContext &context) const
{
    assert(IsRelational(Operator) && "NaryOp operator must be a comparison operator");
    // The byte code format for these operators will be:
    // 0:   push on stack
    // 1:   load in acc
    // [operator]
    // bnt end
    // 1:   pprev (puts 1 on stack)
    // 2:   load in acc
    // bnt end
    // 2:   pprev (puts 2 on stack)
    // 3:   load in acc

    branch_block blockSuccess(context, BranchBlockIndex::Failure);
    for (size_t i = 1; i < _segments.size(); i++)
    {
        if (i < 2)
        {
            // First guy on stack
            COutputContext stackContext(context, OC_Stack);
            _segments[0]->OutputByteCode(context);
        }
        else
        {
            // Add a branch based on the previous 2 operands. This branches to the future failure path
            context.code().inst(GetLineNumber(), Opcode::BNT, context.code().get_undetermined(), BranchBlockIndex::Failure);
            // Subsequent ones use the pprev (the value of the accumulator prior to the previous comparison opcode)
            context.code().inst(GetLineNumber(), Opcode::PPREV);
        }
        
        // Second operand
        {
            COutputContext accContext(context, OC_Accumulator);
            _segments[i]->OutputByteCode(context);
        }

        // The operator
        context.code().inst(GetLineNumber(), GetInstructionForBinaryOperator(Operator));
    }

    // Leave before the possible push
    blockSuccess.leave(); 

    WORD wBytesUsed = PushToStackIfAppropriate(context, GetLineNumber());
    // TODO: We have ignored data type completely
    return CodeResult(wBytesUsed, DataTypeAny);
}

CodeResult BinaryOp::OutputByteCode(CompileContext &context) const
{
    if (context.InConditional() && (Operator == BinaryOperator::LogicalAnd))
    {
        return _OutputByteCodeAnd(context);
    }
    else if (context.InConditional() && (Operator == BinaryOperator::LogicalOr))
    {
        return _OutputByteCodeOr(context);
    }
    else
    {
        if (Operator == BinaryOperator::LogicalAnd || Operator == BinaryOperator::LogicalOr)
        {
            // Handle && and || when not used in a condition.
            // Basically, we write an "if statement" that evaluates to 1 or 0
            return _WriteFakeIfStatement(context, *this);
        }
        else
        {
            SpeciesIndex wTypeLeft;
            SpeciesIndex wTypeRight;
            // pop()  operator  acc
            // Make a stack context and spit out the left operand code.
            {
                COutputContext stackContext(context, OC_Stack);
                wTypeLeft = _statement1->OutputByteCode(context).GetType();
            }

            // Then make an acc context and spit out the right operand code.
            {
                COutputContext accContext(context, OC_Accumulator);
                wTypeRight = _statement2->OutputByteCode(context).GetType();
            }
            
            // Then spit out the correct instruction
            // The result will go into the accumulator, and we may need to push it to the stack.
            WriteSimple(context, GetInstructionForBinaryOperator(Operator), GetLineNumber());

            // Type checking
            SpeciesIndex wType = wTypeLeft;
            // Unless this is a boolean operation...
            if (find(BooleanOps.begin(), BooleanOps.end(), Operator) != BooleanOps.end())
            {
                wType = DataTypeBool;
            }
            else
            {
                // There's no reason to prevent operations here. Everything is a number. KQ5CD decompilation hits
                // this in script 55, when a number is multiplied with a boolean operation.
#ifdef NOT_TRUE
                // If this is not a boolean operation, then the types should match.
                if (!DoesTypeMatch(context, wTypeLeft, wTypeRight, &Operator))
                {
                    context.ReportTypeError(this, wTypeLeft, wTypeRight, "'%s' cannot be compared with '%s'.");
                }
#endif
            }

            WORD wBytesUsed = PushToStackIfAppropriate(context, GetLineNumber());
            return CodeResult(wBytesUsed, wType);
        }
    }
}

std::vector<std::pair<UnaryOperator, Opcode>> UnaryOperatorToOpcode =
{
    { UnaryOperator::BinaryNot, Opcode::BNOT },
    { UnaryOperator::LogicalNot, Opcode::NOT },
    { UnaryOperator::Negate, Opcode::NEG },
};

Opcode GetInstructionForUnaryOperator(UnaryOperator op)
{
    auto findIt = std::find_if(UnaryOperatorToOpcode.begin(), UnaryOperatorToOpcode.end(),
        [op](std::pair<UnaryOperator, Opcode> &entry) { return entry.first == op; }
    );
    if (findIt != UnaryOperatorToOpcode.end())
    {
        return findIt->second;
    }
    assert(false);
    return Opcode::INDETERMINATE;
}

UnaryOperator GetUnaryOperatorForInstruction(Opcode opcode)
{
    auto findIt = std::find_if(UnaryOperatorToOpcode.begin(), UnaryOperatorToOpcode.end(),
        [opcode](std::pair<UnaryOperator, Opcode> &entry) { return entry.second == opcode; }
    );
    if (findIt != UnaryOperatorToOpcode.end())
    {
        return findIt->first;
    }
    return UnaryOperator::None;
}

CodeResult UnaryOp::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    CodeResult result;
    if (Operator == UnaryOperator::Increment)
    {
        change_meaning meaning(context, true);
        if (_statement1->GetNodeType() == NodeTypeComplexValue)
        {
            variable_opcode_modifier modifier(this, context, VM_PreIncrement);
            result = _statement1->OutputByteCode(context);
        }
        else
        {
            context.ReportError(this, "Can only increment simple values.");
        }
    }
    else if (Operator == UnaryOperator::Decrement)
    {
        change_meaning meaning(context, true);
        if (_statement1->GetNodeType() == NodeTypeComplexValue)
        {
            variable_opcode_modifier modifier(this, context, VM_PreDecrement);
            result = _statement1->OutputByteCode(context);
        }
        else
        {
            context.ReportError(this, "Can only increment simple values.");
        }
    }
    else
    {
        // bnot, not, neg
        {
            COutputContext accContext(context, OC_Accumulator);
            result = _statement1->OutputByteCode(context);
            WriteSimple(context, GetInstructionForUnaryOperator(Operator), GetLineNumber());
        }
        // Push the acc onto the stack if necessary.
        result = CodeResult(PushToStackIfAppropriate(context, GetLineNumber()), result.GetType()); // Repackage the result with the # of bytes written to the stack
    }
    return result;
}

CodeResult ForLoop::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);
    GetInitializer()->OutputByteCode(context);

    // Store the code pos of the beginning of the condition - we'll increment it to the first instruction later
    code_pos forStart = context.code().get_cur_pos();

    branch_block blockSuccess(context, BranchBlockIndex::Success);
    branch_block blockFailure(context, BranchBlockIndex::Failure);
    branch_block blockBreak(context, BranchBlockIndex::Break);
    // Put the condition into the accumulator
    {
        COutputContext accContext(context, OC_Accumulator);
        _innerCondition->OutputByteCode(context);
    }

    // Make this correct:
    ++forStart;
    assert(forStart != context.code().get_undetermined()); // Make sure Condition wrote something!
    // From here on, continues should work. But we don't know our continue destination yet,
    // so we need to use a branch_block:
    continue_frame_guard blockContinue(context);

    // Time for the bulk of the code
    blockSuccess.leave(); // successful condition jump to here.
    SingleStatementVectorOutputHelper(_segments, context);

    // And finally the looper.
    // First though, leave the continue block. The continue frame will be remove (no more continues allowed),
    // and the next instruction written will end up being the target of any continues encountered while
    // the continue block was active.
    blockContinue.leave();
    _looper->OutputByteCode(context);

    // Now jump back to the test
    context.code().inst(GetLineNumber(), Opcode::JMP, forStart);
    DEBUG_BRANCH(context.code(), DEBUG_FOR_LOOP_LOOP);

    // condition failure (or break) leads to here.
    return 0; // void
}

CodeResult WhileLoop::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);

    // Store the code pos of the beginning of the loop - we'll increment it to the first instruction later
    code_pos whileStart = context.code().get_cur_pos_dangerous();
    bool wasEmpty = context.code().empty();

    branch_block blockSuccess(context, BranchBlockIndex::Success);
    branch_block blockFailure(context, BranchBlockIndex::Failure);
    branch_block blockbreak(context, BranchBlockIndex::Break);
    // Spit out the condition
    {
        COutputContext accContext(context, OC_Accumulator);
        _innerCondition->OutputByteCode(context);
    }

    // Increment this so it points to the first instruction
    if (wasEmpty)
    {
        // It was empty before, but now there should be a body...
        whileStart = context.code().get_beginning();
    }
    else
    {
        ++whileStart;
    }

    assert(whileStart != context.code().get_undetermined()); // Make sure Condition wrote something!
    continue_frame_guard continueFrame(context, whileStart); // This is where continues will jump

    // Now spit out the code
    blockSuccess.leave(); // We're starting the success part of the while loop.
    SingleStatementVectorOutputHelper(_segments, context);

    // Finally, jmp back to the beginning of the while.
    context.code().inst(GetLineNumber(), Opcode::JMP, whileStart);
    // Failures and breaks jump to here...
    return 0; // void
}

CodeResult DoLoop::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);

    // Store the beginning
    code_pos doStart = context.code().get_cur_pos_dangerous();
    bool wasEmpty = context.code().empty();

    // Now spit out the code
    SingleStatementVectorOutputHelper(_segments, context);

    branch_block blockSuccess(context, BranchBlockIndex::Success);
    branch_block blockContinue(context, BranchBlockIndex::Continue);
    branch_block blockFailure(context, BranchBlockIndex::Failure);
    branch_block blockBreak(context, BranchBlockIndex::Break);
    // Now do the test, with result in the acc
    {
        COutputContext accContext(context, OC_Accumulator);
        _innerCondition->OutputByteCode(context);
    }

    // If it's true, then branch back to the beginning
    // Not the most efficient - ideally a condition would be able to jump backward
    // But instead we'll have it jump here, upon which we'll jump back to the start.
    blockSuccess.leave();
    blockContinue.leave();

    if (wasEmpty)
    {
        // It was empty before, but now there should be a body...
        doStart = context.code().get_beginning();
    }
    else
    {
        ++doStart; // Since we want it to branch to the first instruction that was written...
    }

    if (doStart == context.code().get_end())
    {
        context.ReportError(this, "Empty condition statement.");
        --doStart; // Make this code pos valid again, so we don't crash.
    }

    //context.code().inst(Opcode::BT, doStart);
    // Make this more consistent with how the SCI compiler did it:
    context.code().inst(GetLineNumber(), Opcode::JMP, doStart);

    DEBUG_BRANCH(context.code(), DEBUG_DO_LOOP);
    // And failures and breaks will jump here.
    return 0; // void
}

void IfStatement::PreScan(CompileContext &context)
{
    _innerCondition->PreScan(context);
    _statement1->PreScan(context);
    if (_statement2)
    {
        _statement2->PreScan(context);
    }
}

CodeResult IfStatement::OutputByteCode(CompileContext &context) const
{
    CodeResult result;
    // Put result in accumulator.
    {
        COutputContext accContext(context, OC_Accumulator);
        result = _OutputCodeForIfStatement(context, *_innerCondition.get(), *_statement1, _statement2.get(), true);
    }
    WORD wBytes = PushToStackIfAppropriate(context, GetLineNumber());
    return CodeResult(wBytes, result.GetType());
}

void CaseStatementBase::PreScan(CompileContext &context)
{
    if (!_fDefault)
    {
        _statement1->PreScan(context);
    }
    ForwardPreScan2(_segments, context);
}

CodeResult CaseStatement::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);
    assert(false); // Handled in switch.
    return 0;
}

CodeResult SwitchStatement::OutputByteCode(CompileContext &context) const
{
    {
        // We do this (and imbue meaning) to support using switches as values in expressions.
        COutputContext accContext(context, OC_Accumulator);
        declare_conditional isCondition(context, false);
        change_meaning meaning(context, true);
        // Since switch/case statements have different "unknown" branches, we'll do
        // everything within here, instead of within the case object.

        // This is (or will be) a pointer to the instruction that needs to be fixed up to branch to
        // the beginning of the next case.
        code_pos bntToNextCase = context.code().get_undetermined();
        code_pos undefined = bntToNextCase;

        // Start a branch block here... not for the whole switch statement, because we want
        // "far end" branches to go to the TOSS instruction we throw in a the end.
        {
            branch_block block(context);

            // Place the value we're switching on on the stack.
            {
                change_meaning meaning(context, true); // this does have mean...
                COutputContext stackContext(context, OC_Stack);
                _statement1->OutputByteCode(context);
            }

            // Just get some temporary position we can use when writing branch instructions that take
            // us to the beginning of the next case statement (they will be replaced as soon as we figure
            // out where that is)
            code_pos fakeCodePos = context.code().get_cur_pos();

            std::set<uint16_t> caseValues;

            auto caseIndex = _cases.begin();
            auto defaultIndex = _cases.end();
            bool fHasDefault = false;
            while (caseIndex != _cases.end())
            {
                CaseStatement *pCase = (*caseIndex).get();
                if (pCase->IsDefault())
                {
                    if (fHasDefault)
                    {
                        context.ReportError(pCase, "Only one 'default' case is allowed in a switch statement.");
                    }
                    fHasDefault = true;
                    // Store the "default" case off for later.
                    defaultIndex = caseIndex;
                }
                else
                {

                    // For simple values, catch duplication
                    uint16_t numberValue;
                    if (pCase->GetCaseValue()->Evaluate(context, numberValue, nullptr))
                    {
                        if (caseValues.find(numberValue) != caseValues.end())
                        {
                            context.ReportWarning(pCase, "Duplicate case values. Already encountered a case for '%d'", numberValue);
                        }
                        caseValues.insert(numberValue);
                    }

                    //bool fCaseIsEmpty = pCase->GetCodeSegments().empty(); // Optimization for empty case statements.
                    // This is not an appropriate optimization. Skipping a case value that has side effects (e.g. LauraBow1, CB1:handleEvent) will
                    // change the behavior.
                    bool fCaseIsEmpty = false;
                    if (!fCaseIsEmpty)
                    {
                        // For each case statement, dupe the switch value so it can be used in the Opcode::EQ
                        context.code().inst(pCase->GetLineNumber(), Opcode::DUP);

                        if (bntToNextCase != undefined)
                        {
                            // Someone is waiting to branch to the current pos!
                            (*bntToNextCase).set_branch_target(context.code().get_cur_pos(), true); // true = forward
                            // No need to set bntToNextCase to undefined here, since we always set it to something below
                        }

                        // Put the case value into the accumulator
                        const SyntaxNode *caseValue = pCase->GetCaseValue();
                        {
                            change_meaning meaning(context, true); // this does have meaning
                            COutputContext stackContext(context, OC_Accumulator);
                            caseValue->OutputByteCode(context);
                        }

                        // Now compare it to our switch on the stack... is it equal?
                        context.code().inst(pCase->GetLineNumber(), Opcode::EQ);

                        // If not, then branch to just after the case statement.  We don't know where that is
                        // yet, so use the fake pos.
                        context.code().inst(pCase->GetLineNumber(), Opcode::BNT, fakeCodePos);
                        DEBUG_BRANCH(context.code(), DEBUG_SWITCH_NEXT);
                        bntToNextCase = context.code().get_cur_pos(); // Remember the location we need to change

                        // Now output the code of this case
                        SingleStatementVectorOutputHelper(pCase->GetCodeSegments(), context);
                    }
                    if (!fCaseIsEmpty)
                    {
                        auto caseNext = caseIndex + 1;
                        // Optimize out the last jump if we don't have a default case, and this was the last one.
                        if ((caseNext != _cases.end()) || fHasDefault)
                        {
                            // Jmp to the "far end" (e.g. out of the switch).
                            // This will be resolved when we leave our "branch block"
                            context.code().inst(GetLineNumber(), Opcode::JMP, context.code().get_undetermined());
                            DEBUG_BRANCH(context.code(), DEBUG_SWITCH_END);
                        }
                    }
                }
                caseIndex++;
            }
            if (fHasDefault)
            {
                code_pos defaultBegin = context.code().get_cur_pos(); // Not quite...
                // Output the default section.
                SingleStatementVectorOutputHelper((*defaultIndex)->GetCodeSegments(), context);
                ++defaultBegin; // Advance to the first instruction of the default section
                if (defaultBegin != context.code().get_end())
                {
                    // There was at least on instruction in the default section - make the last case branch
                    // to here.
                    if (bntToNextCase != undefined)
                    {
                        (*bntToNextCase).set_branch_target(defaultBegin, true); // true = forward
                        bntToNextCase = undefined; // So we don't use this below...
                    }
                }
            }
        }

        // Now that we've exited our "branch block", write the TOFSS instruction that signals
        // the end of the cases (and also pops our switch off the stack).
        // This should fix up the "far end" jmps in each case
        context.code().inst(GetLineNumber(), Opcode::TOSS);

        // Also, the last case has a branch that needs to point here.
        if (bntToNextCase != undefined)
        {
            // Someone is waiting to branch to the current pos!
            (*bntToNextCase).set_branch_target(context.code().get_cur_pos(), true);
        }
    }
    WORD wBytes = PushToStackIfAppropriate(context, GetLineNumber());
    return CodeResult(wBytes, DataTypeNone);
}

CodeResult CondStatement::OutputByteCode(CompileContext &context) const
{
    // Pass through
    assert(_clausesTemp.empty());
    if (_statement1) { return _statement1->OutputByteCode(context); }
    return 0;
}

void CondStatement::PreScan(CompileContext &context)
{
    // Pass through
    assert(_clausesTemp.empty());
    if (_statement1) {_statement1->PreScan(context); }
}

//
// Returns the code_pos of the begining of the function.
//
code_pos FunctionOutputByteCodeHelper(const FunctionBase &function, CompileContext &context)
{
    bool fFirstFunction = context.code().empty();
    code_pos functionStart = context.code().get_cur_pos_dangerous();
    // functionState points to previous function (or invalid memory if none).

    // Set the variable context so parameters and local variables are accessible.
    variable_lookup_context varContext(context, &function);

    // Supply the return values that are currently allowed
    std::vector<SpeciesIndex> returnTypes;
    for (auto &signature : function.GetSignatures())
    {
        SpeciesIndex si = DataTypeAny;
        context.LookupTypeSpeciesIndex(signature->GetDataType(), si); // 'Ok' if it fails - we would have already recorded the error.
        returnTypes.push_back(si);
    }
    context.SetReturnValueTypes(returnTypes);

    // Figure out how much to add to the stack (how much space temp vars take up)
    // This is the link instruction.
    WORD wWords = 0;
    const VariableDeclVector &tempVars = function.GetVariables();
    for (auto &varDecl : tempVars)
    {
        wWords += varDecl->GetSize();
    }
    if (wWords > 0)
    {
        context.code().inst(function.GetLineNumber(), Opcode::LINK, wWords);
    }

    // Now for any variables that aren't zero, we need to initialize them
    {
        change_meaning meaning(context, true);
        WORD wIndex = 0;
        for (auto &varDecl : tempVars)
        {
            WORD cInitializers = static_cast<WORD>(varDecl->GetInitializers().size());
            assert((cInitializers <= 1) && "Shoudln't hit this because only single var initializers allowed in function");
            if (cInitializers > (size_t)varDecl->GetSize())
            {
                context.ReportError(varDecl.get(), "Too many initializers (%d) for variable '%s'[%d].", cInitializers, varDecl->GetName().c_str(), varDecl->GetSize());
            }
            WORD wIndexWithinThisVar = wIndex;
            for (auto &initializer : varDecl->GetInitializers())
            {
                // Note: originally, I thought all vars were zero-inited, so we could optimize out
                // zero inits, but this is not the case.
                OutputByteCodeToAccumulator(context, *initializer);
                VariableOperand(context, wIndexWithinThisVar, VO_STORE | VO_ACC | VO_TEMP, function.GetLineNumber()); // Store acc in temp var
                wIndexWithinThisVar++;
            }
            wIndex += varDecl->GetSize();
        }
    }

    // Then spit out our segments.
    SingleStatementVectorOutputHelper(function.GetCodeSegments(), context);

    // If the last instruction was not a return statement (or there was no code generated), put one in now.
    bool fDidntWriteAnything = (functionStart == context.code().get_cur_pos_dangerous());
    bool fAllDanglingPrecededByReturn;
    bool fHasDanglingBranches = context.code().has_dangling_branches(fAllDanglingPrecededByReturn);
    if (fDidntWriteAnything ||                                        // No code generated at all, or
        (context.code().get_cur_pos()->get_opcode() != Opcode::RET) ||      // Last instruction was not ret, or
        (fHasDanglingBranches)// Someone is waiting to branch to something (we'll write two rets in a row here)
        )
    {
        // Stick in a ret statement.
        WriteSimple(context, Opcode::RET, function.GetLineNumber());
    }

    // Ensure no one is waiting for a branch (should have been handled by the ret instruction
    // we just output.
#ifdef DEBUG
    bool fDummy;
    assert(!context.code().has_dangling_branches(fDummy));
#endif

    if (fFirstFunction)
    {
        // This was the first bit of code
        functionStart = context.code().get_beginning();
    }
    else
    {
        functionStart++; // Since we set it to the instruction preceding the current function.
        // That implies that we *must* have written something.
    }
    return functionStart;
}

//
// Generate a unique name for a method on an object in this file
//
std::string GetMethodTrackingName(const ClassDefinition *pClass, const FunctionBase &method, bool makeValidFilename)
{
    if (pClass)
    {
        if (makeValidFilename)
        {
            return (pClass->GetName() + "_" + method.GetName());
        }
        else
        {
            return (pClass->GetName() + "::" + method.GetName());
        }
    }
    else
    {
        return method.GetName();
    }
}

void FunctionBase::PruneExtraneousReturn()
{
    if (!_segments.empty())
    {
        auto &lastStatement = _segments.back();
        ReturnStatement *pReturn = SafeSyntaxNode<ReturnStatement>(lastStatement.get());
        if (pReturn)
        {
            if (!pReturn->GetValue())
            {
                // It's a plain return, remove it.
                _segments.pop_back();
            }
        }
    }
}

CodeResult FunctionBase::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);
    code_pos functionStart = FunctionOutputByteCodeHelper(*this, context);
    std::string methodTrackingName = GetMethodTrackingName(_pOwnerClass, *this);
    context.TrackMethod(methodTrackingName, functionStart);
    return 0; // void
}

CodeResult ProcedureDefinition::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, false);
    ClassContext classContext(context, GetOwnerClass());
    code_pos functionStart = FunctionOutputByteCodeHelper(*this, context);
    if (IsPublic())
    {
        context.TrackExport(functionStart);
    }
    context.TrackLocalProc(GetName(), functionStart);
    return 0; // void
}

void ProcedureDefinition::PreScan(CompileContext &context) 
{
    if (!context.PreScanLocalProc(GetName(), GetClass()))
    {
        context.ReportError(this, "'%s' is already defined.", GetName().c_str());
    }
    if (!_class.empty())
    {
        if (_public)
        {
            context.ReportError(this, "Class procedures cannot be public: '%s'.", _class.c_str());
        }
        else
        {
            assert(GetOwnerClass() == nullptr);
            SetOwnerClass(context.LookupClassDefinition(_class));
            if (GetOwnerClass() == nullptr)
            {
                context.ReportError(this, "Can't find class '%s'.", _class.c_str());
            }
        }
    }
    __super::PreScan(context);
}

void VariableDecl::PreScan(CompileContext &context) 
{
    ValidateVariableDeclaration(context, this, _name);
    _size.PreScan(context);
    if (_size.GetType() != ValueType::Number)
    {
        context.ReportError(this, "Array size is not a constant expression: %s", _size.GetStringValue().c_str());
    }
    ForwardPreScan2(_segments, context);
}

void ClassDefDeclaration::PreScan(CompileContext &context) {}
void SelectorDeclaration::PreScan(CompileContext &context) {}

void GlobalDeclaration::PreScan(CompileContext &context)
{
    if (InitialValue)
    {
        InitialValue->PreScan(context);
    }
}
void ExternDeclaration::PreScan(CompileContext &context)
{
    ScriptNumber.PreScan(context);
}

CodeResult VariableDecl::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    change_meaning meaning(context, true);
    // Handled in MethodDefinition
    assert(false);
    return 0;
}

CodeResult BreakStatement::OutputByteCode(CompileContext &context) const
{
    // jmp to the end of the current break block.
    // Report an error if there is none.
    if (context.code().in_branch_block(BranchBlockIndex::Break, Levels))
    {
        context.code().inst(GetLineNumber(), Opcode::JMP, context.code().get_undetermined(), BranchBlockIndex::Break, Levels);
        DEBUG_BRANCH(context.code(), DEBUG_BREAK);
    }
    else
    {
        context.ReportError(this, "A break statement can only be used within a loop.");
    }
    return 0; // void
}

CodeResult ContinueStatement::OutputByteCode(CompileContext &context) const
{
    // Continues may jump backwards or forwards.
    code_pos continueTarget;
    if (context.code().get_continue_target(Levels, continueTarget))
    {
        if (continueTarget == context.code().get_undetermined())
        {
            // This is a jump forward whose target has not yet been determined. This means there
            // should be a branch block for us.
            if (context.code().in_branch_block(BranchBlockIndex::Continue, Levels))
            {
                context.code().inst(GetLineNumber(), Opcode::JMP, context.code().get_undetermined(), BranchBlockIndex::Continue);
                DEBUG_BRANCH(context.code(), DEBUG_CONTINUE);
            }
            else
            {
                assert(false && "internal compiler error");
            }
        }
        else
        {
            // Easy... jump to the backwards target:
            context.code().inst(GetLineNumber(), Opcode::JMP, continueTarget, BranchBlockIndex::Continue, Levels);
        }
    }
    else
    {
        // NOTE: We could get here if there is a continue in a do-loop also, since that is not supported.
        // That should never happen though, since no syntax supports both do-loops and continues.
        context.ReportError(this, "A continue statement can only be used within a loop.");
    }
    return 0; // void
}

CodeResult AsmBlock::OutputByteCode(CompileContext &context) const
{
    for (auto &statement : _segments)
    {
        statement->OutputByteCode(context);
    }
    return CodeResult(0, DataTypeAny);
}

bool IsPropertyOpcode(Opcode opcode)
{
    uint8_t bOpcode = (uint8_t)opcode;
    return ((bOpcode >= 49) && (bOpcode <= 56));
}

CodeResult Asm::OutputByteCode(CompileContext &context) const
{
    // TODO: Prescan needs to track label
    // Look up the instruction.
    bool leaUsesAccIndex = false;
    Opcode opcode = NameToOpcode(_innerName, leaUsesAccIndex);

    if (opcode == Opcode::INDETERMINATE)
    {
        context.ReportError(this, "Unknown instruction '%s'", _innerName.c_str());
    }
    else
    {
        // Special case some opcodes
        switch (opcode)
        {
            case Opcode::JMP:
            case Opcode::BT:
            case Opcode::BNT:
            {
                // These should have a single argument, which is a string token.
                ComplexPropertyValue *pValue = nullptr;
                if (_segments.size() == 1)
                {
                    pValue = SafeSyntaxNode<ComplexPropertyValue>(_segments[0].get());
                }

                if (pValue)
                {
                    std::string label;
                    if (pValue->GetType() == ValueType::Token)
                    {
                        if (context.DoesLabelExist(pValue->GetStringValue()))
                        {
                            label = pValue->GetStringValue();
                        }
                    }
                    if (!label.empty())
                    {
                        context.code().inst(GetLineNumber(), opcode, context.code().get_undetermined());
                        context.TrackAsmLabelReference(pValue->GetStringValue());
                    }
                    else
                    {
                        context.ReportError(this, "Unknown label '%s'", pValue->GetStringValue().c_str());
                    }
                }
                else
                {
                    context.ReportError(this, "Expected a single label value.");
                }
            }
                break;

            case Opcode::CALL:
            case Opcode::CALLB:
            case Opcode::CALLE:
            case Opcode::CALLK:
                // We have a choice here: Do we use symbols or the actual numeric values?
                // Let's assume symbols for now.
                if (_segments.size() == 2)
                {
                    ComplexPropertyValue *pNumParams = SafeSyntaxNode<ComplexPropertyValue>(_segments[1].get());
                    if (pNumParams->GetType() == ValueType::Number)
                    {
                        // The first argument should be a name
                        ComplexPropertyValue *pValue = SafeSyntaxNode<ComplexPropertyValue>(_segments[0].get());
                        if (pValue->GetType() == ValueType::Token)
                        {
                            uint16_t wScript, wIndex;
                            std::string classOwner;
                            ProcedureType procType = context.LookupProc(pValue->GetStringValue(), wScript, wIndex, classOwner);
                            if ((procType == ProcedureKernel) && (opcode == Opcode::CALLK))
                            {
                                context.code().inst(GetLineNumber(), opcode, wIndex, pNumParams->GetNumberValue());
                            }
                            else if ((procType == ProcedureMain) && (opcode == Opcode::CALLB))
                            {
                                context.code().inst(GetLineNumber(), opcode, wIndex, pNumParams->GetNumberValue());
                            }
                            else if ((procType == ProcedureLocal) && (opcode == Opcode::CALL))
                            {
                                context.code().inst(GetLineNumber(), opcode, wIndex, pNumParams->GetNumberValue());
                                context.TrackLocalProcCall(pValue->GetStringValue());
                            }
                            else if ((procType == ProcedureExternal) && (opcode == Opcode::CALLE))
                            {
                                context.code().inst(GetLineNumber(), opcode, wScript, wIndex, pNumParams->GetNumberValue());
                            }
                            else
                            {
                                context.ReportError(this, "Procedure type does not match call type.");
                            }
                        }
                        else
                        {
                            context.ReportError(this, "Expected procedure name: '%s'", pValue->GetStringValue().c_str());
                        }
                    }
                    else
                    {
                        context.ReportError(this, "Expected a number for the second argument.");
                    }

                }
                else
                {
                    context.ReportError(this, "Expected 2 arguments.");
                }
                break;


            case Opcode::LEA:
                if (!_segments.empty())
                {
                    ComplexPropertyValue *pValue = SafeSyntaxNode<ComplexPropertyValue>(_segments[0].get());
                    SpeciesIndex wType;
                    sci::SyntaxNode *indexer = pValue->GetIndexer();
                    if (indexer)
                    {
                        context.ReportError(this, "An indexer is not allowed.");
                    }
                    std::unique_ptr<Comment> noop;
                    if (leaUsesAccIndex)
                    {
                        // This is an indexed LEA. Use a comment for the index, so that we detect that it's indexed,
                        // but leave the previous value in the accumulator (i.e. don't write anything new to the acc)
                        noop = std::make_unique<Comment>(CommentType::None);
                    }
                    LookupTokenAndPlacePtrInAccumulator(pValue->GetStringValue(), context, noop.get(), pValue, wType);
                }
                else
                {
                    context.ReportError(this, "Not enough arguments for this opcode.");
                }
                break;

            default:
            {
                uint16_t args[3] = {};
                for (size_t i = 0; i < 3; i++)
                {
                    OperandType ot = GetOperandTypes(context.GetVersion(), opcode)[i];
                    if (ot != otEMPTY)
                    {
                        if (_segments.size() >= (i + 1))
                        {
                            // Special cases: jmp, bnt, bt. lofsa, call*
                            ComplexPropertyValue *pValue = SafeSyntaxNode<ComplexPropertyValue>(_segments[i].get());
                            if (pValue)
                            {
                                switch (pValue->GetType())
                                {
                                    case ValueType::Number:
                                        args[i] = pValue->GetNumberValue();
                                        break;

                                    case ValueType::String:
                                    case ValueType::ResourceString:
                                        args[i] = context.GetTempToken(ValueType::String, pValue->GetStringValue());
                                        break;

                                    case ValueType::Said:
                                        args[i] = context.GetTempToken(ValueType::Said, pValue->GetStringValue());
                                        break;

                                    case ValueType::Selector:
                                    {
                                        uint16_t selectorNum;
                                        if (context.LookupSelector(pValue->GetStringValue(), selectorNum))
                                        {
                                            args[i] = selectorNum;
                                        }
                                        else
                                        {
                                            context.ReportError(this, "Unknown property or method: '%s'.", pValue->GetStringValue().c_str());
                                        }
                                    }
                                    break;

                                    case ValueType::Pointer:
                                    {
                                        context.ReportError(this, "Pointer syntax is not valid for this opcode.");
                                    }
                                        break;

                                    case ValueType::Token:
                                    {
                                        // REVIEW: We have additional restrictions here. We'll need special code to handle some cases.
                                        // For instance, lofsa only makes sense with a class/instance/string. A load/store for a global var only
                                        // makes sense with a global var. calle only makes sense with a public proc name. And in that case, two of
                                        // the operands are involved.
                                        uint16_t wInstanceScript;
                                        uint16_t number;
                                        SpeciesIndex wType;
                                        ResolvedToken tokenType = context.LookupToken(pValue, pValue->GetStringValue(), number, wType, &wInstanceScript);
                                        switch (tokenType)
                                        {
                                            case ResolvedToken::Instance:
                                            case ResolvedToken::Class:
                                                args[i] = number;
                                                if ((opcode == Opcode::LOFSA) || (opcode == Opcode::LOFSS))
                                                {
                                                    // A little confused here...
                                                    args[i] = context.GetTempToken(ValueType::Token, pValue->GetStringValue());
                                                }
                                                else
                                                {
                                                   // assert(false);
                                                }
                                                break;

                                            case ResolvedToken::ScriptString:
                                            {
                                                args[i] = context.GetTempToken(ValueType::String, context.GetScriptStringFromToken(pValue->GetStringValue()));
                                                WORD wImmediateIndex = 0;
                                                if (pValue->GetIndexer())
                                                {
                                                    if (!CanDoIndexOptimization(pValue->GetIndexer(), wImmediateIndex))
                                                    {
                                                        context.ReportError(pValue->GetIndexer(), "Expected an integer for the index.");
                                                    }
                                                }
                                            }
                                            break;

                                            case ResolvedToken::ClassProperty:
                                            {
                                                if (IsPropertyOpcode(opcode))
                                                {
                                                    args[0] = number;
                                                }
                                                else
                                                {
                                                    context.ReportError(pValue, "Invalid opcode to use with property '%s'.", pValue->GetStringValue().c_str());
                                                }
                                            }
                                            break;

                                            case ResolvedToken::Parameter:
                                            case ResolvedToken::GlobalVariable:
                                            case ResolvedToken::TempVariable:
                                            case ResolvedToken::ScriptVariable:
                                            {
                                                uint8_t variableOpType = TokenTypeToVOType(tokenType);
                                                uint8_t bOpcode = (uint8_t)opcode;
                                                uint8_t opcodeOpType = (bOpcode & VO_TYPEMASK);
                                                if (opcode == Opcode::REST)
                                                {
                                                    opcodeOpType = VO_PARAM;
                                                }
                                                if (context.GetScriptNumber() == 0)
                                                {
                                                    // Ok if local/global used interchangeably in main.
                                                    if (opcodeOpType == VO_GLOBAL)
                                                    {
                                                        opcodeOpType = VO_LOCAL;
                                                    }
                                                    if (variableOpType == VO_GLOBAL)
                                                    {
                                                        variableOpType = VO_LOCAL;
                                                    }
                                                }
                                                if (opcodeOpType != variableOpType)
                                                {
                                                    context.ReportError(pValue, "Opcode is '%s', but variable is '%s'.", VOTypeToString(bOpcode & VO_TYPEMASK), VOTypeToString(variableOpType));
                                                }
                                                args[0] = number;
                                            }
                                                break;

                                            case ResolvedToken::Unknown:
                                            {
                                                context.ReportError(pValue, "Unknown token '%s'.", pValue->GetStringValue().c_str());
                                            }
                                            break;

                                            default:
                                                context.ReportError(this, "Unimplemented token type: '%d'.", tokenType);
                                        }
                                    }
                                    break;
                                }
                            }
                            else
                            {
                                context.ReportError(this, "Unable to process argument %d for '%s'", i, _innerName.c_str());
                            }
                        }
                        else
                        {
                            context.ReportError(this, "'%s' requires %d arguments.", _innerName.c_str(), (i + 1));
                        }
                    }
                    else
                    {
                        if (_segments.size() > i)
                        {
                            context.ReportError(this, "Too many arguments for '%s'.", _innerName.c_str());
                        }
                    }
                }
                context.code().inst(GetLineNumber(), opcode, args[0], args[1], args[2]);
            }
        }
    }

    if (!_label.empty())
    {
        context.TrackAsmLabelLocation(_label);
    }

    return CodeResult(0, DataTypeAny);
}


CodeResult RestStatement::OutputByteCode(CompileContext &context) const
{
    declare_conditional isCondition(context, false);
    if (context.GetOutputContext() != OC_Stack)
    {
        context.ReportError(this, "The rest modifier cannot be used here.");
    }
    else
    {
        WORD wNumber;
        SpeciesIndex wType;
        std::string lookupName = _innerName.empty() ? g_restLastParamSentinel : _innerName;
        ResolvedToken tokenType = context.LookupToken(this, lookupName, wNumber, wType);
        if (tokenType == ResolvedToken::Parameter)
        {
            context.code().inst(GetLineNumber(), Opcode::REST, wNumber);
        }
        else
        {
            context.ReportError(this, "The rest modifier must be followed by a parameter name.");
        }
    }
    // The rest instruction is special - it adjusts the rest modifier, and the number of params it pushes
    // onto the stack does not need to be considered.
    // REVIEW: We could do stronger type checking here, instead of DataTypeAny.
    return CodeResult(0, DataTypeAny);
}

void ClassDefinition::PreScan(CompileContext &context) 
{
    if (IsSCIKeyword(context.GetLanguage(), _innerName))
    {
        // Can't use keywords as class names.
        ReportKeywordError(context, this, _innerName, "procedure name");
    }

    // Notify about our name
    context.GetTempToken(ValueType::String, _innerName);

    ForwardPreScan2(_properties, context);
    ForwardPreScan2(_methods, context);

    // For error-checking.
    for (auto &method : _methods)
    {
        if (!context.ScanObjectMethod(GetName(), method->GetName()))
        {
            context.ReportWarning(method.get(), "'%s' is already defined on '%s'.", method->GetName().c_str(), GetName().c_str());
        }
    }

    std::set<std::string> propertyNames;
    for (auto &prop : _properties)
    {
        if (propertyNames.find(prop->GetName()) != propertyNames.end())
        {
            context.ReportError(prop.get(), "Duplicate property name: %s", prop->GetName().c_str());
        }
        propertyNames.insert(prop->GetName());
    }
}

CodeResult ClassDefinition::OutputByteCode(CompileContext &context) const
{
    // We need to prepare a stream of data for the class or instance, and provide this to the context.

    // Need to figure out how to track this stuff... we could have properties, saids, etc.., but they're not
    // part of an instruction.  Should we not do the "Track said with instruction" thing?
    // Maybe instead, we should just "addstring, addsaid", etc... (which would return an index), and then associate

    // OK, here's the deal: tracking string and said offsets (not call offsets), so the lofsa and lofss instruction,
    // it makes no sense to directly track the strings, and point back to instructions.  Instead, before writing a
    // lofsa or lofss instruction, we should call TrackSaid or TrackString, and this will return a unique index.
    // We store this index.  When writing the code, we can then "update it" somehow.  Get the index we stored
    // in the instruction, and replace with the real offset.
    // When we write the strings, we'll keep track of where they went in the script resource (so associate index with
    // offset)
    // Now, we also need to know which properties in the instance are actually strings/saids.  So we track said/string
    // to get an index too, and stash that somewhere.  Need to figure this out.  Sure, we'll have a vector of offsets
    // into our raw classdata, and transform each of those into whatevers.

    // Output the code of the methods, in order.
    ClassContext classContext(context, this);
    for_each(_methods.begin(), _methods.end(), GenericOutputByteCode2<MethodDefinition>(context));
    return 0;
}

ResolvedToken ClassDefinition::LookupVariableName(CompileContext &context, const std::string &str, WORD &wIndex, SpeciesIndex &dataType) const
{
    ResolvedToken token = ResolvedToken::Unknown;
    std::vector<species_property> speciesProps = context.GetSpeciesProperties(_fInstance ? _superClass : _innerName);
    WORD wSelectorIndex;
    if (context.LookupSelector(str, wSelectorIndex))
    {
        wIndex = 0;
        std::vector<species_property>::const_iterator propIt = speciesProps.begin();
        while (propIt != speciesProps.end())
        {
            if (propIt->wSelector == wSelectorIndex)
            {
                wIndex *= 2; // Just the way it is.
                token = ResolvedToken::ClassProperty;
                dataType = propIt->wType;
                break;
            }
            wIndex++;
            propIt++;
        }
    }
    return token;
}

//
// This doesn't output byte code
//
vector<uint16_t> VariableDecl::GetSimpleValues() const
{
    vector<uint16_t> result;
    for (auto &value : _segments)
    {
        const PropertyValue *pValue = SafeSyntaxNode<PropertyValue>(value.get());
        assert(pValue); // Must be a property value if we're calling this.
        result.push_back(pValue->GetNumberValue());
    }
    return result;
}

void FunctionSignature::PreScan(CompileContext &context)
{
    // Ensure none of the parameters have the same name.
    std::set<std::string> _paramNames;
    for (auto &param : _params)
    {
        if (_paramNames.find(param->GetName()) != _paramNames.end())
        {
            context.ReportError(param.get(), "%s %s: duplicate parameter names.", param->GetDataType().c_str(), param->GetName().c_str());
        }
        else
        {
            _paramNames.insert(param->GetName());
        }
    }
}

// Some decompiled scripts (e.g. QFG1) have "use" as a method name. Let's allow this.
std::vector<std::string> keywordException =
{
    "use"
};

void FunctionBase::PreScan(CompileContext &context)
{
    if (IsSCIKeyword(context.GetLanguage(), GetName()) && (std::find(keywordException.begin(), keywordException.end(), GetName()) == keywordException.end()))
    {
        // Can't use keywords as procedure names. Top level names are ok.
        ReportKeywordError(context, this, GetName(), "function name");
    }

    context.FunctionBaseForPrescan = this;

    std::set<std::string> parameterCollection; // Collect parameters
    for (auto &signature : _signatures)
    {
        for (auto &param : signature->GetParams())
        {
            parameterCollection.insert(param->GetName());
        }
    }

    std::set<std::string> tempVarCollection; // Collect temp variables
    const VariableDeclVector &scriptVars = GetOwnerScript()->GetScriptVariables();
    for (auto &variable : _tempVars)
    {
        // Find the script.
        if (matches_name(scriptVars.begin(), scriptVars.end(), variable->GetName()))
        {
            context.ReportError(variable.get(), "Function variables cannot have the same name as script variables: %s", variable->GetName().c_str());
        }
        if (parameterCollection.find(variable->GetName()) != parameterCollection.end())
        {
            context.ReportError(variable.get(), "Function variables cannot have the same name as a function parameter: %s", variable->GetName().c_str());
        }
        if (tempVarCollection.find(variable->GetName()) != tempVarCollection.end())
        {
            context.ReportError(variable.get(), "There is already a variable called '%s'", variable->GetName().c_str());
        }
        tempVarCollection.insert(variable->GetName());
    }
    ForwardPreScan2(_tempVars, context);
    ForwardPreScan2(_segments, context);
    ForwardPreScan2(_signatures, context);

    // TODO: emit warning when a local variable hides a class member.
    context.FunctionBaseForPrescan = nullptr;
}
void CodeBlock::PreScan(CompileContext &context)
{
    ForwardPreScan2(_segments, context);
}
void NaryOp::PreScan(CompileContext &context)
{
    ForwardPreScan2(_segments, context);
}

void SendCall::PreScan(CompileContext &context)
{
    // One of three forms of send params.
    ForwardPreScan2(_params, context);
    if (GetTargetName().empty())
    {
        if (!_object3 || _object3->GetName().empty())
        {
            _statement1->PreScan(context);
        }
        else
        {
            _object3->PreScan(context);
        }
    }
}
void ProcedureCall::PreScan(CompileContext &context)
{
    if (IsSCIKeyword(context.GetLanguage(), _innerName))
    {
        // Can't use keywords as procedure calls.
        ReportKeywordError(context, this, _innerName, "procedure call");
    }
    ForwardPreScan2(_segments, context);
}
void ConditionalExpression::PreScan(CompileContext &context)
{
    ForwardPreScan2(_segments, context);
}
void SwitchStatement::PreScan(CompileContext &context)
{
    _statement1->PreScan(context);
    ForwardPreScan2(_cases, context);
}
void ForLoop::PreScan(CompileContext &context) 
{
    GetInitializer()->PreScan(context);
    _innerCondition->PreScan(context);
    _looper->PreScan(context);
    ForwardPreScan2(_segments, context);
}
void WhileLoop::PreScan(CompileContext &context) 
{
    _innerCondition->PreScan(context);
    ForwardPreScan2(_segments, context);
}

void ExportEntry::PreScan(CompileContext &context) {}

void Script::TrackGenText(CompileContext &context)
{
    if (_genTextValue)
    {
        _genTextValue->PreScan(context);
        if (_genTextValue->GetType() == ValueType::Number)
        {
            context.SetAutoText(_genTextValue->GetNumberValue());
        }
        else
        {
            context.ReportError(_genTextValue.get(), "Can't resolve '%s' to a number.", _genTextValue->GetStringValue().c_str());
        }
    }
}


void Script::PreScan(CompileContext &context)
{
    ForwardPreScan2(_defines, context);

    //
    ForwardPreScan2(Globals, context);
    ForwardPreScan2(Externs, context);
    ForwardPreScan2(Selectors, context);
    ForwardPreScan2(ClassDefs, context);

    std::set<int> slots;
    for (auto &theExport : _exports)
    {
        if (slots.find(theExport->Slot) != slots.end())
        {
            context.ReportError(theExport.get(), "Export slot %d has already been used.", theExport->Slot);
        }
        else
        {
            slots.insert(theExport->Slot);
        }

        // Ensure this matches a public object or procedure
        const ClassDefinition *classDef = context.LookupClassDefinition(theExport->Name);
        const ISourceCodePosition *notPublicError = nullptr;
        if (classDef)
        {
            notPublicError = classDef->IsPublic() ? nullptr : classDef;
        }
        else
        {
            const ProcedureDefinition *procFound = nullptr;
            for (const auto &proc : _procedures)
            {
                if (proc->GetName() == theExport->Name)
                {
                    procFound = proc.get();
                    break;
                }
            }
            if (procFound == nullptr)
            {
                context.ReportError(theExport.get(), "Unknown export %s in slot %d.", theExport->Name.c_str(), theExport->Slot);
            }
            else
            {
                notPublicError = procFound->IsPublic() ? nullptr : procFound;
            }
        }

        if (notPublicError)
        {
            context.ReportError(notPublicError, "%s needs to be marked public in order to be exported.", theExport->Name.c_str());
        }
    }

    // Look for duplicate class names
    std::set<std::string> classNames;
    for (auto &theClass : _classes)
    {
        if (classNames.find(theClass->GetName()) != classNames.end())
        {
            context.ReportError(theClass.get(), "There is already a class called '%s' in this script.", theClass->GetName().c_str());
        }
        classNames.insert(theClass->GetName());
    }

    ForwardPreScan2(_procedures, context);
    ForwardPreScan2(_classes, context);


    // Check for duplicate local variable names.
    std::set<std::string> scriptVarNames;
    for (auto &scriptVar : _scriptVariables)
    {
        if (scriptVarNames.find(scriptVar->GetName()) != scriptVarNames.end())
        {
            context.ReportError(scriptVar.get(), "There is already a local variable called '%s' in this script.", scriptVar->GetName().c_str());
        }
        scriptVarNames.insert(scriptVar->GetName());
    }
    ForwardPreScan2(_scriptVariables, context);

    // We need to do special stuff with these, we can't just forward.
    // Here is where we turn them into strings.
    for (auto &stringDecl : _scriptStringDeclarations)
    {
        _PreScanStringDeclaration(context, *stringDecl);
    }
}

void Script::_PreScanStringDeclaration(CompileContext &context, VariableDecl &stringDecl)
{
    string finalString;
    // Build the string up from any initializers.
    for (auto &initializer : stringDecl.GetInitializers())
    {
        PropertyValue *pValue = SafeSyntaxNode<PropertyValue>(initializer.get());
        assert(pValue); // Must be a property value if we're calling this.

        // We can't just "PreScan" the variable declaration, because it will add any strings it finds to the "in code strings",
        // which we don't want. So we emulate a bit of PropertyValueBase::PreScan here.
        ValueType type = pValue->GetType();
        if ((type == ValueType::String) || (type == ValueType::ResourceString))
        {
            for (auto &character : pValue->GetStringValue())
            {
                finalString.push_back(character);
            }
        }
        else if ((type == ValueType::Token) || (type == ValueType::Number))
        {
            WORD wNumber = pValue->GetNumberValue();
            if (type == ValueType::Token)
            {
                // We don't actually need to store this back in the PropertyValue, we'll just use it right now.
                if (!_PreScanPropertyTokenToNumber(context, pValue, pValue->GetStringValue(), wNumber))
                {
                    context.ReportError(pValue, "Unknown token %s.", pValue->GetStringValue().c_str());
                }
            }
            if (wNumber > 127)
            {
                context.ReportWarning(pValue, "Value %s is %d and will be truncated to a single char (< 128)", pValue->GetStringValue().c_str(), wNumber);
            }
            else
            {
                finalString.push_back((char)wNumber);
            }
        }
        else
        {
            assert(false); // Compiler error if we get here. Should have filtered stuff out in parsing.
        }
    }

    if (!stringDecl.IsUnspecifiedSize())
    {
        size_t stringLengthWithNull = finalString.length() + 1;
        size_t statedSize = stringDecl.GetSize();
        if (stringLengthWithNull <= statedSize)
        {
            // Fill it up with nulls
            finalString.append(statedSize - stringLengthWithNull, '\0');
        }
        else
        {
            context.ReportError(&stringDecl, "String size is %d, but requires %d characters.", statedSize, stringLengthWithNull);
        }
    }
    // else we're fine with whatever.

    // Replace the value in the initializer:
    stringDecl.GetStatements().clear();
    stringDecl.AddSimpleInitializer(PropertyValue(finalString, ValueType::String));
}

void DoLoop::PreScan(CompileContext &context) 
{
    _innerCondition->PreScan(context);
    ForwardPreScan2(_segments, context);
}
void Assignment::PreScan(CompileContext &context)
{
    _variable->PreScan(context);
    _statement1->PreScan(context);
}

void AsmBlock::PreScan(CompileContext &context)
{
    ForwardPreScan2(_segments, context);
}
void Asm::PreScan(CompileContext &context)
{
    if (!_label.empty())
    {
        context.ReportLabelName(this, _label);
    }
    ForwardPreScan2(_segments, context);
}

void WeakSyntaxNode::PreScan(CompileContext &context)
{
    assert(false);
}
CodeResult WeakSyntaxNode::OutputByteCode(CompileContext &context) const
{
    if (WeakNode) { return WeakNode->OutputByteCode(context); }
    return 0;
}

// Converts a flat list of statements and andOrs into a tree of binary operations.
void ConvertAndOrSequenceIntoTree(sci::SyntaxNodeVector &statements, std::vector<bool> andOrs)
{
    assert((andOrs.size() + 1) == statements.size());

    // First, let's group the &&'s together. 
    for (size_t i = 0; i < andOrs.size();)
    {
        if (andOrs[i])
        {
            unique_ptr<BinaryOp> binaryOp = make_unique<BinaryOp>();
            binaryOp->Operator = BinaryOperator::LogicalAnd;
            binaryOp->SetStatement1(move(statements[i]));
            binaryOp->SetStatement2(move(statements[i + 1]));
            statements.erase(statements.begin() + i);
            andOrs.erase(andOrs.begin() + i);
            assert(!statements[i]);
            statements[i] = move(binaryOp);
        }
        else
        {
            i++;
        }
    }

    // Now we should only have ||'s left
    for (size_t i = 0; i < andOrs.size();)
    {
        assert(!andOrs[i]);
        unique_ptr<BinaryOp> binaryOp = make_unique<BinaryOp>();
        binaryOp->Operator = BinaryOperator::LogicalOr;
        binaryOp->SetStatement1(move(statements[i]));
        binaryOp->SetStatement2(move(statements[i + 1]));
        statements.erase(statements.begin() + i);
        andOrs.erase(andOrs.begin() + i);
        assert(!statements[i]);
        statements[i] = move(binaryOp);
    }

    assert(statements.size() == 1);
    assert(andOrs.empty());
}

void TrackArraySizes(CompileContext &context, sci::Script &script)
{
    for (auto &varDecl : script.GetScriptVariables())
    {
        if (varDecl->IsUnspecifiedSize())
        {
            // It's pretty straightforward... the size is the number of initializers
            // *unless there is a resource string*
            uint16_t autoText;
            uint16_t size = 0;
            if (context.IsAutoText(autoText))
            {
                for (const auto &initializer : varDecl->GetInitializers())
                {
                    if ((initializer->GetNodeType() == NodeTypeValue) || (initializer->GetNodeType() == NodeTypeComplexValue))
                    {
                        PropertyValueBase &propValue = static_cast<PropertyValueBase&>(*initializer);
                        if (propValue.GetType() == ValueType::ResourceString)
                        {
                            size++; // An additional one...
                        }
                    }
                    size++;
                }
            }
            else
            {
                size = (uint16_t)varDecl->GetInitializers().size();
            }
            // And at least one.
            size = max(1, size);
            context.ScriptArraySizes[varDecl->GetName()] = size;
            varDecl->SetSize(size);
        }
        else
        {
            // Need to pre-scan varDecl first, in case it has a define that needs to be resolved.
            varDecl->PreScan(context);
            context.ScriptArraySizes[varDecl->GetName()] = varDecl->GetSize();
        }
    }
}

void MaybeSubstituteWithSimpleValue(CompileContext &context, std::unique_ptr<SyntaxNode> &node)
{
    if (!node || (node->GetNodeType() == NodeTypeValue) || (node->GetNodeType() == NodeTypeComplexValue))
    {
        // These are already just values...
        return;
    }
    uint16_t result;
    if (node->Evaluate(context, result, nullptr))
    {
        node = std::make_unique<PropertyValue>(result);
    }
}

class EvaluateConstantExpressionsHelper : public sci::IExploreNode
{
public:
    EvaluateConstantExpressionsHelper(CompileContext &context) : _context(context) {}

private:
    void ExploreNode(sci::SyntaxNode &node, sci::ExploreNodeState state)
    {
        if (state == sci::ExploreNodeState::Pre)
        {
            StatementsNode *statements = dynamic_cast<StatementsNode*>(&node);
            if (statements)
            {
                for (std::unique_ptr<SyntaxNode> &statement : statements->GetStatements())
                {
                    MaybeSubstituteWithSimpleValue(_context, statement);
                }
            }

            OneStatementNode *oneStatement = dynamic_cast<OneStatementNode*>(&node);
            if (oneStatement)
            {
                MaybeSubstituteWithSimpleValue(_context, oneStatement->GetStatement1Internal());
            }

            TwoStatementNode *twoStatement = dynamic_cast<TwoStatementNode*>(&node);
            if (twoStatement)
            {
                MaybeSubstituteWithSimpleValue(_context, twoStatement->GetStatement2Internal());
            }

        }
    }

    CompileContext &_context;
};

void EvaluateConstantExpressions(CompileContext &context, sci::Script &script)
{
    // Replace any constant expressions with their evaluated form.
    // REVIEW: This could cause problems if we provided weakptrs to certain objects to the compile context.
    EvaluateConstantExpressionsHelper evaluate(context);
    script.Traverse(evaluate);
}