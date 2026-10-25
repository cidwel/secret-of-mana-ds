package org.somdecomp.companion;

/** Plain main-style JVM test of RunKeys (gradle is offline here).  Run: android/tests/run_thor_tests.sh */
public final class RunKeysTest {
    static int checks, bad;
    static void check(boolean c, String what) { checks++; if (!c) { bad++; System.out.println("FAIL " + what); } }

    public static void main(String[] a) {
        check(RunKeys.signature(null).isEmpty(), "null");
        check(RunKeys.signature("").isEmpty(), "empty");
        check(RunKeys.signature("{\"options\":{}}").isEmpty(), "defaults");
        check(RunKeys.signature(" { \"options\" : { } } ").isEmpty(), "defaults with spaces");
        String s1 = RunKeys.signature("{\"options\":{\"chest\":true}}"), s2 = RunKeys.signature("{\"options\": {\"chest\": true}}"), s3 = RunKeys.signature("{\"options\":{\"chest\":false}}");
        check(s1.length() == 8 && s1.matches("[0-9a-f]{8}"), "shape " + s1);
        check(s1.equals(s2), "whitespace-insensitive");
        check(!s1.equals(s3), "content-sensitive");
        check(RunKeys.originalBase("0123456789abcdef0123").equals("orig-0123456789ab"), "orig");
        check(RunKeys.generatedBase("rando-1/2").equals("gen-rando-1_2"), "gen sanitises");
        check(!RunKeys.originalBase("aaaaaaaaaaaaaaaa").equals(RunKeys.generatedBase("aaaaaaaaaaaaaaaa")), "namespaces differ");
        check(RunKeys.hex(RunKeys.sha256("abc".getBytes()), 4).equals("ba7816bf"), "sha256 vector");
        System.out.println("RunKeys: " + checks + " checks, " + bad + " failures");
        if (bad != 0) System.exit(1);
    }
}
