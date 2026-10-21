package com.mediumdev.scancore.host

import eu.kanade.tachiyomi.network.BrowserCheckException
import eu.kanade.tachiyomi.network.HttpException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.withTimeout
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import java.io.BufferedReader
import java.io.IOException
import java.io.Writer
import java.net.SocketTimeoutException
import java.net.UnknownHostException

// What SCANCORE and the host say to each other: one JSON object per line.
//   -> {"id": 1, "method": "search", "params": {...}}
//   <- {"id": 1, "result": ...}  or  {"id": 1, "error": {"message", "status", "verifyUrl", "verifyAgent"}}
// The host can ask SCANCORE too (a browser check to pass, see ask()):
//   <- {"ask": 7, "method": "solveBrowserCheck", "params": {...}}
//   -> {"id": 0, "method": "answer", "params": {"ask": 7, "result": ...}}
// Calls run concurrently and answer in any order, so a slow site never
// holds up another source.
class RpcServer(
    private val handler: suspend (method: String, params: JsonObject) -> JsonElement,
    private val callTimeoutMs: Long = 120_000,
) {
    private val json = Json { ignoreUnknownKeys = true }
    private val supervisor = SupervisorJob()
    private val scope = CoroutineScope(supervisor + Dispatchers.IO)

    // Calls running, by id, so SCANCORE can cancel one it no longer needs.
    private val running = java.util.concurrent.ConcurrentHashMap<String, Job>()

    // Questions to SCANCORE waiting for their answer, and where lines to it go.
    private val asks = java.util.concurrent.ConcurrentHashMap<Long, java.util.concurrent.CompletableFuture<JsonElement>>()
    private val nextAsk = java.util.concurrent.atomic.AtomicLong(1)

    @Volatile
    private var outbound: ((String) -> Unit)? = null

    // Asks SCANCORE something and blocks until it answers; null when it does
    // not within `timeoutMs` or nothing has been said to the host yet.
    fun ask(
        method: String,
        params: JsonObject,
        timeoutMs: Long,
    ): JsonElement? {
        val send = outbound ?: return null
        val id = nextAsk.getAndIncrement()
        val answer = java.util.concurrent.CompletableFuture<JsonElement>()
        asks[id] = answer
        return try {
            send(
                buildJsonObject {
                    put("ask", id)
                    put("method", method)
                    put("params", params)
                }.toString(),
            )
            answer.get(timeoutMs, java.util.concurrent.TimeUnit.MILLISECONDS)
        } catch (_: Exception) {
            null
        } finally {
            asks.remove(id)
        }
    }

    // Over a pipe (desktop helper process).
    fun serve(
        input: BufferedReader,
        output: Writer,
    ) {
        val reply = { text: String ->
            synchronized(output) {
                output.write(text)
                output.write("\n")
                output.flush()
            }
        }
        while (true) {
            val line = input.readLine() ?: return finish()
            if (!submit(line, reply)) return
        }
    }

    // One request line; its answer goes to `reply` (on another thread).
    // Returns false after a shutdown request. The Android app calls this
    // over JNI.
    fun submit(
        line: String,
        reply: (String) -> Unit,
    ): Boolean {
        if (line.isBlank()) return true
        outbound = reply
        val request =
            runCatching { json.parseToJsonElement(line).jsonObject }.getOrElse {
                reply(error(JsonNull, "The host could not read a request.", 400).toString())
                return true
            }
        val id = request["id"] ?: JsonNull
        val method = request["method"]?.jsonPrimitive?.content.orEmpty()
        val params = request["params"] as? JsonObject ?: JsonObject(emptyMap())
        when (method) {
            "shutdown" -> {
                finish()
                reply(buildJsonObject { put("id", id); put("result", JsonNull) }.toString())
                return false
            }
            // SCANCORE's answer to an ask(); not answered itself.
            "answer" -> {
                params["ask"]?.jsonPrimitive?.content?.toLongOrNull()?.let { asks[it]?.complete(params["result"] ?: JsonNull) }
                return true
            }
            // Not answered: the caller stopped waiting already.
            "cancel" -> {
                params["id"]?.jsonPrimitive?.content?.let { running.remove(it)?.cancel() }
                return true
            }
        }
        val key = id.toString().trim('"')
        val job =
            scope.launch(start = kotlinx.coroutines.CoroutineStart.LAZY) {
                val answer =
                    try {
                        val result = withTimeout(callTimeoutMs) { handler(method, params) }
                        buildJsonObject {
                            put("id", id)
                            put("result", result)
                        }
                    } catch (e: kotlinx.coroutines.CancellationException) {
                        if (e !is TimeoutCancellationException) return@launch
                        failure(id, e)
                    } catch (e: Throwable) {
                        failure(id, e)
                    }
                running.remove(key)
                reply(answer.toString())
            }
        running[key] = job
        job.start()
        return true
    }

    // Lets calls in flight answer (a few seconds at most) before the host
    // stops, at a shutdown or when SCANCORE closes the pipe.
    private fun finish() {
        runBlocking {
            withTimeoutOrNull(5_000) { supervisor.children.toList().joinAll() }
        }
    }

    private fun error(
        id: JsonElement,
        message: String,
        status: Int,
        verifyUrl: String? = null,
        verifyAgent: String? = null,
    ) = buildJsonObject {
        put("id", id)
        put(
            "error",
            buildJsonObject {
                put("message", message)
                put("status", status)
                if (verifyUrl != null) put("verifyUrl", verifyUrl)
                if (verifyAgent != null) put("verifyAgent", verifyAgent)
            },
        )
    }

    // Errors in words the user can act on; never a stack trace.
    private fun failure(
        id: JsonElement,
        e: Throwable,
    ): JsonObject {
        // The first cause of a kind anywhere in the chain: on Android a
        // failed lookup is a GaiException under the UnknownHostException.
        val chain = generateSequence(e) { it.cause }.toList()
        val root = chain.last()
        val browserCheck = chain.firstNotNullOfOrNull { it as? BrowserCheckException }
        val http = chain.firstNotNullOfOrNull { it as? HttpException }
        val unknownHost = chain.firstNotNullOfOrNull { it as? UnknownHostException }
        val io = chain.firstNotNullOfOrNull { it as? IOException }
        return when {
            e is HostError -> error(id, e.message, e.status, e.verifyUrl)
            browserCheck != null -> error(id, browserCheck.message ?: "Browser check", 403, browserCheck.url, browserCheck.userAgent)
            http != null -> error(id, "The site answered HTTP ${http.code}.", http.code)
            e is TimeoutCancellationException || chain.any { it is SocketTimeoutException } -> error(id, "The site took too long to respond.", 504)
            unknownHost != null -> error(id, "Could not find ${hostName(unknownHost.message)}. Check your connection.", 502)
            io != null -> error(id, "Could not reach the site (${io.message ?: io.javaClass.simpleName}).", 502)
            root is UnsupportedOperationException -> error(id, root.message ?: "The extension cannot do that.", 501)
            else -> {
                // Not something the reader can act on: the trace goes to the log.
                e.printStackTrace()
                error(id, "The extension failed: ${root.message ?: root.javaClass.simpleName}", 500)
            }
        }
    }

    // "fixture.test: Name or service not known" (Java) or 'Unable to resolve
    // host "fixture.test": …' (Android).
    private fun hostName(message: String?): String =
        message?.let { Regex("\"([^\"]+)\"").find(it)?.groupValues?.get(1) ?: it.substringBefore(':').trim() }
            ?.ifEmpty { null } ?: "the site"
}

// A failure with a message for the user.
class HostError(
    override val message: String,
    val status: Int = 500,
    val verifyUrl: String? = null,
) : Exception(message)

fun JsonObject.string(key: String): String =
    this[key]?.jsonPrimitive?.content ?: throw HostError("The request has no $key.", 400)

fun JsonObject.optString(key: String): String? = (this[key] as? kotlinx.serialization.json.JsonPrimitive)?.content

fun JsonObject.int(
    key: String,
    fallback: Int,
): Int = runCatching { this[key]!!.jsonPrimitive.int }.getOrDefault(fallback)
