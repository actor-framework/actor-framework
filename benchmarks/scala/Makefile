CC=scalac
LIBPATH=/home/neverlord/akka-2.0.1/lib/akka

CLASSPATH=$(shell for i in $(LIBPATH)/*.jar ; do printf %s $$i: ; done)
FLAGS=-cp $(CLASSPATH)

FILES=ActorCreation.scala Distributed.scala MailboxPerformance.scala MixedCase.scala Matching.scala
CLASSES=$(FILES:.scala=.class)

%.class: %.scala
	$(CC) $(FLAGS) $<

all: $(CLASSES)

clean:
	rm -f *.class

.PHONY: all clean
