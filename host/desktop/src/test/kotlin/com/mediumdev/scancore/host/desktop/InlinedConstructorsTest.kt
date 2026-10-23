package com.mediumdev.scancore.host.desktop

import com.googlecode.d2j.dex.writer.DexFileWriter
import com.googlecode.d2j.reader.DexFileReader
import com.googlecode.d2j.smali.Smali
import com.googlecode.dex2jar.tools.BaksmaliBaseDexExceptionHandler
import com.googlecode.d2j.dex.Dex2jar
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import java.net.URLClassLoader
import java.nio.file.Files

// R8's constructor shortcuts, as Keiyoushi's builds have them, must load
// on Java's verifier after conversion.
class InlinedConstructorsTest {
    private fun classes(vararg smali: String): Map<String, Class<*>> {
        val writer = DexFileWriter()
        smali.forEachIndexed { i, text -> Smali.smaliFile("c$i.smali", text, writer) }
        writer.visitEnd()
        val jar = Files.createTempFile("inlined", ".jar")
        val handler = BaksmaliBaseDexExceptionHandler()
        Dex2jar
            .from(InlinedConstructors.fix(DexFileReader(writer.toByteArray())))
            .withExceptionHandler(handler)
            .reUseReg(false)
            .topoLogicalSort()
            .skipDebug(true)
            .noCode(false)
            .dontSanitizeNames(true)
            .computeFrames(true)
            .to(jar)
        assertFalse(handler.hasException())
        val loader = URLClassLoader(arrayOf(jar.toUri().toURL()), null)
        return listOf("Base", "Mid", "Leaf", "Maker").associateWith { runCatching { loader.loadClass(it) }.getOrNull() }.filterValues { it != null }.mapValues { it.value!! }
    }

    private val base =
        """
        .class public abstract LBase;
        .super Ljava/lang/Object;
        .method public constructor <init>()V
            .registers 1
            invoke-direct {p0}, Ljava/lang/Object;-><init>()V
            return-void
        .end method
        """

    // Komga's `a extends d`: the constructor calls Object's, skipping d's.
    @Test
    fun constructorSkippingItsSuperclass() {
        val leaf =
            """
            .class public final LLeaf;
            .super LBase;
            .field public final value:Ljava/lang/String;
            .method public constructor <init>(Ljava/lang/String;)V
                .registers 2
                invoke-direct {p0}, Ljava/lang/Object;-><init>()V
                iput-object p1, p0, LLeaf;->value:Ljava/lang/String;
                return-void
            .end method
            """
        val leafClass = classes(base, leaf).getValue("Leaf")
        val made = leafClass.getConstructor(String::class.java).newInstance("ok")
        assertEquals("ok", leafClass.getField("value").get(made))
    }

    // The same with a class between that lost its constructor, and the
    // older case: `new Leaf` calling Base's constructor directly.
    @Test
    fun missingConstructorsAreAdded() {
        val mid =
            """
            .class public abstract LMid;
            .super LBase;
            """
        val leaf =
            """
            .class public final LLeaf;
            .super LMid;
            .method public constructor <init>(I)V
                .registers 2
                invoke-direct {p0}, LBase;-><init>()V
                return-void
            .end method
            """
        val maker =
            """
            .class public final LMaker;
            .super Ljava/lang/Object;
            .method public static make()Ljava/lang/Object;
                .registers 1
                new-instance v0, LLeaf;
                invoke-direct {v0}, LBase;-><init>()V
                return-object v0
            .end method
            """
        val loaded = classes(base, mid, leaf, maker)
        assertEquals("Leaf", loaded.getValue("Leaf").getConstructor(Int::class.javaPrimitiveType).newInstance(1).javaClass.name)
        assertEquals("Leaf", loaded.getValue("Maker").getMethod("make").invoke(null).javaClass.name)
    }
}
