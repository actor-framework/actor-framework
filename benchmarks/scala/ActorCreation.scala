import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }

case object GoAhead

case class Spread(value: Int)
case class Result(value: Int)

object global {
    val latch = new java.util.concurrent.CountDownLatch(1)
}

class ThreadedTestee(parent: Actor) extends Actor {
    def act() {
        receive {
            case Spread(s) =>
                if (s > 0) {
                    val next = Spread(s-1)
                    (new ThreadedTestee(this)).start ! next
                    (new ThreadedTestee(this)).start ! next
                    receive {
                        case Result(v1) =>
                            receive {
                                case Result(v2) =>
                                    parent ! Result(v1 + v2)
                            }
                    }
                }
                else {
                    parent ! Result(1)
                }
        }
    }
}

class ThreadlessTestee(parent: Actor) extends Actor {
    def act() {
        react {
            case Spread(s) =>
                if (s > 0) {
                    val next = Spread(s-1)
                    (new ThreadlessTestee(this)).start ! next
                    (new ThreadlessTestee(this)).start ! next
                    react {
                        case Result(v1) =>
                            react {
                                case Result(v2) =>
                                    parent ! Result(v1 + v2)
                            }
                    }
                }
                else {
                    parent ! Result(1)
                }
        }
    }
}

class AkkaTestee(parent: AkkaActorRef) extends AkkaActor {
    def receive = {
        case Spread(0) =>
            parent ! Result(1)
            context.stop(self)
        case Spread(s) =>
            val msg = Spread(s-1)
            context.actorOf(Props(new AkkaTestee(self))) ! msg
            context.actorOf(Props(new AkkaTestee(self))) ! msg
            context.become {
                case Result(r1) =>
                    context.become {
                        case Result(r2) =>
                            parent ! Result(r1 + r2)
                            context.stop(self)
                    }
            }
    }
}

class AkkaRootTestee(n: Int) extends AkkaActor {
    def receive = {
        case GoAhead =>
            context.actorOf(Props(new AkkaTestee(self))) ! Spread(n)
        case Result(v) =>
            if (v != (1 << n)) {
                Console.println("Expected " + (1 << n) + ", received " + v)
                System.exit(42)
            }
            global.latch.countDown
            context.stop(self)
    }
}

object ActorCreation {
    def usage() {
        Console println "usage: (threaded|threadless|akka) POW\n       creates 2^POW actors of given impl"
    }
    def main(args: Array[String]): Unit = {
        if (args.size != 2) {
            usage
            throw new IllegalArgumentException("")
        }
        val n = args(1).toInt
        if (args(0) == "threaded") {
            val newMax = (1 << n) + 100
            System.setProperty("actors.maxPoolSize", newMax.toString)
            (new ThreadedTestee(self)).start ! Spread(n)
            receive {
                case Result(v) =>
                    if (v != (1 << n))
                        Console.println("ERROR: expected " + (1 << n) + ", received " + v)
            }
        }
        else if (args(0) == "threadless") {
            actor {
                (new ThreadlessTestee(self)).start ! Spread(n)
                react {
                    case Result(v) =>
                        if (v != (1 << n))
                            Console.println("ERROR: expected " + (1 << n) + ", received " + v)
                }
            }
        }
        else if (args(0) == "akka") {
            val system = ActorSystem()
            system.actorOf(Props(new AkkaRootTestee(n))) ! GoAhead
            global.latch.await
            system.shutdown
            System.exit(0)
        }
        else usage
    }
}

