all:
	make -f libcppa.Makefile
	make -f test.Makefile

clean:
	make -f libcppa.Makefile clean
	make -f test.Makefile clean
