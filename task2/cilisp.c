#include "cilisp.h"

#define RED             "\033[31m"
#define RESET_COLOR     "\033[0m"
#define FUNC_COUNT 17

RET_VAL evalScopeNode(AST_NODE *node);
RET_VAL evalSymNode(AST_NODE *node);
RET_VAL callNodeTypeEval(AST_NODE *node);

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


AST_NODE *createAstNode(AST_NODE_TYPE type) {
    AST_NODE *node;
    size_t nodeSize;

    nodeSize = sizeof(AST_NODE);
    if ((node = calloc(nodeSize, 1)) == NULL)
    {
        yyerror("Memory allocation failed!");
        exit(1);
    }

    node->type = type;

    return node;

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

// Sets each member of childLists' parent
void setParents(AST_NODE *parent, AST_NODE *childList) {
    AST_NODE *cur = childList;
    while (cur != NULL) {
        cur->parent = parent;
        cur = cur->next;
    }
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

    setParents(node, opList);

    return node;
}

AST_NODE *createScopeNode(SYMBOL_TABLE_NODE *symTable, AST_NODE *s_expr)
{
    AST_NODE *node;
    node = createAstNode(SCOPE_NODE_TYPE);

    // Set parent child relation between node and s_expr
    node->data.scope.child = s_expr;

    // NOTE: Directions state do each of these pg 31:
    s_expr->parent = node;
    s_expr->symbolTable = symTable;
    SYMBOL_TABLE_NODE *cur = symTable;
    while (cur != NULL) {
        cur->value->parent = s_expr; 
        cur = cur->next;
    }

    return node;
}

AST_NODE *createSymbolNode(char *name) {
    AST_NODE *node = createAstNode(SYM_NODE_TYPE);

    node->data.symbol.id = name;

    return node;
}

SYMBOL_TABLE_NODE *createSymbolTableNode(char *id, AST_NODE *val) {
    SYMBOL_TABLE_NODE *node;
    size_t nodeSize;

    nodeSize = sizeof(SYMBOL_TABLE_NODE);
    if ((node = calloc(nodeSize, 1)) == NULL)
    {
        yyerror("Memory allocation failed!");
        exit(1);
    }

    node->id = id;
    node->value = val;

    return node;
}

SYMBOL_TABLE_NODE *findSymbol(char* name, SYMBOL_TABLE_NODE *symList) {
    SYMBOL_TABLE_NODE *cur = symList;

    //printf("---------findSymbol\n");
    while (cur != NULL) {
        if (strcmp(name, cur->id) == 0) {
            // Symbol found
            return cur;
        }

        cur = cur->next;
    }

    // Not found
    return NULL;
}

SYMBOL_TABLE_NODE *addSymbolToList(SYMBOL_TABLE_NODE *sym, SYMBOL_TABLE_NODE *symList) {
    // Check if symbol is defined already (in current scope)
    SYMBOL_TABLE_NODE *node = findSymbol(sym->id, symList); 
    if (node == NULL)
    {
        // Symbol not yet defined; Add new symbol as head
        sym->next = symList;
        return sym;
    }
    else {
        // Symbol already defined; Discard new value definition
        printf("WARNING: multiple (ignored) definitions of %s\n", sym->id); 
        node->value = sym->value; //NOTE: yacc parses right let_elem first so we have to keep the 
                                  // duplicate and get ride of first def
        return symList;
    }
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
    else if (operand->next != NULL) {
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
    else if (operand->next != NULL) {
        printf("WARNING: abs called with extra (ignored) operands\n");
    }

    return (RET_VAL) {operand->data.number.type, fabs(operand->data.number.value)};
}

RET_VAL evalAdd(AST_NODE *opList) {
    if (opList == NULL) {
        printf("WARNING: add called with no operands! nan returned\n");
        return ZERO_RET_VAL;
    }
    else if (opList->next == NULL) {
        return (RET_VAL) opList->data.number;
    }

    NUM_TYPE type = INT_TYPE;
    AST_NODE *appends = opList;
    double sum = 0.0;

    while (appends != NULL) {
        if (appends->data.number.type == DOUBLE_TYPE) {
            type = DOUBLE_TYPE;
        }
        sum += appends->data.number.value;
        appends = appends->next;
    }

    if (type != DOUBLE_TYPE) {
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
    retval.type = opList->data.number.type || opList->next->data.number.type; 

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

    double product = 1.0;
    AST_NODE *factor = opList;
    RET_VAL retval;
    retval.type = INT_TYPE;

    while (factor != NULL) {
        if (factor->data.number.type == DOUBLE_TYPE) {
            retval.type = DOUBLE_TYPE;
        }
        product *= factor->data.number.value;
        factor = factor->next;
    }

    retval.value = product;

    // Check protuct type
    if (retval.type == INT_TYPE) {
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

    if (opList->next->data.number.value == 0.0) {
        printf("WARNING: Divide by zero! nan returned\n");
        return NAN_RET_VAL;
    }

    RET_VAL retval;
    double dividen = opList->data.number.value;
    double divisor = opList->next->data.number.value;
    retval.type = opList->data.number.type || opList->next->data.number.type;

    double remainder = fmod(dividen, divisor);

    // Ensure the remainder is positive
    if (remainder < 0) {
        remainder += fabs(divisor);
    }

    retval.value = remainder;

    return retval; 
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

    if (val < 0) {
        retval.type = DOUBLE_TYPE;
    }
    else {
        retval.type = opList->data.number.type;
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

    RET_VAL retval;
    retval.type = opList->data.number.type || opList->next->data.number.type;
    retval.value = pow(opList->data.number.value, opList->next->data.number.value);

    return retval;

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
        return (RET_VAL) {DOUBLE_TYPE, 0.0};
    }

    RET_VAL retval;
    retval.type = DOUBLE_TYPE;
    
    AST_NODE *cur = opList;
    double sum = 0.0;

    while (cur != NULL) {
        sum += pow(cur->data.number.value, 2);
        cur = cur->next;
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

RET_VAL evalFuncNode(AST_NODE *node);

/*
 * Resolves all functions in an opList and returns the new list
 * */
AST_NODE *resolveOperandList(AST_NODE *opList) {

    AST_NODE *op = opList;

    while (op != NULL) {
        switch(op->type) {
            case FUNC_NODE_TYPE:
                // Replace .data.func with value of resolved func
                op->data.number = evalFuncNode(op);
                op->type = NUM_NODE_TYPE;
                // Move to next
                op = op->next;
                break;
            case SCOPE_NODE_TYPE:
                // Let expression
                op->data.number = evalScopeNode(op);
                op->type = NUM_NODE_TYPE;
                op = op->next;
                break;
            case SYM_NODE_TYPE:
                op->data.number = evalSymNode(op);
                op->type = NUM_NODE_TYPE;
                op = op->next;
                break;
            case NUM_NODE_TYPE:
                op = op->next;
        }
    }
    return opList;

}

/*
 *
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

    FUNC_TYPE funcType = node->data.function.func;
    AST_NODE *opList = node->data.function.opList;

    // Make all operands num_node_type
    if (opList != NULL) {
        opList = resolveOperandList(opList);
    }

    // Function lookup table. NOTE: Depends on correct order
    RET_VAL (*functionTable[FUNC_COUNT])(AST_NODE *) = {
		evalNeg,
		evalAbs,
		evalAdd,
		evalSub,
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
    return functionTable[funcType](opList);

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

    return (RET_VAL) {node->data.number.type, node->data.number.value};
}

//TODO: may be error here
RET_VAL evalSymNode(AST_NODE *node) {
    char* id = node->data.symbol.id;
    SYMBOL_TABLE_NODE *sym;

    // If node has a symbol table search it
    if ((sym = findSymbol(id, node->symbolTable)) == NULL) {
        // Symbol not found in current node symbol table search parents'
        //printf("--%s NOT found in (%p) symtable\n",id,node);
        AST_NODE *parent = node->parent;
        while (parent != NULL) {
            if((sym = findSymbol(id, parent->symbolTable)) != NULL) {
                // Symbol found
                break;
            }
            //printf("--%s NOT found in (%p) symtable\n",id,parent);
            parent = parent->parent;
        }
    }

    if (sym == NULL) {
        // Symbol not found
        printf("WARNING: Undefined symbol \"%s\" evaluated! NAN returned!\n", id);
        return NAN_RET_VAL;
    }

    //printf("--%s found!!\n",id);

    // Symbol found
    AST_NODE *value = sym->value;

    if (value->type != NUM_NODE_TYPE) {
        // Comepute and replace non-number node with number-node
        value->data.number = (AST_NUMBER) callNodeTypeEval(value);
        value->type = NUM_NODE_TYPE;
    }

    //printf("---------evalsymNode-----3\n");

    return (RET_VAL) value->data.number;
}

RET_VAL evalScopeNode(AST_NODE *node) {
    // Check for no child
    if (node->data.scope.child == NULL) {
        printf("ERROR : evalScopeNode called with NULL child\n");
        return NAN_RET_VAL; // Paranotic, shouldn't pass yacc
    }

    return callNodeTypeEval(node->data.scope.child);
}

RET_VAL callNodeTypeEval(AST_NODE *node)
{
    if (!node)
    {
        yyerror("NULL ast node passed into callNodeTypeEval!");
        return NAN_RET_VAL;
    }

    switch (node->type) {
        case NUM_NODE_TYPE:   return evalNumNode(node);
        case FUNC_NODE_TYPE:  return evalFuncNode(node);
        case SYM_NODE_TYPE:   return evalSymNode(node);
        case SCOPE_NODE_TYPE: return evalScopeNode(node);
    }

    return NAN_RET_VAL;
}

// I don't think I need to helper function callNodeTypeEval() as eval is only ever called on the root
RET_VAL eval(AST_NODE *root)
{
    if (!root)
    {
        yyerror("NULL ast node passed into eval!");
        return NAN_RET_VAL;
    }

    //setParents(root); NOTE: Isn't this alread done when creating scope node?

    return callNodeTypeEval(root);
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

void freeOperands(AST_NODE *opList) {
    AST_NODE *prev;

    while (opList != NULL) {
        prev = opList;
        opList = opList->next;
        free(prev);
    }
}


void freeNode(AST_NODE *node)
{
    if (!node)
    {
        return;
    }

    // TODO: Update for symbols

    if (node->type == FUNC_NODE_TYPE) {
        freeOperands(node->data.function.opList);
    }
    else if (node->next != NULL) {
        freeOperands(node->next);
    }

    free(node);
}
