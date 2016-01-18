#ifndef _STMT_H_
#define _STMT_H_

#include "ast.h"
#include <list>

class Stmt : public ASTNode
{

};

class EmptyStmt : public Stmt
{
	friend class TranslationUnit;
public:
	virtual ~EmptyStmt(void) {}
protected:
	EmptyStmt(void) {}
private:
};

//���������Ŀ�����ڣ���Ŀ��������ɵ�ʱ���ܹ�������Ӧ��label
class LabelStmt : public Stmt
{
	friend class TranslationUnit;
public:
	~LabelStmt(void) {}
	int Tag(void) const { return Tag(); }
protected:
	LabelStmt(void): _tag(GenTag()) {}
private:
	static int GenTag(void) {
		static int tag = 0;
		return ++tag;
	}
	int _tag; //ʹ�����͵�tagֵ������ֱ�����ַ���
};

class IfStmt : public Stmt
{
	friend class TranslationUnit;
public:
	virtual ~IfStmt(void) {}
protected:
	IfStmt(Expr* cond, Stmt* then, Stmt* els=nullptr)
		: _cond(cond), _then(then), _else(els) {}
private:
	Expr* _cond;
	Stmt* _then;
	Stmt* _else;
};

class JumpStmt : public Stmt
{
	friend class TranslationUnit;
public:
	virtual ~JumpStmt(void) {}
	void SetLabel(LabelStmt* label) { _label = label; }
protected:
	JumpStmt(LabelStmt* label) : _label(label) {}
private:
	LabelStmt* _label;
};

class CompoundStmt : public Stmt
{
	friend class TranslationUnit;
public:
	virtual ~CompoundStmt(void) {}

protected:
	CompoundStmt(const std::list<Stmt*>& stmts)
		: _stmts(stmts) {}

private:
	std::list<Stmt*> _stmts;
};


#endif
