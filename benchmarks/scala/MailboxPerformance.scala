package org.libcppa.mailbox_performance

import org.libcppa.utility._

import scala.actors.Actor
import scala.actors.Actor._
import akka.actor.{ Props, Actor => AkkaActor, ActorRef => AkkaActorRef, ActorSystem }
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
                context.stop(self)
            }
    }
}

class MailboxPerformance(threads: Int, msgs: Int) {
    val total = threads * msgs;
    def run[T](testee: T, fun: T => Unit) {
        for (_ <- 0 until threads) {
            (new Thread {
                override def run() { for (_ <- 0 until msgs) fun(testee) }
            }).start
        }
    }
    def runThreaded() {
        run((new ThreadedReceiver(total)).start, {(a: Actor) => a ! Msg})
    }
    def runThreadless() {
        run((new ThreadlessReceiver(total)).start, {(a: Actor) => a ! Msg})
    }
    def runAkka() {
        val system = ActorSystem()
        run(system.actorOf(Props(new AkkaReceiver(total))), {(a: AkkaActorRef) => a ! Msg})
        global.latch.await
        system.shutdown
        System.exit(0)
    }
}

object Main {
    def usage() {
        Console println "usage: (threaded|threadless|akka) (num_threads) (msgs_per_thread)"
    }
    def main(args: Array[String]): Unit = args match {
        case Array(impl, IntStr(threads), IntStr(msgs)) => {
            val prog = new MailboxPerformance(threads, msgs)
            impl match {
                case "threaded" => prog.runThreaded
                case "threadless" => prog.runThreadless
                case "akka" => prog.runAkka
                case _ => usage
            }
        }
        case _ => usage
    }
}
