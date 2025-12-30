#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "expreval.h"
#include "expreval-internal.h"

#define DEBUG 0
#if DEBUG
#define DBG(x) printf x
static void print_stack(unsigned long *stack, int depth, const char *op)
{
	int i;
	
	printf("%s: [%d]", op, depth);
	for (i = 0; i < depth; i++)
		printf(" %lu", stack[i]);
	printf("\n");
}
#define PRINT_STACK(op) print_stack(stack, stack_depth, op) 
#else
#define DBG(x)
#define PRINT_STACK(op)
#endif

/* Evaluates a plural string EXP for n=N. */
expreval_value libnls_expreval_evaluate_string(const char *exp, expreval_value n)
{
	expreval_value stack[EVAL_MAXDEPTH];
	int stack_depth = 0;
	
	assert(exp != NULL);
	assert(exp[0] != '\0');
	if (exp == NULL || exp[0] == '\0')
		return EXPREVAL_ERROR_EMPTY_EXPRESSION;
	while (*exp != '\0')
	{
		switch (*exp++)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			{
				expreval_value num = exp[-1] - '0';
	
				while (exp[0] >= '0' && exp[0] <= '9')
				{
					num *= 10;
					num += exp[0] - '0';
					++exp;
				}
				if (*exp == ' ')
					exp++;
				assert(stack_depth < EVAL_MAXDEPTH);
				if (stack_depth >= EVAL_MAXDEPTH)
				{
					return EXPREVAL_ERROR_STACKOVF;
				}
				stack[stack_depth++] = num;
				PRINT_STACK("0");
			}
			break;

		case PLURAL_C_VAR:
			assert(stack_depth < EVAL_MAXDEPTH);
			if (stack_depth >= EVAL_MAXDEPTH)
			{
				return EXPREVAL_ERROR_STACKOVF;
			}
			stack[stack_depth++] = n;
			PRINT_STACK("n");
			break;
		
		case PLURAL_C_NOT:
			assert(stack_depth >= 1);
			if (stack_depth < 1)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack[stack_depth - 1] = stack[stack_depth - 1] == 0;
			PRINT_STACK("!");
			break;

		case PLURAL_C_CMPL:
			assert(stack_depth >= 1);
			if (stack_depth < 1)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack[stack_depth - 1] = ~stack[stack_depth - 1];
			PRINT_STACK("~");
			break;

		case PLURAL_C_LOR:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			/*
			 * Note: this could skip evaluation of the 2nd operand, if the first is true.
			 * Not implemented for now, since our evaluation does not have any side effects.
			 */
			stack[stack_depth - 1] = stack[stack_depth - 1] != 0 || stack[stack_depth] != 0;
			PRINT_STACK("||");
			break;

		case PLURAL_C_LAND:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			/*
			 * Note: this could skip evaluation of the 2nd operand, if the first is false.
			 * Not implemented for now, since our evaluation does not have any side effects.
			 */
			stack[stack_depth - 1] = stack[stack_depth - 1] != 0 && stack[stack_depth] != 0;
			PRINT_STACK("&&");
			break;

		case PLURAL_C_MULT:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] * stack[stack_depth];
			PRINT_STACK("*");
			break;

		case PLURAL_C_DIV:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			if (stack[stack_depth] == 0)
				return EXPREVAL_ERROR_DIVIDE_BY_ZERO;
			stack[stack_depth - 1] = stack[stack_depth - 1] / stack[stack_depth];
			PRINT_STACK("/");
			break;

		case PLURAL_C_MOD:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			if (stack[stack_depth] == 0)
				return EXPREVAL_ERROR_DIVIDE_BY_ZERO;
			stack[stack_depth - 1] = stack[stack_depth - 1] % stack[stack_depth];
			PRINT_STACK("%");
			break;

		case PLURAL_C_ADD:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] + stack[stack_depth];
			PRINT_STACK("+");
			break;

		case PLURAL_C_XOR:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] ^ stack[stack_depth];
			PRINT_STACK("^");
			break;

		case PLURAL_C_SUB:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] - stack[stack_depth];
			PRINT_STACK("-");
			break;

		case PLURAL_C_LT:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] < stack[stack_depth];
			PRINT_STACK("<");
			break;

		case PLURAL_C_GT:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] > stack[stack_depth];
			PRINT_STACK(">");
			break;

		case PLURAL_C_LE:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] <= stack[stack_depth];
			PRINT_STACK("\342\211\244");
			break;

		case PLURAL_C_GE:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] >= stack[stack_depth];
			PRINT_STACK("\342\211\245");
			break;

		case PLURAL_C_EQ:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] == stack[stack_depth];
			PRINT_STACK("==");
			break;

		case PLURAL_C_NE:
			assert(stack_depth >= 2);
			if (stack_depth < 2)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth--;
			stack[stack_depth - 1] = stack[stack_depth - 1] != stack[stack_depth];
			PRINT_STACK("!=");
			break;

		case PLURAL_C_QUEST:
			assert(stack_depth >= 3);
			if (stack_depth < 3)
			{
				return EXPREVAL_ERROR_ASSERT;
			}
			stack_depth -= 2;
			/*
			 * Note: this could skip evaluation of the 2nd or 3rd operand.
			 * Not implemented for now, since our evaluation does not have any side effects.
			 */
			stack[stack_depth - 1] = stack[stack_depth - 1] != 0 ? stack[stack_depth + 0] : stack[stack_depth + 1];
			PRINT_STACK("?");
			break;

		default:
			assert(0);
			return EXPREVAL_ERROR_ASSERT;
		}
	}
	assert(stack_depth == 1);
	if (stack_depth != 1)
	{
		return EXPREVAL_ERROR_ASSERT;
	}
	return (int)stack[0];
}
