import scala.actors.Actor
import scala.actors.Actor._

case class Result(value: Int)

class ThreadedTestee(parent: Actor, n: Int) extends Actor {
    def act() {
        if (n > 0) {
            (new ThreadedTestee(this, n-1)).start
            (new ThreadedTestee(this, n-1)).start
            receive {
                case Result(v1) =>
                    receive {
                        case Result(v2) =>
                            parent ! new Result(v1 + v2)
                    }
            }
        }
        else {
            parent ! new Result(1)
        }
    }
}

class ThreadlessTestee(parent: Actor, n: Int) extends Actor {
    def act() {
        if (n > 0) {
            (new ThreadlessTestee(this, n-1)).start
            (new ThreadlessTestee(this, n-1)).start
            react {
                case Result(v1) =>
                    react {
                        case Result(v2) =>
                            parent ! new Result(v1 + v2)
                    }
            }
        }
        else {
            parent ! new Result(1)
        }
    }
}

object ActorCreation {
    def main(args: Array[String]) = {
        if (args.size != 2) {
            Console println "usage: (threaded|threadless|akka) POW\n       creates 2^POW actors of given impl"
        }
        val n = args(1).toInt
        if (args(0) == "threaded") {
            actor {
                (new ThreadedTestee(self, n-1)).start
                (new ThreadedTestee(self, n-1)).start
                receive {
                    case Result(v1) =>
                        receive {
                            case Result(v2) =>
                                if ((v1 + v2) != (1 << n)) {
                                    throw new RuntimeException("Expected " + (1 << n) + ", received " + (v1 + v2))
                                }
                        }
                }
            }
        } else if (args(0) == "threadless") {
            actor {
                (new ThreadlessTestee(self, n-1)).start
                (new ThreadlessTestee(self, n-1)).start
                react {
                    case Result(v1) =>
                        react {
                            case Result(v2) =>
                                if ((v1 + v2) != (1 << n)) {
                                    throw new RuntimeException("Expected " + (1 << n) + ", received " + (v1 + v2))
                                }
                        }
                }
            }
        }
    }
}
