-module(actor_creation).
-export([testee/1]).

testee(N) ->
    if N > 0 ->
        spawn(actor_creation, testee, [N-1]),
        spawn(actor_creation, testee, [N-1])
    end.

