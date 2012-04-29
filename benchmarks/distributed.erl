-module(distributed).
-export([start/1, ping_loop/2]).

ping_loop(Parent, Pong) ->
    receive
        {pong, 0} -> Parent ! done;
        {pong, X} ->
            Pong ! {self(), ping, X - 1},
            ping_loop(Parent, Pong);
        {kickoff, Value} ->
            Pong ! {ping, self(), Value},
            ping_loop(Parent, Pong)
    end.

server_loop(Pongs) ->
    receive
        {ping, Pid, Value} -> Pid ! {pong, Value};
        {add_pong, Pid, Node} ->
            case lists:any(fun({N, _}) -> N == Node end, Pongs) of
                true ->
                    Pid ! {ok},
                    server_loop(Pongs);
                false ->
                    case rpc:call(Node, erlang, whereis, [pong]) of
                        {badrpc, Reason} -> Pid ! {error, Reason};
                        Pong ->
                            Pid ! {ok},
                            server_loop(Pongs ++ [{Node, Pong}])
                     end
            end;
        {purge} -> server_loop([]);
        {kickoff, Pid, NumPings} ->
            lists:foreach(fun({_, P}) -> spawn(distributed, ping_loop, [Pid, P]) ! {kickoff, NumPings} end, Pongs),
            server_loop(Pongs)
    end.

server_mode() ->
    register(pong, self()),
    server_loop([]).

add_pong_fun(_, _, []) -> true;
add_pong_fun(Pong, Node, [Node|T]) -> add_pong_fun(Pong, Node, T);
add_pong_fun(Pong, Node, [H|T]) ->
    Pong ! {add_pong, H},
    receive
        {ok} -> add_pong_fun(Pong, Node, T);
        {error, Reason} -> error(Reason)
    end.

% receive a {done} message for each node
client_mode([], [], [], _) -> true;
client_mode([], [], [_|T], NumPings) ->
    receive {done} ->

        client_mode([], [], T, NumPings)
    end;

% send kickoff messages
client_mode([Pong|Pongs], [], Nodes, NumPings) ->
    Pong ! {kickoff, self(), NumPings},
    client_mode(Pongs, [], Nodes, NumPings);

client_mode(Pongs, [H|T], Nodes, NumPings) ->
    case rpc:call(H, erlang, whereis, [pong]) of
        {badrpc, Reason} ->
            io:format("cannot connect to ~s~n", [atom_to_list(H)]),
            error(Reason);
        P ->
            add_pong_fun(P, H, Nodes),
            client_mode(Pongs ++ [P], T, Nodes, NumPings)
    end.

run([], undefined, ['mode=server']) -> server_mode();

run(_, undefined, []) -> error("NumPings is undefined");

run(Hosts, _, []) when length(Hosts) < 2 -> error("less than two nodes specified");
run(Hosts, NumPings, []) -> client_mode([], Hosts, Hosts, NumPings);

run(Hosts, NumPings, [H|T]) ->
    Arg = atom_to_list(H),
    SetNumPings = lists:prefix("num_pings=", Arg),
    if
        SetNumPings == true andalso NumPings /= undefined ->
            error("NumPings already set");
        SetNumPings == true ->
            run(Hosts, list_to_integer(lists:sublist(Arg, 11, length(Arg))), T);
        true ->
            run(Hosts ++ [H], NumPings, T)
    end.

start(X) ->
    run([], undefined, X).
