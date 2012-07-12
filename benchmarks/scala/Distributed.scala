import scala.actors._
import scala.actors.Actor.self
import scala.actors.remote.RemoteActor
import scala.actors.remote.RemoteActor.{alive, select, register}
import scala.actors.remote.Node
import scala.actors.TIMEOUT

import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }
import com.typesafe.config.ConfigFactory

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

case class AddPongTimeout(path: String, token: String)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

trait ServerActorPrototype[T] {

    protected def reply(what: Any): Unit
    protected def kickOff(old: T, value: Int): Unit
    protected def connectionEstablished(peers: T, pending: Any): T
    protected def newPending(peers: T, path: String, token: String) : T
    protected def handleTimeout(peers: T): (Boolean, T) = throw new RuntimeException("unsupported timeout")
    protected def handleAddPongTimeout(peers: T, path: String, token: String): (Boolean, T) = throw new RuntimeException("unsupported timeout")

    def recvFun(peers: T { def connected: List[{ def path: String }]; def pending: List[{ def clientToken: String }] }): PartialFunction[Any, (Boolean, T)] = {
        case Ping(value) => reply(Pong(value)); (false, peers)
        case Hello(token) => reply(Olleh(token)); (false, peers)
        case Olleh(token) => peers.pending find (_.clientToken == token) match {
            case Some(x) => (true, connectionEstablished(peers, x))
            case None => (false, peers)
        }
        case AddPong(path, token) => {
            //println("received AddPong(" + path + ", " + token + ")")
            if (peers.connected exists (_.path == path)) {
                reply(Ok(token))
                //println("recv[" + peers + "]: " + path + " cached (replied 'Ok')")
                (false, peers)
            }
            else {
                try { (true, newPending(peers, path, token)) }
                catch {
                    // catches match error and integer conversion failure
                    case e => reply(Error(e.toString, token)); (false, peers)
                }
            }
        }
        case KickOff(value) => kickOff(peers, value); (false, peers)
        case AddPongTimeout(path, token) => handleAddPongTimeout(peers, path, token)
        case TIMEOUT => handleTimeout(peers)
    }
}

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

class ServerActor(port: Int) extends Actor with ServerActorPrototype[Peers] {

    //def reply(what: Any): Unit = sender ! what // inherited from ReplyReactor

    protected override def kickOff(peers: Peers, value: Int) = {
        (new PingActor(sender, peers.connected map (_.channel))).start ! KickOff(value)
    }

    protected override def connectionEstablished(peers: Peers, x: Any) = x match {
        case PendingPeer(path, channel, client, token) => {
            client ! Ok(token)
            Peers(Peer(path, channel) :: peers.connected, peers.pending filterNot (_.clientToken == token))
        }
    }

    protected def newPending(peers: Peers, path: String, token: String) : Peers = path split ":" match {
        case Array(node, port) => {
            val channel = select(new Node(node, port.toInt), 'Pong)
            channel ! Hello(token)
            //println("recv[" + peers + "]: sent 'Hello' to " + path)
            Peers(peers.connected, PendingPeer(path, channel, sender, token) :: peers.pending)
        }
    }

    protected override def handleTimeout(peers: Peers) = {
        peers.pending foreach (x => x.client ! Error("cannot connect to " + x.path, x.clientToken))
        (true, Peers(peers.connected, Nil))
    }

    override def act() {
        RemoteActor classLoader = getClass().getClassLoader
        alive(port)
        register('Pong, self)
        @tailrec def recvLoop(peers: Peers): Nothing = {
            def recv(peers: Peers, receiveFun: PartialFunction[Any, (Boolean, Peers)] => (Boolean, Peers)): Peers = receiveFun(recvFun(peers))._2
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

case class SetParent(parent: AkkaActorRef)

class AkkaPingActor(pongs: List[AkkaActorRef]) extends AkkaActor {

    import context.become

    private var parent: AkkaActorRef = null
    private var left = pongs.length

    private def recvLoop: Receive = {
        case Pong(0) => {
            parent ! Done
            //println(parent.toString + " ! Done")
            if (left > 1) left -= 1
            else context.stop(self)
        }
        case Pong(value) => sender ! Ping(value - 1)
    }

    def receive = {
        case SetParent(p) => parent = p
        case KickOff(value) => pongs.foreach(_ ! Ping(value)); become(recvLoop)
    }
}

case class AkkaPeer(path: String, channel: AkkaActorRef)
case class PendingAkkaPeer(path: String, channel: AkkaActorRef, client: AkkaActorRef, clientToken: String)
case class AkkaPeers(connected: List[AkkaPeer], pending: List[PendingAkkaPeer])

class AkkaServerActor(system: ActorSystem) extends AkkaActor with ServerActorPrototype[AkkaPeers] {

    import context.become

    protected  def reply(what: Any): Unit = sender ! what

    protected def kickOff(peers: AkkaPeers, value: Int): Unit = {
        val ping = context.actorOf(Props(new AkkaPingActor(peers.connected map (_.channel))))
        ping ! SetParent(sender)
        ping ! KickOff(value)
        //println("[" + peers + "]: KickOff(" + value + ")")
    }

    protected def connectionEstablished(peers: AkkaPeers, x: Any): AkkaPeers = x match {
        case PendingAkkaPeer(path, channel, client, token) => {
            client ! Ok(token)
            //println("connected to " + path)
            AkkaPeers(AkkaPeer(path, channel) :: peers.connected, peers.pending filterNot (_.clientToken == token))
        }
    }

    protected def newPending(peers: AkkaPeers, path: String, token: String) : AkkaPeers = {
        val channel = system.actorFor(path)
        channel ! Hello(token)
        import akka.util.duration._
        system.scheduler.scheduleOnce(5 seconds, self, AddPongTimeout(path, token))
        //println("[" + peers + "]: sent 'Hello' to " + path)
        AkkaPeers(peers.connected, PendingAkkaPeer(path, channel, sender, token) :: peers.pending)
    }

    protected override def handleAddPongTimeout(peers: AkkaPeers, path: String, token: String) = {
        peers.pending find (x => x.path == path && x.clientToken == token) match {
            case Some(PendingAkkaPeer(_, channel, client, _)) => {
                client ! Error(path + " did not respond", token)
                //println(path + " did not respond")
                (true, AkkaPeers(peers.connected, peers.pending filterNot (x => x.path == path && x.clientToken == token)))
            }
            case None => (false, peers)
        }
    }

    def bhvr(peers: AkkaPeers): Receive = {
        case x => {
            recvFun(peers)(x) match {
                case (true, newPeers) => become(bhvr(newPeers))
                case _ => Unit
            }
        }
    }

    def receive = bhvr(AkkaPeers(Nil, Nil))

}

case class TokenTimeout(token: String)
case class RunAkkaClient(paths: List[String], numPings: Int)

class AkkaClientActor(system: ActorSystem) extends AkkaActor {

    import context.become

    def collectDoneMessages(left: Int): Receive = {
        case Done => {
//println("Done")
            if (left == 1) {
                global.latch.countDown
                context.stop(self)
            } else {
                become(collectDoneMessages(left - 1))
            }
        }
        case _ => {
            // ignore any other message
        }
    }

    def collectOkMessages(pongs: List[AkkaActorRef], left: Int, receivedTokens: List[String], numPings: Int): Receive = {
        case Ok(token) => {
//println("Ok")
            if (left == 1) {
                //println("collected all Ok messages (wait for Done messages)")
                pongs foreach (_ ! KickOff(numPings))
                become(collectDoneMessages(pongs.length * (pongs.length - 1)))
            }
            else {
                become(collectOkMessages(pongs, left - 1, token :: receivedTokens, numPings))
            }
        }
        case TokenTimeout(token) => {
            if (!receivedTokens.contains(token)) {
                println("Error: " + token + " did not reply within 10 seconds")
                global.latch.countDown
                context.stop(self)
            }
        }
        case Error(what, token) => {
            println("Error [from " + token+ "]: " + what)
            global.latch.countDown
            context.stop(self)
        }
    }

    def receive = {
        case RunAkkaClient(paths, numPings) => {
//println("RunAkkaClient(" + paths.toString + ", " + numPings + ")")
            import akka.util.duration._
            val pongs = paths map (x => {
                val pong = system.actorFor(x)
                paths foreach (y => if (x != y) {
                    val token = x + " -> " + y
                    pong ! AddPong(y, token)
//println(x + " ! AddPong(" + y + ", " + token + ")")
                    system.scheduler.scheduleOnce(10 seconds, self, TokenTimeout(token))
                })
                pong
            })
            become(collectOkMessages(pongs, pongs.length * (pongs.length - 1), Nil, numPings))
        }
    }
}

object DistributedClientApp {

    val NumPings = "num_pings=([0-9]+)".r
    val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

    @tailrec def run(args: List[String], paths: List[String], numPings: Option[Int], finalizer: (List[String], Int) => Unit): Unit = args match {
        case NumPings(num) :: tail => numPings match {
            case Some(x) => throw new IllegalArgumentException("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
            case None => run(tail, paths, Some(num.toInt), finalizer)
        }
        case arg :: tail => run(tail, arg :: paths, numPings, finalizer)
        case Nil => numPings match {
            case Some(x) => {
                if (paths.length < 2) throw new RuntimeException("at least two hosts required")
                finalizer(paths, x)
            }
            case None => throw new RuntimeException("no \"num_pings\" found")
        }
    }

    def main(args: Array[String]): Unit = {
        try {
            args(0) match {
                case "remote_actors" => run(args.toList.drop(1), Nil, None, ((paths, x) => {
                    (new ClientActor(paths map (path => path match { case SimpleUri(host, port) => RemoteActorPath(path, host, port.toInt) }), x)).start
                }))
                case "akka" => run(args.toList.drop(1), Nil, None, ((paths, x) => {
                    val system = ActorSystem("benchmark", ConfigFactory.load.getConfig("benchmark"))
                    system.actorOf(Props(new AkkaClientActor(system))) ! RunAkkaClient(paths, x)
                    global.latch.await
                    system.shutdown
                    System.exit(0)
                }))
            }
        }
        catch {
            case e => {
                println("usage: DistributedClientApp (remote_actors|akka) {nodes...} num_pings={num}\nexample: DistributedClientApp remote_actors localhost:1234 localhost:2468 num_pings=1000\n")
                throw e
            }
        }
    }
}

object DistributedServerApp {

    val IntStr = "([0-9]+)".r

    def main(args: Array[String]): Unit = args match {
        case Array("remote_actors", IntStr(istr)) => (new ServerActor(istr.toInt)).start
        case Array("akka") => {
            val system = ActorSystem("pongServer", ConfigFactory.load.getConfig("pongServer"))
            val pong = system.actorOf(Props(new AkkaServerActor(system)), "pong")
            Unit
        }
        case _ => println("usage: DistributedServerApp remote_actors PORT\n" +
                     "   or: DistributedServerApp akka")
    }
}
