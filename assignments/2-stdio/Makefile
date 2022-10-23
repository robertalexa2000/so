all: libso_stdio

libso_stdio: libso_stdio.o
	gcc -Wall -shared libso_stdio.o -o libso_stdio.so

libso_stdio.o: libso_stdio.c
	gcc -Wall -Iutil/ -fPIC -c libso_stdio.c

clean:
	rm libso_stdio.o libso_stdio.so
