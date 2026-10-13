/* Gramatica da SSL efetivamente usada pelo compilador (VF).
 * Simbolos terminais: os valores de t_token do analisador lexico.
 * Simbolos nao terminais: numerados logo apos os terminais, de modo que a
 * Tabela de Acao tenha uma coluna por simbolo (fig. 3.17 e 3.19). */

#ifndef GRAMATICA_H
#define GRAMATICA_H

#include <vector>

extern "C" {
#include "lexer.h"
}

const int NUM_TERMINALS = UNKNOWN + 1;

typedef enum {
    P_ = NUM_TERMINALS, LDE_, DE_, T_, DT_, DC_, DF_, LP_, B_, LDV_, LS_, DV_,
    LI_, S_, E_, L_, R_, Y_, F_, LE_, LV_,
    IDD_, IDU_, ID_, TRUE_, FALSE_, CHR_, STR_, NUM_,
    NB_, NF_, MF_, MC_, ML_,                    /* marcadores semanticos */
    START_,                                     /* simbolo da gramatica aumentada */
    NUM_SYMBOLS
} t_nont;

/* uma constante por regra, na mesma ordem da tabela Rules (cap. 4.1) */
typedef enum {
    START_RULE,
    P_LDE, LDE_LIST, LDE_DE, DE_DF, DE_DT,
    T_INTEGER, T_CHAR, T_BOOLEAN, T_STRING, T_IDU,
    DT_ARRAY, DT_STRUCT, DT_ALIAS, DC_LIST, DC_LI,
    DF_RULE, LP_LIST, LP_IDD, B_RULE, LDV_LIST, LDV_DV, LS_LIST, LS_S,
    DV_RULE, LI_LIST, LI_IDD,
    S_IF, S_IF_ELSE, S_WHILE, S_DO_WHILE, S_BLOCK, S_ASSIGN,
    S_BREAK, S_CONTINUE, S_RETURN,
    E_AND, E_OR, E_L,
    L_LT, L_GT, L_LE, L_GE, L_EQ, L_NE, L_R,
    R_PLUS, R_MINUS, R_Y,
    Y_TIMES, Y_DIVIDE, Y_F,
    F_LV, F_PRE_INC, F_PRE_DEC, F_POS_INC, F_POS_DEC, F_PAREN, F_CALL,
    F_MINUS, F_NOT, F_TRUE, F_FALSE, F_CHR, F_STR, F_NUM,
    LE_LIST, LE_E,
    LV_FIELD, LV_INDEX, LV_IDU,
    IDD_ID, IDU_ID, ID_ID,
    TRUE_RULE, FALSE_RULE, CHR_RULE, STR_RULE, NUM_RULE,
    NB_RULE, NF_RULE, MF_RULE, MC_RULE, ML_RULE,
    NUM_RULES
} t_rule;

struct t_ruleDef {
    t_rule           rule;      /* conferido com a posicao na tabela */
    int              left;
    std::vector<int> right;
};

extern const std::vector<t_ruleDef> Rules;

bool        IsTerminal(int symbol);
const char *SymbolName(int symbol);          /* "'while'", "E", "ID" ...  */
void        PrintRule(int rule);

#endif /* GRAMATICA_H */
