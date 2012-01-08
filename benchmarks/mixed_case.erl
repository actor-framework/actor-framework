-module(mixed_case).
-export([start/1, master/1, worker/1, chain_link/1]).

%next_factor(N, M) when N rem M =:= 0 -> M;
%next_factor(N, M) -> next_factor(N, M+1).

%factorize(1, _) -> [];
%factorize(N, M) ->
%    X = next_factor(N,M),
%    [ X | factorize(N div X, X) ].

%factorize(1) -> [];
%factorize(N) -> factorize(N,2).

factorize(N,M,R) when N =:= M -> R ++ [M];
factorize(N,M,R) when (N rem M) =:= 0 -> factorize(N div M, M, R ++ [M]);
factorize(N,2,R) -> factorize(N,3,R);
factorize(N,M,R) -> factorize(N,M+2,R).

factorize(1) -> [1];
factorize(2) -> [2];
factorize(N) -> factorize(N,2,[]).

worker(Supervisor) ->
    receive
        {calc, Val} ->
            Supervisor ! {result, factorize(Val)},
            worker(Supervisor)
    end.

chain_link(Next) ->
    receive
        {token, 0} ->
            Next ! {token, 0},
            true;
        {token, Val} ->
            Next ! {token, Val},
            chain_link(Next)
    end.

cr_ring(Next, InitialTokenValue, 0) ->
    Next ! {token, InitialTokenValue},
    Next;
cr_ring(Next, InitialTokenValue, RingSize) ->
    cr_ring(spawn(mixed_case, chain_link, [Next]), InitialTokenValue, RingSize-1).

master_recv_tokens(Next) ->
    receive
        {token, 0} -> true;
        {token, Val} ->
            Next ! {token, Val-1},
            master_recv_tokens(Next)
    end.

master_bhvr(Supervisor, _, _, _, 0) ->
    Supervisor ! master_done;
master_bhvr(Supervisor, Worker, RingSize, InitialTokenValue, Repetitions) ->
    Worker ! {calc, 28350160440309881},
    master_recv_tokens(cr_ring(self(), InitialTokenValue, RingSize-1)),
    master_bhvr(Supervisor, Worker, RingSize, InitialTokenValue, Repetitions-1).

master(Supervisor) ->
    receive
        {init, RingSize, InitialTokenValue, Repetitions} ->
            master_bhvr(Supervisor, spawn(mixed_case, worker, [Supervisor]), RingSize, InitialTokenValue, Repetitions)
    end.

check_result(List) ->
    if
        List == [329545133,86028157] -> true;
        List == [86028157,329545133] -> true;
        true -> error("wrong factorization result")
    end.

wait4all(0) -> true;
wait4all(Remaining) ->
    receive
        master_done -> wait4all(Remaining-1);
        {result, List} ->
            check_result(List),
            wait4all(Remaining-1)
    end.

cr_masters(_, _, _, 0) ->
    true;
cr_masters(RingSize, InitialTokenValue, Repetitions, NumRings) ->
    spawn(mixed_case, master, [self()]) ! {init, RingSize, InitialTokenValue, Repetitions},
    cr_masters(RingSize, InitialTokenValue, Repetitions, NumRings-1).

start(X) ->
    [H0|T0] = X,
    NumRings = list_to_integer(atom_to_list(H0)),
    [H1|T1] = T0,
    RingSize = list_to_integer(atom_to_list(H1)),
    [H2|T2] = T1,
    InitialTokenValue = list_to_integer(atom_to_list(H2)),
    [H3|_] = T2,
    Repetitions = list_to_integer(atom_to_list(H3)),
    cr_masters(RingSize, InitialTokenValue, Repetitions, NumRings),
    % each master sends one master_done message and one
    % factorization is calculated per repetition
    wait4all(NumRings+(NumRings*Repetitions)).
