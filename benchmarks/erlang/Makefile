FILES=actor_creation.erl  distributed.erl  mailbox_performance.erl  matching.erl  mixed_case.erl
BEAMS=$(FILES:.erl=.beam)
ERLC=erlc

%.beam: %.erl
	$(ERLC) $<

all: $(BEAMS)

clean:
	rm -f $(BEAMS)

.PHONY: all clean
