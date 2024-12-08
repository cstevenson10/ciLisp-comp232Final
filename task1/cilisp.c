#include "cilisp.h"

#define RED             "\033[31m"
#define RESET_COLOR     "\033[0m"
#define FUNC_COUNT 17

// yyerror:
// Something went so wrong that the whole program should crash.
// You should basically never call this unless an allocation fails.
// (see the "yyerror("Memory allocation failed!")" calls and do the same.
// This is basically printf, but red, with "\nERROR: " prepended, "\n" appended,
// and an "exit(1);" at the end to crash the program.
// It's called "yyerror" instead of "error" so the parser will use it for errors too.
void yyerror(char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsnprintf (buffer, 255, format, args);

    printf(RED "\nERROR: %s\nExiting...\n" RESET_COLOR, buffer);
    fflush(stdout);

    va_end (args);
    exit(1);
}

// warning:
// Something went mildly wrong (on the user-input level, probably)
// Let the user know what happened and what you're doing about it.
// Then, move on. No big deal, they can enter more inputs. ¯\_(ツ)_/¯
// You should use this pretty often:
//      too many arguments, let them know and ignore the extra
//      too few arguments, let them know and return NAN
//      invalid arguments, let them know and return NAN
//      many more uses to be added as we progress...
// This is basically printf, but red, and with "\nWARNING: " prepended and "\n" appended.
void warning(char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start (args, format);
    vsnprintf (buffer, 255, format, args);

    printf(RED "WARNING: %s\n" RESET_COLOR, buffer);
    fflush(stdout);

    va_end (args);
}

FUNC_TYPE resolveFunc(char *funcName)
{
    // Array of string values for function names.
    // Must be in sync with members of the FUNC_TYPE enum in order for resolveFunc to work.
    // For example, funcNames[NEG_FUNC] should be "neg"
    char *funcNames[] = {
            "neg",
            "abs",
            "add",
            "sub",
            "mult",
            "div",
            "remainder",
            "exp",
            "exp2", // HUH?
            "pow",
            "log",
            "sqrt",
            "cbrt",
            "hypot",
            "max",
            "min",
            "custom", // NOTE: No idea if this is supposed to be here
                      // if it isn't CUSTOM_FUNC is empty string
            ""
    };
    int i = 0;
    while (funcNames[i][0] != '\0')
    {
        if (strcmp(funcNames[i], funcName) == 0)
        {
            return i;
        }
        i++;
    }
    return CUSTOM_FUNC;
}



AST_NODE *createNumberNode(double value, NUM_TYPE type)
{
    AST_NODE *node;
    size_t nodeSize;

    nodeSize = sizeof(AST_NODE);
    if ((node = calloc(nodeSize, 1)) == NULL)
    {
        yyerror("Memory allocation failed!");
        exit(1);
    }

    // Populate node atributes
    node->type = NUM_NODE_TYPE;
    node->data.number.type = type;
    node->data.number.value = value;

    return node;
}

AST_NODE *createFunctionNode(FUNC_TYPE func, AST_NODE *opList)
{
    AST_NODE *node;
    size_t nodeSize;

    nodeSize = sizeof(AST_NODE);
    if ((node = calloc(nodeSize, 1)) == NULL)
    {
        yyerror("Memory allocation failed!");
        exit(1);
    }

    // Populate the allocated AST_NODE *node's data
    node->type = FUNC_NODE_TYPE;
    node->data.function.func = func;
    node->data.function.opList = opList;

    return node;
}

AST_NODE *addExpressionToList(AST_NODE *newExpr, AST_NODE *exprList)
{
    // Add new expression as head
    newExpr->next = exprList;

    return newExpr;
}

int getOperandCound(AST_NODE *opList, int number) {
    int count = 0;
    AST_NODE *cur = opList;

    while (cur != NULL) {
        cur = cur->next;
        count++;
    }

    return count;
}

/*
 * 1 if int, 0 if not
 */
int isInt(double num) {
    if ((num - trunc(num)) < .000001) {
        return 1;
    }
    return 0;
}


RET_VAL evalNeg(AST_NODE *operand) {
    if (operand == NULL) {
        printf("WARNING: neg called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (operand->next->next != NULL) {
        printf("WARNING: neg called with extra (ignored) operands\n");
    }

    RET_VAL retval;

    // Populate retVal
    retval.type = operand->data.number.type;
    retval.value = (-1) * (operand->data.number.value);

    return retval;
}

RET_VAL evalAbs(AST_NODE *operand) {
    if (operand == NULL) {
        printf("WARNING: abs called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (operand->next->next != NULL) {
        printf("WARNING: abs called with extra (ignored) operands\n");
    }

    return (RET_VAL) {operand->data.number.type, fabs(operand->data.number.value)};
}

RET_VAL evalAdd(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: add called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next == NULL) {
        return (RET_VAL) opList->data.number;
    }

    AST_NODE *appends = opList;
    double sum;

    while (appends != NULL) {
        sum += opList->data.number.value;
    }

    if (isInt(sum)) {
        return (RET_VAL) {INT_TYPE, sum};
    }

    return (RET_VAL) {DOUBLE_TYPE, sum};
}

RET_VAL evalSub(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: sub called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next == NULL) {
        printf("WARNING: sub called with only one operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next->next != NULL) {
        printf("WARNING: sub called with extra (ignored) operands\n");
    }

    RET_VAL retval;

    retval.value = opList->data.number.value - opList->next->data.number.value;

    if (isInt(retval.value)){
        retval.type = INT_TYPE;
        return retval;
    }

    retval.type = DOUBLE_TYPE;
    return retval;
}

RET_VAL evalMult(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: mult called with no operands! nan returned\n");
        return (RET_VAL) {INT_TYPE, 1.0};
    }
    else if (opList->next == NULL) {
        return (RET_VAL) opList->data.number;
    }

    double product;
    AST_NODE *factor = opList;
    RET_VAL retval;
    retval.type = INT_TYPE;

    while (factor != NULL) {
        product *= factor->data.number.value;
        factor = factor->next;
    }

    retval.value = product;

    // Check protuct type
    if (isInt(product)) {
        return retval;
    }

    retval.type = DOUBLE_TYPE;
    return retval;
}

RET_VAL evalDiv(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: div called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next == NULL) {
        printf("WARNING: div called with only one operand! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next->next != NULL) {
        printf("WARNING: div called with extra (ignored) operands\n");
    }


    double div;
    AST_NUMBER op1 = opList->data.number;
    AST_NUMBER op2 = opList->next->data.number;

    div = op1.value / op2.value;

    // Integer divison
    if (op1.type == INT_TYPE && op2.type == INT_TYPE) {
        return (RET_VAL) {INT_TYPE, trunc(div)};
    }

    return (RET_VAL) {DOUBLE_TYPE, div};
}

RET_VAL evalRem(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: remainder called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next == NULL) {
        printf("WARNING: remainder called with only one operand! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next->next != NULL) {
        printf("WARNING: remainder called with extra (ignored) operands\n");
    }


    double rem = fmod(opList->data.number.value, opList->next->data.number.value);

    rem = fabs(rem);

    if(isInt(rem)) {
        return (RET_VAL) {INT_TYPE, rem};
    }
    else {
        return (RET_VAL) {DOUBLE_TYPE, rem};
    }
}

RET_VAL evalExp(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: exp called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next != NULL) {
        printf("WARNING: exp called with extra (ignored) operands\n");
    }

    return (RET_VAL) {DOUBLE_TYPE, exp(opList->data.number.value)};
}

RET_VAL evalExp2(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: exp2 called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next != NULL) {
        printf("WARNING: exp2 called with extra (ignored) operands\n");
    }

    RET_VAL retval;
    double val = opList->data.number.value;

    if (val < 1 || trunc(val) != val) {
        retval.type = DOUBLE_TYPE;
    }
    else {
        retval.type = INT_TYPE;
    }

    retval.value = exp2(val);

    return retval;
}

RET_VAL evalPow(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: pow called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next == NULL) {
        printf("WARNING: pow called with only one operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next->next != NULL) {
        printf("WARNING: pow called with extra (ignored) operands\n");
    }

    return (RET_VAL) {DOUBLE_TYPE, pow(opList->data.number.value,
                                       opList->next->data.number.value)};
}

RET_VAL evalLog(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: log called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next != NULL) {
        printf("WARNING: log called with extra (ignored) operands!\n");
    }

    return (RET_VAL) {DOUBLE_TYPE, log(opList->data.number.value)};
}

RET_VAL evalSqrt(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: sqrt called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next != NULL) {
        printf("WARNING: sqrt called with extra (ignored) operands!\n");
    }

    return (RET_VAL) {DOUBLE_TYPE, sqrt(opList->data.number.value)};
}

RET_VAL evalCbrt(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: cbrt called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }
    else if (opList->next != NULL) {
        printf("WARNING: cbrt called with extra (ignored) operands!\n");
    }

    return (RET_VAL) {DOUBLE_TYPE, cbrt(opList->data.number.value)};
}

RET_VAL evalHypot(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: hypot called with no operands! 0 returned\n");
        return ZERO_RET_VAL;
    }

    RET_VAL retval;
    retval.type = DOUBLE_TYPE;
    
    AST_NODE *cur = opList;
    double sum;

    while (cur != NULL) {
        sum += pow(cur->data.number.value, 2);
    }

    retval.value = sqrt(sum);

    return retval;
}

/*
 * Min op: 2
 * Max op: none
 */
RET_VAL evalMax(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: max called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }

    NUM_TYPE type = opList->data.number.type;
    double max = opList->data.number.value;

    AST_NODE *ptr = opList->next;

    while (ptr != NULL) {
        if (max < fmax(max, ptr->data.number.value)) {
            // Set new max and type
            max = ptr->data.number.value;
            type = ptr->data.number.type;
        }

        ptr = ptr->next;
    }

    return (RET_VAL) {type, max};
}


/*
 * Min op: 2
 * Max op: none
 */
RET_VAL evalMin(AST_NODE *opList) {
    // Check for opperands
    if (opList == NULL) {
        printf("WARNING: min called with no operands! nan returned\n");
        return NAN_RET_VAL;
    }

    NUM_TYPE type = opList->data.number.type;
    double min = opList->data.number.value;

    AST_NODE *ptr = opList->next;

    while (ptr != NULL) {
        if (min > fmin(min, ptr->data.number.value)) {
            // Set new min and type
            min = ptr->data.number.value;
            type = ptr->data.number.type;

        }
        ptr = ptr->next;
    }

    return (RET_VAL) {type, min};
}

/*
 * NOTE: I believe because yacc is bottom up parser it will deal with
 * inner most s-expr first
 * */
RET_VAL evalFuncNode(AST_NODE *node)
{
    if (!node)
    {
        yyerror("NULL ast node passed into evalFuncNode!");
        return NAN_RET_VAL; // unreachable but kills a clang-tidy warning
    }
    // Check for valid function type (need to do bcs using look-up table)
    else if (node->data.function.func < 0 ||
             node->data.function.func > CUSTOM_FUNC) { // last element in FUNC_TYPE enum
        yyerror("Invalid function type in node passed into evalFuncNode!");
        return NAN_RET_VAL; 
    }

    RET_VAL (*functionTable[FUNC_COUNT])(AST_NODE *) = {
		evalNeg,
		evalAbs,
		evalSub,
		evalAdd,
		evalMult,
		evalDiv,
		evalRem,
		evalExp,
		evalExp2,
		evalPow,
		evalLog,
		evalSqrt,
		evalCbrt,
		evalHypot,
		evalMax,
		evalMin
    };

    // Call corrisponding function  NOTE: Passing in opList
    RET_VAL retval = functionTable[node->data.function.func](node->data.function.opList);
    
    // Free node
    freeNode(node);

    return retval;

    /*
    //  could use a sorta look-up table here but thats for another time
    // Decide function
    switch (node->data.function.func) {
        case NEG_FUNC:
            return evalNeg(node);
        case ABS_FUNC:
            return evalAbs(node);
        case ADD_FUNC:
            return evalAdd(node);
        case SUB_FUNC:
            return evalSub(node);
        case MULT_FUNC:
            return evalMult(node);
        case DIV_FUNC:
            return evalDiv(node);
        case REM_FUNC:
            return evalRem(node);
        case EXP_FUNC:
            return evalExp(node);
        case EXP2_FUNC:
            return evalExp2(node);
        case POW_FUNC:
            return evalPow(node);
        case LOG_FUNC:
            return evalLog(node);
        case SQRT_FUNC:
            return evalSqrt(node);
        case CBRT_FUNC:
            return evalCbrt(node);
        case HYPOT_FUNC:
            return evalHypot(node);
        case MAX_FUNC:
            return evalMax(node);
        case MIN_FUNC:
            return evalMin(node);
        case CUSTOM_FUNC:
            yyerror("Custom func not setup yet\n");
        defualt:
            yyerror("Error in evalFuncNode(), node.data.function.func likely not set\n");
    }


    return NAN_RET_VAL; // Paranotic
    */
}

RET_VAL evalNumNode(AST_NODE *node)
{
    if (!node)
    {
        yyerror("NULL ast node passed into evalNumNode!");
        return NAN_RET_VAL;
    }

    // TODO complete the function

    return NAN_RET_VAL;
}

RET_VAL eval(AST_NODE *node)
{
    if (!node)
    {
        yyerror("NULL ast node passed into eval!");
        return NAN_RET_VAL;
    }

    // Number node
    if (node->type == NUM_NODE_TYPE) {
        return evalNumNode(node);
    }
    // Funcion node
    else {
        return evalFuncNode(node);
    }

    return NAN_RET_VAL;
}

// prints the type and value of a RET_VAL
void printRetVal(RET_VAL val)
{
    switch (val.type)
    {
        case INT_TYPE:
            printf("Integer : %.lf\n", val.value);
            break;
        case DOUBLE_TYPE:
            printf("Double : %lf\n", val.value);
            break;
        default:
            printf("No Type : %lf\n", val.value);
            break;
    }
}



void freeNode(AST_NODE *node)
{
    if (!node)
    {
        return;
    }

    // TODO complete the function

    // look through the AST_NODE struct, decide what
    // referenced data should have freeNode called on it
    // (hint: it might be part of an s_expr_list, with more
    // nodes following it in the list)

    // if this node is FUNC_TYPE, it might have some operands
    // to free as well (but this should probably be done in
    // a call to another function, named something like
    // freeFunctionNode)

    // and, finally,
    free(node);
}
