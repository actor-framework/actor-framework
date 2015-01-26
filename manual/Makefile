DIRS := build-html build-pdf
SOURCES = ActorManagement.tex Actors.tex Appendix.tex CommonPitfalls.tex FirstSteps.tex GroupCommunication.tex MessagePriorities.tex NetworkTransparency.tex PatternMatching.tex ReceivingMessages.tex SendingMessages.tex StronglyTypedActors.tex SpawningActors.tex SynchronousMessages.tex TypeSystem.tex manual.tex

all: pdf html

pdf: manual.pdf

html: manual.html

clean:
	@for i in $(DIRS); do $(MAKE) -C $$i $@; done
	rm -f manual.pdf
	rm -f manual.html

manual.pdf: $(SOURCES)
	$(MAKE) -C build-pdf && cp build-pdf/manual.pdf manual.pdf

manual.html: $(SOURCES)
	$(MAKE) -C build-html && cp build-html/manual.html manual.html


.PHONY: all clean
