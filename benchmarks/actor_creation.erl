-module(actor_creation).
-export([start/1, testee/1]).

testee(Pid) ->
    receive
        {spread, 0} ->
            Pid ! {result, 1};
        {spread, X} ->
            spawn(actor_creation, testee, [self()]) ! {spread, X-1},
            spawn(actor_creation, testee, [self()]) ! {spread, X-1},
            receive
                {result, X1} ->
                    receive
                        {result, X2} ->
                            Pid ! {result, (X1+X2)}
                    end
            end
    end.

start(X) ->
    [H|_] = X,
    N = list_to_integer(atom_to_list(H)),
    spawn(actor_creation, testee, [self()]) ! {spread, N},
    receive
        {result, X} ->
            if
                X == (2 bsl N) ->
                    X;
                true ->
                    error("unexpted result!")
            end
    end.

