import ctoKString.*
import kotlinx.cinterop.*
import kotlin.native.*
import kotlin.test.*

fun main() {
    assertEquals("", empty()!!.toKStringFromUtf8())
    assertEquals("foo", foo()!!.toKStringFromUtf8())
    assertEquals("куку", kuku()!!.toKStringFromUtf8())
    assertEquals("\uFFFD\uFFFD", invalid_utf8()!!.toKStringFromUtf8())
}
