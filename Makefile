all:
	./create_libcppa_Makefile.sh > libcppa.Makefile
	make -f libcppa.Makefile
	make -C unit_testing
	make -C queue_performances

clean:
	make -f libcppa.Makefile clean
	make -C unit_testing clean
	make -C queue_performances clean
