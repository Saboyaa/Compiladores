/* Geracao de Codigo (cap. 7): instrucoes da maquina virtual de pilha,
 * area de codigo e listagem. As instrucoes sao emitidas por Semantics()
 * (semantico.cpp) e executadas por vm.cpp. Ver README, secao 6. */

#ifndef CODIGO_H
#define CODIGO_H

#include <cstdio>
#include <string>
#include <vector>

/* prefixo I_ porque o lexico ja usa AND, OR, NOT, RETURN ... */
typedef enum {
    I_LABEL,                                     /* rotulo: a                  */
    I_BEGIN_FUNC, I_END_FUNC, I_RETURN, I_CALL,  /* funcoes                    */
    I_JMP_FW, I_JMP_BW, I_TJMP_FW, I_TJMP_BW, I_FJMP_FW, I_FJMP_BW,
    I_LOAD_REF, I_LOAD_VAR, I_STORE_VAR,         /* variaveis do frame         */
    I_DE_REF, I_STORE_REF,                       /* acesso por endereco        */
    I_OFFSET, I_INDEX, I_STR_INDEX,              /* campo, array e string      */
    I_LOAD_CONST, I_LOAD_TRUE, I_LOAD_FALSE,
    I_DUP, I_POP,
    I_ADD, I_SUB, I_MUL, I_DIV, I_NEG, I_INC, I_DEC, I_CONCAT, I_NOT,
    I_LT, I_GT, I_LE, I_GE, I_EQ, I_NE
} t_op;

struct t_instr {
    t_op        op;
    int         a, b, c;    /* operandos; o significado depende de op */
    std::string s;          /* LOAD_CONST de string                   */
};

/* tipo da constante em LOAD_CONST (c), como no pool do lexico */
enum { CONST_CHAR, CONST_INT, CONST_STRING };

extern std::vector<t_instr> Code;

void ResetCode(void);
int  Emit(t_op op, int a = 0, int b = 0, int c = 0);   /* devolve o indice */
int  NewLabel(void);

const char *OpName(t_op op);
void        PrintCode(FILE *out);

#endif /* CODIGO_H */
