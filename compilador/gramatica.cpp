/* Regras da gramatica da SSL com os marcadores semanticos (caps. 5 e 6)
 * e a extensao 'return'. Ver README, secao 2. */

#include <cstdio>
#include "gramatica.h"

const std::vector<t_ruleDef> Rules = {
    { START_RULE, START_, { P_, ENDOFFILE } },        /* S' -> P '$'          */

    { P_LDE,    P_,   { LDE_ } },
    { LDE_LIST, LDE_, { LDE_, DE_ } },
    { LDE_DE,   LDE_, { DE_ } },
    { DE_DF,    DE_,  { DF_ } },
    { DE_DT,    DE_,  { DT_ } },

    { T_INTEGER, T_, { INTEGER } },
    { T_CHAR,    T_, { CHAR } },
    { T_BOOLEAN, T_, { BOOLEAN } },
    { T_STRING,  T_, { STRING } },
    { T_IDU,     T_, { IDU_ } },

    { DT_ARRAY,  DT_, { TYPE, IDD_, EQUALS, ARRAY, LEFT_SQUARE, NUM_,
                        RIGHT_SQUARE, OF, T_ } },
    { DT_STRUCT, DT_, { TYPE, IDD_, EQUALS, STRUCT, NB_, LEFT_BRACES, DC_,
                        RIGHT_BRACES } },
    { DT_ALIAS,  DT_, { TYPE, IDD_, EQUALS, T_ } },
    { DC_LIST,   DC_, { DC_, SEMI_COLON, LI_, COLON, T_ } },
    { DC_LI,     DC_, { LI_, COLON, T_ } },

    { DF_RULE,  DF_,  { FUNCTION, IDD_, NF_, LEFT_PARENTHESIS, LP_,
                        RIGHT_PARENTHESIS, COLON, T_, MF_, B_ } },
    { LP_LIST,  LP_,  { LP_, COMMA, IDD_, COLON, T_ } },
    { LP_IDD,   LP_,  { IDD_, COLON, T_ } },
    { B_RULE,   B_,   { LEFT_BRACES, LDV_, LS_, RIGHT_BRACES } },
    { LDV_LIST, LDV_, { LDV_, DV_ } },
    { LDV_DV,   LDV_, { DV_ } },
    { LS_LIST,  LS_,  { LS_, S_ } },
    { LS_S,     LS_,  { S_ } },
    { DV_RULE,  DV_,  { VAR, LI_, COLON, T_, SEMI_COLON } },
    { LI_LIST,  LI_,  { LI_, COMMA, IDD_ } },
    { LI_IDD,   LI_,  { IDD_ } },

    { S_IF,       S_, { IF, LEFT_PARENTHESIS, E_, RIGHT_PARENTHESIS, S_ } },
    { S_IF_ELSE,  S_, { IF, LEFT_PARENTHESIS, E_, RIGHT_PARENTHESIS, S_,
                        ELSE, S_ } },
    { S_WHILE,    S_, { WHILE, LEFT_PARENTHESIS, E_, RIGHT_PARENTHESIS, ML_,
                        S_ } },
    { S_DO_WHILE, S_, { DO, ML_, S_, WHILE, LEFT_PARENTHESIS, E_,
                        RIGHT_PARENTHESIS, SEMI_COLON } },
    { S_BLOCK,    S_, { NB_, B_ } },
    { S_ASSIGN,   S_, { LV_, EQUALS, E_, SEMI_COLON } },
    { S_BREAK,    S_, { BREAK, SEMI_COLON } },
    { S_CONTINUE, S_, { CONTINUE, SEMI_COLON } },
    { S_RETURN,   S_, { RETURN, E_, SEMI_COLON } },

    { E_AND, E_, { E_, AND, L_ } },
    { E_OR,  E_, { E_, OR, L_ } },
    { E_L,   E_, { L_ } },

    { L_LT, L_, { L_, LESS_THAN, R_ } },
    { L_GT, L_, { L_, GREATER_THAN, R_ } },
    { L_LE, L_, { L_, LESS_OR_EQUAL, R_ } },
    { L_GE, L_, { L_, GREATER_OR_EQUAL, R_ } },
    { L_EQ, L_, { L_, EQUAL_EQUAL, R_ } },
    { L_NE, L_, { L_, NOT_EQUAL, R_ } },
    { L_R,  L_, { R_ } },

    { R_PLUS,  R_, { R_, PLUS, Y_ } },
    { R_MINUS, R_, { R_, MINUS, Y_ } },
    { R_Y,     R_, { Y_ } },

    { Y_TIMES,  Y_, { Y_, TIMES, F_ } },
    { Y_DIVIDE, Y_, { Y_, DIVIDE, F_ } },
    { Y_F,      Y_, { F_ } },

    { F_LV,      F_, { LV_ } },
    { F_PRE_INC, F_, { PLUS_PLUS, LV_ } },
    { F_PRE_DEC, F_, { MINUS_MINUS, LV_ } },
    { F_POS_INC, F_, { LV_, PLUS_PLUS } },
    { F_POS_DEC, F_, { LV_, MINUS_MINUS } },
    { F_PAREN,   F_, { LEFT_PARENTHESIS, E_, RIGHT_PARENTHESIS } },
    { F_CALL,    F_, { IDU_, MC_, LEFT_PARENTHESIS, LE_, RIGHT_PARENTHESIS } },
    { F_MINUS,   F_, { MINUS, F_ } },
    { F_NOT,     F_, { NOT, F_ } },
    { F_TRUE,    F_, { TRUE_ } },
    { F_FALSE,   F_, { FALSE_ } },
    { F_CHR,     F_, { CHR_ } },
    { F_STR,     F_, { STR_ } },
    { F_NUM,     F_, { NUM_ } },

    { LE_LIST, LE_, { LE_, COMMA, E_ } },
    { LE_E,    LE_, { E_ } },

    { LV_FIELD, LV_, { LV_, DOT, ID_ } },
    { LV_INDEX, LV_, { LV_, LEFT_SQUARE, E_, RIGHT_SQUARE } },
    { LV_IDU,   LV_, { IDU_ } },

    { IDD_ID, IDD_, { ID } },
    { IDU_ID, IDU_, { ID } },
    { ID_ID,  ID_,  { ID } },

    { TRUE_RULE,  TRUE_,  { TRUE } },
    { FALSE_RULE, FALSE_, { FALSE } },
    { CHR_RULE,   CHR_,   { CHARACTER } },
    { STR_RULE,   STR_,   { STRINGVAL } },
    { NUM_RULE,   NUM_,   { NUMERAL } },

    { NB_RULE, NB_, { } },
    { NF_RULE, NF_, { } },
    { MF_RULE, MF_, { } },
    { MC_RULE, MC_, { } },
    { ML_RULE, ML_, { } },
};

bool IsTerminal(int symbol)
{
    return symbol < NUM_TERMINALS;
}

static const char *terminalNames[NUM_TERMINALS] = {
    "'array'", "'boolean'", "'break'", "'char'", "'continue'", "'do'",
    "'else'", "'false'", "'function'", "'if'", "'integer'", "'of'",
    "'return'", "'string'", "'struct'", "'true'", "'type'", "'var'",
    "'while'",
    "':'", "';'", "','", "'='", "'['", "']'", "'{'", "'}'", "'('", "')'",
    "'&&'", "'||'", "'<'", "'>'", "'<='", "'>='", "'!='", "'=='", "'+'",
    "'++'", "'-'", "'--'", "'*'", "'/'", "'.'", "'!'",
    "c", "n", "s", "Id",
    "$", "?"
};

static const char *nontNames[NUM_SYMBOLS - NUM_TERMINALS] = {
    "P", "LDE", "DE", "T", "DT", "DC", "DF", "LP", "B", "LDV", "LS", "DV",
    "LI", "S", "E", "L", "R", "Y", "F", "LE", "LV",
    "IDD", "IDU", "ID", "TRUE", "FALSE", "CHR", "STR", "NUM",
    "NB", "NF", "MF", "MC", "ML",
    "S'"
};

const char *SymbolName(int symbol)
{
    return IsTerminal(symbol) ? terminalNames[symbol]
                              : nontNames[symbol - NUM_TERMINALS];
}

void PrintRule(int rule)
{
    const t_ruleDef &d = Rules[rule];

    printf("%s ->", SymbolName(d.left));
    if (d.right.empty())
        printf(" <vazio>");
    for (int s : d.right)
        printf(" %s", SymbolName(s));
}
