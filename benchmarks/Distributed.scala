import akka.actor.{ Props, Actor, ActorRef, ActorSystem }
import com.typesafe.config.ConfigFactory
import Console.println

case object Ok
case object Done
case class Error(msg: String)
case class KickOff(value: Int)
case class Ping(value: Int)
case class Pong(value: Int)
case class AddPong(path: String)
case object Hello
case object Olleh

case class RunClient(pongs: List[ActorRef], numPings: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(parent: ActorRef, pong: ActorRef) extends Actor {
    def receive = {
        case Pong(0) => {
//println(parent + " ! Done")
            parent ! Done
            context.stop(self)
        }
        case Pong(value) => {
            sender ! Ping(value - 1)
        }
        case KickOff(value) => {
//println("PingActor::KickOff " + value)
            pong ! Ping(value)
        }
    }
}

class ServerActor(system: ActorSystem) extends Actor {

    var pongs = List[ActorRef]()

    def receive = {
        case Ping(value) => {
            sender ! Pong(value)
        }
        case AddPong(path) => {
//println("add actor " + path)
            if (pongs.exists((x) => x.path == path)) {
                sender ! Ok
            } else {
                val pong = system.actorFor(path)
                pong ! Hello
                pongs = pong :: pongs
                sender ! Ok
            }
        }
        case KickOff(value) => {
            val client = sender
//println("KickOff(" + value + ") from " + client)
            pongs.foreach((x) => context.actorOf(Props(new PingActor(client, x))) ! KickOff(value))
        }
        case Hello => {
            sender ! Olleh
        }
        case Olleh => {
//println("Olleh from " + sender)
        }
    }

}

class ClientActor extends Actor {

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
        case x => {
            // ignore any other message
        }
    }

    def collectOkMessages(pongs: List[ActorRef], numPings: Int): Receive = {
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
    }

    def receive = {
        case RunClient(pongs, numPings) => {
            pongs.foreach(x => pongs.foreach(y => if (x != y) {
                x ! AddPong(y.path.toString)
            }))
            left = pongs.length * (pongs.length - 1)
            become(collectOkMessages(pongs, numPings))
        }
    }
}

object distributedClientApp {
    def main(args: Array[String]) = {

        val system = ActorSystem("benchmark", ConfigFactory.load.getConfig("benchmark"))

        var numPings: Int = 0

        var pongs = List[ActorRef]()

        val NumPings = "num_pings=([0-9]+)".r
        args.foreach(arg => arg match {
            case NumPings(num) => {
                numPings = num.toInt
            }
            case _ => {
//println("add actor " + arg)
                pongs = system.actorFor(arg) :: pongs
            }
        })

        system.actorOf(Props[ClientActor]) ! RunClient(pongs, numPings)

        global.latch.await
        System.exit(0)
    }
}

object distributedServerApp {
    def main(args: Array[String]) = {

        val sysName = if (args.length > 0) args(0) else "pongServer"

        val system = ActorSystem(sysName, ConfigFactory.load.getConfig("pongServer"))

        val pong = system.actorOf(Props(new ServerActor(system)), "pong")

        //val subSystem = ActorSystem(sysName + "Client", ConfigFactory.load.getConfig("benchmark"))
        //val pong = system.actorOf(Props(new ServerActor(subSystem)), "pong")

    }
}

