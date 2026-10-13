/* Construcao da Tabela de Acao la-LR(1) a partir da gramatica (cap. 3.8).
 * Codificacao da fig. 3.19: p > 0 empilha o estado p (ou transicao com nao
 * terminal), p < 0 reduz pela regra -p, p == 0 e erro. */

#ifndef LALR_H
#define LALR_H

#include <vector>

#define IS_SHIFT(p)      ((p) > 0)
#define IS_REDUCTION(p)  ((p) < 0)
#define RULE(p)          (-(p))

struct t_conflict {
    int  state, symbol;
    int  kept, discarded;    /* acoes na codificacao acima */
};

extern std::vector<std::vector<int> > Action;   /* Action[q][simbolo]      */
extern std::vector<int>               Len;      /* Len[r]                  */
extern std::vector<int>               Left;     /* Left[r]                 */
extern int                            FINAL;    /* estado de aceitacao     */

extern std::vector<t_conflict> ShiftReduce, ReduceReduce;

void BuildTables(void);
void PrintTableStats(void);

#endif /* LALR_H */
