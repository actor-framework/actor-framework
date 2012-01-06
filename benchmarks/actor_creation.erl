-module(actor_creation).
-export([start/1, testee/2]).

testee(Pid, 0) ->
    Pid ! {result, 1};

testee(Pid, N) ->
    spawn(actor_creation, testee, [self(),N-1]),
    spawn(actor_creation, testee, [self(),N-1]),
    receive
        {result, X1} ->
            receive
                {result, X2} ->
                    Pid ! {result, (X1+X2)}
            end
    end.

start(X) ->
    [H|_] = X,
    N = list_to_integer(atom_to_list(H)),
    spawn(actor_creation, testee, [self(),N]),
    spawn(actor_creation, testee, [self(),N]),
    receive
        {result, X1} ->
            receive
                {result, X2} ->
                    if
                        (X1+X2) == (2 bsl N) ->
                            X1+X2;
                        true ->
                            error("unexpted result!")
                    end
            end
    end.

