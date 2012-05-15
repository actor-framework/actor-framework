import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }

import scala.actors.{Actor, AbstractActor, OutputChannel}
import scala.actors.remote.RemoteActor
import scala.actors.remote.RemoteActor._
import scala.actors.remote.Node

import com.typesafe.config.ConfigFactory
import Console.println

case object Done
case object OkTimeout
case class Error(msg: String, token: String)
case class KickOff(value: Int)
case class Ping(value: Int)
case class Pong(value: Int)

case class Ok(token: String)
case class AddPong(path: String, token: String)

case object Hello
case object Olleh

case class NoTokenReceived(token: String)
case class PongDidNotRespond(pong: AbstractActor)
case class AkkaPongDidNotRespond(pong: AkkaActorRef, ping: AkkaActorRef)

case class RemoteActorPath(uri: String, host: String, port: Int)

case class RunClient(pongs: List[RemoteActorPath], numPings: Int)
case class RunAkkaClient(pongs: List[AkkaActorRef], numPings: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(parent: OutputChannel[Any], pong: OutputChannel[Any]) extends Actor {
    override def act() = react {
        case Pong(0) => {
            parent ! Done
        }
        case Pong(value) => {
            reply(Ping(value - 1))
            act
        }
        case KickOff(value) => {
            pong ! Ping(value)
            act
        }
    }
}

class DelayActor(msec: Long, parent: AbstractActor, msg: Any) extends Actor {
    override def act() {
        reactWithin(msec) {
            case scala.actors.TIMEOUT => parent ! msg
        }
    }
}

class ServerActor(port: Int) extends Actor {

    private var pongs: List[(String,OutputChannel[Any])] = Nil
    private var pendingPongs: List[(String,AbstractActor,OutputChannel[Any],String)] = Nil

    final def msgLoop(): Nothing = react {
        case Ping(value) => {
            reply(Pong(value))
            msgLoop
        }
        case Olleh => {
            val pong = sender
            pendingPongs.filter(x => x match {
                case (path, `pong`, ping, token) => {
                    ping ! Ok(token)
                    pongs = (path, pong) :: pongs
                    true
                }
                case _ => false
            })
        }
        case PongDidNotRespond(pong) => {
            pendingPongs.filter(x => x match {
                case (_, `pong`, ping, token) => {
                    ping ! Error(pong + " did not respond within 5 seconds", token)
                    true
                }
                case _ => false
            })
        }
        case AddPong(path, token) => {
            if (pongs.exists(x => x._1 == path)) {
                sender ! Ok(token)
            }
            else {
                try {
                    path.split(":") match {
                        case Array(node, port) => {
                            val pong = select(new Node(node, port.toInt), 'Pong)
                             (new DelayActor(5000, Actor.self, PongDidNotRespond(pong))).start
                            pendingPongs = (path, pong, sender, token) :: pendingPongs
                        }
                    }
                }
                catch {
                    case e => {
                        // catches match error and integer conversion failure
                        reply(Error(e.toString, token))
                    }
                }
            }
            msgLoop
        }
        case KickOff(value) => {
            val client = sender
            pongs.foreach(x => (new PingActor(client, x._2)).start() ! KickOff(value))
            msgLoop
        }
    }

    override def act() {
        RemoteActor.classLoader = getClass().getClassLoader
        alive(port)
        register('Pong, Actor.self)
        msgLoop
    }
}

class ClientActor extends Actor {

    private var left: Int = 0
    private var numPings: Int = 0
    private var pongs: List[AbstractActor] = Nil
    private var tokens: List[String] = Nil

    def collectDoneMessages(): Nothing  = react {
        case Done => {
            if (left > 1) {
                left -= 1
                collectDoneMessages
            }
        }
    }

    def collectOkMessages(): Nothing = react {
        case Ok(token) => {
            tokens = token :: tokens
            if (tokens.length == left) {
                pongs.foreach(x => x ! KickOff(numPings))
                collectDoneMessages
            } else {
                collectOkMessages
            }
        }
        case NoTokenReceived(token) => {
//println("NoTokenReceived("+token+")")
            if (!tokens.contains(token)) {
                println("Error: " + token + " did not respond within 10 seconds")
            }
        }
        case Error(what, token) => {
            println("Error [from " + token + "]: " + what)
        }
    }

    override def act() = {
        RemoteActor.classLoader = getClass().getClassLoader()
        react {
            case RunClient(pongPaths, numPings) => {
                this.numPings = numPings
                pongs = pongPaths.map(x => {
//println("select pong ...")
                    val pong = select(new Node(x.host, x.port), 'Pong)
                    pongPaths.foreach(y => if (x != y) {
//println("send AddPong")
                        pong ! AddPong(y.uri, x.uri)
//println("spawn DelayActor")
                        (new DelayActor(10000, Actor.self, NoTokenReceived(x.uri))).start
                    })
                    pong
                })
                left = pongs.length * (pongs.length - 1)
                collectOkMessages
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
        case Olleh => {
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
                pong ! Hello
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
        case Hello => {
            sender ! Olleh
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
                (new ClientActor).start ! RunClient(pongs, x)
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
