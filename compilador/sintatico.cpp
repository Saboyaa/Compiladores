/* Analisador Sintatico ascendente dirigido pela Tabela de Acao la-LR(1)
 * (fig. 3.20 com a chamada a Semantics() da fig. 4.6). */

#include <string>
#include "lalr.h"
#include "sintatico.h"

/* le o arquivo inteiro: erros lexicos impedem a analise sintatica */
std::vector<t_tokenInfo> ReadTokens(FILE *f)
{
    std::vector<t_tokenInfo> tokens;

    initLexer(f);
    while (nextToken() != ENDOFFILE)
        tokens.push_back({ token, tokenSecundario, { nTokenLine, nTokenCol } });
    tokens.push_back({ ENDOFFILE, -1, { nLine, 1 } });
    return tokens;
}

static std::string Describe(const t_tokenInfo &a)
{
    if (a.token == ENDOFFILE)
        return "fim de arquivo";
    if (a.token == ID)
        return std::string("Id '") + getName(a.secondary) + "'";
    return SymbolName(a.token);
}

/* x e esperado se, apos as reducoes que provoca, chega a ser empilhado.
 * Consultar so Action[q][x] listaria tambem look-aheads que a fusao dos
 * estados la-LR(1) trouxe de outros contextos. */
static bool Acceptable(std::vector<int> stack, int x)
{
    for (;;) {
        int p = Action[stack.back()][x];
        if (IS_SHIFT(p))
            return true;
        if (!IS_REDUCTION(p))
            return false;
        stack.resize(stack.size() - Len[RULE(p)]);
        stack.push_back(Action[stack.back()][Left[RULE(p)]]);
    }
}

static void SyntaxError(const std::vector<int> &stack, const t_tokenInfo &a)
{
    std::string text = "encontrado " + Describe(a) + "; esperado:";

    for (int x = 0; x < NUM_TERMINALS; x++)
        if (Acceptable(stack, x))
            text += std::string(" ") + SymbolName(x);
    RulePos = a.pos;
    Error(ERR_SYNTAX, text);
}

static bool Parse(const std::vector<t_tokenInfo> &tokens)
{
    std::vector<int>   stack;          /* pilha sintatica: estados          */
    std::vector<t_pos> posStack;       /* inicio de cada simbolo empilhado */
    size_t             next = 0;
    int                q, p, r;

    q = 0; stack.push_back(0); posStack.push_back({ 1, 1 });
    t_tokenInfo a = tokens[next++];
    do {
        p = Action[q][a.token];
        if (IS_SHIFT(p)) {
            stack.push_back(p);
            posStack.push_back(a.pos);
            nShiftedSecondary = a.secondary;     /* Id, c, s ou n que sera reduzido */
            if (next < tokens.size())
                a = tokens[next++];
        } else if (IS_REDUCTION(p)) {
            r = RULE(p);
            RulePos = Len[r] > 0 ? posStack[posStack.size() - Len[r]] : a.pos;
            stack.resize(stack.size() - Len[r]);
            posStack.resize(posStack.size() - Len[r]);
            stack.push_back(Action[stack.back()][Left[r]]);
            posStack.push_back(RulePos);
            Semantics(r);
        } else {
            SyntaxError(stack, a);
            return false;
        }
        q = stack.back();
    } while (q != FINAL);
    return true;
}

bool Compile(FILE *f)
{
    static bool built = false;

    if (!built) {
        BuildTables();
        built = true;
    }
    InitSemantics();

    std::vector<t_tokenInfo> tokens = ReadTokens(f);
    for (const t_tokenInfo &t : tokens)
        if (t.token == UNKNOWN) {
            RulePos = t.pos;
            Error(ERR_LEXICAL);
        }

    if (Errors.empty())
        Parse(tokens);
    return Errors.empty();
}
