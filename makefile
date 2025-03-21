#                    GROUP - 8
# 2020B1A70630P                       Aditya Thakur
# 2021A7PS2001P                       Amal Sayeed
# 2021A7PS2005P                       Ohiduz Zaman
# 2021A7PS2682P                       Priyansh Patel
# 2021A7PS2002P                       Rachoita Das
# 2020B1A70611P                       Subhramit Basu Bhowmick

var = gcc -c #change to "clang" if you're using Clang to compile
all: lexer.c parser.c driver.c
	make clean
	mkdir -p build
	
	$(var) lexer.c -lm -o build/lexer.o
	$(var) parser.c -o build/parser.o
	$(var) driver.c -o build/driver.o

	gcc build/*.o -lm -o stage1exe
	
clean:
	rm -f build/*.o
	rm -f stage1exe