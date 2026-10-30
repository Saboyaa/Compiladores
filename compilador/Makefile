CC       = gcc
CXX      = g++
CFLAGS   = -Wall -Wextra -std=c99 -pedantic -O2 -D_POSIX_C_SOURCE=200809L
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++11 -O2

OBJS = lexer.o gramatica.o lalr.o semantico.o sintatico.o codigo.o vm.o

all: sslc testes

sslc: main.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ main.o $(OBJS)

testes: testes.o $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ testes.o $(OBJS)

lexer.o:     lexer.c lexer.h
gramatica.o: gramatica.cpp gramatica.h lexer.h
lalr.o:      lalr.cpp lalr.h gramatica.h lexer.h
semantico.o: semantico.cpp semantico.h codigo.h gramatica.h lexer.h
codigo.o:    codigo.cpp codigo.h semantico.h lexer.h
vm.o:        vm.cpp vm.h codigo.h semantico.h lexer.h
sintatico.o: sintatico.cpp sintatico.h lalr.h semantico.h gramatica.h lexer.h
main.o:      main.cpp vm.h codigo.h sintatico.h lalr.h semantico.h gramatica.h lexer.h
testes.o:    testes.cpp vm.h codigo.h sintatico.h lalr.h semantico.h gramatica.h lexer.h

# testes da VF e, em seguida, os exemplos (validos devem compilar, os com erro nao)
test: sslc testes
	./testes
	@for f in exemplos/validos/*.ssl; do ./sslc $$f > /dev/null || { echo "FALHOU: $$f"; exit 1; }; done
	@for f in exemplos/erros/*.ssl; do ! ./sslc $$f > /dev/null || { echo "FALHOU: $$f"; exit 1; }; done
	@echo "exemplos: ok"

clean:
	rm -f *.o sslc testes

.PHONY: all test clean
