#ifndef __EXPREVAL_H__
#define __EXPREVAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * define EXPREVAL_SILENT to use a silent version that does not track errors
 */
#ifndef EXPREVAL_SILENT
# define EXPREVAL_SILENT 0
#endif


typedef unsigned long expreval_value;


/* Forward declarations */
typedef struct _Parser Parser;
typedef struct _ExprEvalNode ExprEvalNode;



/*
 * Types of errors
 */
typedef enum _expreval_error
{
	EXPREVAL_ERROR_NONE = 0,
	EXPREVAL_ERROR_NOTFOUND = -1,
	EXPREVAL_ERROR_NULLPOINTER = -2,
	EXPREVAL_ERROR_MATH = -3,
	EXPREVAL_ERROR_DIVIDE_BY_ZERO = -4,
	EXPREVAL_ERROR_EMPTY_EXPRESSION = -5,
	EXPREVAL_ERROR_GARBAGE = -6,
	EXPREVAL_ERROR_SYNTAX = -7,
	EXPREVAL_ERROR_UNMATCHED_PARENTHESIS = -8,
	EXPREVAL_ERROR_MISSING_PARENTHESIS = -9,
	EXPREVAL_ERROR_STACKOVF = -10,
	EXPREVAL_ERROR_STROVF = -11,
	EXPREVAL_ERROR_ASSERT = -12,
	EXPREVAL_ERROR_RANGE = -13,
#if !EXPREVAL_REDUCED
	EXPREVAL_ERROR_ALREADY_EXISTS = -14,
	EXPREVAL_ERROR_CONSTANT_ASSIGN = -15,
	EXPREVAL_ERROR_CONSTANT_REFERENCE = -16,
	EXPREVAL_ERROR_LVALUE_NEEDED = -17,
	EXPREVAL_ERROR_INVALID_ARGUMENT_COUNT = -18,
	EXPREVAL_ERROR_INVALID_ARGUMENT_TYPE = -19,
#endif
} expreval_error;

typedef struct _expreval_error_info expreval_error_info;
struct _expreval_error_info
{
	expreval_error type;
#if !EXPREVAL_SILENT
	int err_no;
	size_t start;
	size_t end;
	char *detail;
#endif
};


typedef struct _ExprEvalFunctionDef ExprEvalFunctionDef;

/* misc */
Parser *libnls_expreval_parser_new(void);
void libnls_expreval_parser_delete(Parser *parser);

/* Parse an expression */
#if EXPREVAL_SILENT
#define libnls_expreval_parse_string libnls_expreval_parse_string_silent
#else
#define libnls_expreval_parse_string libnls_expreval_parse_string_verbose
void libnls_expreval_print_error(Parser *parser);
#endif

expreval_error libnls_expreval_parse_string(Parser *parser, ExprEvalNode **expr, const char *exstr, size_t len);
void libnls_free_expression(ExprEvalNode *node);

/* Evaluate expression */
expreval_value libnls_expreval_evaluate(ExprEvalNode *expr, expreval_value n);
expreval_value libnls_expreval_evaluate_string(const char *exp, expreval_value n);
int libnls_expreval_print_expression(ExprEvalNode *expr, char *buf, size_t size, int utf8);

#ifdef __cplusplus
}
#endif

#endif /* __EXPREVAL_H__ */
