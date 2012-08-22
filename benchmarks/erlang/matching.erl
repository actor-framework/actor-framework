-module(matching).
-export([start/1]).

partFun(msg1, X) when X =:= 0 -> true;
partFun(msg2, 0.0) -> true;
partFun(msg3, [0|[]]) -> true.

partFun(msg4, 0, "0") -> true.

partFun(msg5, A, B, C) when A =:= 0; B =:= 0; C =:= 0 -> true;
partFun(msg6, A, 0.0, "0") when A =:= 0 -> true.

loop(0) -> true;
loop(N) ->
    partFun(msg1, 0),
    partFun(msg2, 0.0),
    partFun(msg3, [0]),
    partFun(msg4, 0, "0"),
    partFun(msg5, 0, 0, 0),
    partFun(msg6, 0, 0.0, "0"),
    loop(N-1).

start(X) ->
    [H|[]] = X,
    N = list_to_integer(atom_to_list(H)),
    loop(N).
