all:
	make -f libcppa.Makefile
	make -C unit_testing

clean:
	make -f libcppa.Makefile clean
	make -C unit_testing clean
