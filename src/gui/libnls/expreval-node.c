#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "expreval.h"
#include "expreval-internal.h"


#if 0
static const char *expreval_nodetype_name(nodetype type)
{
#define CASE(x) case x: return #x
	switch (type)
	{
	CASE(NODETYPE_FUNCTION);
	CASE(NODETYPE_ASSIGN);
	CASE(NODETYPE_ASSIGN_ADD);
	CASE(NODETYPE_ASSIGN_SUBTRACT);
	CASE(NODETYPE_ASSIGN_MULTIPLY);
	CASE(NODETYPE_ASSIGN_DIVIDE);
	CASE(NODETYPE_ASSIGN_MODULO);
	CASE(NODETYPE_ASSIGN_LSHIFT);
	CASE(NODETYPE_ASSIGN_RSHIFT);
	CASE(NODETYPE_ASSIGN_XOR);
	CASE(NODETYPE_ASSIGN_BOR);
	CASE(NODETYPE_ASSIGN_BAND);
	CASE(NODETYPE_ADD);
	CASE(NODETYPE_SUBTRACT);
	CASE(NODETYPE_MULTIPLY);
	CASE(NODETYPE_DIVIDE);
	CASE(NODETYPE_MODULO);
	CASE(NODETYPE_COMMA);
	CASE(NODETYPE_LAND);
	CASE(NODETYPE_LOR);
	CASE(NODETYPE_NEGATE);
	CASE(NODETYPE_NOT);
	CASE(NODETYPE_PLUS);
	CASE(NODETYPE_COMPLEMENT);
	CASE(NODETYPE_VARIABLE);
	CASE(NODETYPE_VALUE);
	CASE(NODETYPE_REFERENCE);
	CASE(NODETYPE_LESS);
	CASE(NODETYPE_LESS_EQUAL);
	CASE(NODETYPE_GREATER);
	CASE(NODETYPE_GREATER_EQUAL);
	CASE(NODETYPE_EQUAL);
	CASE(NODETYPE_NOT_EQUAL);
	CASE(NODETYPE_BOR);
	CASE(NODETYPE_BAND);
	CASE(NODETYPE_XOR);
	CASE(NODETYPE_TERNARY);
	CASE(NODETYPE_COLON);
	CASE(NODETYPE_LSHIFT);
	CASE(NODETYPE_RSHIFT);
	default:
		return "???";
	}
#undef CASE
}
#endif


static void dumpindent(int level)
{
	int i;

	for (i = 0; i < level; i++)
		fputs("  ", stderr);
}


static void dumpbinary(ExprEvalNode *node, int level, const char *op)
{
	dumpindent(level);
	fprintf(stderr, "%s\n", op);
	expreval_node_dump(node->u.binary.m_lhs, level + 1);
	expreval_node_dump(node->u.binary.m_rhs, level + 1);
}


static void dumpternary(ExprEvalNode *node, int level, const char *op)
{
	dumpindent(level);
	fprintf(stderr, "%s\n", op);
	expreval_node_dump(node->u.ternary.m_lhs, level + 1);
	expreval_node_dump(node->u.ternary.m_rhs, level + 1);
}


static void dumpassign(ExprEvalNode *node, int level, const char *op)
{
	dumpindent(level);
	fprintf(stderr, "%s\n", op);
	dumpindent(level + 1);
	fprintf(stderr, "%s\n", node->u.assign.m_var->m_name);
	expreval_node_dump(node->u.assign.m_rhs, level + 1);
}


void expreval_node_dump(ExprEvalNode *node, int level)
{
	if (node == NULL)
		return;
	switch (node->type)
	{
	case NODETYPE_FUNCTION:
		dumpindent(level);
		fprintf(stderr, "function '%s'\n", node->u.function.m_function->name);
		break;
	case NODETYPE_ASSIGN:
		dumpassign(node, level, "=");
		break;
	case NODETYPE_ASSIGN_ADD:
		dumpassign(node, level, "+=");
		break;
	case NODETYPE_ASSIGN_SUBTRACT:
		dumpassign(node, level, "-=");
		break;
	case NODETYPE_ASSIGN_MULTIPLY:
		dumpassign(node, level, "*=");
		break;
	case NODETYPE_ASSIGN_DIVIDE:
		dumpassign(node, level, "/=");
		break;
	case NODETYPE_ASSIGN_MODULO:
		dumpassign(node, level, "%=");
		break;
	case NODETYPE_ASSIGN_LSHIFT:
		dumpassign(node, level, "<<=");
		break;
	case NODETYPE_ASSIGN_RSHIFT:
		dumpassign(node, level, ">>=");
		break;
	case NODETYPE_ASSIGN_XOR:
		dumpassign(node, level, "^=");
		break;
	case NODETYPE_ASSIGN_BOR:
		dumpassign(node, level, "|=");
		break;
	case NODETYPE_ASSIGN_BAND:
		dumpassign(node, level, "&=");
		break;
	case NODETYPE_ADD:
		dumpbinary(node, level, "+");
		break;
	case NODETYPE_SUBTRACT:
		dumpbinary(node, level, "-");
		break;
	case NODETYPE_MULTIPLY:
		dumpbinary(node, level, "*");
		break;
	case NODETYPE_DIVIDE:
		dumpbinary(node, level, "/");
		break;
	case NODETYPE_MODULO:
		dumpbinary(node, level, "%");
		break;
	case NODETYPE_LAND:
		dumpbinary(node, level, "&&");
		break;
	case NODETYPE_BAND:
		dumpbinary(node, level, "&");
		break;
	case NODETYPE_LOR:
		dumpbinary(node, level, "||");
		break;
	case NODETYPE_BOR:
		dumpbinary(node, level, "|");
		break;
	case NODETYPE_EXPONENT:
		dumpbinary(node, level, "^ (pow)");
		break;
	case NODETYPE_XOR:
		dumpbinary(node, level, "^ (xor)");
		break;
	case NODETYPE_LESS:
		dumpbinary(node, level, "<");
		break;
	case NODETYPE_LESS_EQUAL:
		dumpbinary(node, level, "<=");
		break;
	case NODETYPE_GREATER:
		dumpbinary(node, level, ">");
		break;
	case NODETYPE_GREATER_EQUAL:
		dumpbinary(node, level, ">=");
		break;
	case NODETYPE_EQUAL:
		dumpbinary(node, level, "==");
		break;
	case NODETYPE_NOT_EQUAL:
		dumpbinary(node, level, "!=");
		break;
	case NODETYPE_COMMA:
		dumpbinary(node, level, ",");
		break;
	case NODETYPE_LSHIFT:
		dumpbinary(node, level, "<<");
		break;
	case NODETYPE_RSHIFT:
		dumpbinary(node, level, ">>");
		break;
	case NODETYPE_TERNARY:
		dumpternary(node, level, "?");
		break;
	case NODETYPE_COLON:
		dumpternary(node, level, ":");
		break;
	case NODETYPE_NEGATE:
		dumpindent(level);
		fprintf(stderr, "- (unary)\n");
		expreval_node_dump(node->u.unary.m_rhs, level + 1);
		break;
	case NODETYPE_NOT:
		dumpindent(level);
		fprintf(stderr, "! (unary)\n");
		expreval_node_dump(node->u.unary.m_rhs, level + 1);
		break;
	case NODETYPE_PLUS:
		dumpindent(level);
		fprintf(stderr, "+ (unary)\n");
		expreval_node_dump(node->u.unary.m_rhs, level + 1);
		break;
	case NODETYPE_COMPLEMENT:
		dumpindent(level);
		fprintf(stderr, "~ (unary)\n");
		expreval_node_dump(node->u.unary.m_rhs, level + 1);
		break;
	case NODETYPE_VARIABLE:
		dumpindent(level);
		fprintf(stderr, "variable '%s'\n", node->u.variable.m_var->m_name);
		break;
	case NODETYPE_VALUE:
		dumpindent(level);
		fprintf(stderr, "value '%ld'\n", node->u.value.m_val);
		break;
	case NODETYPE_REFERENCE:
		dumpindent(level);
		fprintf(stderr, "& '%s'\n", node->u.reference.m_var ? node->u.reference.m_var->m_name : "(null)");
		expreval_node_dump(node->u.reference.m_rhs, level + 1);
		break;
	default:
		assert(0);
		break;
	}
}
