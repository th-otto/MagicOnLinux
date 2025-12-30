#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include "expreval.h"
#include "expreval-internal.h"

#define expreval_value_setzero(expr, v) (v) = 0
#define expreval_value_setone(expr, v) (v) = 1

#ifdef _DEBUG
#define DBG(x) fprintf(x)
#else
#define DBG(x)
#endif
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define _(x) x


/* with base 2, this can be as long as the number of bits */
/* we also may need 2 more chars for the prefix */
#define INT_BITS_STRLEN_BOUND(b) (b)
#define INT_STRLEN_BOUND(t) (2 + INT_BITS_STRLEN_BOUND(sizeof(t) * CHAR_BIT) + 1)
#define INT_BUFSIZE_BOUND(t) (INT_STRLEN_BOUND(t) + 1)

#define ISALPHA(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
#define ISALNUM(c) ((c) == '_' || ISALPHA((c)) || ISDIGIT((c)))
#define ISSPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == '\f' || (c) == '\v')
#define ISXDIGIT(c) (ISDIGIT(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))

/*
 * override GLib assertions to be only used in debug code
 */
#undef g_assert_not_reached
#undef g_assert
#define g_assert_not_reached()          assert(0)
#define g_assert(expr)                  assert(expr)


#if !defined(EXRPEVAL_USE_GLIB) || !EXRPEVAL_USE_GLIB

#if !EXPREVAL_SILENT
static char *g_strdup(const char *str)
{
	char *p = g_new(char, strlen(str) + 1);
	return strcpy(p, str);
}

static char *g_strndup(const char *str, size_t n)
{
	char *p = g_new(char, n + 1);
	strncpy(p, str, n);
	p[n] = '\0';
	return p;
}
#endif

#endif /* EXRPEVAL_USE_GLIB */


void libnls_free_expression(ExprEvalNode *node)
{
	if (node != NULL)
	{
		switch (node->type)
		{
		case NODETYPE_FUNCTION:
			node->u.function.m_args = NULL;
			break;
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
			libnls_free_expression(node->u.assign.m_rhs);
			break;
		case NODETYPE_ADD:
		case NODETYPE_SUBTRACT:
		case NODETYPE_MULTIPLY:
		case NODETYPE_DIVIDE:
		case NODETYPE_MODULO:
		case NODETYPE_LOR:
		case NODETYPE_LAND:
		case NODETYPE_BOR:
		case NODETYPE_BAND:
		case NODETYPE_XOR:
		case NODETYPE_LESS:
		case NODETYPE_LESS_EQUAL:
		case NODETYPE_GREATER:
		case NODETYPE_GREATER_EQUAL:
		case NODETYPE_EQUAL:
		case NODETYPE_NOT_EQUAL:
		case NODETYPE_COMMA:
		case NODETYPE_LSHIFT:
		case NODETYPE_RSHIFT:
			libnls_free_expression(node->u.binary.m_lhs);
			libnls_free_expression(node->u.binary.m_rhs);
			break;
		case NODETYPE_TERNARY:
		case NODETYPE_COLON:
			libnls_free_expression(node->u.ternary.m_lhs);
			libnls_free_expression(node->u.ternary.m_rhs);
			break;
		case NODETYPE_NEGATE:
		case NODETYPE_PLUS:
		case NODETYPE_COMPLEMENT:
		case NODETYPE_NOT:
			libnls_free_expression(node->u.unary.m_rhs);
			break;
		case NODETYPE_VARIABLE:
			/* nothing to do */
			break;
		case NODETYPE_VALUE:
			expreval_value_free(node->u.value.m_val);
			break;
		case NODETYPE_REFERENCE:
			libnls_free_expression(node->u.reference.m_rhs);
			break;
		default:
			g_assert_not_reached();
			break;
		}
		g_free(node);
	}
}



#if !EXPREVAL_SILENT
static void expreval_print_error(const char *expr, const expreval_error_info *info)
{
	size_t i;
	gboolean show_pos = FALSE;
	
	switch (info->type)
	{
	case EXPREVAL_ERROR_NONE:
		fprintf(stderr, _("No error\n"));
		break;

	case EXPREVAL_ERROR_NOTFOUND:
		fprintf(stderr, _("Identifier not found\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_NULLPOINTER:
		fprintf(stderr, _("Null pointer passed\n"));
		break;

	case EXPREVAL_ERROR_MATH:
		fprintf(stderr, _("Math error %d: %s: %s\n"), info->err_no, info->detail ? info->detail : "", strerror(info->err_no));
		break;

	case EXPREVAL_ERROR_DIVIDE_BY_ZERO:
		fprintf(stderr, _("Division by zero\n"));
		break;

	case EXPREVAL_ERROR_EMPTY_EXPRESSION:
		fprintf(stderr, _("Empty Expression\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_GARBAGE:
		fprintf(stderr, _("garbage at end of expression\n"));
		show_pos = TRUE;
		break;

#if !EXPREVAL_REDUCED
	case EXPREVAL_ERROR_ALREADY_EXISTS:
		fprintf(stderr, _("Identifier already exists\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_INVALID_ARGUMENT_COUNT:
		fprintf(stderr, _("Invalid argument count\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_INVALID_ARGUMENT_TYPE:
		fprintf(stderr, _("Invalid argument type\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_CONSTANT_ASSIGN:
		fprintf(stderr, _("Assignment to constant\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_CONSTANT_REFERENCE:
		fprintf(stderr, _("Constant passed by reference\n"));
		show_pos = TRUE;
		break;
#endif

	case EXPREVAL_ERROR_SYNTAX:
		if (info->detail)
			fprintf(stderr, _("Syntax Error: %s\n"), info->detail);
		else
			fprintf(stderr, _("Syntax Error\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_MISSING_PARENTHESIS:
		fprintf(stderr, _("Missing closing ')' parenthesis\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_UNMATCHED_PARENTHESIS:
		fprintf(stderr, _("Unmatched closing ')' parenthesis\n"));
		show_pos = TRUE;
		break;

#if !EXPREVAL_REDUCED
	case EXPREVAL_ERROR_LVALUE_NEEDED:
		fprintf(stderr, _("LValue needed\n"));
		show_pos = TRUE;
		break;
#endif

	case EXPREVAL_ERROR_RANGE:
		fprintf(stderr, _("Value out of range: %s\n"), info->detail);
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_STACKOVF:
		fprintf(stderr, _("Stack overflow\n"));
		show_pos = TRUE;
		break;

	case EXPREVAL_ERROR_STROVF:
		fprintf(stderr, _("String overflow\n"));
		show_pos = FALSE;
		break;

	case EXPREVAL_ERROR_ASSERT:
		fprintf(stderr, _("internal error\n"));
		show_pos = FALSE;
		break;

	default:
		fprintf(stderr, _("Unknown Error %d\n"), (int) info->type);
		break;
	}
	if (show_pos)
	{
		fprintf(stderr, _("Expression: %s\n"), expr);
		for (i = 0; i < 12 + (unsigned int)info->start; i++)
			fprintf(stderr, " ");
		fprintf(stderr, "^--\n");
	}
}
#endif


static void parser_seterror(Parser *parser, expreval_error type, size_t start, size_t end, const char *detail)
{
	if (parser->error.type != EXPREVAL_ERROR_NONE)
	{
		DBG((stderr, "attempting to overwrite error already set\n"));
#if !EXPREVAL_SILENT
		expreval_print_error(parser->expr_string, &parser->error);
#endif
	}
	parser->error.type = type;
#if !EXPREVAL_SILENT
	{
		char *str;
		parser->error.err_no = errno;
		parser->error.start = start;
		parser->error.end = end;
		if (detail == NULL && end > start)
			str = g_strndup(parser->expr_string + start, end - start);
		else
			str = g_strdup(detail);
		g_free(parser->error.detail);
		parser->error.detail = str;
	}
#else
	UNUSED(start);
	UNUSED(end);
	UNUSED(detail);
#endif
}


/* Constructor */
Parser *libnls_expreval_parser_new(void)
{
	Parser *parser;

	parser = g_new0(Parser, 1);
	if (parser == NULL)
		return NULL;
	parser->error.type = EXPREVAL_ERROR_NONE;
#if !EXPREVAL_SILENT
	parser->error.err_no = 0;
	parser->error.start = 0;
	parser->error.end = 0;
	parser->error.detail = NULL;
	parser->expr_string = NULL;
	parser->string_end = 0;
#endif
	return parser;
}


/* Destructor */
void libnls_expreval_parser_delete(Parser *parser)
{
	if (parser != NULL)
	{
#if !EXPREVAL_SILENT
		g_free(parser->error.detail);
#endif
		g_free(parser);
	}
}


#if !EXPREVAL_SILENT
void libnls_expreval_print_error(Parser *parser)
{
	expreval_print_error(parser->expr_string, &parser->error);
}
#endif


static size_t parse_expression(Parser *parser, size_t pos, ExprEvalNode **top);
static size_t parse_statement_list(Parser *parser, size_t pos, ExprEvalNode **top);


/* Constructor */
static ExprEvalNode *expreval_node_new(nodetype type)
{
	ExprEvalNode *node;
	
	node = g_new0(ExprEvalNode, 1);
	if (node != NULL)
		node->type = type;
	return node;
}


#define parser_seterror_range(parser, err, from, to, detail) \
	parser_seterror(parser, err, from, to, detail)
#define parser_seterror_pos(parser, err, pos, detail) \
	parser_seterror_range(parser, err, pos, pos, detail)

static gboolean make_unary(ExprEvalNode **top, nodetype type, ExprEvalNode *r)
{
	ExprEvalNode *node = expreval_node_new(type);
	
	if (node == NULL)
	{
		libnls_free_expression(r);
		return FALSE;
	}
	node->u.unary.m_rhs = r;
	*top = node;
	return TRUE;
}


/* Make a new expression with operation OP and right hand side
   RHS and left hand side lhs.  Put the result in R.  */
static gboolean make_binary(ExprEvalNode **top, nodetype type, ExprEvalNode *l, ExprEvalNode *r)
{
	ExprEvalNode *node = expreval_node_new(type);
	
	if (node == NULL)
	{
		libnls_free_expression(l);
		libnls_free_expression(r);
		*top = NULL;
		return FALSE;
	}
	node->u.binary.m_lhs = l;
	node->u.binary.m_rhs = r;
	*top = node;
	return TRUE;
}


static gboolean make_ternary(ExprEvalNode **top, nodetype type, ExprEvalNode *l, ExprEvalNode *r)
{
	ExprEvalNode *node = expreval_node_new(type);
	
	if (node == NULL)
	{
		libnls_free_expression(l);
		libnls_free_expression(r);
		*top = NULL;
		return FALSE;
	}
	node->u.ternary.m_lhs = l;
	node->u.ternary.m_rhs = r;
	*top = node;
	return TRUE;
}


static gboolean make_variable(Parser *parser, ExprEvalNode **top, const char *ident, size_t identstart)
{
	ExprEvalNode *node;
	
	/* we only have a single variable: "n" */
	if (strcmp(ident, "n") != 0)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_NOTFOUND, identstart, ident);
		return FALSE;
	}

	node = expreval_node_new(NODETYPE_VARIABLE);
	if (node == NULL)
		return FALSE;

	/* Set information */

	node->u.variable.m_var = NULL;
	*top = node;
	return TRUE;
}


static gboolean make_value(Parser *parser, ExprEvalNode **top, expreval_value value)
{
	ExprEvalNode *node = expreval_node_new(NODETYPE_VALUE);
	
	UNUSED(parser);
	if (node == NULL)
	{
		expreval_value_free(value);
		return FALSE;
	}
	node->u.value.m_val = value;
	*top = node;
	return TRUE;
}


#if !EXPREVAL_REDUCED
static gboolean make_reference(Parser *parser, ExprEvalNode **top, ExprEvalNode *r)
{
	ExprEvalNode *node = expreval_node_new(NODETYPE_REFERENCE);
	
	UNUSED(parser);
	if (node == NULL)
	{
		return FALSE;
	}
	node->u.reference.m_rhs = r;
	*top = node;
	return TRUE;
}
#endif


#if !EXPREVAL_REDUCED
static gboolean make_assignment(Parser *parser, ExprEvalNode **top, nodetype type, size_t pos, ExprEvalNode *l, ExprEvalNode *r)
{
	ExprEvalNode *node;
	const char *ident;
	
	if (l->type != NODETYPE_VARIABLE)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_LVALUE_NEEDED, pos, NULL);
		libnls_free_expression(r);
		return FALSE;
	}
	
	node = expreval_node_new(type);
	if (node == NULL)
	{
		libnls_free_expression(r);
		return FALSE;
	}
	
	/* Determine variable name */
	ident = l->u.variable.m_var->m_name;

	if (strcmp(ident, "n") != 0)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_NOTFOUND, pos, ident);
		libnls_free_expression(node);
		libnls_free_expression(r);
		return FALSE;
	}
	
	libnls_free_expression(l);
	
	node->u.assign.m_var = NULL;
	node->u.assign.m_rhs = r;
	*top = node;
	return TRUE;
}
#endif


static void parse_error(Parser *parser, expreval_error err, size_t pos, const char *detail)
{
	parser_seterror_pos(parser, err, pos, detail);
}


static void syntax_error(Parser *parser, size_t pos, const char *detail)
{
	parse_error(parser, EXPREVAL_ERROR_SYNTAX, pos, detail);
}


static size_t skip_whitespace(Parser *parser, size_t pos)
{
	const char *str = parser->expr_string;
	
	while (pos < parser->string_end)
	{
		switch (str[pos])
		{
		case ' ':
		case '\t':
		case '\v':
			break;
			
			/* Newline ends comment */
		case '\r':
		case '\n':
			break;

		default:
			return pos;
		}
		pos++;
	}

	return pos;
}


/* Parse an expression string */
expreval_error libnls_expreval_parse_string(Parser *parser, ExprEvalNode **expr, const char *exstr, size_t len)
{
	size_t pos;

	parser->expr_string = exstr;
	if (exstr == NULL)
	{
		parser_seterror(parser, EXPREVAL_ERROR_EMPTY_EXPRESSION, 0, 0, NULL);
		return EXPREVAL_ERROR_EMPTY_EXPRESSION;
	}

	if (len == (size_t)-1)
	{
		len = strlen(exstr);
	}
	parser->string_end = len;
	
	/* Parse the expression */
	
	*expr = NULL;
	pos = parse_statement_list(parser, 0, expr);
	if (parser->error.type == EXPREVAL_ERROR_NONE)
	{
		if (*expr == NULL)
		{
			parser_seterror(parser, EXPREVAL_ERROR_EMPTY_EXPRESSION, pos, pos, NULL);
		} else if (pos < parser->string_end && parser->expr_string[pos] != ';' && parser->expr_string[pos] != '\0')
		{
			if (parser->expr_string[pos] == ')')
				parser_seterror_range(parser, EXPREVAL_ERROR_UNMATCHED_PARENTHESIS, pos, pos + 1, NULL);
			else
				parser_seterror_range(parser, EXPREVAL_ERROR_GARBAGE, pos, pos + 1, NULL);
		}
	}
	if (parser->error.type != EXPREVAL_ERROR_NONE)
	{
		libnls_free_expression(*expr);
		*expr = NULL;
	}
	
	return parser->error.type;
}


#define NEXTCHAR_IS(c) \
	(pos < parser->string_end && parser->expr_string[pos] == (c))

#define NEXT2CHAR_ARE(c1, c2) \
	((pos + 1) < parser->string_end && parser->expr_string[pos] == (c1) && parser->expr_string[pos+1] == (c2))

#define NEXT3CHAR_ARE(c1, c2, c3) \
	((pos + 2) < parser->string_end && parser->expr_string[pos] == (c1) && parser->expr_string[pos+1] == (c2) && parser->expr_string[pos+2] == (c3))


#if !EXPREVAL_REDUCED
static gboolean gather_args(Parser *parser, ExprEvalNode *func, size_t pos, ExprEvalNode *arg)
{
	if (arg == NULL)
		return TRUE;
	if (arg->type == NODETYPE_COMMA)
	{
		if (!gather_args(parser, func, pos, arg->u.binary.m_lhs))
			return FALSE;
		arg->u.binary.m_lhs = NULL;
		if (!gather_args(parser, func, pos, arg->u.binary.m_rhs))
			return FALSE;
		arg->u.binary.m_rhs = NULL;
		/*
		 * delete the comma node, but not its childs.
		 */
		libnls_free_expression(arg);
		return TRUE;
	}
	if (arg->type == NODETYPE_REFERENCE)
	{
		ExprEvalNode *ref;
		
		if ((ref = arg->u.reference.m_rhs) == NULL || ref->type != NODETYPE_VARIABLE)
		{
			syntax_error(parser, pos, _("variable name expected"));
			return FALSE;
		}
		arg->u.reference.m_var = ref->u.variable.m_var;
		libnls_free_expression(ref);
		arg->u.reference.m_rhs = NULL;
		if (arg->u.reference.m_var->m_constant)
		{
			parser_seterror_pos(parser, EXPREVAL_ERROR_CONSTANT_REFERENCE, pos, arg->u.reference.m_var->m_name);
			return FALSE;
		}
	}
	return TRUE;
}


static size_t function_args_len(ExprEvalNode *m_args)
{
	UNUSED(m_args);
	return 0;
}


static gboolean make_function(Parser *parser, ExprEvalNode **top, const char *ident, size_t identstart, ExprEvalNode *argument_list)
{
	ExprEvalNode *func;
	size_t argc;
	const ExprEvalFunctionDef *def;
	
	def = NULL; /* expreval_functionlist_get(expreval_expression_get_functionlist(parser), ident); */

	if (def == NULL)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_NOTFOUND, identstart, ident);
		return FALSE;
	}
	func = expreval_node_new(NODETYPE_FUNCTION);
	func->u.function.m_function = def;

	if (!gather_args(parser, func, identstart, argument_list))
	{
		/* delete the function and the already handled argument nodes */
		libnls_free_expression(func);
		/* delete the remaining arguments */
		libnls_free_expression(argument_list);
		return FALSE;
	}
	
	/* Check argument count */
	argc = function_args_len(func->u.function.m_args);
	if (def->m_argMin != ((size_t)-1) && argc < def->m_argMin)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_INVALID_ARGUMENT_COUNT, identstart, def->name);
		libnls_free_expression(func);
		return FALSE;
	}

	if (def->m_argMax != ((size_t)-1) && argc > def->m_argMax && !def->varargs)
	{
		parser_seterror_pos(parser, EXPREVAL_ERROR_INVALID_ARGUMENT_COUNT, identstart, def->name);
		libnls_free_expression(func);
		return FALSE;
	}

	*top = func;
	return TRUE;
}
#endif


static gboolean string_looks_like_number(const char *s, size_t len, const char **endptr, expreval_value *value, int *value_err)
{
	const char *cp = s;
	size_t digits = 0;
	gboolean negative;

	*value = 0;
	*value_err = 0;
	if (cp == NULL)
	{
		if (endptr)
			*endptr = NULL;
		return TRUE;
	}
	if (len == (size_t)-1)
		len = strlen(cp);
	if (len == 0)
	{
		if (endptr)
			*endptr = s;
		return TRUE;
	}
	negative = FALSE;
	if (len && (*cp == '-' || *cp == '+'))
	{
		len--;
		negative = *cp++ == '-';
	}
	if (len >= 2 && cp[0] == '0' && (cp[1] == 'x' || cp[1] == 'X'))
	{
		cp += 2;
		len -= 2;
		while (len && ISXDIGIT(*cp))
		{
			*value = *value * 16 + (*cp >= 'a') ? (*cp - 'a' + 10) : (*cp >= 'A') ? (*cp - 'A' + 10) : (*cp - '0');
			cp++;
			len--;
			digits++;
		}
	} else
	{
		while (len && ISDIGIT(*cp))
		{
			*value = *value * 10 + (*cp - '0');
			len--;
			cp++;
			digits++;
		}
	}
	if (negative)
		*value = -(*value);
	if (endptr)
		*endptr = cp;
	return digits != 0;
}


/*
 * Precedences:
 *
 * 1.  Left-to-Right
 *     ::                           Scope (not handled here)
 * 2:  Left-to-Right
 *     ++ --                        Suffix increment/decrement (not handled here)
 *     ()                           Function call
 *     []                           Array subscripting (not handled here)
 *     .  ->                        Element selection
 *     typeid(), <const_cast) etc   Type casts
 * 3.  Right-to-Left
 *     ++ --                        Prefix increment/decrement (not handled here)
 *     + -                          Unary
 *     ! ~                          Logical and bitwise NOT
 *     (type)                       Cast (not handled here)
 *     *                            Indirection (dereference) (not handled here)
 *     &                            Address-of (not handled here)
 *     sizeof                       Size-Of (not handled here)
 *     new, delete                  Dynamic allocation (not handled here)
 * 4.  Left-to-Right
 *     .* ->*                       Pointer to Member (not handled here)
 * 5.  Left-to-Right
 *     * / %                        Multiplication, Division, Modulo
 * 6.  Left-to-Right
 *     + -                          Addition, Subtraction
 * 7.  Left-to-Right
 *     << >>                        Bitwise left and right shift
 * 8.  Left-to-Right
 *     < <= > >=                    Comparisons
 * 9.  Left-to-Right
 *     == !=                        Equality
 * 10. Left-to-Right
 *     &                            Bitwise and
 * 11. Left-to-Right
 *     ^                            Bitwise xor
 * 12. Left-to-Right
 *     |                            Bitwise or
 * 13. Left-to-Right
 *     &&                           Logical and
 * 14. Left-to-Right
 *     ||                           Logical or
 * 15. Right-to-Left
 *     :?                           Ternary
 *     = += -= etc                  Assignments
 * 16. Right-to-Left
 *     throw                        Throw operatoror (not handled here)
 * 17. Left-to-Right
 *     ,                            Comma
 */

/* Handle bare operands and ( expr ) syntax.  */

static size_t parse_primary(Parser *parser, size_t pos, ExprEvalNode **top)
{
	const char *exstr = parser->expr_string;
	const char *end;
	int value_err;
	expreval_value value;
	
	pos = skip_whitespace(parser, pos);
	
	if (pos < parser->string_end &&
 		(exstr[pos] == '_' || ISALPHA(exstr[pos])))
	{
#if !EXPREVAL_REDUCED
		/* We are an identifier */
		size_t identstart = pos;
		size_t identend;
		gboolean foundname = TRUE;	/* Found name part */
		char *ident;
		
		/*
		 * Search for name, then period, etc.
		 * An identifier can be multiple parts.  Each part
		 * is formed as an identifier and seperated by a period,
		 * An identifier can not end in a period
		 *
		 * color1.red : 1 identifier token
		 * color1. : color1 is identifier, . begins new token
		 * color1.1red : Not value (part 2 is not right)
		 */
		while (foundname)
		{
			/* Part before period */
			while (ISALNUM(exstr[pos]))
				pos++;

			/* Is there a period */
			if (exstr[pos] == '.')
			{
				pos++;

				/* There is a period, look for the name again */
				if (exstr[pos] == '_' || ISALPHA(exstr[pos]))
				{
					foundname = TRUE;
				} else
				{
					/* No name after period */
					foundname = FALSE;

					/* Remove period from identifier */
					pos--;
				}
			} else
			{
				/* No period after name, so no new name */
				foundname = FALSE;
			}
		}

		identend = pos;
		ident = g_strndup(exstr + identstart, identend - identstart);
		pos = skip_whitespace(parser, pos);
		
		if (NEXTCHAR_IS('('))
		{
			ExprEvalNode *lhs = NULL, *rhs = NULL;
			gboolean needarg = FALSE;
			
			pos = skip_whitespace(parser, pos + 1);
			while (needarg || !NEXTCHAR_IS(')'))
			{
				rhs = NULL;
				pos = parse_expression(parser, pos, &rhs);
				if (pos == 0)
				{
					libnls_free_expression(lhs);
					g_free(ident);
					return 0;
				}
				if (lhs == NULL)
				{
					lhs = rhs;
				} else if (!make_binary(&lhs, NODETYPE_COMMA, lhs, rhs))
				{
					g_free(ident);
					return 0;
				}
				pos = skip_whitespace(parser, pos);
				if (NEXTCHAR_IS(','))
				{
					needarg = TRUE;
					pos = skip_whitespace(parser, pos + 1);
				} else
				{
					pos = skip_whitespace(parser, pos);
					break;
				}
			}
			if (!NEXTCHAR_IS(')'))
			{
				syntax_error(parser, pos, _("Missing closing ')' parenthesis"));
				libnls_free_expression(lhs);
				g_free(ident);
				return 0;
			}
			if (!make_function(parser, top, ident, identstart, lhs))
			{
				g_free(ident);
				return 0;
			}
			g_free(ident);
			return pos + 1;
		} else
		{
			if (!make_variable(parser, top, ident, identstart))
			{
				g_free(ident);
				return 0;
			}
			g_free(ident);
		}
#else
		if (exstr[pos] != 'n' || !make_variable(parser, top, "n", pos))
		{
			return pos;
		}
		pos++;
#endif
		return pos;
	}

	if (pos < parser->string_end && string_looks_like_number(exstr + pos, parser->string_end - pos, &end, &value, &value_err))
	{
		size_t start;

		/* We are a value */
		start = pos;
	
		/* Create token */
		pos = end - exstr;
		if (pos > start)
		{
			if (value_err)
			{
				expreval_value_free(value);
				parser_seterror(parser, EXPREVAL_ERROR_RANGE, start, pos, NULL);
				return 0;
			}
			if (!make_value(parser, top, value))
				return 0;
		} else
		{
			/* didn't parse anything */
			if (exstr[pos] == '.')
			{
				syntax_error(parser, pos, _("operand expected"));
				return 0;
			} else
			{
				/* should not happen, only other chars are digits */
				g_assert_not_reached();
			}
		}
		return pos;
	}
	
	if (NEXTCHAR_IS('('))
	{
		pos = parse_expression(parser, pos + 1, top);
		if (pos == 0)
			return 0;
		pos = skip_whitespace(parser, pos);
		if (!NEXTCHAR_IS(')'))
		{
			syntax_error(parser, pos, _("Missing closing ')' parenthesis"));
			return 0;
		}
		pos++;
		return pos;
	}
	
	if (NEXTCHAR_IS('"'))
	{
		/* TODO: parse string literal here */
	}
	
	if (NEXTCHAR_IS('.'))
	{
		syntax_error(parser, pos, _("operator '.' unimplemented"));
		return 0;
	}
	
	if (NEXT2CHAR_ARE('-', '>'))
	{
		syntax_error(parser, pos, _("operator '->' unimplemented"));
		return 0;
	}
	
	syntax_error(parser, pos, _("operand expected"));
	return 0;
}


static size_t parse_unary(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;

	pos = skip_whitespace(parser, pos);
#if !EXPREVAL_REDUCED
	if (NEXT2CHAR_ARE('+', '+'))
	{
		/* PREINC */
		syntax_error(parser, pos, _("operator '++' unimplemented"));
		return 0;
	} else
	if (NEXT2CHAR_ARE('-', '-'))
	{
		/* PREDEC */
		syntax_error(parser, pos, _("operator '--' unimplemented"));
		return 0;
	} else
#endif
	if (NEXTCHAR_IS('+')
#if !EXPREVAL_REDUCED
		&& !NEXT2CHAR_ARE('+', '=')
#endif
		)
	{
		pos = parse_primary(parser, pos + 1, &rhs);
		if (pos == 0)
		{
			libnls_free_expression(rhs);
			return 0;
		}
		if (!make_unary(top, NODETYPE_PLUS, rhs))
			return 0;
	} else if (NEXTCHAR_IS('-')
#if !EXPREVAL_REDUCED
		&& !NEXT2CHAR_ARE('-', '=')
#endif
		)
	{
		pos = parse_primary(parser, pos + 1, &rhs);
		if (pos == 0)
		{
			libnls_free_expression(rhs);
			return 0;
		}
		if (!make_unary(top, NODETYPE_NEGATE, rhs))
			return 0;
	} else if (NEXTCHAR_IS('~')
#if !EXPREVAL_REDUCED
		&& !NEXT2CHAR_ARE('~', '=')
#endif
		)
	{
		pos = parse_unary(parser, pos + 1, &rhs);
		if (pos == 0)
		{
			libnls_free_expression(rhs);
			return 0;
		}
		if (!make_unary(top, NODETYPE_COMPLEMENT, rhs))
			return 0;
	} else if (NEXTCHAR_IS('!')
#if !EXPREVAL_REDUCED
		&& !NEXT2CHAR_ARE('!', '=')
#endif
		)
	{
		pos = parse_unary(parser, pos + 1, &rhs);
		if (pos == 0)
		{
			libnls_free_expression(rhs);
			return 0;
		}
		if (!make_unary(top, NODETYPE_NOT, rhs))
			return 0;
#if !EXPREVAL_REDUCED
	} else if (NEXTCHAR_IS('&')
		&& !NEXT2CHAR_ARE('&', '=')
		)
	{
		pos = parse_unary(parser, pos + 1, &rhs);
		if (rhs == NULL)
			return pos;
		if (!make_reference(parser, top, rhs))
			return 0;
	} else if (NEXTCHAR_IS('*') && !NEXT2CHAR_ARE('*', '='))
	{
		syntax_error(parser, pos, _("operator '*' (dereference) unimplemented"));
		return 0;
#endif
	} else
	{
		pos = parse_primary(parser, pos, top);
	}
	return pos;
}


/* Handle : operator (pattern matching). */

static size_t parse_colon(Parser *parser, size_t pos, ExprEvalNode **top)
{
	pos = parse_unary(parser, pos, top);
	if (pos == 0)
		return 0;
	return pos;
}


/* Handle ^ operator (exponent). */

static size_t parse_exponent(Parser *parser, size_t pos, ExprEvalNode **top)
{
	pos = parse_colon(parser, pos, top);
	if (pos == 0)
		return 0;
	return pos;
}


/* Handle *, /, % operators. */

static size_t parse_mul(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_exponent(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS('*'))
		{
			rhs = NULL;
			pos = parse_exponent(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_MULTIPLY, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXTCHAR_IS('/'))
		{
			rhs = NULL;
			pos = parse_exponent(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_DIVIDE, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXTCHAR_IS('%'))
		{
			rhs = NULL;
			pos = parse_exponent(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_MODULO, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle +, - operators. */

static size_t parse_plus(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_mul(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS('+') && !NEXT2CHAR_ARE('+', '=') && !NEXT2CHAR_ARE('+', '+'))
		{
			rhs = NULL;
			pos = parse_mul(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_ADD, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXTCHAR_IS('-') && !NEXT2CHAR_ARE('-', '=') && !NEXT2CHAR_ARE('-', '-'))
		{
			rhs = NULL;
			pos = parse_mul(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_SUBTRACT, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle <<, >> operators. */

static size_t parse_shift(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_plus(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXT2CHAR_ARE('<', '<') && !NEXT3CHAR_ARE('<', '<', '='))
		{
			rhs = NULL;
			pos = parse_plus(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_LSHIFT, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXT2CHAR_ARE('>', '>') && !NEXT3CHAR_ARE('>', '>', '='))
		{
			rhs = NULL;
			pos = parse_plus(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_RSHIFT, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle comparisons. */

static size_t parse_comparison(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_shift(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXT2CHAR_ARE('<', '='))
		{
			rhs = NULL;
			pos = parse_shift(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_LESS_EQUAL, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXT2CHAR_ARE('>', '='))
		{
			rhs = NULL;
			pos = parse_shift(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_GREATER_EQUAL, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXT2CHAR_ARE('!', '='))
		{
			rhs = NULL;
			pos = parse_shift(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_NOT_EQUAL, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXT2CHAR_ARE('=', '='))
		{
			rhs = NULL;
			if (NEXT2CHAR_ARE('=', '='))
				pos += 2;
			else
				pos++;
			pos = parse_shift(parser, pos, &rhs);
			if (!make_binary(top, NODETYPE_EQUAL, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXTCHAR_IS('<') && !NEXT2CHAR_ARE('<', '<'))
		{
			rhs = NULL;
			pos = parse_shift(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_LESS, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else if (NEXTCHAR_IS('>') && !NEXT2CHAR_ARE('>', '>'))
		{
			rhs = NULL;
			pos = parse_shift(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_GREATER, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle & operator. */

static size_t parse_band(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_comparison(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS('&') && !NEXT2CHAR_ARE('&', '&'))
		{
			rhs = NULL;
			pos = parse_comparison(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_BAND, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle ^ (xor) operator. */

static size_t parse_xor(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_band(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS('^') && !NEXT2CHAR_ARE('^', '='))
		{
			rhs = NULL;
			pos = parse_band(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_XOR, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle | operator. */

static size_t parse_bor(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_xor(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS('|') && !NEXT2CHAR_ARE('|', '|'))
		{
			rhs = NULL;
			pos = parse_xor(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_BOR, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle && operator. */

static size_t parse_land(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_bor(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXT2CHAR_ARE('&', '&'))
		{
			rhs = NULL;
			pos = parse_bor(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_LAND, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle || operator. */

static size_t parse_lor(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = parse_land(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXT2CHAR_ARE('|', '|'))
		{
			rhs = NULL;
			pos = parse_land(parser, pos + 2, &rhs);
			if (!make_binary(top, NODETYPE_LOR, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


/* Handle assignments, and ?: */
static size_t parse_assignment(Parser *parser, size_t pos, ExprEvalNode **top)
{
	pos = parse_lor(parser, pos, top);
	if (pos == 0)
		return 0;
	pos = skip_whitespace(parser, pos);
	if (NEXTCHAR_IS('?'))
	{
		ExprEvalNode *rhs = NULL;
		ExprEvalNode *lhs = NULL;
		ExprEvalNode *colon = NULL;
		
		pos = parse_assignment(parser, pos + 1, &lhs);
		if (pos == 0)
		{
			libnls_free_expression(lhs);
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		pos = skip_whitespace(parser, pos);
		if (!NEXTCHAR_IS(':'))
		{
			syntax_error(parser, pos, _("':' expected"));
			libnls_free_expression(lhs);
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		pos = parse_assignment(parser, pos + 1, &rhs);
		if (pos == 0)
		{
			libnls_free_expression(rhs);
			libnls_free_expression(lhs);
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (!make_ternary(&colon, NODETYPE_COLON, lhs, rhs))
		{
			libnls_free_expression(rhs);
			libnls_free_expression(lhs);
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (!make_ternary(top, NODETYPE_TERNARY, *top, colon))
		{
			libnls_free_expression(colon);
			libnls_free_expression(rhs);
			libnls_free_expression(lhs);
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
#if !EXPREVAL_REDUCED
	} else if (NEXTCHAR_IS('=') && !NEXT2CHAR_ARE('=', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 1, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('+', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_ADD, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('-', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_ADD, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('*', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_MULTIPLY, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('/', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_DIVIDE, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('%', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_MODULO, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT3CHAR_ARE('<', '<', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 3, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_LSHIFT, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT3CHAR_ARE('>', '>', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 3, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_RSHIFT, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('^', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_XOR, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('|', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_BOR, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
	} else if (NEXT2CHAR_ARE('&', '='))
	{
		ExprEvalNode *rhs = NULL;
		size_t prevpos;

		prevpos = pos;
		pos = parse_assignment(parser, pos + 2, &rhs);
		if (!make_assignment(parser, top, NODETYPE_ASSIGN_BAND, prevpos, *top, rhs))
		{
			libnls_free_expression(*top);
			*top = NULL;
			return 0;
		}
		if (pos == 0)
			return 0;
#endif
	}
	return pos;
}


static size_t parse_expression(Parser *parser, size_t pos, ExprEvalNode **top)
{
	return parse_assignment(parser, pos, top);
}


static size_t parse_expression_list(Parser *parser, size_t pos, ExprEvalNode **top)
{
	ExprEvalNode *rhs = NULL;
	
	pos = skip_whitespace(parser, pos);
	pos = parse_expression(parser, pos, top);
	if (pos == 0)
		return 0;
	for (;;)
	{
		pos = skip_whitespace(parser, pos);
		if (NEXTCHAR_IS(','))
		{
			rhs = NULL;
			pos = parse_expression(parser, pos + 1, &rhs);
			if (!make_binary(top, NODETYPE_COMMA, *top, rhs))
				return 0;
			if (pos == 0)
				return 0;
		} else
		{
			return pos;
		}
	}
}


static size_t parse_statement_list(Parser *parser, size_t pos, ExprEvalNode **top)
{
	pos = parse_expression_list(parser, pos, top);
	pos = skip_whitespace(parser, pos);
	return pos;
}


#ifdef MAIN

/* gcc -Wall -DMAIN libnls/expreval.c libnls/expreval-exp.c libnls/expreval-silent.c libnls/expreval-print.c libnls/plurals.c */

#include "libnls.h"
#include "libnlsI.h"

static void check_n(ExprEvalNode *expr, const char *eval_str, int nplurals, unsigned long n)
{
	int id1;
	int id2;

	id1 = libnls_expreval_evaluate(expr, n);
	id2 = libnls_expreval_evaluate_string(eval_str, n);
	/* printf("%lu: %d %d\n", n, id1, id2); */
	fflush(stdout);
	if (id1 < 0 || id1 >= nplurals)
		fprintf(stderr, "error plural_eval: %d\n", id1);
	if (id2 < 0 || id2 >= nplurals)
		fprintf(stderr, "error plural_eval_string: %d\n", id2);
	if (id1 != id2)
		fprintf(stderr, "error: %d != %d\n", id1, id2);
}


static void check(const struct _nls_plural *plural)
{
	unsigned long n;
	ExprEvalNode *expr;
	char buf[200];
	int nplurals;

	if (libnls_extract_plural_expression(plural->exp, &expr, &nplurals))
	{
		printf("%s: %s: %d ", plural->id_str, plural->exp, nplurals);
		if (libnls_expreval_print_expression(expr, buf, sizeof(buf), FALSE) == FALSE)
		{
			printf("\n");
			fflush(stdout);
			fprintf(stderr, "%s: error plural_print\n", plural->id_str);
		} else
		{
			fputs(buf, stdout);
			printf("\n");
			if (strcmp(buf, plural->str) != 0)
				fprintf(stderr, "%s: eval strings differ: '%s' != '%s'\n", plural->id_str, buf, plural->str);
			for (n = 0; n != 0; n++)
			{
				check_n(expr, buf, nplurals, n);
			}
		}
	}
	libnls_free_expression(expr);
}


int main(void)
{
	const struct _nls_plural *plural;
	
	for (plural = libnls_plurals; plural->exp != NULL; plural++)
		check(plural);
	return 0;
}

#endif /* MAIN */
