# Compilador da Simple Script Language — VF

Continuação do analisador léxico da VC (`../analisador`): análise **sintática**,
de **escopo** e de **tipos** da SSL, seguindo os capítulos 3 a 6 do livro do
Prof. Luiz Guedes. A geração de código (capítulo 7) não faz parte desta entrega.

```
make            # compila o compilador (sslc) e os testes (testes)
make test       # roda os testes e confere os exemplos
./sslc exemplos/validos/tipos.ssl
```

---

## 1. Entendimento

O compilador é **dirigido pela sintaxe**: o analisador sintático controla a
compilação e, a cada redução, chama `Semantics(regra)`, onde estão as regras
de atributos de escopo e de tipos.

```
arquivo.ssl ──► léxico (VC) ──► tokens ──► sintático la-LR(1) ──► Semantics(r)
                    │                          │                    │
              erros léxicos            erro de sintaxe       erros de escopo e tipo
              (param tudo)             (para no 1º)          (acumulados)
```

| Fase | Capítulo | Arquivo | O que faz |
|---|---|---|---|
| Léxica | 2 | `lexer.c` | autômato da VC, com `return` e a coluna dos tokens |
| Sintática | 3.5–3.8 | `gramatica.cpp`, `lalr.cpp`, `sintatico.cpp` | constrói a Tabela de Ação la-LR(1) e roda o algoritmo da fig. 4.6 |
| Atributos | 4 | `semantico.cpp` | pilha semântica paralela à sintática e `Semantics()` com `switch` por regra |
| Escopo | 5 | `semantico.cpp` | tabela de símbolos como pilha de níveis; `IDD`, `IDU`, `NB` |
| Tipos | 6 | `semantico.cpp` | kinds, tipos pré-definidos, tipo universal, `CheckTypes` e verificação das expressões e comandos |

O arquivo inteiro passa pelo léxico antes da análise sintática, de modo que
qualquer lexema inválido é relatado e impede as fases seguintes. O sintático
para no primeiro erro (o livro não trata recuperação). Os erros de escopo e de
tipo são acumulados, e o tipo universal evita que um erro gere outros em cascata.

## 2. Gramática efetivamente usada

É a gramática oficial (`Gramática.pdf`) com os marcadores semânticos dos
capítulos 5 e 6 e uma única extensão, o comando `return`. São 84 regras,
contando a regra aumentada `S' → P '$'`. A tabela das regras está em
`gramatica.cpp`, na mesma ordem do `enum t_rule`.

```
P   → LDE
LDE → LDE DE | DE
DE  → DF | DT
T   → 'integer' | 'char' | 'boolean' | 'string' | IDU
DT  → 'type' IDD '=' 'array' '[' NUM ']' 'of' T
    | 'type' IDD '=' 'struct' NB '{' DC '}'                     -- NB (fig. 5.9)
    | 'type' IDD '=' T
DC  → DC ';' LI ':' T | LI ':' T                                -- 'T' lido como T
DF  → 'function' IDD NF '(' LP ')' ':' T MF B                   -- NF e MF (fig. 6.13, 7.18)
LP  → LP ',' IDD ':' T | IDD ':' T
B   → '{' LDV LS '}'
LDV → LDV DV | DV
LS  → LS S | S
DV  → 'var' LI ':' T ';'
LI  → LI ',' IDD | IDD
S   → 'if' '(' E ')' S
    | 'if' '(' E ')' S 'else' S
    | 'while' '(' E ')' ML S                                    -- ML: novo
    | 'do' ML S 'while' '(' E ')' ';'                           -- ML: novo
    | NB B                                                      -- NB (fig. 6.14)
    | LV '=' E ';'
    | 'break' ';'
    | 'continue' ';'
    | 'return' E ';'                                            -- extensão
E   → E '&&' L | E '||' L | L
L   → L '<' R | L '>' R | L '<=' R | L '>=' R | L '==' R | L '!=' R | R
R   → R '+' Y | R '-' Y | Y
Y   → Y '*' F | Y '/' F | F
F   → LV | '++' LV | '--' LV | LV '++' | LV '--' | '(' E ')'
    | IDU MC '(' LE ')'                                         -- MC (fig. 6.23)
    | '-' F | '!' F | TRUE | FALSE | CHR | STR | NUM
LE  → LE ',' E | E
LV  → LV '.' ID | LV '[' E ']' | IDU                            -- ID no campo (cap. 5)
IDD → Id      IDU → Id      ID → Id
TRUE → 'true'   FALSE → 'false'   CHR → c   STR → s   NUM → n
NB → ε   NF → ε   MF → ε   MC → ε   ML → ε
```

Diferenças em relação à gramática oficial e por quê:

| Diferença | Justificativa |
|---|---|
| `S → 'return' E ';'` e a palavra reservada `return` | **Única alteração da linguagem.** Sem ela uma função não devolve valor. Como os parâmetros são passados por valor e não há variáveis globais (`DE` não tem `DV`), nenhum programa teria efeito observável. Os exemplos do cap. 7 usam `return`. |
| `DC → ... ':' T` em vez de `':' 'T'` | As aspas em `'T'` são erro de digitação: não existe o lexema `T` (decisão da VC). |
| `LV → LV '.' ID` em vez de `LV '.' IDU` | O campo só pode ser resolvido pelo tipo da struct, na análise de tipos. O cap. 5 diz isso e usa `ID`. |
| `NB` em `DT` struct e em `S → NB B` | São os marcadores de escopo do cap. 5 (fig. 5.9) e do cap. 6 (fig. 6.14). Não alteram a linguagem. |
| `NF` e `MF` em `DF` | `NF` substitui o `NB` da fig. 5.10: marca o identificador como função e abre o nível dos parâmetros. `MF` preenche a função antes do corpo, o que permite a recursão (fig. 6.13). |
| `MC` na chamada | Verifica o kind e disponibiliza a lista de parâmetros antes dos argumentos (fig. 6.23). |
| `ML` em `while` e `do` | Marcador novo, sem efeito na linguagem. Conta os laços abertos para acusar `break`/`continue` fora de laço, que o livro não trata. |

## 3. Análise sintática

**Construção da tabela** (`lalr.cpp`), feita pelo próprio programa a partir da
gramática:

1. `First` e `nullable` de cada não terminal por ponto fixo (cap. 3.2).
2. O estado inicial é o item `S' → . P '$'` com look-ahead vazio.
3. `Closure`: para cada item `A → α . B β, s`, acrescenta `B → . γ` com
   look-aheads `First(β s)` (cap. 3.7). Repete até nada mudar.
4. `goto`: o ponto caminha sobre cada símbolo X e forma o kernel do estado de
   destino.
5. **Fusão pelo núcleo** (cap. 3.8): os estados são identificados pelo núcleo
   (os itens LR(0) do kernel). Quando um goto chega a um núcleo já existente, os
   look-aheads são unidos aos desse estado, que volta à fila se ganhou algum
   look-ahead novo. É o cálculo "direto" descrito no livro e dá o mesmo
   autômato que fundir os estados LR(1) canônicos de mesmo núcleo.
6. **Tabela de Ação** na codificação da fig. 3.19: `p > 0` empilha (ou é a
   transição com não terminal), `p < 0` reduz pela regra `-p` e `0` é erro.
   `Len[r]` e `Left[r]` vêm da gramática, e `FINAL = Action[Action[0][P]]['$']`.

Os conjuntos de look-ahead são `unsigned long long`, um bit por terminal (51
terminais).

**Estatísticas** (`./sslc --tabela`):

```
regras: 84   terminais: 51   não terminais: 35   estados: 171   estado final: 11
empilhamentos: 464   transições com N: 320   reduções: 883
conflitos empilha/reduz: 1   conflitos reduz/reduz: 0
empilha/reduz no estado 158 com 'else':
  mantido:    empilha 163
  descartado: reduz 27: S -> 'if' '(' E ')' S
```

Para conferir, a mesma gramática foi passada ao bison apenas como verificação
(ele não é usado pelo compilador). O bison acusou 1 conflito empilha/reduz no
mesmo estado 158 e 172 estados: o estado a mais é o de aceitação que ele cria
para `$accept`.

**Else pendente.** É o único conflito: em `if (E) S . else S`, o `else` pode ser
empilhado ou causar a redução do `if` sem `else`. Ao montar a tabela,
`SetReduction` mantém o empilhamento. Assim o `else` fica com o `if` mais
próximo, como em C. Se houvesse um conflito reduz/reduz, seria mantida a regra de
menor número, mas não há nenhum.

**Algoritmo** (`sintatico.cpp`): é o da fig. 4.6, com duas pilhas paralelas à
sintática.

* A pilha de **posições** guarda a linha e a coluna do primeiro token de cada
  símbolo. Na redução, `RulePos` recebe a posição do início da regra, que é
  usada nas mensagens de erro.
* Ao empilhar um terminal, o parser guarda o seu token secundário em
  `nShiftedSecondary`. A variável `tokenSecundario` do léxico não serve: quando
  `IDD → Id` é reduzida, o léxico já leu o token seguinte. Uma regra cujo lado
  direito é um único terminal é sempre reduzida logo após o empilhamento desse
  terminal, então o valor guardado é o correto.

**Mensagens de erro sintático**: linha, coluna, token encontrado e tokens
esperados. Um token é listado como esperado se, depois das reduções que provoca
(simuladas sobre uma cópia da pilha), ele chega a ser empilhado. Consultar só
`Action[q][a]` listaria também look-aheads que a fusão de estados trouxe de
outros contextos. Exemplo:

```
x.ssl:3:2: erro de sintaxe: encontrado Id 'i'; esperado: ';' [ERR_SYNTAX]
```

## 4. Análise de escopo

A tabela de símbolos é uma **pilha de níveis** (fig. 5.1). Cada nível é um
`std::vector<object*>`, e os objetos ficam num `std::deque<object>`, em memória
estável: nenhum ponteiro é invalidado quando outros objetos são criados.

| Função | Fig. | Comportamento |
|---|---|---|
| `NewBlock()` | 5.4 | empilha um nível vazio |
| `EndBlock()` | 5.4 | desempilha o nível. Os objetos continuam existindo, porque structs e funções guardam ponteiros para campos e parâmetros |
| `Define(nome)` | 5.5 | cria o objeto no nível corrente |
| `Search(nome)` | 5.6 | busca só no nível corrente |
| `Find(nome)` | 5.7 | busca do nível corrente até o nível 0 |

Regras:

* **`IDD → Id`** (declaração): se o nome já existe no nível corrente, acusa
  `ERR_REDECL`. Em qualquer caso, cria um objeto novo com kind `NO_KIND_DEF_`.
* **`IDU → Id`** (uso): se o nome não é encontrado em nenhum nível visível, acusa
  `ERR_NOT_DECL` e define o nome no nível corrente com kind `UNIVERSAL_`. Assim
  os usos seguintes não repetem o erro nem geram erros de tipo.
* **`ID → Id`** (campo): não consulta a tabela. O campo é procurado na lista de
  campos da struct, em `LV → LV '.' ID`.
* **Níveis**: o nível 0 tem os tipos e as funções. `NF` abre o nível da função,
  depois de o nome ter sido declarado no nível 0. Parâmetros e variáveis locais
  ficam no **mesmo nível** (fig. 5.2): um local com o nome de um parâmetro é
  redeclaração, e `B` não abre nível. `S → NB B` abre um nível para cada
  bloco-comando, onde é permitido sombrear nomes externos. `NB` na struct põe os
  campos num nível próprio, fechado em `DT`. Por isso dois tipos struct podem
  ter campos de mesmo nome, e um campo não é visível como variável.
* **Recursão**: o nome da função está no nível 0 e é completado em `MF`, antes
  do corpo.
* **Declare antes de usar**: uma função só pode chamar funções declaradas antes
  dela (não há protótipos na gramática).

## 5. Análise de tipos

**Kinds** (fig. 6.1): `NO_KIND_DEF_`, `VAR_`, `PARAM_`, `FUNCTION_`, `FIELD_`,
`ARRAY_TYPE_`, `STRUCT_TYPE_`, `ALIAS_TYPE_`, `SCALAR_TYPE_` e `UNIVERSAL_`.

**Objeto** (fig. 6.2): é uma `struct` sem a `union`, porque em C++11 um
`std::vector` não pode ficar numa `union` sem construtores manuais. As listas
encadeadas por `pNext` viraram vetores: `vParams` na função, `vFields` na struct
e `list` nos atributos de `LI`, `DC` e `LP`.

**Tipos pré-definidos** (fig. 6.3): os objetos `int_`, `char_`, `bool_`,
`string_` (todos com kind `SCALAR_TYPE_`) e `universal_`, com kind `UNIVERSAL_`.

**Compatibilidade.** `CheckTypes` usa **equivalência por nome com os alias
resolvidos**. Antes de comparar, `BaseType()` segue a cadeia de alias. Dois tipos
são compatíveis se, depois disso, são o mesmo objeto ou se algum deles tem kind
`UNIVERSAL_`. Consequências:

* `type Inteiro = integer` é compatível com `integer`, e alias de alias também.
* Dois `struct` (ou dois `array`) declarados separadamente são incompatíveis,
  mesmo que tenham a mesma forma.
* Acesso a campo e indexação também resolvem os alias: um alias de struct tem
  campos e um alias de `string` pode ser indexado.

**Pilha semântica** (cap. 4.1): um único `t_attrib` com os atributos de todos os
não terminais. Seguindo a convenção do livro, todo não terminal empilha um nó:
depois do `switch`, `Semantics` desempilha um nó por não terminal do lado direito
e empilha o do lado esquerdo. `TopSem(k)` devolve o nó a `k` posições do topo.

**Regras implementadas:**

| Construção | Regra (fig.) | Verificação |
|---|---|---|
| uso de tipo | `T → IDU` (6.5) | o kind deve ser de tipo, senão `ERR_TYPE_EXPECTED` e `T.type = universal` |
| variáveis | `DV` (6.6) | kind `VAR_` e o tipo de `T` |
| literais | 6.7 | `true`/`false` são boolean, `c` é char, `s` é string, `n` é integer, com o valor do pool |
| array | `DT` (6.8) | `ARRAY_TYPE_`, número de elementos e tipo do elemento |
| alias | `DT` (6.9) | `ALIAS_TYPE_` e tipo base |
| campos e struct | `DC`, `DT` (6.10, 6.11) | `FIELD_`, `STRUCT_TYPE_`, lista de campos e `EndBlock` |
| parâmetros | `LP` (6.12) | `PARAM_` e lista de parâmetros |
| função | `NF`, `MF`, `DF` (6.13) | `FUNCTION_`, tipo de retorno e parâmetros antes do corpo; `EndBlock` |
| condições | `if`, `while`, `do` (6.15) | a expressão deve ser boolean (`ERR_BOOL_TYPE_EXPECTED`) |
| atribuição | `LV '=' E` (6.15) | tipos compatíveis (`ERR_TYPE_MISMATCH`) |
| lógicas | `&&`, `\|\|` (6.18) | operandos boolean; resultado boolean |
| relacionais | 6.19 | operandos compatíveis; resultado boolean |
| aritméticas | 6.20 | operandos compatíveis e integer; `+` aceita também string (concatenação) |
| unárias | 6.21 | `++`, `--` e `-` exigem integer; `!` exige boolean |
| left-values | 6.22 | campo de struct; índice integer de array ou de string (o elemento de string é char); `IDU` deve ser variável ou parâmetro |
| chamada | `MC`, `LE`, `F` (6.23) | kind função, quantidade e tipo de cada argumento |
| `return` | extensão | o tipo do valor deve ser compatível com o tipo de retorno da função corrente |
| `break`/`continue` | `ML` (novo) | só dentro de `while` ou `do` |

**Erros** (`t_error` em `semantico.h`; cada um tem um caso em `testes.cpp`):

| Código | Fase | Quando |
|---|---|---|
| `ERR_LEXICAL` | léxica | lexema inválido (`UNKNOWN`) |
| `ERR_SYNTAX` | sintática | `Action[q][a] == 0` |
| `ERR_REDECL` | escopo | nome já declarado no nível corrente |
| `ERR_NOT_DECL` | escopo | nome não declarado em nenhum nível visível |
| `ERR_TYPE_EXPECTED` | tipos | `T → IDU` com algo que não é tipo (inclui tipo recursivo) |
| `ERR_BOOL_TYPE_EXPECTED` | tipos | condição ou operando de `&&`/`\|\|` não boolean |
| `ERR_TYPE_MISMATCH` | tipos | atribuição, relacional ou aritmética com tipos incompatíveis |
| `ERR_INVALID_TYPE` | tipos | operador aplicado a um tipo que ele não aceita |
| `ERR_KIND_NOT_STRUCT` | tipos | `.campo` em algo que não é struct |
| `ERR_FIELD_NOT_DECL` | tipos | campo inexistente na struct |
| `ERR_KIND_NOT_ARRAY` | tipos | `[ ]` em algo que não é array nem string |
| `ERR_INVALID_INDEX_TYPE` | tipos | índice não integer |
| `ERR_KIND_NOT_VAR` | tipos | `LV → IDU` com função ou tipo |
| `ERR_KIND_NOT_FUNC` | tipos | chamada de algo que não é função |
| `ERR_TOO_MANY_ARGS` | tipos | argumentos demais |
| `ERR_PARAM_TYPE` | tipos | argumento `n` incompatível com o parâmetro `n` |
| `ERR_TOO_FEW_ARGS` | tipos | argumentos de menos |
| `ERR_RETURN_TYPE` | tipos | `return` com tipo incompatível (extensão) |
| `ERR_NOT_IN_LOOP` | tipos | `break`/`continue` fora de laço (marcador `ML`) |

**Tipo universal.** Quem recebe o tipo universal não gera novos erros: um nome
não declarado, um `T` que não é tipo, um campo inexistente, um acesso inválido,
uma chamada de algo que não é função. Em `MC`, `LV → IDU`, `LV '.' ID` e
`LV '[' E ']'`, o kind `UNIVERSAL_` é aceito em silêncio, e com `MC.err` os
argumentos não são verificados.

## 6. Geração de código

Fora do escopo desta entrega, por decisão do enunciado. Nenhuma instrução da
máquina virtual é gerada. A estrutura já está pronta para recebê-la: os
marcadores `NF` e `MF` estão nos lugares onde o cap. 7 gera `BEGIN_FUNC`, a
pilha semântica aceita novos atributos e `ML` marca o início dos laços (onde
ficariam os rótulos de `break`/`continue`). As inconsistências do cap. 7 estão
registradas na seção 7 para o trabalho futuro.

## 7. Decisões e ambiguidades

**Levantadas no enunciado:**

| # | Inconsistência | Decisão |
|---|---|---|
| 1 | `'T'` entre aspas em `DC` | É o não terminal `T` (decisão da VC). |
| 2 | Cap. 5 usa `LV '.' ID`; a gramática diz `IDU` | `ID`: o campo é resolvido pelo tipo da struct (fig. 6.22). Com `IDU`, todo campo seria acusado como não declarado. |
| 3 | `return` não existe na gramática | Extensão `S → 'return' E ';'`, com palavra reservada `return` no léxico (seção 2) e verificação do tipo. **Não** se verifica se todo caminho retorna um valor: isso exige análise de fluxo, que os capítulos não cobrem. |
| 4 | `B → '{' LDV LS '}'` exige uma declaração e um comando; os exemplos violam | Mantida a gramática oficial. Não é indispensável alterar: todo bloco pode declarar uma variável, e um corpo de laço com um só comando dispensa o bloco. Os exemplos do cap. 7 foram reescritos assim (seção 9). |
| 5 | Fig. 7.18: `NF` substitui `NB`, mas `MF` ainda lê `NB = TopSem(-2)` | `NF` marca o kind `FUNCTION_` e chama `NewBlock()` antes de `LP`. `MF` preenche retorno e parâmetros e define `curFunction`. |
| 6 | Fig. 7.27: `LOAD_REF` com o tamanho do tipo | Geração de código: fora do escopo. |
| 7 | Tipo universal com kind `SCALAR_TYPE_` (fig. 6.3) | `universal_` tem kind `UNIVERSAL_`. Também recebem kind `UNIVERSAL_` os nomes definidos por `IDU` após `ERR_NOT_DECL`; no livro o kind fica indefinido e gera `ERR_TYPE_EXPECTED`/`ERR_KIND_NOT_VAR` em cascata. `MC` também aceita `UNIVERSAL_` sem erro. |
| 8 | Listas por `pNext` (fig. 6.6–6.13) misturam parâmetros e locais do mesmo nível | Vetores: `vParams`, `vFields` e `list`. Com `pNext`, os locais apareceriam como parâmetros e toda chamada daria `ERR_TOO_FEW_ARGS`. |
| 9 | `CheckTypes` estrutural (6.17) não avança `f1`/`f2` e entra em laço infinito; o por nome (6.16) torna o alias incompatível com o tipo base | Por nome, com alias resolvidos (seção 5). O livro define alias como "sinônimo", o que justifica resolvê-los. Equivalência estrutural de structs é dispensada. |
| 10 | `true`/`false`: `LOAD_TRUE`/`LOAD_FALSE` ou `LOAD_CONST` | Geração de código: fora do escopo. Na análise de tipos, os dois são boolean. |
| 11 | `BEGIN_FUNC` com ou sem os parâmetros | Geração de código: fora do escopo. |
| 12 | Indexação de string nos tipos (6.22), só de array na geração (7.27) | Nos tipos, `string[integer]` é `char`, inclusive como left-value. O código ficará para o cap. 7 (com `CALL_LIB`, se for o caso). |
| 13 | `break`/`continue` sem regras de atributos | Marcador `ML` e contador de laços: `ERR_NOT_IN_LOOP` fora de laço. O destino do `continue` no `do-while` é assunto da geração de código. |

**Encontradas durante o trabalho:**

| # | Inconsistência | Decisão |
|---|---|---|
| 14 | A gramática da VC (`gramatica.txt`) tem `DE → DV` (variáveis globais); a oficial não | Segue a oficial: não há variáveis globais. |
| 15 | `DT` não termina em `';'`, mas o exemplo 7.4.4 escreve `type intArray = ... ;` | Segue a gramática: `type A = integer` sem `';'`. O exemplo foi reescrito. |
| 16 | A gramática oficial define `Id` sem `'_'`; a VC aceita `'_'` | Mantida a decisão da VC. |
| 17 | `LP` e `LE` não são vazios: não há função sem parâmetro nem chamada sem argumento | Mantido. Não é indispensável mudar (basta um parâmetro sem uso). |
| 18 | A fig. 7.5 fala em expressão como comando (com `POP`), mas a gramática não tem esse comando: `i++;` é erro de sintaxe | Mantida a gramática: `i = i + 1;` ou `x = v[i++];`. |
| 19 | Os exemplos do cap. 7 não passam na análise: `somaFat` chama `fat` (não declarada); `strcmp` devolve `char - char` (`ERR_INVALID_TYPE` e retorno incompatível); `'\0'` não é um lexema válido (`c = ''' any '''`); `strinv` incrementa `l` em vez de decrementar; `bSort` usa o parâmetro `n` como temporário | Reescritos em `exemplos/validos/` com cada mudança comentada. Strings recebem o tamanho como parâmetro. |
| 20 | Fig. 6.4: na redeclaração, `IDD.obj` é o objeto existente, e o seu kind é zerado e redefinido (por exemplo, o parâmetro vira `char`) | Acusa `ERR_REDECL` e cria um objeto novo no mesmo nível. A busca dentro do nível vai do mais novo para o mais antigo, então os usos seguintes veem a última declaração (sem erros em cascata), e o objeto original, que pode estar em `vParams`, não muda. |
| 21 | `TopSem(1)` na fig. 4.9 e `TopSem(-1)` nas fig. 6.13 e 6.23 | `TopSem(k)` com `k ≥ 0` posições abaixo do topo, como na definição do cap. 4. |
| 22 | `tokenSecundario` na redução é o do look-ahead, não o do `Id` | O parser guarda o secundário do último terminal empilhado (seção 3). |
| 23 | Fig. 6.23: `LE.n` não é definido no caminho de erro e há chaves faltando | Reescrita com um índice: `LE.n` é o número de argumentos já verificados. |
| 24 | Fig. 5.8: a regra `ID → Id` atribui `IDU.name` | Atribui `ID.name`. |
| 25 | Tamanho de array não é verificado (`array [0]`) | Mantido como no livro. |
| 26 | O analisador da VC não compila com o GCC atual (`strdup` não é declarada com `-std=c99 -pedantic`) | Única mudança na VC: `-D_POSIX_C_SOURCE=200809L` no `Makefile`. Os fontes não mudaram. |
| 27 | No léxico da VC, o token `/` (divisão) não atualiza `nTokenLine` | Corrigido na cópia de `compilador/`, junto com a coluna. |
| 28 | Uso de nome não declarado antes de declará-lo no mesmo nível (`type A = B` e depois `type B = ...`) | Gera `ERR_NOT_DECL` e, depois, `ERR_REDECL`, como no livro (`IDU` define o nome). Na SSL tudo é declarado antes do uso. |

**Mudanças no léxico** (cópia em `compilador/lexer.c`; `diff -u ../analisador/lexer.c lexer.c`):

* a palavra reservada `return` (token `RETURN`), inserida em ordem alfabética
  entre `of` e `string`, o que preserva a busca binária;
* a coluna do início de cada token (`nTokenCol`), usada nas mensagens de erro;
* a posição do token `/` (item 27).

O léxico é compilado como C (`gcc -std=c99`) e incluído no C++ com
`extern "C"`. A VC em `../analisador/` continua intacta.

## 8. Como compilar, executar e testar

Requisitos: `g++` e `gcc` com suporte a C++11 e C99, e `make`.

```
cd compilador
make                                  # gera sslc e testes, sem avisos com -Wall -Wextra -Wpedantic
./sslc programa.ssl                   # compila; sai com 0 se não houver erros e 1 se houver
./sslc --tokens programa.ssl          # mesma saída do analisador da VC
./sslc --tabela                       # estatísticas e conflitos da tabela la-LR(1)
make test                             # ./testes + exemplos válidos (sem erro) e com erros (com erro)

cd ../analisador && make test         # a VC, inalterada
```

`testes.cpp` é um executável único com `assert` (73 verificações). Ele cobre:

* a tabela: exatamente 1 conflito empilha/reduz, no `else` e resolvido por
  empilhar, e 0 reduz/reduz;
* o léxico: `return` e as linhas e colunas dos tokens;
* os quatro programas de `exemplos/validos/` e 13 trechos válidos, todos sem
  erros (alias, strings, else pendente, sombreamento, recursão, retorno de
  struct e array);
* pelo menos um caso para cada código de erro, conferindo a lista **exata** de
  erros, o que também verifica que não há erros em cascata;
* a posição e os tokens esperados de uma mensagem de erro sintático.

Como não há geração de código, a comparação com o código esperado das figuras
7.4 a 7.10 fica para a próxima etapa.

## 9. Exemplos de entrada e saída

`exemplos/validos/`: `fatorial.ssl` (7.4.1), `strings.ssl` (7.4.2 e 7.4.3),
`bsort.ssl` (7.4.4) e `tipos.ssl` (structs, arrays, alias, recursão,
sombreamento, `do-while`, `break` e `continue`). Todos compilam sem erro:

```
$ ./sslc exemplos/validos/bsort.ssl
Compilacao concluida sem erros.
```

Exemplo 7.4.4 reescrito (trecho):

```
type intArray = array [30] of integer

function bSort(a: intArray, n: integer): intArray
{
    var i: integer;
    i = -1;
    while (++i < n)
    {
        var j: integer;
        j = i;
        while (++j < n)
            if (a[i] > a[j])
            {
                var t: integer;
                t = a[i];
                a[i] = a[j];
                a[j] = t;
            }
    }
    return a;
}
```

`exemplos/erros/`:

```
$ ./sslc exemplos/erros/lexico.ssl
exemplos/erros/lexico.ssl:4:11: lexema invalido [ERR_LEXICAL]
exemplos/erros/lexico.ssl:5:14: lexema invalido [ERR_LEXICAL]

$ ./sslc exemplos/erros/sintatico.ssl
exemplos/erros/sintatico.ssl:5:5: erro de sintaxe: encontrado 'return'; esperado: ';' '&&' '||' '<' '>' '<=' '>=' '!=' '==' '+' '-' '*' '/' [ERR_SYNTAX]

$ ./sslc exemplos/erros/escopo.ssl
exemplos/erros/escopo.ssl:1:36: identificador 'a' ja declarado neste escopo [ERR_REDECL]
exemplos/erros/escopo.ssl:3:24: identificador 'n' ja declarado neste escopo [ERR_REDECL]
exemplos/erros/escopo.ssl:5:9: identificador 'n' ja declarado neste escopo [ERR_REDECL]
exemplos/erros/escopo.ssl:6:9: identificador 'y' nao declarado [ERR_NOT_DECL]
exemplos/erros/escopo.ssl:9:13: identificador 'z' nao declarado [ERR_NOT_DECL]
exemplos/erros/escopo.ssl:11:12: identificador 'g' nao declarado [ERR_NOT_DECL]
exemplos/erros/escopo.ssl:14:10: identificador 'f' ja declarado neste escopo [ERR_REDECL]
exemplos/erros/escopo.ssl:16:12: identificador 'Tipo' nao declarado [ERR_NOT_DECL]

$ ./sslc exemplos/erros/tipos.ssl
exemplos/erros/tipos.ssl:9:5: tipos incompativeis [ERR_TYPE_MISMATCH]
exemplos/erros/tipos.ssl:10:9: expressao booleana esperada [ERR_BOOL_TYPE_EXPECTED]
exemplos/erros/tipos.ssl:11:9: tipo invalido para o operador [ERR_INVALID_TYPE]
exemplos/erros/tipos.ssl:12:9: campo 'z' nao declarado na struct [ERR_FIELD_NOT_DECL]
exemplos/erros/tipos.ssl:13:9: acesso a campo de algo que nao e struct [ERR_KIND_NOT_STRUCT]
exemplos/erros/tipos.ssl:14:9: indexacao de algo que nao e array nem string [ERR_KIND_NOT_ARRAY]
exemplos/erros/tipos.ssl:15:9: indice deve ser inteiro [ERR_INVALID_INDEX_TYPE]
exemplos/erros/tipos.ssl:16:5: 'f' nao e variavel nem parametro [ERR_KIND_NOT_VAR]
exemplos/erros/tipos.ssl:17:10: 'p' nao e funcao [ERR_KIND_NOT_FUNC]
exemplos/erros/tipos.ssl:18:17: argumentos demais na chamada [ERR_TOO_MANY_ARGS]
exemplos/erros/tipos.ssl:19:11: argumento 1 de tipo incompativel com o parametro [ERR_PARAM_TYPE]
exemplos/erros/tipos.ssl:19:14: argumento 2 de tipo incompativel com o parametro [ERR_PARAM_TYPE]
exemplos/erros/tipos.ssl:20:9: argumentos de menos na chamada [ERR_TOO_FEW_ARGS]
exemplos/erros/tipos.ssl:21:5: 'break' fora de comando de repeticao [ERR_NOT_IN_LOOP]
exemplos/erros/tipos.ssl:22:12: tipo do valor retornado incompativel com o da funcao [ERR_RETURN_TYPE]
exemplos/erros/tipos.ssl:27:12: 'f' nao e um tipo [ERR_TYPE_EXPECTED]
```

Na linha 28 de `tipos.ssl`, `x = 1` não gera erro: `x` recebeu o tipo universal
na linha 27.

---

## Revisão final: regras × ações semânticas

* **Todas as 84 regras** têm tratamento em `Semantics()`. As que só repassam
  estrutura (`P`, `LDE`, `DE`, `B`, `LDV`, `LS`) caem no `default`, que empilha
  o nó do lado esquerdo conforme a convenção do cap. 4. A regra 0 (`S' → P '$'`)
  nunca é reduzida, porque a análise termina no estado `FINAL`.
* **Cap. 5:** fig. 5.3 a 5.10 implementadas (5.10 via `NF`).
* **Cap. 6:** fig. 6.1 a 6.23 implementadas. 6.3 com o universal corrigido;
  6.16/6.17 substituídas pela equivalência por nome com alias resolvidos;
  6.6–6.13 com vetores no lugar de `pNext`.
* **Cap. 7:** não implementado (fora do escopo). As inconsistências estão
  registradas nos itens 5, 6, 10, 11, 12 e 13 da seção 7.
