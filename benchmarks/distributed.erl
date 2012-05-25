-module(distributed).
-export([start/1, ping_loop/2]).

ping_loop(Parent, Pong) ->
    receive
        {pong, 0} -> Parent ! done;
        {pong, X} ->
            Pong ! {ping, self(), X - 1},
            ping_loop(Parent, Pong);
        {kickoff, X} ->
            Pong ! {ping, self(), X},
            ping_loop(Parent, Pong);
        _ -> ping_loop(Parent, Pong)
    end.

server_loop(Pongs) ->
    receive
        {ping, Pid, X} ->
             Pid ! {pong, X},
             server_loop(Pongs);
        {add_pong, Pid, Node} ->
            case lists:any(fun({N, _}) -> N == Node end, Pongs) of
                true ->
                    Pid ! {ok, cached},
                    server_loop(Pongs);
                false ->
                    case rpc:call(Node, erlang, whereis, [pong]) of
                        {badrpc, Reason} ->
                            Pid ! {error, Reason},
                            server_loop(Pongs);
                        undefined ->
                            Pid ! {error, 'pong is undefined'},
                            server_loop(Pongs);
                        Pong ->
                            Pid ! {ok, added},
                            server_loop(Pongs ++ [{Node, Pong}])
                     end
            end;
        {purge} -> server_loop([]);
        {get_pongs, Pid} ->
            Pid ! Pongs,
            server_loop(Pongs);
        {kickoff, Pid, NumPings} ->
            lists:foreach(fun({_, P}) -> spawn(distributed, ping_loop, [Pid, P]) ! {kickoff, NumPings} end, Pongs),
            server_loop(Pongs);
        _ -> server_loop(Pongs)
    end.

server_mode() ->
    register(pong, self()),
    server_loop([]).

add_pong_fun(_, _, []) -> true;
add_pong_fun(Pong, Node, [Node|T]) -> add_pong_fun(Pong, Node, T);
add_pong_fun(Pong, Node, [H|T]) ->
    Pong ! {add_pong, self(), H},
    receive
        {ok, _} -> add_pong_fun(Pong, Node, T);
        {error, Reason} -> error(Reason)
        after 10000 -> error(timeout)
    end.

client_mode_receive_done_msgs(0) -> true;
client_mode_receive_done_msgs(Left) ->
    receive done -> client_mode_receive_done_msgs(Left - 1) end.


% receive a {done} message for each node
client_mode([], [], [], _) -> error("no node, no fun");
client_mode([], [], Nodes, _) ->
    client_mode_receive_done_msgs(length(Nodes) * (length(Nodes) - 1));

% send kickoff messages
client_mode([Pong|Pongs], [], Nodes, NumPings) ->
    Pong ! {kickoff, self(), NumPings},
    client_mode(Pongs, [], Nodes, NumPings);

client_mode(Pongs, [H|T], Nodes, NumPings) ->
    case rpc:call(H, erlang, whereis, [pong]) of
        {badrpc, Reason} ->
            io:format("cannot connect to ~s~n", [atom_to_list(H)]),
            error(Reason);
        undefined ->
            io:format("no 'pong' defined on node ~s~n", [atom_to_list(H)]);
        P ->
            add_pong_fun(P, H, Nodes),
            client_mode(Pongs ++ [P], T, Nodes, NumPings)
    end.

run(_, undefined, []) -> error("NumPings is undefined");
run(Hosts, _, []) when length(Hosts) < 2 -> error("less than two nodes specified");
run(Hosts, NumPings, []) -> client_mode([], Hosts, Hosts, NumPings);
run(Hosts, NumPings, [H|T]) ->
    Arg = atom_to_list(H),
    case lists:prefix("num_pings=", Arg) of
        true when NumPings /= undefined -> error("NumPings already set");
        true -> run(Hosts, list_to_integer(lists:sublist(Arg, 11, length(Arg))), T);
        false -> run(Hosts ++ [H], NumPings, T)
    end.

start(X) ->
    case X of
        ['mode=server'|[]] -> server_mode();
        ['mode=server'|_] -> io:format("too much arguments~n", []);
        ['mode=benchmark'|T] -> run([], undefined, T);
        _ -> io:format("invalid arguments~n", [])
    end.
