#ifndef __EXPREVAL_INTERNAL_H__
#define __EXPREVAL_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define expreval_value_free(val)


#if !defined(EXRPEVAL_USE_GLIB) || !EXRPEVAL_USE_GLIB

/*
 * re-implement some Glib functionality.
 * We are using only a small subset.
 */

typedef int gboolean;

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

/* g_free() defined in public header,
   as it needs to be used by the application
 */
#ifndef g_malloc
#define g_malloc(s) malloc(s)
#define g_free(p) free(p)
#define g_new(t, n) ((t *)g_malloc(sizeof(t) * (n)))
#define g_new0(t, n) ((t *)calloc(n, sizeof(t)))
#define g_realloc(p, s) realloc(p, s)
#endif

#endif /* EXRPEVAL_USE_GLIB */

/*
 * define EXPREVAL_REDUCED to use only a recuced set
 * of operators (no assignments etc.)
 */
#ifndef EXPREVAL_REDUCED
# define EXPREVAL_REDUCED 1
#endif

#ifndef EVAL_MAXDEPTH
# define EVAL_MAXDEPTH 100
#endif

typedef enum _nodetype {
	NODETYPE_FUNCTION,
	NODETYPE_ASSIGN,
	NODETYPE_ASSIGN_ADD,
	NODETYPE_ASSIGN_SUBTRACT,
	NODETYPE_ASSIGN_MULTIPLY,
	NODETYPE_ASSIGN_DIVIDE,
	NODETYPE_ASSIGN_MODULO,
	NODETYPE_ASSIGN_LSHIFT,
	NODETYPE_ASSIGN_RSHIFT,
	NODETYPE_ASSIGN_XOR,
	NODETYPE_ASSIGN_BOR,
	NODETYPE_ASSIGN_BAND,
	NODETYPE_ADD,
	NODETYPE_SUBTRACT,
	NODETYPE_MULTIPLY,
	NODETYPE_DIVIDE,
	NODETYPE_MODULO,
	NODETYPE_EXPONENT,
	NODETYPE_NEGATE,
	NODETYPE_NOT,
	NODETYPE_COMPLEMENT,
	NODETYPE_PLUS,
	NODETYPE_VARIABLE,
	NODETYPE_VALUE,
	NODETYPE_LOR,
	NODETYPE_LAND,
	NODETYPE_BOR,
	NODETYPE_BAND,
	NODETYPE_XOR,
	NODETYPE_EQUAL,
	NODETYPE_NOT_EQUAL,
	NODETYPE_LESS,
	NODETYPE_LESS_EQUAL,
	NODETYPE_GREATER,
	NODETYPE_GREATER_EQUAL,
	NODETYPE_COMMA,
	NODETYPE_REFERENCE,
	NODETYPE_TERNARY,
	NODETYPE_COLON,
	NODETYPE_LSHIFT,
	NODETYPE_RSHIFT
} nodetype;


#define PLURAL_C_VAR  'n'
#define PLURAL_C_NOT  '!'
#define PLURAL_C_CMPL '~'
#define PLURAL_C_XOR  '^'
#define PLURAL_C_LOR  '|'
#define PLURAL_C_LAND '&'
#define PLURAL_C_MULT '*'
#define PLURAL_C_DIV  '/'
#define PLURAL_C_MOD  '%'
#define PLURAL_C_ADD  '+'
#define PLURAL_C_SUB  '-'
#define PLURAL_C_UMINUS  '~'
#define PLURAL_C_LT   '<'
#define PLURAL_C_GT   '>'
#define PLURAL_C_LE   '('
#define PLURAL_C_GE   ')'
#define PLURAL_C_EQ   '='
#define PLURAL_C_NE   '#'
#define PLURAL_C_QUEST '?'


struct _Parser {
	const char *expr_string;
	size_t string_end;
	expreval_error_info error;
};


typedef struct _ExprEvalFunctionArg
{
	expreval_value m_value_internal;
	gboolean isref;
	gboolean b_changed;
} ExprEvalFunctionArg;

typedef struct _ExprEvalExpression ExprEvalExpression;

typedef struct _ExprEvalValueListItem ExprEvalValueListItem;
struct _ExprEvalValueListItem
{
	expreval_value *p_value;
	char *m_name;					/* Name of value */
	gboolean m_constant;			/* Value is constant */
	expreval_value m_value_internal;
	expreval_value m_def;			/* Default value when reset */
	void (*f_set)(ExprEvalValueListItem *var, ExprEvalExpression *expr, expreval_value *, gboolean dofree);
	void (*f_get)(ExprEvalValueListItem *var, ExprEvalExpression *expr, expreval_value *, gboolean dofree);
};

struct _ExprEvalFunctionDef
{
	const char *name;
	/* Argument count */
	size_t m_argMin;
	size_t m_argMax;
	gboolean varargs;
	expreval_value (*evaluate) (ExprEvalExpression *expr, const ExprEvalFunctionDef *func, size_t argc, ExprEvalFunctionArg *args);
	const char *description;
};

struct _ExprEvalNode {
	nodetype type;
	union {
		struct {
			ExprEvalNode *m_lhs;
			ExprEvalNode *m_rhs;
		} binary;
		struct {
			ExprEvalNode *m_rhs;
		} unary;
		struct {
			ExprEvalNode *m_lhs;
			ExprEvalNode *m_rhs;
		} ternary;
		struct {
			ExprEvalValueListItem *m_var;
		} variable;
		struct {
			ExprEvalValueListItem *m_var;
			ExprEvalNode *m_rhs;
		} assign;
		struct {	
			expreval_value m_val;
		} value;
		struct {
			ExprEvalValueListItem *m_var;
			ExprEvalNode *m_rhs;
		} reference;
		struct {
			ExprEvalNode *m_args;
			const ExprEvalFunctionDef *m_function;
		} function;
	} u;
};




void expreval_node_dump(ExprEvalNode *node, int level);
extern ExprEvalNode _libnls_germanic_plural;

int libnls_extract_plural_expression(const char *nullentry, ExprEvalNode **pluralp, int *npluralsp);

#ifdef __cplusplus
}
#endif


#endif /* __EXPREVAL_INTERNAL_H__ */
