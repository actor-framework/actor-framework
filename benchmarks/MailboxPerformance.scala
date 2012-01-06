import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import scala.annotation.tailrec

case object Msg

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class ThreadedReceiver(n: Long) extends Actor {
    def act() {
        var i: Long = 0
        while (i < n)
            receive {
                case Msg => i += 1
            }
       Console println "received " + i + " messages"
    }
}

class ThreadlessReceiver(n: Long) extends Actor {
    def rcv(rest: Long) {
        react {
            case Msg => if (rest > 0) rcv(rest-1);
        }
    }
    def act() {
        rcv(n);
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
    def cr_threads(receiver: { def !(msg:Any): Unit }, threads: Int, msgs: Int) {
            for (i <- 0 until threads)
                (new java.lang.Thread {
                    override def run() {
                        for (j <- 0 until msgs)
                            receiver ! Msg
                    }
                }).start
    }
    def main(args: Array[String]) = {
        if (args.size != 3) {
            usage
            throw new IllegalArgumentException("")
        }
        val threads = args(1).toInt
        val msgs = args(2).toInt
        if (args(0) == "threaded") {
            cr_threads((new ThreadedReceiver(threads*msgs)).start, threads, msgs)
        }
        else if (args(0) == "threadless") {
            cr_threads((new ThreadlessReceiver(threads*msgs)).start, threads, msgs)
        }
        else if (args(0) == "akka") {
            val a = actorOf(new AkkaReceiver(threads*msgs)).start
            // akka actors define operator! with implicit argument
            cr_threads(new AnyRef { def !(what: Any) { a ! what } }, threads, msgs)
            global.latch.await
        }
        else usage
    }
}
