SCALAC=scalac
LIBPATH=/home/neverlord/akka-2.0.3/lib/akka

CLASSPATH=$(shell for i in $(LIBPATH)/*.jar ; do printf %s $$i: ; done)
#CLASSPATH=$(LIBPATH)/akka-kernel-2.0.3.jar:$(LIBPATH)/akka-actor-2.0.3.jar
FLAGS=-cp $(CLASSPATH)

#FILES=ActorCreation.scala Distributed.scala MailboxPerformance.scala MixedCase.scala Matching.scala
#CLASS_FILES=$(FILES:.scala=.class)

FILES=ActorCreation.scala Distributed.scala MailboxPerformance.scala Matching.scala MixedCase.scala
CLASS_FILES=$(foreach FILE,$(FILES),org/libcppa/$(shell echo $(FILE:.scala=) | sed 's/\(.\)\([A-Z]\)/\1_\2/g' | tr [:upper:] [:lower:])/$(FILE:.scala=.class))

UTILITY=org/libcppa/utility/Utility.class

all: $(CLASS_FILES)

org/libcppa/utility/Utility.class: Utility.scala
	$(SCALAC) $(FLAGS) Utility.scala

#$(CLASS_FILES): $(FILES) org/libcppa/utility/Utility.class

org/libcppa/actor_creation/ActorCreation.class: ActorCreation.scala $(UTILITY)
	$(SCALAC) $(FLAGS) $<

org/libcppa/distributed/Distributed.class: Distributed.scala $(UTILITY)
	$(SCALAC) $(FLAGS) $<

org/libcppa/mailbox_performance/MailboxPerformance.class: MailboxPerformance.scala $(UTILITY)
	$(SCALAC) $(FLAGS) $<

org/libcppa/matching/Matching.class: Matching.scala $(UTILITY)
	$(SCALAC) $(FLAGS) $<

org/libcppa/mixed_case/MixedCase.class: MixedCase.scala $(UTILITY)
	$(SCALAC) $(FLAGS) $<

clean:
	rm -rf org/

.PHONY: all clean
