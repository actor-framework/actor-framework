import scala.actors._
import scala.actors.Actor.self
import scala.actors.remote.RemoteActor
import scala.actors.remote.RemoteActor.{alive, select, register}
import scala.actors.remote.Node
import scala.actors.TIMEOUT

import scala.annotation.tailrec

import Console.println

case class Ping(value: Int)
case class Pong(value: Int)
case class KickOff(value: Int)

case object Done
case class Ok(token: String)
case class Hello(token: String)
case class Olleh(token: String)
case class Error(msg: String, token: String)
case class AddPong(path: String, token: String)

case class RemoteActorPath(uri: String, host: String, port: Int)

class PingActor(parent: OutputChannel[Any], pongs: List[OutputChannel[Any]]) extends Actor {

    private var left = pongs.length

    private def recvLoop: Nothing = react {
        case Pong(0) => {
            parent ! Done
            if (left > 1) {
                left -= 1
                recvLoop
            }
        }
        case Pong(value) => sender ! Ping(value - 1); recvLoop
    }

    override def act() = react {
        case KickOff(value) => pongs.foreach(_ ! Ping(value)); recvLoop
    }
}

case class Peer(path: String, channel: OutputChannel[Any])
case class PendingPeer(path: String, channel: OutputChannel[Any], client: OutputChannel[Any], clientToken: String)
case class Peers(connected: List[Peer], pending: List[PendingPeer])

class ServerActor(port: Int) extends Actor {

    override def act() {
        RemoteActor classLoader = getClass().getClassLoader
        alive(port)
        register('Pong, self)
        @tailrec def recvLoop(peers: Peers): Nothing = {
            def recv(peers: Peers, receiveFun: PartialFunction[Any, Peers] => Peers): Peers = receiveFun {
                case Ping(value) => sender ! Pong(value); peers
                case Hello(token) => sender ! Olleh(token); peers
                case Olleh(token) => {
                    peers.pending find (_.clientToken == token) match {
                        case Some(PendingPeer(path, channel, client, _)) => {
                            client ! Ok(token)
                            //println("recv[" + peers + "]: received Olleh from " + path)
                            Peers(Peer(path, channel) :: peers.connected, peers.pending filterNot (_.clientToken == token))
                        }
                        case None => peers
                    }
                }
                case AddPong(path, token) => {
                    //println("received AddPong(" + path + ", " + token + ")")
                    if (peers.connected exists (_.path == path)) {
                        sender ! Ok(token)
                        //println("recv[" + peers + "]: " + path + " cached (replied 'Ok')")
                        peers
                    }
                    else {
                        try path split ":" match {
                            case Array(node, port) => {
                                val channel = select(new Node(node, port.toInt), 'Pong)
                                channel ! Hello(token)
                                //println("recv[" + peers + "]: sent 'Hello' to " + path)
                                Peers(peers.connected, PendingPeer(path, channel, sender, token) :: peers.pending)
                            }
                        }
                        catch {
                            // catches match error and integer conversion failure
                            case e => sender ! Error(e.toString, token); peers
                        }
                    }
                }
                case KickOff(value) => {
                    (new PingActor(sender, peers.connected map (_.channel))).start ! KickOff(value)
                    //println("recv[" + peers + "]: KickOff(" + value + ")")
                    peers
                }
                case TIMEOUT => {
                    peers.pending foreach (x => x.client ! Error("cannot connect to " + x.path, x.clientToken))
                    Peers(peers.connected, Nil)
                }
            }
            recvLoop(recv(peers, if (peers.pending isEmpty) receive else receiveWithin(5000)))
        }
        recvLoop(Peers(Nil, Nil))
    }

}

class ClientActor(pongPaths: List[RemoteActorPath], numPings: Int) extends Actor {
    override def act() = {
        RemoteActor classLoader = getClass().getClassLoader
        val pongs = pongPaths map (x => {
            val pong = select(new Node(x.host, x.port), 'Pong)
            pongPaths foreach (y => if (x != y) pong ! AddPong(y.uri, x.uri + " -> " + y.uri))
            pong
        })
        @tailrec def collectOkMessages(left: Int, receivedTokens: List[String]): Unit = {
            if (left > 0)
                collectOkMessages(left - 1, receiveWithin(10000) {
                    case Ok(token) => token :: receivedTokens
                    case Error(msg, token) => throw new RuntimeException("connection failed: " + token + ", message from server: " + msg)
                    case TIMEOUT => throw new RuntimeException("no Ok within 10sec.\nreceived tokens:\n" + receivedTokens.sortWith(_.compareTo(_) < 0).mkString("\n"))
                })
        }
        collectOkMessages(pongs.length * (pongs.length - 1), Nil)
        // kickoff
        pongs foreach (_ ! KickOff(numPings))
        // collect done messages
        for (_ <- 1 until (pongs.length * (pongs.length - 1))) {
            receiveWithin(30*60*1000) {
                case Done => Unit
                case TIMEOUT => throw new RuntimeException("no Done within 30min")
                case x => throw new RuntimeException("Unexpected message: " + x.toString)
            }
        }
    }
}

object DistributedClientApp {

    val NumPings = "num_pings=([0-9]+)".r
    val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

    @tailrec def runRemoteActors(args: List[String], pongs: List[RemoteActorPath], numPings: Option[Int]): Unit = args match {
        case NumPings(num) :: tail => numPings match {
            case Some(x) => throw new IllegalArgumentException("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
            case None => runRemoteActors(tail, pongs, Some(num.toInt))
        }
        case arg :: tail => arg match {
            case SimpleUri(host, port) => runRemoteActors(tail, RemoteActorPath(arg, host, port.toInt) :: pongs, numPings)
            case _ => throw new IllegalArgumentException("illegal argument: " + arg)
        }
        case Nil => numPings match {
            case Some(x) => {
                if (pongs.length < 2) throw new RuntimeException("at least two hosts required")
                (new ClientActor(pongs, x)).start
            }
            case None => throw new RuntimeException("no \"num_pings\" found")
        }
    }

    def main(args: Array[String]): Unit = {
        try runRemoteActors(args.toList, Nil, None)
        catch {
            case e => {
                println("usage: DistributedClientApp {nodes...} num_pings={num}\nexample: DistributedClientApp localhost:1234 localhost:2468 num_pings=1000\n")
                throw e
            }
        }
    }
}

object DistributedServerApp {

    def usage = println("usage: DistributedServerApp {port}")

    def main(args: Array[String]): Unit = {
        if (args.length > 1) {
            usage
            println("\ntoo much arguments, please give a valid TCP port only")
        }
        try {
            (new ServerActor(args(0).toInt)).start
        }
        catch {
            case e => usage; throw e
        }
    }
}
