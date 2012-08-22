-module(mailbox_performance).
-export([start/1,sender/2]).

sender(_, 0) ->
    0;
sender(Pid, Num) ->
    Pid ! {msg},
    sender(Pid, Num-1).

start_senders(_, _, 0) ->
    0;
start_senders(Pid, Arg, Num) ->
    spawn(mailbox_performance, sender, [Pid, Arg]),
    start_senders(Pid, Arg, Num-1).

receive_messages(0) ->
    0;
receive_messages(Num) ->
    receive
        {msg} -> receive_messages(Num-1)
    end.

start(Args) ->
    [H1|T] = Args,
    NumSender = list_to_integer(atom_to_list(H1)),
    [H2|_] = T,
    NumMsgs = list_to_integer(atom_to_list(H2)),
    start_senders(self(), NumMsgs, NumSender),
    receive_messages(NumMsgs * NumSender).

