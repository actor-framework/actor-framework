import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import scala.annotation.tailrec

case class Token(value: Int)
case class Init(ringSize: Int, repetitions: Int)

case class Calc(value: Long)
case class Factors(values: List[Long])

object global {
    final val taskN = ((Long) 86028157) * 329545133
    final val factor1: Long = 86028157
    final val factor2: Long = 329545133
    final val numMessages = 1000
    val latch = new java.util.concurrent.CountDownLatch(1)
    val checkFactors(f: List[Long]) {
        assert(f.length == 2 && f(1) == factor1 && f(2) == factor2)
    }
}

object factorize {
    def apply(arg: Long): List[Long] {
        var n = arg
        if (n <= 3)
            return List[Long](n)
        var result = new List[Long]
        var d: Long = 2
        while (d < n) {
            if ((n % d) == 0) {
                result append d
                n = n / d
            }
            else
                d = if (d == 2) 3 else d + 2
        }
        result append d
    }
}

class ThreadedChainLink(next: Actor) extends Actor {
    override def act() {
        receive {
            case Token(value) =>
                next ! Token(value)
                if (value > 0) act
        }
    }
}

class ThreadedChainMaster extends Actor {
    @tailrec
    final def newRing(a: Actor, i: Int) {
        val next = new ThreadedChainLink(a)
        if (i > 0) newRing(next, i-1) else next
    }
    def checkFactors(f: List[Long]) {
        
    }
    override def act() {
        val worker = actor {
            receive {
                case Calc(value) => reply factorize(value)
            }
        }
        receive {
            case Init(rsize, iterations) =>
                var remainingFactors = iterations
                for (_ <- 0 until iterations) {
                    val next = newRing(rsize)
                    worker ! Calc(global.taskN)
                    while (state != 0x2)
                        receive {
                            case Token(value) =>
                                if (value > 0) next ! Token(value-1)

                        }
                }

        }
    }
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
