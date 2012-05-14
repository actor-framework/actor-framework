import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }

import scala.actors.{Actor, AbstractActor, OutputChannel}
import scala.actors.remote.RemoteActor
import scala.actors.remote.RemoteActor._
import scala.actors.remote.Node

import com.typesafe.config.ConfigFactory
import Console.println

case object Ok
case object Done
case object OkTimeout
case class Error(msg: String)
case class KickOff(value: Int)
case class Ping(value: Int)
case class Pong(value: Int)
case class AddPong(path: String)
case object Hello
case object Olleh

case class PongDidNotRespond(pong: AkkaActorRef, ping: AkkaActorRef)

case class RemoteActorPath(uri: String, host: String, port: Int)

case class RunClient(pongs: List[RemoteActorPath], numPings: Int)
case class RunAkkaClient(pongs: List[AkkaActorRef], numPings: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(parent: OutputChannel[Any], pong: AbstractActor) extends Actor {
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

class ServerActor(port: Int) extends Actor {

    private var pongs: List[Pair[String,AbstractActor]] = Nil

    final def msgLoop(): Nothing = react {
        case Ping(value) => {
            reply(Pong(value))
            msgLoop
        }
        case AddPong(path) => {
            if (pongs.exists(x => x._1 == path)) {
                sender ! Ok
            }
            else {
                try {
                    path.split(":") match {
                        case Array(node, port) => {
                            val pong = select(new Node(node, port.toInt), 'Pong)
                            sender ! Ok
                            pongs = Pair(path, pong) :: pongs
                        }
                    }
                }
                catch {
                    case e => {
                        // catches match error, connection error and
                        // integer conversion failure
                        reply(Error(e.toString))
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

    def collectDoneMessages(): Nothing  = react {
        case Done => {
            if (left > 1) {
                left -= 1
                collectDoneMessages
            }
        }
    }

    def collectOkMessages(): Nothing = react {
        case Ok => {
            if (left == 1) {
                pongs.foreach(x => x ! KickOff(numPings))
                left = pongs.length * (pongs.length - 1)
                collectDoneMessages
            } else {
                left -= 1
                collectOkMessages
            }
        }
        case Error(what) => {
            println("Error: " + what)
        }
    }

    override def act() = {
        RemoteActor.classLoader = getClass().getClassLoader()
        react {
            case RunClient(pongPaths, numPings) => {
                this.numPings = numPings
                pongs = pongPaths.map(x => {
                    val pong = select(new Node(x.host, x.port), 'Pong)
                    pong ! AddPong(x.uri)
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

    def recvLoop(pongs: List[AkkaActorRef], pendingPongs: List[Pair[AkkaActorRef, AkkaActorRef]]): Receive = {
        case Ping(value) => {
            sender ! Pong(value)
        }
        case Olleh => {
            pendingPongs.find(x => x._1 == sender) match {
                case Some((pong, ping)) => {
println("added actor " + pong.path)
                    ping ! Ok
                    become(recvLoop(pong :: pongs, pendingPongs.filterNot(x => x._1 == sender)))
                }
                case None => {
                    // operation already timed out
                }
            }
        }
        case PongDidNotRespond(pong, ping) => {
            pendingPongs.find(x => x._1 == sender) match {
                case Some(Pair(_, y)) => {
                    ping ! Error(pong + " did not respond")
                    become(recvLoop(pongs, pendingPongs.filterNot(x => x._1 == sender)))
                }
                case None => {
                    // operation already succeeded (received Olleh)
                }
            }
        }
        case AddPong(path) => {
            if (pongs.exists((x) => x.path == path)) {
                sender ! Ok
            }
            else {
                import akka.util.duration._
println("try to add actor " + path)
                val pong = system.actorFor(path)
                // wait at most 5sec. for response
                pong ! Hello
                system.scheduler.scheduleOnce(5 seconds, self, PongDidNotRespond(pong, sender))
                become(recvLoop(pongs, Pair(pong, sender) :: pendingPongs))
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

    var left: Int = 0

    def collectDoneMessages: Receive = {
        case Done => {
//println("Done")
            if (left == 1) {
                global.latch.countDown
                context.stop(self)
            } else {
                left = left - 1
            }
        }
        case _ => {
            // ignore any other message
        }
    }

    def collectOkMessages(pongs: List[AkkaActorRef], numPings: Int): Receive = {
        case Ok => {
//println("Ok")
            if (left == 1) {
                left = pongs.length * (pongs.length - 1)
                pongs.foreach(x => x ! KickOff(numPings))
                become(collectDoneMessages)
            } else {
                left = left - 1
            }
        }
        case Error(what) => {
            println("Error from " + sender + " => " + what)
            global.latch.countDown
            context.stop(self)
        }
        case OkTimeout => {
            println("At least one pong did not reply... left: " + left)
        }
    }

    def receive = {
        case RunAkkaClient(pongs, numPings) => {
            import akka.util.duration._
            pongs.foreach(x => pongs.foreach(y => if (x != y) {
                x ! AddPong(y.path.toString)
            }))
            system.scheduler.scheduleOnce(10 seconds, self, OkTimeout)
            left = pongs.length * (pongs.length - 1)
            become(collectOkMessages(pongs, numPings))
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
        }
        case "remote_actors" :: port :: Nil => {
            (new ServerActor(port.toInt)).start
        }
        case _ => usage
    }
}

