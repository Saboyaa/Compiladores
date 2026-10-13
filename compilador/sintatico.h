/* Analisador Sintatico ascendente la-LR(1) com avaliacao de atributos
 * (fig. 4.6) e driver da compilacao: lexico -> sintatico -> semantica. */

#ifndef SINTATICO_H
#define SINTATICO_H

#include <cstdio>
#include <vector>
#include "gramatica.h"
#include "semantico.h"

struct t_tokenInfo {
    t_token token;
    int     secondary;
    t_pos   pos;
};

std::vector<t_tokenInfo> ReadTokens(FILE *f);

/* compila o arquivo; os erros ficam em Errors. Devolve true se nao houver */
bool Compile(FILE *f);

#endif /* SINTATICO_H */
