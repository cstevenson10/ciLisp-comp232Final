%{
    #include "cilisp.h"
    #define ylog(r, p) {printf("BISON: %s ::= %s \n", #r, #p);}
    int yylex();
    void yyerror(char*, ...);
%}

%union {
    double dval;
    int ival;
    struct ast_node *astNode;  // NOTE: AST_NODES hold either AST_NUMBER or AST_FUNCTION
};                             // in nodeptr->data.number/function

%token <ival> FUNC
%token <dval> INT DOUBLE
%token QUIT EOL EOFT

%type <astNode> number s_expr f_expr s_expr_list s_expr_section  


%%

program:
    s_expr EOL {
        ylog(program, s_expr EOL);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        YYACCEPT;
    }
    | s_expr EOFT {
        ylog(program, s_expr EOFT);
        if ($1) {
            printRetVal(eval($1));
            freeNode($1);
        }
        exit(EXIT_SUCCESS);
    }
    | EOL {
        ylog(program, EOL);
        YYACCEPT;  // paranoic; main skips blank lines
    }
    | EOFT {
        ylog(program, EOFT);
        exit(EXIT_SUCCESS);
    };


s_expr:
    f_expr {
        ylog(s_expr, f_expr);
        // Evaluate f_expr
        $$ = $1;
    }
    | number {
        ylog(s_expr, number);
        // Evaluate number TODO: make sure number is AST_NODE*
        $$ = $1;
    }
    | QUIT {
        ylog(s_expr, QUIT);
        exit(EXIT_SUCCESS);
    }
    | error {
        ylog(s_expr, error);
        yyerror("unexpected token");
        $$ = NULL;
    };

f_expr:
    '(' FUNC s_expr_section ')' {                 // TODO: Should I be tokenizing parens?
        ylog(f_expr, '(' FUNC s_expr_section ')');
        // NOTE: I think all I need to do here is this
        // 
        $$ = createFunctionNode($2, $3);
    };

s_expr_section:
    s_expr_list {
        ylog(s_expr_section, s_expr_list);
        $$ = $1;
    }
    | { ylog(s_expr_section, <empty>); $$ = NULL; // TODO:Should I set this to null? 
    };

s_expr_list:
    s_expr {
        ylog(s_expr_list, s_expr);
        $$ = $1;
    }
    | s_expr s_expr_list {
        ylog(s_expr_list, s_expr s_expr_list);
        // Add new s_expr to list
        $$ = addExpressionToList($1, $2);
    };

number: 
    INT {
        ylog(number, INT);
        $$ = createNumberNode($1, INT_TYPE);
    }
    |
    DOUBLE {
        ylog(number, DOUBLE);
        $$ = createNumberNode($1, DOUBLE_TYPE);
    };
%%

