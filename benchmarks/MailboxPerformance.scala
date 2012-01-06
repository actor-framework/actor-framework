import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import scala.annotation.tailrec

case object Msg

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class ThreadedReceiver(n: Long) extends Actor {
    override def act() {
        var i: Long = 0
        while (i < n)
            receive {
                case Msg => i += 1
            }
    }
}

class ThreadlessReceiver(n: Long) extends Actor {
    var i: Long = 0
    override def act() {
        react {
            case Msg =>
                i += 1
                if (i < n) act
        }
    }
}

class AkkaReceiver(n: Long) extends akka.actor.Actor {
    var received: Long = 0
    def receive = {
        case Msg =>
            received += 1
            if (received == n) {
                global.latch.countDown
                self.exit
            }
    }
}

object MailboxPerformance {
    def usage() {
        Console println "usage: (threaded|threadless|akka) (num_threads) (msgs_per_thread)"
    }
    def main(args: Array[String]) = {
        if (args.size != 3) {
            usage
            throw new IllegalArgumentException("")
        }
        val threads = args(1).toInt
        val msgs = args(2).toInt
        val impl = List("threaded", "threadless", "akka").indexOf(args(0))
        if (impl == -1) {
            usage
        }
        else if (impl < 2) {
            val rcvRef = if (impl == 0) (new ThreadedReceiver(threads*msgs)).start
                         else (new ThreadlessReceiver(threads*msgs)).start
            for (i <- 0 until threads)
                (new java.lang.Thread {
                    override def run() { for (_ <- 0 until msgs) rcvRef ! Msg }
                }).start
        }
        else {
            val rcvRef = actorOf(new AkkaReceiver(threads*msgs)).start
            for (i <- 0 until threads)
                (new java.lang.Thread {
                    override def run() { for (_ <- 0 until msgs) rcvRef ! Msg }
                }).start
            global.latch.await
        }
    }
}
