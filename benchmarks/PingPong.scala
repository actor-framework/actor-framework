import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import akka.actor.Actor.remote
import Console.println

case object KickOff
case class Ping(value: Int)
case class Pong(value: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class PingActor(pong: akka.actor.ActorRef) extends akka.actor.Actor {
    def receive = {
        case Pong(1000) => {
            //println("Received final pong")
            global.latch.countDown
            self.exit
        }
        case Pong(value: Int) => {
            //println("Received Pong(" + value + ")")
            self.reply(Ping(value))
        }
        case KickOff => {
            pong ! Ping(0)
        }
    }
}

class PongActor extends akka.actor.Actor {
    def receive = {
        case Ping(value: Int) => {
            //println("Received Ping(" + value + ")")
            self.reply(Pong(value + 1))
        }
    }
}

object pingApp {
    def main(args: Array[String]) = {
        if (args.size != 2) {
            println("usage: pingApp (host) (port)")
            println("       (connects to pong-service of (host) on given port)")
            System.exit(1)
        }
        val pong = remote.actorFor("pong-service", args(0), args(1).toInt)
        val myPing = actorOf(new PingActor(pong)).start
        remote.start("localhost", 64002).register("ping-service", myPing)
        myPing ! KickOff
        global.latch.await
        remote.shutdown
        System.exit(0)
    }
}

object pongApp {
    def main(args: Array[String]) = {
        if (args.size != 1) {
            println("usage: pongApp (port)")
            println("       (binds pong-service to given port)")
            System.exit(1)
        }
        val myPong = actorOf(new PongActor).start
        remote.start("localhost", args(0).toInt)
              .register("pong-service", myPong)
    }
}
