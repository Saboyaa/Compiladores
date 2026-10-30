/* Maquina virtual de pilha que executa o codigo gerado (cap. 7).
 * Serve para conferir a geracao de codigo: sslc --executar e testes.cpp. */

#ifndef VM_H
#define VM_H

#include <stdexcept>
#include <string>
#include <vector>
#include "semantico.h"

/* uma palavra da maquina: inteiro (integer, char, boolean), string ou
 * endereco (posicao na memoria e, para s[i], o indice do caractere) */
struct t_value {
    enum { NUM, STR, ADDR } kind;
    int         n;          /* NUM: valor; ADDR: posicao na memoria */
    int         chr;        /* ADDR: indice no string, ou -1        */
    std::string s;          /* STR                                  */
};

t_value Num(int n);
t_value Str(const std::string &s);

/* executa a funcao f com os argumentos (palavras, na ordem dos parametros)
 * e devolve as palavras do valor de retorno. Erros de execucao lancam
 * std::runtime_error. */
std::vector<t_value> Execute(int f, const std::vector<t_value> &args);

object     *FindFunction(const char *name);
bool        ParseArg(object *type, const char *text, std::vector<t_value> &out);
std::string FormatValue(object *type, const std::vector<t_value> &words);

#endif /* VM_H */
