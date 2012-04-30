import scala.actors.Actor
import scala.actors.Actor._
import Console.println

case object KickOff
case class Ping(value: Int)
case class Pong(value: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(num: Int, pong: akka.actor.ActorRef) extends akka.actor.Actor {
    def receive = {
        case Pong(`num`) => {
            //println("Received final pong")
            global.latch.countDown
            context.stop(self)
        }
        case Pong(value) => sender ! Ping(value + 1)
        case KickOff => pong ! Ping(0)
    }
}

class PongActor extends akka.actor.Actor {
    def receive = {
        case Ping(value) => sender ! Pong(value)
    }
}

object pingApp {
    def main(args: Array[String]) = {
        if (args.size != 3) {
            println("usage: pingApp (host) (port) (num pings)")
            println("       connects to pong-service@(host):(port)")
            System.exit(1)
        }
        val host = args(0)
        val port = args(1).toInt
        val numPings = args(2).toInt
        val pong = remote.actorFor("pong-service", host, port)
        val ping = system.actorOf(Props(new PingActor(numPings, pong)), name="ping")
        remote.start("localhost", 64002).register("ping-service", ping)
        ping ! KickOff
        global.latch.await
        remote.shutdown
        System.exit(0)
    }
}

object pongApp {
    def main(args: Array[String]) = {
        if (args.size != 1) {
            println("usage: pongApp (port)")
            println("       binds pong-service to given port")
            System.exit(1)
        }
        remote.start("localhost", args(0).toInt)
              .register("pong-service",
                        actorOf(new PongActor).start)
    }
}

