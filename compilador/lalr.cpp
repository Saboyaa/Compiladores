/* Construcao da Tabela de Acao la-LR(1) (cap. 3.5 a 3.8).
 *
 * Os estados sao calculados diretamente na forma la-LR(1): cada estado e
 * identificado pelo seu nucleo (itens LR(0) do kernel) e, quando o goto de
 * um estado produz um nucleo ja existente, os look-aheads sao unidos aos do
 * estado existente, que volta a ser processado se ganhou look-aheads novos
 * (cap. 3.8, "calculados diretamente reaproveitando estados ja existentes").
 * O resultado e o mesmo da fusao dos estados LR(1) canonicos de mesmo nucleo. */

#include <cstdio>
#include <deque>
#include <map>
#include "gramatica.h"
#include "lalr.h"

typedef unsigned long long t_set;     /* conjunto de terminais (look-aheads) */
static_assert(NUM_TERMINALS <= 64, "t_set comporta no maximo 64 terminais");

#define BIT(a) (1ULL << (a))

struct t_item {
    int rule, dot;
    bool operator<(const t_item &o) const
    {
        return rule != o.rule ? rule < o.rule : dot < o.dot;
    }
};

struct t_state {
    std::vector<t_item> kernel;       /* ordenado: e o nucleo do estado */
    std::vector<t_set>  la;           /* look-aheads de cada item        */
};

std::vector<std::vector<int> > Action;
std::vector<int>               Len;
std::vector<int>               Left;
int                            FINAL;
std::vector<t_conflict>        ShiftReduce, ReduceReduce;

static std::vector<bool>              nullable;
static std::vector<t_set>             first;
static std::vector<std::vector<int> > rulesOf;     /* regras de cada nao terminal */
static std::vector<t_state>           states;

/* ------------------------------------------------------------------ */
/* First (cap. 3.2), calculado por ponto fixo                          */
/* ------------------------------------------------------------------ */

static void ComputeFirst(void)
{
    nullable.assign(NUM_SYMBOLS, false);
    first.assign(NUM_SYMBOLS, 0);
    rulesOf.assign(NUM_SYMBOLS, std::vector<int>());

    for (int a = 0; a < NUM_TERMINALS; a++)
        first[a] = BIT(a);
    for (int r = 0; r < NUM_RULES; r++)
        rulesOf[Rules[r].left].push_back(r);

    bool changed = true;
    while (changed) {
        changed = false;
        for (const t_ruleDef &d : Rules) {
            t_set f       = first[d.left];
            bool  allNull = true;

            for (int s : d.right) {
                f |= first[s];
                if (!nullable[s]) { allNull = false; break; }
            }
            if (f != first[d.left])            { first[d.left] = f;       changed = true; }
            if (allNull && !nullable[d.left])  { nullable[d.left] = true; changed = true; }
        }
    }
}

/* First( right[from..] la ) */
static t_set FirstSeq(const std::vector<int> &right, size_t from, t_set la)
{
    t_set s = 0;

    for (size_t i = from; i < right.size(); i++) {
        s |= first[right[i]];
        if (!nullable[right[i]])
            return s;
    }
    return s | la;
}

/* ------------------------------------------------------------------ */
/* fechamento de um estado (cap. 3.5 e 3.7)                            */
/* ------------------------------------------------------------------ */

static void Closure(const t_state &st, std::vector<t_item> &items,
                    std::vector<t_set> &la)
{
    std::map<t_item, size_t> pos;

    items = st.kernel;
    la    = st.la;
    for (size_t i = 0; i < items.size(); i++)
        pos[items[i]] = i;

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < items.size(); i++) {
            const t_ruleDef &d = Rules[items[i].rule];
            size_t dot = items[i].dot;

            if (dot >= d.right.size() || IsTerminal(d.right[dot]))
                continue;

            t_set s = FirstSeq(d.right, dot + 1, la[i]);
            for (int r : rulesOf[d.right[dot]]) {
                t_item it = { r, 0 };
                std::map<t_item, size_t>::iterator p = pos.find(it);

                if (p == pos.end()) {
                    pos[it] = items.size();
                    items.push_back(it);
                    la.push_back(s);
                    changed = true;
                } else if ((la[p->second] | s) != la[p->second]) {
                    la[p->second] |= s;
                    changed = true;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* estados la-LR(1) e Tabela de Acao                                   */
/* ------------------------------------------------------------------ */

static void SetReduction(int q, int a, int r)
{
    int &cell = Action[q][a];

    if (cell == 0) {
        cell = -r;
    } else if (IS_SHIFT(cell)) {                  /* empilha/reduz: empilha */
        ShiftReduce.push_back({ q, a, cell, -r });
    } else if (RULE(cell) != r) {                 /* reduz/reduz: menor regra */
        int keep = RULE(cell) < r ? cell : -r;
        ReduceReduce.push_back({ q, a, keep, keep == cell ? -r : cell });
        cell = keep;
    }
}

void BuildTables(void)
{
    std::map<std::vector<t_item>, int> byCore;
    std::vector<std::map<int, int> >   go;
    std::deque<int>                    work;
    std::vector<bool>                  queued;

    for (int r = 0; r < NUM_RULES; r++)
        if (Rules[r].rule != r)
            fprintf(stderr, "erro interno: regra %d fora de ordem\n", r);

    ComputeFirst();
    states.clear();
    ShiftReduce.clear();
    ReduceReduce.clear();

    states.push_back({ { { START_RULE, 0 } }, { 0 } });   /* S' -> . P '$', vazio */
    byCore[states[0].kernel] = 0;
    go.push_back(std::map<int, int>());
    queued.push_back(true);
    work.push_back(0);

    while (!work.empty()) {
        int q = work.front();
        work.pop_front();
        queued[q] = false;

        std::vector<t_item> items;
        std::vector<t_set>  la;
        Closure(states[q], items, la);

        /* o ponto caminha sobre cada simbolo X: kernel do estado destino */
        std::map<int, std::map<t_item, t_set> > next;
        for (size_t i = 0; i < items.size(); i++) {
            const t_ruleDef &d = Rules[items[i].rule];
            if ((size_t)items[i].dot < d.right.size()) {
                t_item it = { items[i].rule, items[i].dot + 1 };
                next[d.right[items[i].dot]][it] |= la[i];
            }
        }

        for (const auto &g : next) {
            t_state dest;
            for (const auto &kv : g.second) {
                dest.kernel.push_back(kv.first);
                dest.la.push_back(kv.second);
            }

            std::map<std::vector<t_item>, int>::iterator f = byCore.find(dest.kernel);
            int target;
            if (f == byCore.end()) {                          /* estado novo */
                target = (int)states.size();
                byCore[dest.kernel] = target;
                states.push_back(dest);
                go.push_back(std::map<int, int>());
                queued.push_back(true);
                work.push_back(target);
            } else {                                          /* fusao       */
                target = f->second;
                bool grew = false;
                for (size_t k = 0; k < dest.la.size(); k++) {
                    t_set u = states[target].la[k] | dest.la[k];
                    if (u != states[target].la[k]) {
                        states[target].la[k] = u;
                        grew = true;
                    }
                }
                if (grew && !queued[target]) {
                    queued[target] = true;
                    work.push_back(target);
                }
            }
            go[q][g.first] = target;
        }
    }

    /* Tabela de Acao: transicoes (empilhamentos e nao terminais) e reducoes */
    Action.assign(states.size(), std::vector<int>(NUM_SYMBOLS, 0));
    for (size_t q = 0; q < states.size(); q++)
        for (const auto &t : go[q])
            Action[q][t.first] = t.second;

    for (size_t q = 0; q < states.size(); q++) {
        std::vector<t_item> items;
        std::vector<t_set>  la;
        Closure(states[q], items, la);

        for (size_t i = 0; i < items.size(); i++)
            if ((size_t)items[i].dot == Rules[items[i].rule].right.size())
                for (int a = 0; a < NUM_TERMINALS; a++)
                    if (la[i] & BIT(a))
                        SetReduction((int)q, a, items[i].rule);
    }

    Len.resize(NUM_RULES);
    Left.resize(NUM_RULES);
    for (int r = 0; r < NUM_RULES; r++) {
        Len[r]  = (int)Rules[r].right.size();
        Left[r] = Rules[r].left;
    }

    FINAL = Action[Action[0][P_]][ENDOFFILE];
}

/* ------------------------------------------------------------------ */
/* estatisticas da tabela (modo --tabela)                              */
/* ------------------------------------------------------------------ */

static void PrintConflict(const char *kind, const t_conflict &c)
{
    printf("  %s no estado %d com %s:\n", kind, c.state, SymbolName(c.symbol));
    for (int p : { c.kept, c.discarded }) {
        printf("    %s ", p == c.kept ? "mantido:   " : "descartado:");
        if (IS_SHIFT(p)) {
            printf("empilha %d\n", p);
        } else {
            printf("reduz %d: ", RULE(p));
            PrintRule(RULE(p));
            printf("\n");
        }
    }
}

void PrintTableStats(void)
{
    int nShift = 0, nGoto = 0, nReduce = 0;

    for (const std::vector<int> &row : Action)
        for (int s = 0; s < NUM_SYMBOLS; s++) {
            if (IS_SHIFT(row[s]) && IsTerminal(s))  nShift++;
            if (IS_SHIFT(row[s]) && !IsTerminal(s)) nGoto++;
            if (IS_REDUCTION(row[s]))               nReduce++;
        }

    printf("Tabela la-LR(1)\n");
    printf("  regras:                %d\n", NUM_RULES);
    printf("  terminais:             %d\n", NUM_TERMINALS);
    printf("  nao terminais:         %d\n", NUM_SYMBOLS - NUM_TERMINALS);
    printf("  estados:               %d\n", (int)Action.size());
    printf("  estado final:          %d\n", FINAL);
    printf("  empilhamentos:         %d\n", nShift);
    printf("  transicoes com N:      %d\n", nGoto);
    printf("  reducoes:              %d\n", nReduce);
    printf("  conflitos empilha/reduz: %d\n", (int)ShiftReduce.size());
    printf("  conflitos reduz/reduz:   %d\n", (int)ReduceReduce.size());

    for (const t_conflict &c : ShiftReduce)  PrintConflict("empilha/reduz", c);
    for (const t_conflict &c : ReduceReduce) PrintConflict("reduz/reduz", c);
}
