import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }

import scala.actors.{Actor, AbstractActor, OutputChannel}
import scala.actors.remote.RemoteActor
import scala.actors.remote.RemoteActor._
import scala.actors.remote.Node
import scala.actors.TIMEOUT


import scala.annotation.tailrec

import com.typesafe.config.ConfigFactory
import Console.println

case object Done
case object OkTimeout
case class Error(msg: String, token: String)

case class Ping(value: Int)
case class Pong(value: Int)
case class KickOff(value: Int)

case class Ok(token: String)
case class AddPong(path: String, token: String)

case class Hello(token: String)
case class Olleh(token: String)

case class NoTokenReceived(token: String)
case class PongDidNotRespond(pong: AbstractActor)
case class AkkaPongDidNotRespond(pong: AkkaActorRef, ping: AkkaActorRef)

case class RemoteActorPath(uri: String, host: String, port: Int)

case class RunClient(pongs: List[RemoteActorPath], numPings: Int)
case class RunAkkaClient(pongs: List[AkkaActorRef], numPings: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(parent: OutputChannel[Any], pongs: List[OutputChannel[Any]]) extends Actor {

    var left = pongs.length

    private def recvLoop: Nothing = react {
        case Pong(0) => {
            parent ! Done
            if (left > 1) {
                left -= 1
                recvLoop
            }
        }
        case Pong(value) => {
            sender ! Ping(value - 1)
            recvLoop
        }
    }

    override def act() = react {
        case KickOff(value) => {
            pongs.foreach(x => x ! Ping(value))
            recvLoop
        }
    }
}

class DelayActor(msec: Long, parent: AbstractActor, msg: Any) extends Actor {
    override def act() {
        reactWithin(msec) {
            case TIMEOUT => parent ! msg
        }
    }
}

class ServerActor(port: Int) extends Actor {

    type PeerList = List[(String,OutputChannel[Any])]
    type PendingPeerList = List[(String,AbstractActor,OutputChannel[Any],String)]
    type PListPair = (PeerList, PendingPeerList)

    private def recv(peers: PListPair): PListPair = receive {
        case Ping(value) => {
            sender ! Pong(value)
            peers
        }
        case Hello(token) => {
            sender ! Olleh(token)
            peers
        }
        case Olleh(token) => {
            peers._2.find(x => x._4 == token) match {
                case Some((path, pong, ping, _)) => {
                    ping ! Ok(token)
                    //println("recv[" + peers + "]: received Olleh from " + path)
                    ((path, pong) :: peers._1, peers._2.filterNot(y => y._4 == token))
                }
                case None => {
                    peers
                }
            }
        }
        case PongDidNotRespond(pong) => {
            peers._2.find(x => x._2 == pong) match {
                case Some ((path, _, ping, token)) => {
                    //println("recv[" + peers + "]: " + path + " did not respond")
                    ping ! Error(path + " did not respond within 5 seconds", token)
                    (peers._1, peers._2.filterNot(y => y._2 == pong))
                }
                case None => {
                    peers
                }
            }
        }
        case AddPong(path, token) => {
            //println("received AddPong(" + path + ", " + token + ")")
            if (peers._1.exists(x => x._1 == path)) {
                sender ! Ok(token)
                //println("recv[" + peers + "]: " + path + " cached (replied 'Ok')")
                peers
            }
            else {
                try {
                    path.split(":") match {
                        case Array(node, port) => {
                            val pong = select(new Node(node, port.toInt), 'Pong)
                            (new DelayActor(5000, Actor.self, PongDidNotRespond(pong))).start
                            pong ! Hello(token)
                            //println("recv[" + peers + "]: sent 'Hello' to " + path)
                            (peers._1, (path, pong, sender, token) :: peers._2)
                        }
                    }
                }
                catch {
                    case e => {
                        // catches match error and integer conversion failure
                        sender ! Error(e.toString, token)
                        peers
                    }
                }
            }
        }
        case KickOff(value) => {
            (new PingActor(sender, peers._1.map(x => x._2))).start ! KickOff(value)
            //println("recv[" + peers + "]: KickOff(" + value + ")")
            peers
        }
    }

    @tailrec private def recvLoop(peers: PListPair): Nothing = {
        recvLoop(recv(peers))
    }

    override def act() {
        RemoteActor.classLoader = getClass().getClassLoader
        alive(port)
        register('Pong, Actor.self)
        recvLoop((Nil, Nil))
    }
}

class ClientActor(pongPaths: List[RemoteActorPath], numPings: Int) extends Actor {
    override def act() = {
        RemoteActor.classLoader = getClass().getClassLoader
        val pongs = pongPaths.map(x => {
            val pong = select(new Node(x.host, x.port), 'Pong)
            pongPaths.foreach(y => {
                if (x != y) {
                    val token = x.uri + " -> " + y.uri
                    pong ! AddPong(y.uri, token)
                }
            })
            pong
        })
        def receiveOkMessage(receivedTokens: List[String]): List[String] = {
            receiveWithin(10*1000) {
                case Ok(token) => {
                    token :: receivedTokens
                }
                case TIMEOUT => {
                    throw new RuntimeException("no Ok within 10sec.\nreceived tokens:\n" + receivedTokens.reduceLeft(_ + "\n" + _))
                }
            }
        }
        def collectOkMessages(left: Int, receivedTokens: List[String]): Unit = {
            if (left > 1) collectOkMessages(left - 1, receiveOkMessage(receivedTokens))
        }
        collectOkMessages(pongs.length * (pongs.length - 1), Nil)
        // collect ok messages
        var receivedTokens: List[String] = Nil
        for (_ <- 1 until (pongs.length * (pongs.length - 1))) {
            receiveWithin(10*1000) {
                case Ok(token) => {
                    receivedTokens = token :: receivedTokens
                }
                case TIMEOUT => {
                    println("no Ok within 10sec.")
                    println("received tokens: " + receivedTokens)
                    System.exit(1)
                }
            }
        }
        // kickoff
        pongs.foreach(x => x ! KickOff(numPings))
        // collect done messages
        for (_ <- 1 until (pongs.length * (pongs.length - 1))) {
            receiveWithin(30*60*1000) {
                case Done => {
                }
                case TIMEOUT => {
                    println("no Done within 30min")
                    System.exit(1)
                }
            }
        }
    }
}

class PingAkkaActor(parent: AkkaActorRef, pong: AkkaActorRef) extends AkkaActor {
    def receive = {
        case Pong(0) => {
            parent ! Done
            context.stop(self)
        }
        case Pong(value) => {
            sender ! Ping(value - 1)
        }
        case KickOff(value) => {
            pong ! Ping(value)
        }
    }
}

class ServerAkkaActor(system: ActorSystem) extends AkkaActor {

    import context.become

    def recvLoop(pongs: List[AkkaActorRef], pendingPongs: List[(AkkaActorRef, AkkaActorRef, String)]): Receive = {
        case Ping(value) => {
            sender ! Pong(value)
        }
        case Olleh(_) => {
            pendingPongs.find(x => x == sender) match {
                case Some((pong, ping, token)) if pong == sender => {
println("added actor " + pong.path)
                    ping ! Ok(token)
                    become(recvLoop(pong :: pongs, pendingPongs.filterNot(x => x._1 == sender)))
                }
                case None => {
                    // operation already timed out
                }
            }
        }
        case AkkaPongDidNotRespond(pong, ping) => {
            pendingPongs.find(x => x._1 == sender) match {
                case Some((_, _, token)) => {
                    ping ! Error(pong + " did not respond", token)
                    become(recvLoop(pongs, pendingPongs.filterNot(x => x._1 == sender)))
                }
                case None => {
                    // operation already succeeded (received Olleh)
                }
            }
        }
        case AddPong(path, token) => {
            if (pongs.exists((x) => x.path == path)) {
                sender ! Ok(token)
            }
            else {
                import akka.util.duration._
println("try to add actor " + path)
                val pong = system.actorFor(path)
                // wait at most 5sec. for response
                pong ! Hello("")
                system.scheduler.scheduleOnce(5 seconds, self, AkkaPongDidNotRespond(pong, sender))
                become(recvLoop(pongs, (pong, sender, token) :: pendingPongs))
                //pong ! Hello
                //pongs = pong :: pongs
                //sender ! Ok
            }
        }
        case KickOff(value) => {
            val client = sender
//println("KickOff(" + value + ") from " + client)
            pongs.foreach((x) => context.actorOf(Props(new PingAkkaActor(client, x))) ! KickOff(value))
        }
        case Hello(token) => {
            sender ! Olleh(token)
        }
    }

    def receive = recvLoop(Nil, Nil)

}

class ClientAkkaActor extends AkkaActor {

    import context._

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

    def collectOkMessages(pongs: List[AkkaActorRef], tokens: List[String], numPings: Int): Receive = {
        case Ok(token) => {
//println("Ok")
            if (tokens.length + 1 == pongs.length) {
                val left = pongs.length * (pongs.length - 1)
                pongs.foreach(x => x ! KickOff(numPings))
                become(collectDoneMessages(left))
            }
            else {
                become(collectOkMessages(pongs, token :: tokens, numPings))
            }
        }
        case NoTokenReceived(token) => {
            if (!tokens.contains(token)) {
                println("Error: " + token + " did not reply within 10 seconds");
            }
        }
        case Error(what, token) => {
            println("Error [from " + token+ "]: " + what)
            global.latch.countDown
            context.stop(self)
        }
    }

    def receive = {
        case RunAkkaClient(pongs, numPings) => {
            import akka.util.duration._
            pongs.foreach(x => pongs.foreach(y => if (x != y) {
                val token = x.toString
                x ! AddPong(y.path.toString, token)
                system.scheduler.scheduleOnce(10 seconds, self, NoTokenReceived(token))
            }))
            become(collectOkMessages(pongs, Nil, numPings))
        }
    }
}

object DistributedClientApp {

    val NumPings = "num_pings=([0-9]+)".r
    val SimpleUri = "([0-9a-zA-Z\\.]+):([0-9]+)".r

    def runAkka(system: ActorSystem, args: List[String], pongs: List[AkkaActorRef], numPings: Option[Int]): Unit = args match {
        case NumPings(num) :: tail => numPings match {
            case Some(x) => {
                println("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
            }
            case None => runAkka(system, tail, pongs, Some(num.toInt))
        }
        case path :: tail => {
            runAkka(system, tail, system.actorFor(path) :: pongs, numPings)
        }
        case Nil => numPings match {
            case Some(x) => {
                if (pongs isEmpty) throw new RuntimeException("No pong no fun")
                system.actorOf(Props[ClientAkkaActor]) ! RunAkkaClient(pongs, x)
                global.latch.await
                system.shutdown
                System.exit(0)
            }
            case None => {
                throw new RuntimeException("no \"num_pings\" found")
            }
        }
    }

    def runRemoteActors(args: List[String], pongs: List[RemoteActorPath], numPings: Option[Int]): Unit = args match {
        case NumPings(num) :: tail => numPings match {
            case Some(x) => {
                println("\"num_pings\" already defined, first value = " + x + ", second value = " + num)
            }
            case None => runRemoteActors(tail, pongs, Some(num.toInt))
        }
        case arg :: tail => arg match {
            case SimpleUri(host, port) => {
                runRemoteActors(tail, RemoteActorPath(arg, host, port.toInt) :: pongs, numPings)
            }
            case _ => {
                throw new IllegalArgumentException("illegal argument: " + arg)
            }
        }
        case Nil => numPings match {
            case Some(x) => {
                if (pongs.length < 2) throw new RuntimeException("at least two hosts required")
                (new ClientActor(pongs, x)).start
            }
            case None => {
                throw new RuntimeException("no \"num_pings\" found")
            }
        }
    }

    def main(args: Array[String]): Unit = args.toList match {
        case "akka" :: akkaArgs => {
            val system = ActorSystem("benchmark", ConfigFactory.load.getConfig("benchmark"))
            runAkka(system, akkaArgs, Nil, None)
        }
        case "remote_actors" :: tail => {
            runRemoteActors(tail, Nil, None)
        }
        case Nil => {
            println("usage: ...")
        }
    }
}

object DistributedServerApp {

    def usage = println("usage: (akka [configName]) | (remote_actors {port})")

    def main(args: Array[String]): Unit = args.toList match {
        case "akka" :: tail if tail.length < 2 => {
            val system = ActorSystem(if (tail.isEmpty) "pongServer" else tail.head, ConfigFactory.load.getConfig("pongServer"))
            val pong = system.actorOf(Props(new ServerAkkaActor(system)), "pong")
            pong
        }
        case "remote_actors" :: port :: Nil => {
            (new ServerActor(port.toInt)).start
        }
        case _ => usage
    }
}
