import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import scala.annotation.tailrec

case class Token(value: Int)
case class Init(ringSize: Int, repetitions: Int)

case class Calc(value: Long)
case class Factors(values: List[Long])

object global {
    final val taskN: Long = 86028157l * 329545133
    final val factor1: Long = 86028157
    final val factor2: Long = 329545133
    final val numMessages = 1000
    val latch = new java.util.concurrent.CountDownLatch(1)
    def checkFactors(f: List[Long]) {
        assert(f.length == 2 && f(1) == factor1 && f(2) == factor2)
    }
    def factorize(arg: Long): List[Long] = {
        var n = arg
        if (n <= 3)
            return List[Long](n)
        var result = new scala.collection.mutable.LinkedList[Long]
        var d: Long = 2
        while (d < n) {
            if ((n % d) == 0) {
                result :+ d
                n = n / d
            }
            else
                d = if (d == 2) 3 else d + 2
        }
        (result :+ d) toList
    }
}

class ThreadedWorker extends Actor {
    override def act() {
        receive {
            case Calc(value) =>
                reply(global.factorize(value))
                act()
        }
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
    @tailrec final def newRing(a: Actor, i: Int): Actor = {
        val next = (new ThreadedChainLink(a)).start
        if (i > 0) newRing(next, i-1) else next
    }
    override def act() {
        val worker = (new ThreadedWorker).start
        receive {
            case Init(rsize, iterations) =>
                var remainingFactors = 0
                for (_ <- 0 until iterations) {
                    val next = newRing(this, rsize)
                    remainingFactors += 1
                    worker ! Calc(global.taskN)
                    var done = false
                    while (done == false)
                        receive {
                            case Token(value) =>
                                if (value > 0) next ! Token(value-1) else done = true
                            case Factors(f) =>
                                global.checkFactors(f)
                                remainingFactors -= 1
                        }
                }
                while (remainingFactors > 0)
                    receive {
                        case Factors(f) =>
                            global.checkFactors(f)
                            remainingFactors -= 1
                    }

        }
    }
}

object MixedCase {
    def usage() {
        Console println "usage: (threaded|threadless|akka) (ring_size) (repetitions)"
    }
    def main(args: Array[String]) = {
        if (args.size != 3) {
            usage
            throw new IllegalArgumentException("")
        }
        val ringSize = args(1).toInt
        val repetitions = args(2).toInt
        val impl = List("threaded", "threadless", "akka").indexOf(args(0))
        if (impl == -1) {
            usage
        }
        else if (impl == 0) {
            System.setProperty("actors.maxPoolSize", ((11 * ringSize) + 10).toString)
            for (_ <- 0 until 10) {
                val a = (new ThreadedChainMaster()).start
                a ! Init(ringSize, repetitions)
            }
        }
        /*else {
            val rcvRef = actorOf(new AkkaReceiver(threads*msgs)).start
            for (i <- 0 until threads)
                (new java.lang.Thread {
                    override def run() { for (_ <- 0 until msgs) rcvRef ! Msg }
                }).start
            global.latch.await
        }*/
    }
}
