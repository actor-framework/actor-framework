import Console.println
import scala.{PartialFunction => PF}

case class Msg1(val0: Int)
case class Msg2(val0: Double)
case class Msg3(val0: List[Int])
case class Msg4(val0: Int, val1: String)
case class Msg5(val0: Int, val1: Int, val2: Int)
case class Msg6(val0: Int, val1: Double, val2: String)

object Matching {
    def main(args: Array[String]) = {
        if (args.size != 1) {
            println("usage: Matching {NUM_LOOPS}")
            System.exit(1)
        }
        val zero: Long = 0
        val numLoops = args(0).toLong
        var msg1Matched: Long = 0;
        var msg2Matched: Long = 0;
        var msg3Matched: Long = 0;
        var msg4Matched: Long = 0;
        var msg5Matched: Long = 0;
        var msg6Matched: Long = 0;
        val partFun: PF[Any, Unit] = {
            case Msg1(0) => msg1Matched += 1
            case Msg2(0.0) => msg2Matched += 1
            case Msg3(List(0)) => msg3Matched += 1
            case Msg4(0, "0") => msg4Matched += 1
            case Msg5(0, 0, 0) => msg5Matched += 1
            case Msg6(0, 0.0, "0") => msg6Matched += 1
        }
        val m1: Any = Msg1(0)
        val m2: Any = Msg2(0.0)
        val m3: Any = Msg3(List(0))
        val m4: Any = Msg4(0, "0")
        val m5: Any = Msg5(0, 0, 0)
        val m6: Any = Msg6(0, 0.0, "0")
        for (_ <- zero until numLoops) {
            partFun(m1)
            partFun(m2)
            partFun(m3)
            partFun(m4)
            partFun(m5)
            partFun(m6)
        }
        assert(msg1Matched == numLoops)
        assert(msg2Matched == numLoops)
        assert(msg3Matched == numLoops)
        assert(msg4Matched == numLoops)
        assert(msg5Matched == numLoops)
        assert(msg6Matched == numLoops)
        println("msg1Matched = " + msg1Matched.toString)
    }
}
