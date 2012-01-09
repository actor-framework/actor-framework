import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.Actor.actorOf
import scala.annotation.tailrec

case class Token(value: Int)
case class Init(ringSize: Int, initialTokenValue: Int, repetitions: Int)

case class Calc(value: Long)
case class Factors(values: List[Long])

case object Done
case object MasterExited

object global {
    final val taskN: Long = 86028157l * 329545133
    final val factor1: Long = 86028157
    final val factor2: Long = 329545133
    final val factors = List(factor2,factor1)
    val latch = new java.util.concurrent.CountDownLatch(1)
    def checkFactors(f: List[Long]) {
        assert(f equals factors)
    }
    @tailrec final def fac(n: Long, m: Long, interim: List[Long]) : List[Long] = {
        if (n == m) m :: interim
        else if ((n % m) == 0) fac(n/m, m, m :: interim)
        else fac(n, if (m == 2) 3 else m + 2, interim)
    }
    def factorize(arg: Long): List[Long] = {
        if (arg <= 3) List(arg)
        else fac(arg, 2, List())
    }
}

class ThreadedWorker(supervisor: Actor) extends Actor {
    override def act() {
        var done = false
        while (done == false) {
            receive {
                case Calc(value) => supervisor ! Factors(global.factorize(value))
                case Done => done = true
            }
        }
    }
}

class ThreadedChainLink(next: Actor) extends Actor {
    override def act() {
        var done = false
        while (done == false)
            receive {
                case Token(value) => next ! Token(value); if (value == 0) done = true
            }
    }
}

class ThreadedChainMaster(supervisor: Actor) extends Actor {
    val worker = (new ThreadedWorker(supervisor)).start
    @tailrec final def newRing(next: Actor, rsize: Int): Actor = {
        if (rsize == 0) next
        else newRing((new ThreadedChainLink(next)).start, rsize-1)
    }
    override def act() = receive {
        case Init(rsize, initialTokenValue, repetitions) =>
            for (_ <- 0 until repetitions) {
                worker ! Calc(global.taskN)
                val next = newRing(this, rsize-1)
                next ! Token(initialTokenValue)
                var ringDone = false
                while (ringDone == false) {
                    receive {
                        case Token(0) => ringDone = true
                        case Token(value) => next ! Token(value-1)
                    }
                }
            }
            worker ! Done
            supervisor ! MasterExited
    }
}

class ThreadedSupervisor(numMessages: Int) extends Actor {
    override def act() = for (_ <- 0 until numMessages) {
        receive {
            case Factors(f) => global.checkFactors(f)
            case MasterExited =>
        }
    }
}

class ThreadlessWorker(supervisor: Actor) extends Actor {
    override def act() = react {
        case Calc(value) => supervisor ! Factors(global.factorize(value)); act
        case Done => // recursion ends
    }
}

class ThreadlessChainLink(next: Actor) extends Actor {
    override def act() = react {
        case Token(value) => next ! Token(value); if (value > 0) act
    }
}

class ThreadlessChainMaster(supervisor: Actor) extends Actor {
    val worker = (new ThreadlessWorker(supervisor)).start
    @tailrec final def newRing(next: Actor, rsize: Int): Actor = {
        if (rsize == 0) next
        else newRing((new ThreadlessChainLink(next)).start, rsize-1)
    }
    var initialTokenValue = 0
    var repetitions = 0
    var iteration = 0
    var rsize = 0
    var next: Actor = null
    def rloop(): Nothing = react {
        case Token(0) =>
            iteration += 1
            if (iteration < repetitions) {
                worker ! Calc(global.taskN)
                next = newRing(this, rsize-1)
                next ! Token(initialTokenValue)
                rloop
            }
            else
            {
                worker ! Done
                supervisor ! MasterExited
            }
        case Token(value) => next ! Token(value-1) ; rloop
    }
    override def act() = react {
        case Init(rs, itv, rep) =>
            rsize = rs ; initialTokenValue = itv ; repetitions = rep
            worker ! Calc(global.taskN)
            next = newRing(this, rsize-1)
            next ! Token(initialTokenValue)
            rloop
    }
}

class ThreadlessSupervisor(numMessages: Int) extends Actor {
    def rcv(remaining: Int): Nothing = react {
        case Factors(f) => global.checkFactors(f); if (remaining > 1) rcv(remaining-1)
        case MasterExited => if (remaining > 1) rcv(remaining-1)
    }
    override def act() = rcv(numMessages)
}

class AkkaWorker(supervisor: akka.actor.ActorRef) extends akka.actor.Actor {
    def receive = {
        case Calc(value) => supervisor ! Factors(global.factorize(value))
        case Done => self.exit
    }
}

class AkkaChainLink(next: akka.actor.ActorRef) extends akka.actor.Actor {
    def receive = {
        case Token(value) => next ! Token(value); if (value == 0) self.exit
    }
}

class AkkaChainMaster(supervisor: akka.actor.ActorRef) extends akka.actor.Actor {
    var initialTokenValue = 0
    var repetitions = 0
    var iteration = 0
    var rsize = 0
    var next: akka.actor.ActorRef = null
    val worker = actorOf(new AkkaWorker(supervisor)).start
    @tailrec final def newRing(next: akka.actor.ActorRef, rsize: Int): akka.actor.ActorRef = {
        if (rsize == 0) next
        else newRing(actorOf(new AkkaChainLink(next)).start, rsize-1)
    }
    def initialized: Receive = {
        case Token(0) =>
            iteration += 1
            if (iteration < repetitions) {
                worker ! Calc(global.taskN)
                next = newRing(self, rsize-1)
                next ! Token(initialTokenValue)
            }
            else
            {
                worker ! Done
                supervisor ! MasterExited
                self.exit
            }
        case Token(value) => next ! Token(value-1)
    }
    def receive = {
        case Init(rs, itv, rep) =>
            rsize = rs ; initialTokenValue = itv ; repetitions = rep
            worker ! Calc(global.taskN)
            next = newRing(self, rsize-1)
            next ! Token(initialTokenValue)
            become(initialized)
    }
}

class AkkaSupervisor(numMessages: Int) extends akka.actor.Actor {
    var i = 0
    def inc() {
        i += 1;
        if (i == numMessages) {
            global.latch.countDown
            self.exit
        }
    }
    def receive = {
        case Factors(f) => global.checkFactors(f); inc
        case MasterExited => inc
    }
}

object MixedCase {
    def usage(): Nothing = {
        Console println "usage: ('threaded'|'threadless'|'akka') (num rings) (ring size) (initial token value) (repetitions)"
        System.exit(1) // why doesn't exit return Nothing?
        throw new RuntimeException("")
    }
    def main(args: Array[String]) = {
        if (args.size != 5) usage
        val numRings = args(1).toInt
        val ringSize = args(2).toInt
        val initialTokenValue = args(3).toInt
        val repetitions = args(4).toInt
        val initMsg = Init(ringSize, initialTokenValue, repetitions)
        val numMessages = (numRings + (numRings * repetitions))
        val impl = args(0)
        if (impl == "threaded") {
            //System.setProperty("actors.maxPoolSize", (numRings + (numRings * ringSize) + 10).toString)
            val s = (new ThreadedSupervisor(numMessages)).start
            for (_ <- 0 until numRings)
                (new ThreadedChainMaster(s)).start ! initMsg
        }
        else if (impl == "threadless") {
            val s = (new ThreadlessSupervisor(numMessages)).start
            for (_ <- 0 until numRings)
                (new ThreadlessChainMaster(s)).start ! initMsg
        }
        else if (impl == "akka") {
            val s = actorOf(new AkkaSupervisor(numMessages)).start
            for (_ <- 0 until numRings)
                actorOf(new AkkaChainMaster(s)).start ! initMsg
            global.latch.await
        }
        else usage
    }
}
