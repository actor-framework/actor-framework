package org.libcppa.utility

object IntStr {
    val IntRegex = "([0-9]+)".r
    def unapply(s: String): Option[Int] = s match {
        case IntRegex(`s`) => Some(s.toInt)
        case _ => None
    }
}

object KeyValuePair {
    val Rx = "([^=])+=([^=]*)".r
    def unapply(s: String): Option[Pair[String, String]] = s match {
        case Rx(key, value) => Some(Pair(key, value))
        case _ => None
    }
}

class Utility {

}
