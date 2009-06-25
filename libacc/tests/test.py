#
# Test the acc compiler

import unittest
import subprocess
import os

def compile(args):
    proc = subprocess.Popen(["acc"] + args, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    result = proc.communicate()
    return result

def runCmd(args):
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    result = proc.communicate()
    return result[0].strip()

def which(item):
    return runCmd(["which", item])

def fileType(item):
    return runCmd(["file", item])

def outputCanRun():
    ft = fileType(which("acc"))
    return ft.find("ELF 32-bit LSB executable, Intel 80386") >= 0

def adb(args):
    return runCmd(["adb"] + args)

gArmInitialized = False

def setupArm():
    if gArmInitialized:
        return
    print "Setting up arm"
    adb(["remount"])
    adb(["shell", "rm", "/system/bin/acc"])
    adb(["shell", "mkdir", "/system/bin/accdata"])
    adb(["shell", "mkdir", "/system/bin/accdata/data"])
    # Clear out old data TODO: handle recursion
    adb(["shell", "rm", "/system/bin/accdata/data/*"])
    # Copy over data
    for root, dirs, files in os.walk("data"):
        for d in dirs:
            adb(["shell", "mkdir", os.path.join(root, d)])
        for f in files:
            adb(["push", os.path.join(root, f), os.path.join("/system/bin/accdata", root, f)])
    # Copy over compiler
    adb(["sync"])
    gArmInitialied = True

def compileArm(args):
    setupArm()
    proc = subprocess.Popen(["adb", "shell", "/system/bin/acc"] + args, stdout=subprocess.PIPE)
    result = proc.communicate()
    return result[0].replace("\r","")

def compare(a, b):
    if a != b:
        firstDiff = firstDifference(a,b)
        print "Strings differ at character", firstDiff, a[firstDiff], b[firstDiff]

def firstDifference(a, b):
    commonLen = min(len(a), len(b))
    for i in xrange(0, commonLen):
        if a[i] != b[i]:
            return i
    return commonLen

class TestACC(unittest.TestCase):
 
    def compileCheck(self, args, stdErrResult, stdOutResult=""):
        out, err = compile(args)
        compare(out, stdOutResult)
        compare(err, stdErrResult)
        self.assertEqual(out, stdOutResult)
        self.assertEqual(err, stdErrResult)

    def compileCheckArm(self, args, result):
        self.assertEqual(compileArm(args), result)

    def testCompileReturnVal(self):
        self.compileCheck(["data/returnval-ansi.c"], "") 

    def testCompileReturnVal(self):
        self.compileCheck(["data/otcc-ansi.c"], "")

    def testRunReturnVal(self):
        self.compileCheck(["-R", "data/returnval-ansi.c"],
		"Executing compiled code:\nresult: 42\n")
    def testRunOTCCANSI(self):
        self.compileCheck(["-R", "data/otcc-ansi.c", "data/returnval.c"], 
            "Executing compiled code:\notcc-ansi.c: About to execute compiled code:\natcc-ansi.c: result: 42\nresult: 42\n")

    def testRunOTCCANSI2(self):
        self.compileCheck(["-R", "data/otcc-ansi.c", "data/otcc.c", "data/returnval.c"], 
            "Executing compiled code:\notcc-ansi.c: About to execute compiled code:\notcc.c: about to execute compiled code.\natcc-ansi.c: result: 42\nresult: 42\n")

    def testRunConstants(self):
        self.compileCheck(["-R", "data/constants.c"],
            "Executing compiled code:\nresult: 12\n",
            "0 = 0\n010 = 8\n0x10 = 16\n'\\a' = 7\n'\\b' = 8\n'\\f' = 12\n'\\n' = 10\n'\\r' = 13\n'\\t' = 9\n'\\v' = 11\n'\\\\' = 92\n'\\'' = 39\n" +
            "'\\\"' = 34\n'\\?' = 63\n'\\0' = 0\n'\\1' = 1\n'\\12' = 10\n'\\123' = 83\n'\\x0' = 0\n'\\x1' = 1\n'\\x12' = 18\n'\\x123' = 291\n'\\x1f' = 31\n'\\x1F' = 31\n")

    def testArmRunReturnVal(self):
        self.compileCheckArm(["-R", "/system/bin/accdata/data/returnval-ansi.c"],
            "Executing compiled code:\nresult: 42\n")

if __name__ == '__main__':
    if not outputCanRun():
        print "Many tests are expected to fail, because acc is not a 32-bit x86 Linux executable."
    unittest.main()

