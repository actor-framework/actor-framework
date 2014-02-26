PDFLATEX="pdflatex"

all:
	@TEXINPUTS="..:" $(PDFLATEX) manual.tex
	@TEXINPUTS="..:" $(PDFLATEX) manual.tex
	@TEXINPUTS="..:" $(PDFLATEX) manual.tex

clean:
	rm -f *.aux *.toc *.pdf *.log *.out

.PHONY: all clean
