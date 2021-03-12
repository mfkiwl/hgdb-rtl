from ooze import Log, Ooze, LogPrintfParser, LogItem
import tempfile
import os


def test_log_parsing():
    with tempfile.TemporaryDirectory() as temp:
        file = os.path.join(temp, "test.log")
        with open(file, "w+") as f:
            for i in range(100):
                f.write("@{0} a.b.c: 0x{0:08X}\n".format(i))
        parser = LogPrintfParser("@%t %m: 0x%08X", ["module", "value"])
        log = Log()
        log.add_file(file, parser)
        o = Ooze()
        o.add_source(log)
    res = o.select(LogItem)
    assert res[42].time == 42
    assert res[42].value == 42
    # selecting using parser types
    res = o.select(parser.TYPE)
    assert len(res) == 100
    assert res[43].value == 43


if __name__ == "__main__":
    test_log_parsing()
