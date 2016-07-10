#include "parser.h"

#include "scope.h"
#include "error.h"
#include "type.h"

#include <set>
#include <string>

using namespace std;


/*
 * Allocation
 */
ConditionalOp* Parser::NewConditionalOp(Expr* cond,
        Expr* exprTrue, Expr* exprFalse)
{
    auto ret = new (_conditionalOpPool.Alloc())
            ConditionalOp(&_conditionalOpPool, cond, exprTrue, exprFalse);

    ret->TypeChecking(_coord);
    return ret;
}

BinaryOp* Parser::NewBinaryOp(int op, Expr* lhs, Expr* rhs)
{
    switch (op) {
    case '=': 
    case '[':
    case '*':
    case '/':
    case '%':
    case '+':
    case '-':
    case '&':
    case '^':
    case '|':
    case '<':
    case '>':
    case Token::LEFT_OP:
    case Token::RIGHT_OP:
    case Token::LE_OP:
    case Token::GE_OP:
    case Token::EQ_OP:
    case Token::NE_OP: 
    case Token::AND_OP:
    case Token::OR_OP:
        break;

    default:
        assert(0);
    }

    auto ret = new (_binaryOpPool.Alloc())
            BinaryOp(&_binaryOpPool, op, lhs, rhs);
    
    ret->TypeChecking(_coord);
    
    return ret;
}

BinaryOp* Parser::NewMemberRefOp(int op, Expr* lhs, const std::string& rhsName)
{
    assert('.' == op || Token::PTR_OP == op);
    
    //the initiation of rhs is lefted in type checking
    auto ret = new (_binaryOpPool.Alloc())
            BinaryOp(&_binaryOpPool, op, lhs, nullptr);
    
    ret->MemberRefOpTypeChecking(_coord, rhsName);

    return ret;
}

/*
UnaryOp* Parser::NewUnaryOp(Type* type, int op, Expr* expr) {
    return new UnaryOp(type, op, expr);
}
*/


FuncCall* Parser::NewFuncCall(Expr* designator, const std::list<Expr*>& args)
{
    auto ret = new (_funcCallPool.Alloc())
            FuncCall(&_funcCallPool, designator, args);

    ret->TypeChecking(_coord);
    
    return ret;
}


Identifier* Parser::NewIdentifier(Type* type,
        Scope* scope, enum Linkage linkage)
{
    auto ret = new (_identifierPool.Alloc())
            Identifier(&_identifierPool, type, scope, linkage);

    return ret;
}

Object* Parser::NewObject(Type* type, Scope* scope,
        int storage, enum Linkage linkage, int offset)
{
    auto ret = new (_objectPool.Alloc())
            Object(&_objectPool, type, scope, storage, linkage, offset);
    
    ret->TypeChecking(_coord);
    return ret;
}

Constant* Parser::NewConstantInteger(ArithmType* type, long long val)
{
    auto ret = new (_constantPool.Alloc()) Constant(&_constantPool, type, val);

    return ret;
}

Constant* Parser::NewConstantFloat(ArithmType* type, double val)
{
    auto ret = new (_constantPool.Alloc()) Constant(&_constantPool, type, val);

    return ret;
}


TempVar* Parser::NewTempVar(Type* type)
{
    auto ret = new (_tempVarPool.Alloc()) TempVar(&_tempVarPool, type);

    return ret;
}

UnaryOp* Parser::NewUnaryOp(int op, Expr* operand, Type* type)
{
    auto ret = new (_unaryOpPool.Alloc())
            UnaryOp(&_unaryOpPool, op, operand, type);
    
    ret->TypeChecking(_coord);

    return ret;
}


/********** Statement ***********/

//��Ȼ��stmtֻ��Ҫһ��
EmptyStmt* Parser::NewEmptyStmt(void)
{
    auto ret = new (_emptyStmtPool.Alloc()) EmptyStmt(&_emptyStmtPool);

    return ret;
}

//else stmt Ĭ���� null
IfStmt* Parser::NewIfStmt(Expr* cond, Stmt* then, Stmt* els)
{
    auto ret = new (_ifStmtPool.Alloc()) IfStmt(&_ifStmtPool, cond, then, els);

    return ret;
}

CompoundStmt* Parser::NewCompoundStmt(std::list<Stmt*>& stmts)
{
    auto ret = new (_compoundStmtPool.Alloc())
            CompoundStmt(&_compoundStmtPool, stmts);

    return ret;
}

JumpStmt* Parser::NewJumpStmt(LabelStmt* label)
{
    auto ret = new (_jumpStmtPool.Alloc()) JumpStmt(&_jumpStmtPool, label);

    return ret;
}

ReturnStmt* Parser::NewReturnStmt(Expr* expr)
{
    auto ret = new (_returnStmtPool.Alloc())
            ReturnStmt(&_returnStmtPool, expr);

    return ret;
}

LabelStmt* Parser::NewLabelStmt(void)
{
    auto ret = new (_labelStmtPool.Alloc()) LabelStmt(&_labelStmtPool);

    return ret;
}

FuncDef* Parser::NewFuncDef(FuncType* type, CompoundStmt* stmt)
{
    auto ret = new (_funcDefPool.Alloc()) FuncDef(&_funcDefPool, type, stmt);
    
    return ret;
}

void Parser::Delete(ASTNode* node)
{
    if (node == nullptr)
        return;

    MemPool* pool = node->_pool;
    node->~ASTNode();
    pool->Free(node);
}


/*
 * Recursive descent parser
 */
void Parser::Expect(int expect, int follow1, int follow2)
{
    auto tok = Next();
    if (tok->Tag() != expect) {
        PutBack();
        //TODO: error
        Error(tok->Coord(), "'%s' expected, but got '%s'",
                Token::Lexeme(expect), tok->Str().c_str());
        Panic(follow1, follow2);
    }
}

void Parser::EnterFunc(const char* funcName) {
    //TODO: 添加编译器自带的 __func__ 宏
}

void Parser::ExitFunc(void) {
    //TODO: resolve 那些待定的jump；
	//TODO: 如果有jump无法resolve，也就是有未定义的label，报错；
    for (auto iter = _unresolvedJumps.begin();
            iter != _unresolvedJumps.end(); iter++) {
        
        auto labelStmt = FindLabel(iter->first);
        if (nullptr == labelStmt) {
            // TODO(wgtdkp):
            //Error("unresolved label '%s'", iter->first);
        }
            
        
        iter->second->SetLabel(labelStmt);
    }
    
    _unresolvedJumps.clear();	//清空未定的 jump 动作
    _curLabels.clear();	//清空 label map
}

void Parser::ParseTranslationUnit(void)
{
    while (!Peek()->IsEOF()) {
        if (IsFuncDef()) {
            _unit->Add(ParseFuncDef());
        } else {
            _unit->Add(ParseDecl());
        }
    }

    _externalSymbols->Print();
}

Expr* Parser::ParseExpr(void)
{
    return ParseCommaExpr();
}

Expr* Parser::ParseCommaExpr(void)
{
    auto lhs = ParseAssignExpr();
    while (Try(',')) {
        auto rhs = ParseAssignExpr();
        lhs = NewBinaryOp(',', lhs, rhs);
    }
    return lhs;
}

Expr* Parser::ParsePrimaryExpr(void)
{
    if (Peek()->IsKeyWord()) //can never be a expression
        return nullptr;

    auto tok = Next();
    if (tok->IsEOF())
        return nullptr;

    if (tok->Tag() == '(') {
        auto expr = ParseExpr();
        Expect(')');
        return expr;
    }

    if (tok->IsIdentifier()) {
        // TODO(wgtdkp): create a expression node with symbol
        auto ident = _curScope->Find(tok->Str());
        /* if (ident == nullptr || ident->ToObject() == nullptr) { */
        if (ident == nullptr) {
            Error(tok->Coord(), "undefined symbol '%s'", 
                    tok->Str().c_str());
        }
        return ident;
    } else if (tok->IsConstant()) {
        return ParseConstant(tok);
    } else if (tok->IsString()) {
        return ParseString(tok);
    } else if (tok->Tag() == Token::GENERIC) {
        return ParseGeneric();
    } 

    //TODO: error
    Error(tok->Coord(), "Expect expression");
    return nullptr; // Make compiler happy
}

Constant* Parser::ParseConstant(const Token* tok)
{
    assert(tok->IsConstant());

    if (tok->Tag() == Token::I_CONSTANT) {
        auto ival = atoi(tok->Str().c_str());
        auto type = Type::NewArithmType(T_SIGNED | T_INT);
        return NewConstantInteger(type, ival);
    } else {
        auto fval = atoi(tok->Str().c_str());
        auto type = Type::NewArithmType(T_DOUBLE);
        return NewConstantFloat(type, fval);
    }
}

// TODO(wgtdkp):
Expr* Parser::ParseString(const Token* tok)
{
    assert(tok->IsString());
    assert(0);
    return nullptr;
}

// TODO(wgtdkp):
Expr* Parser::ParseGeneric(void)
{
    assert(0);
    return nullptr;
}

Expr* Parser::ParsePostfixExpr(void)
{
    auto tok = Next();
    if (tok->IsEOF())
        return nullptr;

    if ('(' == tok->Tag() && IsTypeName(Peek())) {
        // TODO(wgtdkp):
        //compound literals
        Error(tok->Coord(), "compound literals not supported yet");
        //return ParseCompLiteral();
    }

    PutBack();
    auto primExpr = ParsePrimaryExpr();
    
    return ParsePostfixExprTail(primExpr);
}

//return the constructed postfix expression
Expr* Parser::ParsePostfixExprTail(Expr* lhs)
{
    for (; ;) {
        auto tag= Next()->Tag();
        
        switch (tag) {
        case '[':
            lhs = ParseSubScripting(lhs);
            break;

        case '(':
            lhs = ParseFuncCall(lhs);
            break;

        case '.':
        case Token::PTR_OP:
            lhs = ParseMemberRef(tag, lhs);
            break;

        case Token::INC_OP:
        case Token::DEC_OP:
            lhs = ParsePostfixIncDec(tag, lhs);
            break;

        default:
            PutBack();
            return lhs;
        }
    }
}

Expr* Parser::ParseSubScripting(Expr* pointer)
{
    auto indexExpr = ParseExpr();
    Expect(']');

    return NewBinaryOp('[', pointer, indexExpr);
}


Expr* Parser::ParseMemberRef(int tag, Expr* lhs)
{
    auto memberName = Peek()->Str();
    Expect(Token::IDENTIFIER);
    
    return NewMemberRefOp(tag, lhs, memberName);
}

UnaryOp* Parser::ParsePostfixIncDec(int tag, Expr* operand)
{
    if (tag == Token::INC_OP) {
        tag = Token::POSTFIX_INC;
    } else {
        tag = Token::POSTFIX_DEC;
    }
    return NewUnaryOp(tag, operand);
}

FuncCall* Parser::ParseFuncCall(Expr* designator)
{
    FuncType* type = designator->Ty()->ToFuncType();
    assert(type);

    list<Expr*> args;
    auto iter = type->Params().begin();
    while (true) {
        auto tok = Peek();
        auto arg = ParseAssignExpr();
        args.push_back(arg);
        if (!(*iter)->Compatible(*arg->Ty())) {
            // TODO(wgtdkp): function name
            Error(tok->Coord(), "incompatible type for argument 1 of ''");
        }

        ++iter;
        
        if (iter == type->Params().end()) {
            break;
        }
        Expect(',');
    }
    
    if (!type->HasEllipsis()) {
        Expect(')');
    } else {
        while (!Try(')')) {
            Expect(',');
            auto arg = ParseAssignExpr();
            args.push_back(arg);
        }
    }

    return NewFuncCall(designator, args);
}

Expr* Parser::ParseUnaryExpr(void)
{
    auto tag = Next()->Tag();
    switch (tag) {
    case Token::ALIGNOF:
        return ParseAlignof();
    case Token::SIZEOF:
        return ParseSizeof();
    case Token::INC_OP:
        return ParsePrefixIncDec(Token::INC_OP);
    case Token::DEC_OP:
        return ParsePrefixIncDec(Token::DEC_OP);
    case '&':
        return ParseUnaryOp(Token::ADDR);
    case '*':
        return ParseUnaryOp(Token::DEREF); 
    case '+':
        return ParseUnaryOp(Token::PLUS);
    case '-':
        return ParseUnaryOp(Token::MINUS); 
    case '~':
        return ParseUnaryOp('~');
    case '!':
        return ParseUnaryOp('!');
    default:
        PutBack();
        return ParsePostfixExpr();
    }
}

Constant* Parser::ParseSizeof(void)
{
    Type* type;
    auto tok = Next();
    Expr* unaryExpr = nullptr;

    if (tok->Tag() == '(' && IsTypeName(Peek())) {
        type = ParseTypeName();
        Expect(')');
    } else {
        PutBack();
        unaryExpr = ParseUnaryExpr();
        type = unaryExpr->Ty();
    }

    if (type->ToFuncType()) {
        Error(tok->Coord(), "sizeof operator can't act on function");
    }

    auto intType = Type::NewArithmType(T_UNSIGNED | T_LONG);

    return NewConstantInteger(intType, type->Width());
}

Constant* Parser::ParseAlignof(void)
{
    Expect('(');
    auto type = ParseTypeName();
    Expect(')');
    auto intType = Type::NewArithmType(T_UNSIGNED | T_LONG);
    
    return NewConstantInteger(intType, type->Align());
}

UnaryOp* Parser::ParsePrefixIncDec(int op)
{
    assert(Token::INC_OP == op || Token::DEC_OP == op);
    auto operand = ParseUnaryExpr();
    
    return NewUnaryOp(op, operand);
}

UnaryOp* Parser::ParseUnaryOp(int op)
{
    _coord = Peek()->Coord();
    auto operand = ParseCastExpr();
    return NewUnaryOp(op, operand);
}

Type* Parser::ParseTypeName(void)
{
    auto type = ParseSpecQual();
    if (Try('*') || Try('(')) //abstract-declarator ??FIRST????
        return ParseAbstractDeclarator(type);
    
    return type;
}

Expr* Parser::ParseCastExpr(void)
{
    auto tok = Next();
    if (tok->Tag() == '(' && IsTypeName(Peek())) {
        auto desType = ParseTypeName();
        Expect(')');
        auto operand = ParseCastExpr();
        return NewUnaryOp(Token::CAST, operand, desType);
    } 
    
    PutBack();
    return ParseUnaryExpr();
}

Expr* Parser::ParseMultiplicativeExpr(void)
{
    auto lhs = ParseCastExpr();
    auto tag = Next()->Tag();
    while ('*' == tag || '/' == tag || '%' == tag) {
        auto rhs = ParseCastExpr();
        lhs = NewBinaryOp(tag, lhs, rhs);
        tag = Next()->Tag();
    }
    
    return PutBack(), lhs;
}

Expr* Parser::ParseAdditiveExpr(void)
{
    auto lhs = ParseMultiplicativeExpr();
    auto tag = Next()->Tag();
    while ('+' == tag || '-' == tag) {
        auto rhs = ParseMultiplicativeExpr();
        lhs = NewBinaryOp(tag, lhs, rhs);
        tag = Next()->Tag();
    }
    
    return PutBack(), lhs;
}

Expr* Parser::ParseShiftExpr(void)
{
    auto lhs = ParseAdditiveExpr();
    auto tag = Next()->Tag();
    while (Token::LEFT_OP == tag || Token::RIGHT_OP == tag) {
        auto rhs = ParseAdditiveExpr();
        lhs = NewBinaryOp(tag, lhs, rhs);
        tag = Next()->Tag();
    }
    
    return PutBack(), lhs;
}

Expr* Parser::ParseRelationalExpr(void)
{
    auto lhs = ParseShiftExpr();
    auto tag = Next()->Tag();
    while (Token::LE_OP == tag || Token::GE_OP == tag 
        || '<' == tag || '>' == tag) {
        auto rhs = ParseShiftExpr();
        lhs = NewBinaryOp(tag, lhs, rhs);
        tag = Next()->Tag();
    }
    
    return PutBack(), lhs;
}

Expr* Parser::ParseEqualityExpr(void)
{
    auto lhs = ParseRelationalExpr();
    auto tag = Next()->Tag();
    while (Token::EQ_OP == tag || Token::NE_OP == tag) {
        auto rhs = ParseRelationalExpr();
        lhs = NewBinaryOp(tag, lhs, rhs);
        tag = Next()->Tag();
    }
    
    return PutBack(), lhs;
}

Expr* Parser::ParseBitiwiseAndExpr(void)
{
    auto lhs = ParseEqualityExpr();
    while (Try('&')) {
        auto rhs = ParseEqualityExpr();
        lhs = NewBinaryOp('&', lhs, rhs);
    }
    
    return lhs;
}

Expr* Parser::ParseBitwiseXorExpr(void)
{
    auto lhs = ParseBitiwiseAndExpr();
    while (Try('^')) {
        auto rhs = ParseBitiwiseAndExpr();
        lhs = NewBinaryOp('^', lhs, rhs);
    }
    
    return lhs;
}

Expr* Parser::ParseBitwiseOrExpr(void)
{
    auto lhs = ParseBitwiseXorExpr();
    while (Try('|')) {
        auto rhs = ParseBitwiseXorExpr();
        lhs = NewBinaryOp('|', lhs, rhs);
    }
    
    return lhs;
}

Expr* Parser::ParseLogicalAndExpr(void)
{
    auto lhs = ParseBitwiseOrExpr();
    while (Try(Token::AND_OP)) {
        auto rhs = ParseBitwiseOrExpr();
        lhs = NewBinaryOp(Token::AND_OP, lhs, rhs);
    }
    
    return lhs;
}

Expr* Parser::ParseLogicalOrExpr(void)
{
    auto lhs = ParseLogicalAndExpr();
    while (Try(Token::OR_OP)) {
        auto rhs = ParseLogicalAndExpr();
        lhs = NewBinaryOp(Token::OR_OP, lhs, rhs);
    }
    
    return lhs;
}

Expr* Parser::ParseConditionalExpr(void)
{
    auto tok = Peek();

    auto cond = ParseLogicalOrExpr();
    if (Try('?')) {
        if (!cond->Ty()->IsScalar()) {
            Error(tok->Coord(), "scalar is required");
        }

        auto exprTrue = ParseExpr();
        Expect(':');
        auto exprFalse = ParseConditionalExpr();

        return NewConditionalOp(cond, exprTrue, exprFalse);
    }
    
    return cond;
}

Expr* Parser::ParseAssignExpr(void)
{
    //yes i know the lhs should be unary expression, let it handled by type checking
    Expr* lhs = ParseConditionalExpr();
    Expr* rhs;
    switch (Next()->Tag()) {
    case Token::MUL_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('*', lhs, rhs);
        break;

    case Token::DIV_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('/', lhs, rhs);
        break;

    case Token::MOD_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('%', lhs, rhs);
        break;

    case Token::ADD_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('+', lhs, rhs);
        break;

    case Token::SUB_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('-', lhs, rhs);
        break;

    case Token::LEFT_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp(Token::LEFT_OP, lhs, rhs);
        break;

    case Token::RIGHT_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp(Token::RIGHT_OP, lhs, rhs);
        break;

    case Token::AND_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('&', lhs, rhs);
        break;

    case Token::XOR_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('^', lhs, rhs);
        break;

    case Token::OR_ASSIGN:
        rhs = ParseAssignExpr();
        rhs = NewBinaryOp('|', lhs, rhs);
        break;

    case '=':
        rhs = ParseAssignExpr();
        break;

    default:
        PutBack();
        return lhs; // Could be constant
    }

    return NewBinaryOp('=', lhs, rhs);
}


/**************** Declarations ********************/

/*
 * If there is an initializer,
 * then return the initializer expression,
 * else, return null.
 */
CompoundStmt* Parser::ParseDecl(void)
{
    std::list<Stmt*> stmts;
    if (Try(Token::STATIC_ASSERT)) {
        //TODO: static_assert();
    } else {
        int storageSpec, funcSpec;
        auto type = ParseDeclSpec(&storageSpec, &funcSpec);
        
        //init-declarator 的 FIRST 集合：'*', identifier, '('
        if (Test('*') || Test(Token::IDENTIFIER) || Test('(')) {
            do {
                auto initExpr = ParseInitDeclarator(type, storageSpec, funcSpec);
                if (nullptr != initExpr)
                    stmts.push_back(initExpr);
            } while (Try(','));
            
            Expect(';');
        }
    }

    return NewCompoundStmt(stmts);
}

//for state machine
enum {
    //compatibility for these key words
    COMP_SIGNED = T_SHORT | T_INT | T_LONG | T_LONG_LONG,
    COMP_UNSIGNED = T_SHORT | T_INT | T_LONG | T_LONG_LONG,
    COMP_CHAR = T_SIGNED | T_UNSIGNED,
    COMP_SHORT = T_SIGNED | T_UNSIGNED | T_INT,
    COMP_INT = T_SIGNED | T_UNSIGNED | T_LONG | T_SHORT | T_LONG_LONG,
    COMP_LONG = T_SIGNED | T_UNSIGNED | T_LONG | T_INT,
    COMP_DOUBLE = T_LONG | T_COMPLEX,
    COMP_COMPLEX = T_FLOAT | T_DOUBLE | T_LONG,

    COMP_THREAD = S_EXTERN | S_STATIC,
};

static inline void TypeLL(int& typeSpec)
{
    if (typeSpec & T_LONG) {
        typeSpec &= ~T_LONG;
        typeSpec |= T_LONG_LONG;
    } else {
        typeSpec |= T_LONG;
    }
}

Type* Parser::ParseSpecQual(void)
{
    return ParseDeclSpec(nullptr, nullptr);
}

/*
param: storage: null, only type specifier and qualifier accepted;
*/
Type* Parser::ParseDeclSpec(int* storage, int* func)
{
    Type* type = nullptr;
    int align = -1;
    int storageSpec = 0;
    int funcSpec = 0;
    int qualSpec = 0;
    int typeSpec = 0;
    
    Token* tok;
    for (; ;) {
        tok = Next();
        switch (tok->Tag()) {
        //function specifier
        case Token::INLINE:
            funcSpec |= F_INLINE;
            break;

        case Token::NORETURN:
            funcSpec |= F_NORETURN;
            break;

        //alignment specifier
        case Token::ALIGNAS:
            align = ParseAlignas();
            break;

        //storage specifier
            //TODO: typedef needs more constraints
        case Token::TYPEDEF:
            if (storageSpec != 0)
                goto error;
            storageSpec |= S_TYPEDEF;
            break;

        case Token::EXTERN:
            if (storageSpec & ~S_THREAD)
                goto error;
            storageSpec |= S_EXTERN;
            break;

        case Token::STATIC:
            if (storageSpec & ~S_THREAD)
                goto error;
            storageSpec |= S_STATIC;
            break;

        case Token::THREAD:
            if (storageSpec & ~COMP_THREAD)
                goto error;
            storageSpec |= S_THREAD;
            break;

        case Token::AUTO:
            if (storageSpec != 0)
                goto error;
            storageSpec |= S_AUTO;
            break;

        case Token::REGISTER:
            if (storageSpec != 0)
                goto error; 
            storageSpec |= S_REGISTER;
            break;
        
        //type qualifier
        case Token::CONST:
            qualSpec |= Q_CONST;
            break;

        case Token::RESTRICT:
            qualSpec |= Q_RESTRICT;
            break;

        case Token::VOLATILE:
            qualSpec |= Q_VOLATILE;
            break;
        /*
        atomic_qual:
            qualSpec |= Q_ATOMIC;
            break;
        */

        //type specifier
        case Token::SIGNED:
            if (typeSpec & ~COMP_SIGNED)
                goto error; 
            typeSpec |= T_SIGNED;
            break;

        case Token::UNSIGNED:
            if (typeSpec & ~COMP_UNSIGNED)
                goto error;
            typeSpec |= T_UNSIGNED;
            break;

        case Token::VOID:
            if (0 != typeSpec)
                goto error;
            typeSpec |= T_VOID;
            break;

        case Token::CHAR:
            if (typeSpec & ~COMP_CHAR)
                goto error;
            typeSpec |= T_CHAR;
            break;

        case Token::SHORT:
            if (typeSpec & ~COMP_SHORT)
                goto error;
            typeSpec |= T_SHORT;
            break;

        case Token::INT:
            if (typeSpec & ~COMP_INT)
                goto error;
            typeSpec |= T_INT;
            break;

        case Token::LONG:
            if (typeSpec & ~COMP_LONG)
                goto error; 
            TypeLL(typeSpec); 
            break;
            
        case Token::FLOAT:
            if (typeSpec & ~T_COMPLEX)
                goto error;
            typeSpec |= T_FLOAT;
            break;

        case Token::DOUBLE:
            if (typeSpec & ~COMP_DOUBLE)
                goto error;
            typeSpec |= T_DOUBLE;
            break;

        case Token::BOOL:
            if (typeSpec != 0)
                goto error;
            typeSpec |= T_BOOL;
            break;

        case Token::COMPLEX:
            if (typeSpec & ~COMP_COMPLEX)
                goto error;
            typeSpec |= T_COMPLEX;
            break;

        case Token::STRUCT: 
        case Token::UNION:
            if (typeSpec != 0)
                goto error; 
            type = ParseStructUnionSpec(Token::STRUCT == tok->Tag()); 
            typeSpec |= T_STRUCT_UNION;
            break;

        case Token::ENUM:
            if (typeSpec != 0)
                goto error;
            type = ParseEnumSpec();
            typeSpec |= T_ENUM;
            break;

        case Token::ATOMIC:\
            assert(false);
            /*
            if (Peek()->Tag() != '(')
                goto atomic_qual;
            if (typeSpec != 0)
                goto error;
            type = ParseAtomicSpec();
            typeSpec |= T_ATOMIC;
            break;
            */
        default:
            if (0 == typeSpec && IsTypeName(tok)) {
                auto ident = _curScope->Find(tok->Str());
                if (ident) {
                    type = ident->ToType();
                }
                typeSpec |= T_TYPEDEF_NAME;
            } else  {
                goto end_of_loop;
            }
        }
    }

    //TODO: 语义部分
end_of_loop:
    PutBack();
    switch (typeSpec) {
    case 0:
        Error(tok->Coord(), "expect type specifier");
        break;

    case T_VOID:
        type = Type::NewVoidType();
        break;

    case T_ATOMIC:
    case T_STRUCT_UNION:
    case T_ENUM:
    case T_TYPEDEF_NAME:
        break;

    default:
        type = ArithmType::NewArithmType(typeSpec);
        break;
    }

    if ((nullptr == storage || nullptr == func)) {
        if (0 != funcSpec && 0 != storageSpec && -1 != align) {
            Error(tok->Coord(), "type specifier/qualifier only");
        }
    } else {
        *storage = storageSpec;
        *func = funcSpec;
    }

    return type;

error:
    Error(tok->Coord(), "type speficier/qualifier/storage error");
    return nullptr;	// Make compiler happy
}

int Parser::ParseAlignas(void)
{
    int align;
    Expect('(');
    if (IsTypeName(Peek())) {
        auto type = ParseTypeName();
        Expect(')');
        align = type->Align();
    } else {
        _coord = Peek()->Coord();
        auto expr = ParseExpr();
        align = expr->EvalInteger(_coord);
        Expect(')');
        Delete(expr);
    }

    return align;
}

static inline string MakeStructUnionName(const char* name)
{
    static string ret = "struct/union@";
    return ret + name;
}

Type* Parser::ParseEnumSpec(void)
{
    std::string tagName;
    auto tok = Next();
    
    if (tok->IsIdentifier()) {
        tagName = tok->Str();
        if (Try('{')) {
            //定义enum类型
            auto tagIdent = _curScope->FindTagInCurScope(tagName);
            if (tagIdent == nullptr)
                goto enum_decl;

            if (!tagIdent->Ty()->Complete()) {
                return ParseEnumerator(tagIdent->Ty()->ToArithmType());
            } else {
                Error(tok->Coord(), "redefinition of enumeration tag '%s'",
                        tagName.c_str());
            }
        } else {
            //Type* type = _curScope->FindTag(tagName);
            auto tagIdent = _curScope->FindTag(tagName);
            if (tagIdent) {
                return tagIdent->Ty();
            }
            auto type = Type::NewArithmType(T_INT);
            type->SetComplete(false);   //尽管我们把 enum 当成 int 看待，但是还是认为他是不完整的
            auto ident = NewIdentifier(type, _curScope, L_NONE);
            _curScope->InsertTag(tagName, ident);
        }
    }
    
    Expect('{');

enum_decl:
    auto type = Type::NewArithmType(T_INT);
    if (tagName.size() != 0) {
        auto ident = NewIdentifier(type, _curScope, L_NONE);
        _curScope->InsertTag(tagName, ident);
    }
    
    return ParseEnumerator(type);   //处理反大括号: '}'
}

Type* Parser::ParseEnumerator(ArithmType* type)
{
    assert(type && !type->Complete() && type->IsInteger());
    int val = 0;
    do {
        auto tok = Peek();
        if (!tok->IsIdentifier())
            Error(tok->Coord(), "enumration constant expected");
        
        auto enumName = tok->Str();
        auto ident = _curScope->FindInCurScope(enumName);
        if (ident) {
            Error(tok->Coord(), "redefinition of enumerator '%s'",
                    enumName.c_str());
        }
        if (Try('=')) {
            _coord = Peek()->Coord();
            auto expr = ParseExpr();
            val = expr->EvalInteger(_coord);
            // TODO(wgtdkp): checking conflict
        }

        // TODO(wgtdkp):
        /*
        auto Constant = NewConstantInteger(
                Type::NewArithmType(T_INT), val++);

        _curScope->InsertConstant(enumName, Constant);
        */
        Try(',');
    } while (!Try('}'));
    
    type->SetComplete(true);
    
    return type;
}

/*
 * 四种 name space：
 * 1.label, 如 goto end; 它有函数作用域
 * 2.struct/union/enum 的 tag
 * 3.struct/union 的成员
 * 4.其它的普通的变量
 */
Type* Parser::ParseStructUnionSpec(bool isStruct)
{
    std::string tagName;
    auto tok = Next();
    if (tok->IsIdentifier()) {
        tagName = tok->Str();
        if (Try('{')) {
            //看见大括号，表明现在将定义该struct/union类型
            auto tagIdent = _curScope->FindTagInCurScope(tagName);
            if (tagIdent == nullptr) //我们不用关心上层scope是否定义了此tag，如果定义了，那么就直接覆盖定义
                goto struct_decl; //现在是在当前scope第一次看到name，所以现在是第一次定义，连前向声明都没有；
            
            /*
             * 在当前scope找到了类型，但可能只是声明；注意声明与定义只能出现在同一个scope；
             * 1.如果声明在定义的外层scope,那么即使在内层scope定义了完整的类型，此声明仍然是无效的；
             *   因为如论如何，编译器都不会在内部scope里面去找定义，所以声明的类型仍然是不完整的；
             * 2.如果声明在定义的内层scope,(也就是先定义，再在内部scope声明)，这时，不完整的声明会覆盖掉完整的定义；
             *   因为编译器总是向上查找符号，不管找到的是完整的还是不完整的，都要；
             */
            if (!tagIdent->Ty()->Complete()) {
                //找到了此tag的前向声明，并更新其符号表，最后设置为complete type
                return ParseStructDecl(tagIdent->Ty()->ToStructUnionType());
            } else {
                //在当前作用域找到了完整的定义，并且现在正在定义同名的类型，所以报错；
                Error(tok->Coord(), "redefinition of struct tag '%s'",
                        tok->Str().c_str());
            }
        } else {
            /*
			 * 没有大括号，表明不是定义一个struct/union;那么现在只可能是在：
			 * 1.声明；
			 * 2.声明的同时，定义指针(指针允许指向不完整类型) (struct Foo* p; 是合法的) 或者其他合法的类型；
			 *   如果现在索引符号表，那么：
			 *   1.可能找到name的完整定义，也可能只找得到不完整的声明；不管name指示的是不是完整类型，我们都只能选择name指示的类型；
			 *   2.如果我们在符号表里面压根找不到name,那么现在是name的第一次声明，创建不完整的类型并插入符号表；
			 */
            auto tagIdent = _curScope->FindTag(tagName);
            
            //如果tag已经定义或声明，那么直接返回此定义或者声明
            if (tagIdent) {
                return tagIdent->Ty();
            }
            
            //如果tag尚没有定义或者声明，那么创建此tag的声明(因为没有见到‘{’，所以不会是定义)
            auto type = Type::NewStructUnionType(isStruct); //创建不完整的类型
            
            //因为有tag，所以不是匿名的struct/union， 向当前的scope插入此tag
            auto ident = NewIdentifier(type, _curScope, L_NONE);
            _curScope->InsertTag(tagName, ident);
            return type;
        }
    }
    //没见到identifier，那就必须有struct/union的定义，这叫做匿名struct/union;
    Expect('{');

struct_decl:
    //现在，如果是有tag，那它没有前向声明；如果是没有tag，那更加没有前向声明；
	//所以现在是第一次开始定义一个完整的struct/union类型
    auto type = Type::NewStructUnionType(isStruct);
    if (tagName.size() != 0) {
        auto ident = NewIdentifier(type, _curScope, L_NONE);
        _curScope->InsertTag(tagName, ident);
    }
    
    return ParseStructDecl(type); //处理反大括号: '}'
}

StructUnionType* Parser::ParseStructDecl(StructUnionType* type)
{
    //既然是定义，那输入肯定是不完整类型，不然就是重定义了
    assert(type && !type->Complete());
    
    while (!Try('}')) {
        if (Peek()->IsEOF())
            Error(Peek()->Coord(), "premature end of input");

        //解析type specifier/qualifier, 不接受storage等
        auto fieldType = ParseSpecQual();
        //TODO: 解析declarator

    }

    // TODO(wgtdkp): calculate width

    //struct/union定义结束，设置其为完整类型
    type->SetComplete(true);
    
    return type;
}

int Parser::ParseQual(void)
{
    int qualSpec = 0;
    for (; ;) {
        switch (Next()->Tag()) {
        case Token::CONST:
            qualSpec |= Q_CONST;
            break;

        case Token::RESTRICT:
            qualSpec |= Q_RESTRICT;
            break;

        case Token::VOLATILE:
            qualSpec |= Q_VOLATILE;
            break;

        case Token::ATOMIC:
            qualSpec |= Q_ATOMIC;
            break;

        default:
            PutBack();
            return qualSpec;
        }
    }
}

Type* Parser::ParsePointer(Type* typePointedTo)
{
    Type* retType = typePointedTo;
    while (Try('*')) {
        retType = Type::NewPointerType(typePointedTo);
        retType->SetQual(ParseQual());
        typePointedTo = retType;
    }

    return retType;
}

static Type* ModifyBase(Type* type, Type* base, Type* newBase)
{
    if (type == base)
        return newBase;
    
    auto ty = type->ToDerivedType();
    ty->SetDerived(ModifyBase(ty->Derived(), base, newBase));
    
    return ty;
}

/*
 * Return: pair of token(must be identifier) and it's type
 *     if token is nullptr, then we are parsing abstract declarator
 *     else, parsing direct declarator.
 */
TokenTypePair Parser::ParseDeclarator(Type* base)
{
    // May be pointer
    auto pointerType = ParsePointer(base);
    
    if (Try('(')) {
        //现在的 pointerType 并不是正确的 base type
        auto tokenTypePair = ParseDeclarator(pointerType);
        Expect(')');
        auto newBase = ParseArrayFuncDeclarator(pointerType);
        //修正 base type
        auto retType = ModifyBase(tokenTypePair.second, pointerType, newBase);
        return TokenTypePair(tokenTypePair.first, retType);
    } else if (Peek()->IsIdentifier()) {
        auto tok = Next();
        auto retType = ParseArrayFuncDeclarator(pointerType);
        return TokenTypePair(tok, retType);
    }
    
    _coord = Peek()->Coord();
    //Error(Peek()->Coord(), "expect identifier or '(' but get '%s'",
    //        Peek()->Str().c_str());
    
    return TokenTypePair(nullptr, pointerType);
}

Identifier* Parser::ProcessDeclarator(Token* tok, Type* type,
        int storageSpec, int funcSpec)
{
    assert(tok);
    /*
     * TODO: 检查在同一 scope 是否已经定义此变量
	 * 如果 storage 是 typedef，那么应该往符号表里面插入 type
	 * 定义 void 类型变量是非法的，只能是指向void类型的指针
	 * 如果 funcSpec != 0, 那么现在必须是在定义函数，否则出错
     */
    auto name = tok->Str();
    Identifier* ident;

    if (storageSpec & S_TYPEDEF) {
        ident = _curScope->FindInCurScope(tok->Str());
        if (ident) { // There is prio declaration in the same scope
            // The same declaration, simply return the prio declaration
            if (*type == *ident->Ty())
                return ident;
            Error(tok->Coord(), "conflicting types for '%s'", name.c_str());
        }
        ident = NewIdentifier(type, _curScope, L_NONE);
        _curScope->Insert(name, ident);
        return ident;
    }

    if (type->ToVoidType()) {
        Error(tok->Coord(), "variable or field '%s' declared void",
                name.c_str());
    }

    if (!type->Complete()) {
        Error(tok->Coord(), "storage size of '%s' isn’t known",
                name.c_str());
    }

    if (type->ToFuncType() && _curScope->Type() != S_FILE
            && (storageSpec & S_STATIC)) {
        Error(tok->Coord(), "invalid storage class for function '%s'",
                name.c_str());
    }

    enum Linkage linkage;
    // Identifiers in function prototype have no linkage
    if (_curScope->Type() == S_PROTO) {
        linkage = L_NONE;
    } else if (_curScope->Type() == S_FILE) {
        linkage = L_EXTERNAL; // Default linkage for file scope identifiers
        if (storageSpec & S_STATIC)
            linkage = L_INTERNAL;
    } else if (!(storageSpec & S_EXTERN)) {
        linkage = L_NONE; // Default linkage for block scope identifiers
        if (type->ToFuncType())
            linkage = L_EXTERNAL;
    } else {
        linkage = L_EXTERNAL;
    }

    ident = _curScope->FindInCurScope(name);
    if (ident) { // There is prio declaration in the same scope
        if (*type != *ident->Ty()) {
            Error(tok->Coord(), "conflicting types for '%s'", name.c_str());
        }

        // The same scope prio declaration has no linkage,
        // there is a redeclaration error
        if (linkage == L_NONE) {
            Error(tok->Coord(), "redeclaration of '%s' with no linkage",
                    name.c_str());
        } else if (linkage == L_EXTERNAL) {
            if (ident->Linkage() == L_NONE) {
                Error(tok->Coord(), "conflicting linkage for '%s'", name.c_str());
            }
        } else {
            if (ident->Linkage() != L_INTERNAL) {
                Error(tok->Coord(), "conflicting linkage for '%s'", name.c_str());
            }
        }
        // The same redeclaration, simply return the prio declaration 
        return ident;
    } else if (linkage == L_EXTERNAL) {
        ident = _curScope->Find(name);
        if (ident) {
            if (*type != *ident->Ty()) {
            	Error(tok->Coord(), "conflicting types for '%s'",
                        name.c_str());
            }
            if (ident->Linkage() != L_NONE) {
                linkage = ident->Linkage();
            }
        } else {
            ident = _externalSymbols->FindInCurScope(name);
            if (ident) {
                if (*type != *ident->Ty()) {
                    Error(tok->Coord(), "conflicting types for '%s'",
                            name.c_str());
                }
                // Don't return
            }
        }
    }

    Identifier* ret;
    if (type->ToFuncType()) {
        ret = NewIdentifier(type, _curScope, linkage);
    } else {
        ret = NewObject(type, _curScope, storageSpec, linkage);
    }
    _curScope->Insert(name, ret);
    
    if (linkage == L_EXTERNAL && ident == nullptr) {
        _externalSymbols->Insert(name, ret);
    }

    return ret;
}

Type* Parser::ParseArrayFuncDeclarator(Type* base)
{
    if (Try('[')) {
        if (nullptr != base->ToFuncType()) {
            Error(Peek()->Coord(), "the element of array can't be a function");
        }
        //TODO: parse array length expression
        auto len = ParseArrayLength();
        if (0 == len) {
            Error(Peek()->Coord(), "can't declare an array of length 0");
        }
        Expect(']');
        base = ParseArrayFuncDeclarator(base);
        
        return Type::NewArrayType(len, base);
    } else if (Try('(')) {	//function declaration
        if (base->ToFuncType()) {
            Error(Peek()->Coord(),
                    "the return value of function can't be function");
        } else if (nullptr != base->ToArrayType()) {
            Error(Peek()->Coord(),
                    "the return value of function can't be array");
        }

        //TODO: parse arguments
        std::list<Type*> params;
        EnterBlock();
        bool hasEllipsis = ParseParamList(params);
        ExitBlock();
        
        Expect(')');
        base = ParseArrayFuncDeclarator(base);
        
        return Type::NewFuncType(base, 0, hasEllipsis, params);
    }

    return base;
}

/*
 * return: -1, 没有指定长度；其它，长度；
 */
int Parser::ParseArrayLength(void)
{
    auto hasStatic = Try(Token::STATIC);
    auto qual = ParseQual();
    if (0 != qual)
        hasStatic = Try(Token::STATIC);
    /*
    if (!hasStatic) {
        if (Try('*'))
            return Expect(']'), -1;
        if (Try(']'))
            return -1;
        else {
            auto expr = ParseAssignExpr();
            auto len = Evaluate(expr);
            Expect(']');
            return len;
        }
    }*/

    //不支持变长数组
    if (!hasStatic && Try(']'))
        return -1;
    
    _coord = Peek()->Coord();
    auto expr = ParseAssignExpr();
    return expr->EvalInteger(_coord);
}

/*
 * Return: true, has ellipsis;
 */
bool Parser::ParseParamList(std::list<Type*>& params)
{
    auto paramType = ParseParamDecl();
    params.push_back(paramType);
    /*
     * The parameter list is 'void'
     */
    if (paramType->ToVoidType()) {
        return false;
    }

    while (Try(',')) {
        if (Try(Token::ELLIPSIS)) {
            return true;
        }

        auto tok = Peek();
        paramType = ParseParamDecl();
        if (paramType->ToVoidType()) {
            Error(tok->Coord(), "'void' must be the only parameter");
        }

        params.push_back(paramType);
    }

    return false;
}

Type* Parser::ParseParamDecl(void)
{
    int storageSpec, funcSpec;
    auto type = ParseDeclSpec(&storageSpec, &funcSpec);
    
    // No declarator
    if (Peek()->Tag() == ',' || Peek()->Tag() == ')')
        return type;
    
    //TODO: declarator 和 abstract declarator 都要支持
	//TODO: 区分 declarator 和 abstract declarator
    auto tokTypePair = ParseDeclarator(type);
    auto tok = tokTypePair.first;
    type = tokTypePair.second;
    if (tok == nullptr) { // Abstract declarator
        return type;
    }

    ProcessDeclarator(tok, type, storageSpec, funcSpec);

    return type;
}

Type* Parser::ParseAbstractDeclarator(Type* type)
{
    auto tokenTypePair = ParseDeclarator(type);
    auto tok = tokenTypePair.first;
    type = tokenTypePair.second;
    if (tok) { // Not a abstract declarator!
        Error(tok->Coord(), "unexpected identifier '%s'",
                tok->Str().c_str());
    }
    return type;
    /*
    auto pointerType = ParsePointer(type);
    if (nullptr != pointerType->ToPointerType() && !Try('('))
        return pointerType;
    
    auto ret = ParseAbstractDeclarator(pointerType);
    Expect(')');
    auto newBase = ParseArrayFuncDeclarator(pointerType);
    
    return ModifyBase(ret, pointerType, newBase);
    */
}

Identifier* Parser::ParseDirectDeclarator(Type* type,
        int storageSpec, int funcSpec)
{
    auto tokenTypePair = ParseDeclarator(type);
    auto tok = tokenTypePair.first;
    type = tokenTypePair.second;
    if (tok == nullptr) {
        Error(_coord, "expect identifier or '('");
    }

    return ProcessDeclarator(tok, type, storageSpec, funcSpec);
}

/*
 * Initialization is translated into assignment expression
 */
Stmt* Parser::ParseInitDeclarator(Type* type, int storageSpec, int funcSpec)
{
    auto ident = ParseDirectDeclarator(type, storageSpec, funcSpec);
    
    if (Try('=')) {
        if (ident->ToObject() == nullptr) {
            Error(Peek()->Coord(), "unexpected initializer");
        }
        return ParseInitializer(ident->ToObject());
    }

    return nullptr;
}

Stmt* Parser::ParseInitializer(Object* obj)
{
    auto type = obj->Ty();

    if (Try('{')) {
        if (type->ToArrayType()) {
            // Expect array initializer
            return ParseArrayInitializer(obj);
        } else if (type->ToStructUnionType()) {
            // Expect struct/union initializer
            return ParseStructInitializer(obj);
        }
    }

    Expr* rhs = ParseAssignExpr();

    return NewBinaryOp('=', obj, rhs);
}

Stmt* Parser::ParseArrayInitializer(Object* arr)
{
    assert(arr->Ty()->ToArrayType());
    auto type = arr->Ty()->ToArrayType();

    size_t defaultIdx = 0;
    std::set<size_t> idxSet;
    std::list<Stmt*> stmts;

    while (true) {
        auto tok = Next();
        if (tok->Tag() == '}')
            break;

        if (tok->Tag() == '[') {
            _coord = Peek()->Coord();
            auto expr = ParseExpr();
            // TODO(wgtdkp): make sure it is constant integer!

            auto idx = expr->EvalInteger(_coord);
            idxSet.insert(idx);
            // TODO(wgtdkp): GetArrayElement() create new object
            // Will there be memory leak?
            //auto ele = arr->GetElementOffset(this, idx);
            int offset = type->GetElementOffset(idx);
            auto ele = NewObject(type->Derived(), arr->Scope());
            ele->SetOffset(offset + arr->Offset());
            ele->SetStorage(arr->Storage());
            ele->SetLinkage(arr->Linkage());

            Expect(']');
            Expect('=');

            stmts.push_back(ParseInitializer(ele));
        } else {
            // If not specified designator,
            // the default index INC from 0 and jump over designators
            while (idxSet.find(defaultIdx) != idxSet.end())
                defaultIdx++;
            
            int offset = type->GetElementOffset(defaultIdx);
            auto ele = NewObject(type->Derived(), arr->Scope());
            ele->SetOffset(offset + arr->Offset());
            ele->SetStorage(arr->Storage());
            ele->SetLinkage(arr->Linkage());

            stmts.push_back(ParseInitializer(ele));
        }

        // Needless comma at the end is allowed
        if (!Try(',')) {
            if (Peek()->Tag() != '}') {
                Error(Peek()->Coord(), "expect ',' or '}'");
            }
        }
    }

    return NewCompoundStmt(stmts);
}

Stmt* Parser::ParseStructInitializer(Object* obj)
{
    return nullptr;
}


/*
 * Statements
 */

Stmt* Parser::ParseStmt(void)
{
    auto tok = Next();
    if (tok->IsEOF())
        Error(tok->Coord(), "premature end of input");

    switch (tok->Tag()) {
    case ';':
        return NewEmptyStmt();
    case '{':
        return ParseCompoundStmt();
    case Token::IF:
        return ParseIfStmt();
    case Token::SWITCH:
        return ParseSwitchStmt();
    case Token::WHILE:
        return ParseWhileStmt();
    case Token::DO:
        return ParseDoStmt();
    case Token::FOR:
        return ParseForStmt();
    case Token::GOTO:
        return ParseGotoStmt();
    case Token::CONTINUE:
        return ParseContinueStmt();
    case Token::BREAK:
        return ParseBreakStmt();
    case Token::RETURN:
        return ParseReturnStmt();
    case Token::CASE:
        return ParseCaseStmt();
    case Token::DEFAULT:
        return ParseDefaultStmt();
    }

    if (tok->IsIdentifier() && Try(':'))
        return ParseLabelStmt(tok);
    
    PutBack();
    auto expr = ParseExpr();
    Expect(';');

    return expr;
}

CompoundStmt* Parser::ParseCompoundStmt(void)
{
    EnterBlock();
    std::list<Stmt*> stmts;
    
    while (!Try('}')) {
        if (Peek()->IsEOF()) {
            Error(Peek()->Coord(), "premature end of input");
        }

        if (IsType(Peek())) {
            stmts.push_back(ParseDecl());
        } else {
            stmts.push_back(ParseStmt());
        }
    }

    ExitBlock();
    
    return NewCompoundStmt(stmts);
}

IfStmt* Parser::ParseIfStmt(void)
{
    Expect('(');
    _coord = Peek()->Coord();
    auto cond = ParseExpr();
    if (!cond->Ty()->IsScalar()) {
        Error(_coord, "expect scalar");
    }
    Expect(')');

    auto then = ParseStmt();
    Stmt* els = nullptr;
    if (Try(Token::ELSE))
        els = ParseStmt();
    
    return NewIfStmt(cond, then, els);
}

/*
 * for 循环结构：
 *      for (declaration; expression1; expression2) statement
 * 展开后的结构：
 *		declaration
 * cond: if (expression1) then empty
 *		else goto end
 *		statement
 * step: expression2
 *		goto cond
 * next:
 */

#define ENTER_LOOP_BODY(breakDest, continueDest)    \
{											        \
    LabelStmt* breakDestBackup = _breakDest;	    \
    LabelStmt* continueDestBackup = _continueDest;  \
    _breakDest = breakDest;			                \
    _continueDest = continueDest; 

#define EXIT_LOOP_BODY()		        \
    _breakDest = breakDestBackup;       \
    _continueDest = continueDestBackup;	\
}

CompoundStmt* Parser::ParseForStmt(void)
{
    EnterBlock();
    Expect('(');
    
    std::list<Stmt*> stmts;

    if (IsType(Peek())) {
        stmts.push_back(ParseDecl());
    } else if (!Try(';')) {
        stmts.push_back(ParseExpr());
        Expect(';');
    }

    Expr* condExpr = nullptr;
    if (!Try(';')) {
        condExpr = ParseExpr();
        Expect(';');
    }

    Expr* stepExpr = nullptr;
    if (!Try(')')) {
        stepExpr = ParseExpr();
        Expect(')');
    }

    auto condLabel = NewLabelStmt();
    auto stepLabel = NewLabelStmt();
    auto endLabel = NewLabelStmt();
    stmts.push_back(condLabel);
    if (nullptr != condExpr) {
        auto gotoEndStmt = NewJumpStmt(endLabel);
        auto ifStmt = NewIfStmt(condExpr, nullptr, gotoEndStmt);
        stmts.push_back(ifStmt);
    }

    //我们需要给break和continue语句提供相应的标号，不然不知往哪里跳
    Stmt* bodyStmt;
    ENTER_LOOP_BODY(endLabel, condLabel);
    bodyStmt = ParseStmt();
    //因为for的嵌套结构，在这里需要回复break和continue的目标标号
    EXIT_LOOP_BODY()
    
    stmts.push_back(bodyStmt);
    stmts.push_back(stepLabel);
    stmts.push_back(stepExpr);
    stmts.push_back(NewJumpStmt(condLabel));
    stmts.push_back(endLabel);

    ExitBlock();
    
    return NewCompoundStmt(stmts);
}

/*
 * while 循环结构：
 * while (expression) statement
 * 展开后的结构：
 * cond: if (expression1) then empty
 *		else goto end
 *		statement
 *		goto cond
 * end:
 */
CompoundStmt* Parser::ParseWhileStmt(void)
{
    std::list<Stmt*> stmts;
    Expect('(');
    auto condExpr = ParseExpr();
    //TODO: ensure scalar type
    Expect(')');

    auto condLabel = NewLabelStmt();
    auto endLabel = NewLabelStmt();
    auto gotoEndStmt = NewJumpStmt(endLabel);
    auto ifStmt = NewIfStmt(condExpr, nullptr, gotoEndStmt);
    stmts.push_back(condLabel);
    stmts.push_back(ifStmt);
    
    Stmt* bodyStmt;
    ENTER_LOOP_BODY(endLabel, condLabel)
    bodyStmt = ParseStmt();
    EXIT_LOOP_BODY()
    
    stmts.push_back(bodyStmt);
    stmts.push_back(NewJumpStmt(condLabel));
    stmts.push_back(endLabel);
    
    return NewCompoundStmt(stmts);
}

/*
 * do-while 循环结构：
 *      do statement while (expression)
 * 展开后的结构：
 * begin: statement
 * cond: if (expression) then goto begin
 *		 else goto end
 * end:
 */
CompoundStmt* Parser::ParseDoStmt(void)
{
    auto beginLabel = NewLabelStmt();
    auto condLabel = NewLabelStmt();
    auto endLabel = NewLabelStmt();
    
    Stmt* bodyStmt;
    ENTER_LOOP_BODY(endLabel, beginLabel)
    bodyStmt = ParseStmt();
    EXIT_LOOP_BODY()

    Expect(Token::WHILE);
    Expect('(');
    auto condExpr = ParseExpr();
    Expect(')');

    auto gotoBeginStmt = NewJumpStmt(beginLabel);
    auto gotoEndStmt = NewJumpStmt(endLabel);
    auto ifStmt = NewIfStmt(condExpr, gotoBeginStmt, gotoEndStmt);

    std::list<Stmt*> stmts;
    stmts.push_back(beginLabel);
    stmts.push_back(bodyStmt);
    stmts.push_back(condLabel);
    stmts.push_back(ifStmt);
    stmts.push_back(endLabel);
    
    return NewCompoundStmt(stmts);
}

#define ENTER_SWITCH_BODY(breakDest, caseLabels)    \
{ 												    \
    CaseLabelList* caseLabelsBackup = _caseLabels;  \
    LabelStmt* defaultLabelBackup = _defaultLabel;  \
    LabelStmt* breakDestBackup = _breakDest;        \
    _breakDest = breakDest;                         \
    _caseLabels = &caseLabels; 

#define EXIT_SWITCH_BODY()			    \
    _caseLabels = caseLabelsBackup;     \
    _breakDest = breakDestBackup;       \
    _defaultLabel = defaultLabelBackup;	\
}

/*
 * switch
 *  jump stmt (skip case labels)
 *  case labels
 *  jump stmts
 *  default jump stmt
 */
CompoundStmt* Parser::ParseSwitchStmt(void)
{
    std::list<Stmt*> stmts;
    Expect('(');
    auto tok = Peek();
    auto expr = ParseExpr();
    Expect(')');

    if (!expr->Ty()->IsInteger()) {
        Error(tok->Coord(), "switch quantity not an integer");
    }

    auto testLabel = NewLabelStmt();
    auto endLabel = NewLabelStmt();
    auto t = NewTempVar(expr->Ty());
    auto assign = NewBinaryOp('=', t, expr);
    stmts.push_back(assign);
    stmts.push_back(NewJumpStmt(testLabel));

    CaseLabelList caseLabels;
    ENTER_SWITCH_BODY(endLabel, caseLabels);

    auto bodyStmt = ParseStmt(); // Fill caseLabels and defaultLabel
    stmts.push_back(bodyStmt);
    stmts.push_back(testLabel);

    for (auto iter = caseLabels.begin();
            iter != caseLabels.end(); iter++) {
        auto rhs = NewConstantInteger(
                Type::NewArithmType(T_INT), iter->first);
        auto cond = NewBinaryOp(Token::EQ_OP, t, rhs);
        auto then = NewJumpStmt(iter->second);
        auto ifStmt = NewIfStmt(cond, then, nullptr);
        stmts.push_back(ifStmt);
    }
    
    stmts.push_back(NewJumpStmt(_defaultLabel));
    EXIT_SWITCH_BODY();

    stmts.push_back(endLabel);

    return NewCompoundStmt(stmts);
}

CompoundStmt* Parser::ParseCaseStmt(void)
{
    _coord = Peek()->Coord();
    auto expr = ParseExpr();
    Expect(':');
    
    auto val = expr->EvalInteger(_coord);
    auto labelStmt = NewLabelStmt();
    _caseLabels->push_back(std::make_pair(val, labelStmt));
    std::list<Stmt*> stmts;
    stmts.push_back(labelStmt);
    stmts.push_back(ParseStmt());
    
    return NewCompoundStmt(stmts);
}

CompoundStmt* Parser::ParseDefaultStmt(void)
{
    auto tok = Peek();
    Expect(':');
    if (_defaultLabel != nullptr) { // There is a 'default' stmt
        Error(tok->Coord(), "multiple default labels in one switch");
    }
    auto labelStmt = NewLabelStmt();
    _defaultLabel = labelStmt;
    
    std::list<Stmt*> stmts;
    stmts.push_back(labelStmt);
    stmts.push_back(ParseStmt());
    
    return NewCompoundStmt(stmts);
}

JumpStmt* Parser::ParseContinueStmt(void)
{
    auto tok = Peek();
    Expect(';');
    if (_continueDest == nullptr) {
        Error(tok->Coord(), "'continue' is allowed only in loop");
    }
    
    return NewJumpStmt(_continueDest);
}

JumpStmt* Parser::ParseBreakStmt(void)
{
    auto tok = Peek();
    Expect(';');
    // ERROR(wgtdkp):
    if (_breakDest == nullptr) {
        Error(tok->Coord(), "'break' is allowed only in switch/loop");
    }
    
    return NewJumpStmt(_breakDest);
}

ReturnStmt* Parser::ParseReturnStmt(void)
{
    Expr* expr;

    if (Try(';')) {
        expr = nullptr;
    } else {
        expr = ParseExpr();
        Expect(';');
    }

    return NewReturnStmt(expr);
}

JumpStmt* Parser::ParseGotoStmt(void)
{
    Expect(Token::IDENTIFIER);
    auto label = Peek()->Str();
    Expect(';');

    auto labelStmt = FindLabel(label);
    if (nullptr != labelStmt)
        return NewJumpStmt(labelStmt);
    
    auto unresolvedJump = NewJumpStmt(nullptr);;
    _unresolvedJumps.push_back(std::make_pair(label, unresolvedJump));
    
    return unresolvedJump;
}

CompoundStmt* Parser::ParseLabelStmt(const Token* label)
{
    Expect(':');

    auto labelStr = label->Str();
    auto stmt = ParseStmt();
    if (nullptr != FindLabel(labelStr)) {
        Error(label->Coord(), "redefinition of label '%s'", labelStr.c_str());
    }

    auto labelStmt = NewLabelStmt();
    AddLabel(labelStr, labelStmt);
    std::list<Stmt*> stmts;
    stmts.push_back(labelStmt);
    stmts.push_back(stmt);

    return NewCompoundStmt(stmts);
}

/*
 * function-definition:
 *   declaration-specifiers declarator declaration-list? compound-statement
 */

bool Parser::IsFuncDef(void)
{
    if (Test(Token::STATIC_ASSERT))	//declaration
        return false;

    Mark();
    int storageSpec = 0, funcSpec = 0;
    auto type = ParseDeclSpec(&storageSpec, &funcSpec);
    ParseDeclarator(type);
    // FIXME(wgtdkp): Memory leak

    bool ret = !(Test(',') || Test('=') || Test(';'));
    Release();
    
    return ret;
}

FuncDef* Parser::ParseFuncDef(void)
{
    int storageSpec, funcSpec;
    auto type = ParseDeclSpec(&storageSpec, &funcSpec);
    auto ident = ParseDirectDeclarator(type, storageSpec, funcSpec);
    type = ident->Ty();

    Expect('{');
    auto stmt = ParseCompoundStmt();
    
    assert(type->ToFuncType());
    return NewFuncDef(type->ToFuncType(), stmt);
}
