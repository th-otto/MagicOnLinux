#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "expreval.h"
#include "expreval-internal.h"

/* These structs are the constant expression for the germanic plural
   form determination.  It represents the expression  "n != 1".  */
static ExprEvalNode plvar = {
	NODETYPE_VARIABLE,
	{ .variable = { NULL } }
};

static ExprEvalNode plone = {
	NODETYPE_VALUE,
	{ .value = { 1 } }
};

ExprEvalNode _libnls_germanic_plural = {
	NODETYPE_NOT_EQUAL,				/* operation */
	{ .binary = { &plvar, &plone } }
};


/* Evaluate the plural expression and return an index value.  */
static int plural_eval_recurse(const ExprEvalNode *expr, expreval_value *n, unsigned int allowed_depth)
{
	int result;
	expreval_value left_arg;
	expreval_value right_arg;

	if (allowed_depth == 0)
    	/* The allowed recursion depth is exhausted.  */
		return EXPREVAL_ERROR_STACKOVF;
	allowed_depth--;

	switch (expr->type)
	{
	case NODETYPE_VARIABLE:
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_VALUE:
		*n = expr->u.value.m_val;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_NOT:
		right_arg = *n;
		result = plural_eval_recurse(expr->u.unary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = right_arg == 0;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_PLUS:
		right_arg = *n;
		result = plural_eval_recurse(expr->u.unary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_NEGATE:
		right_arg = *n;
		result = plural_eval_recurse(expr->u.unary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = -right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_COMPLEMENT:
		right_arg = *n;
		result = plural_eval_recurse(expr->u.unary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = ~right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_LOR:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		if (left_arg != 0)
		{
			*n = TRUE;
			return EXPREVAL_ERROR_NONE;
		}
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = right_arg != 0;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_LAND:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		if (left_arg == 0)
		{
			*n = FALSE;
			return EXPREVAL_ERROR_NONE;
		}
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = right_arg != 0;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_MULTIPLY:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg * right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_DIVIDE:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		if (right_arg == 0)
			return EXPREVAL_ERROR_DIVIDE_BY_ZERO;
		*n = left_arg / right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_MODULO:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		if (right_arg == 0)
			return EXPREVAL_ERROR_DIVIDE_BY_ZERO;
		*n = left_arg % right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_ADD:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg + right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_SUBTRACT:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg - right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_LESS:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg < right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_GREATER:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg > right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_LESS_EQUAL:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg <= right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_GREATER_EQUAL:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg >= right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_EQUAL:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg == right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_NOT_EQUAL:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg != right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_BOR:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg | right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_BAND:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg & right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_XOR:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg ^ right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_LSHIFT:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg << right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_RSHIFT:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		right_arg = *n;
		result = plural_eval_recurse(expr->u.binary.m_rhs, &right_arg, allowed_depth);
		if (result < 0)
			return result;
		*n = left_arg >> right_arg;
		return EXPREVAL_ERROR_NONE;

	case NODETYPE_TERNARY:
		left_arg = *n;
		result = plural_eval_recurse(expr->u.ternary.m_lhs, &left_arg, allowed_depth);
		if (result < 0)
			return result;
		assert(expr->u.ternary.m_rhs->type == NODETYPE_COLON);
		result = plural_eval_recurse(left_arg ? expr->u.ternary.m_rhs->u.ternary.m_lhs : expr->u.ternary.m_rhs->u.ternary.m_rhs, n, allowed_depth);
		return result;

	case NODETYPE_COLON:
		/* should already be handled above */
		break;

	case NODETYPE_FUNCTION:
	case NODETYPE_ASSIGN:
	case NODETYPE_ASSIGN_ADD:
	case NODETYPE_ASSIGN_SUBTRACT:
	case NODETYPE_ASSIGN_MULTIPLY:
	case NODETYPE_ASSIGN_DIVIDE:
	case NODETYPE_ASSIGN_MODULO:
	case NODETYPE_ASSIGN_LSHIFT:
	case NODETYPE_ASSIGN_RSHIFT:
	case NODETYPE_ASSIGN_XOR:
	case NODETYPE_ASSIGN_BOR:
	case NODETYPE_ASSIGN_BAND:
	case NODETYPE_EXPONENT:
	case NODETYPE_COMMA:
	case NODETYPE_REFERENCE:
		break;
	}
	/* NOTREACHED */
	assert(0);
	return EXPREVAL_ERROR_ASSERT;
}


/* Evaluate expression */
expreval_value libnls_expreval_evaluate(ExprEvalNode *expr, expreval_value n)
{
	int result;

	result = plural_eval_recurse(expr, &n, EVAL_MAXDEPTH);
	if (result < 0)
		return result;
	return (int)n;
}

#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\n')

int libnls_extract_plural_expression(const char *nullentry, ExprEvalNode **pluralp, int *npluralsp)
{
	const char *plural;
	const char *nplurals;

	/* By default we are using the Germanic form: singular form only
	   for `one', the plural form otherwise.  Yes, this is also what
	   English is using since English is a Germanic language.  */
	*pluralp = &_libnls_germanic_plural;
	*npluralsp = 2;

	if (nullentry != NULL)
	{
		if (strncmp(nullentry, "nplurals=", 9) == 0)
		{
			const char *endp;
			unsigned long int n;

			/* First get the number.  */
			nplurals = nullentry + 9;
			while (ISSPACE((unsigned char) *nplurals))
				++nplurals;
			for (endp = nplurals, n = 0; *endp >= '0' && *endp <= '9'; endp++)
				n = n * 10 + (*endp - '0');
			if (nplurals != endp)
			{
				*npluralsp = n;
				
				plural = endp;
				if (*plural == ';')
					plural++;
				while (ISSPACE((unsigned char) *plural))
					++plural;
				if (strncmp(plural, "plural=", 7) == 0)
				{
					Parser parser;
					ExprEvalNode *expr;
					expreval_error error;

					parser.error.type = EXPREVAL_ERROR_NONE;
#if !EXPREVAL_SILENT
					parser.error.err_no = 0;
					parser.error.start = 0;
					parser.error.end = 0;
					parser.error.detail = NULL;
					parser.expr_string = NULL;
					parser.string_end = 0;
#endif
					/* Due to the restrictions bison imposes onto the interface of the
					   scanner function we have to put the input string and the result
					   passed up from the parser into the same structure which address
					   is passed down to the parser.  */
					plural += 7;
					while (ISSPACE((unsigned char) *plural))
						++plural;
					error = libnls_expreval_parse_string(&parser, &expr, plural, strlen(plural));
					if (error != EXPREVAL_ERROR_NONE)
					{
#if !EXPREVAL_SILENT
						libnls_expreval_print_error(&parser);
						g_free(parser.error.detail);
#endif
					} else
					{
#if !EXPREVAL_SILENT
						g_free(parser.error.detail);
#endif
						*pluralp = expr;
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}


