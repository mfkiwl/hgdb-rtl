from ooze import Log, Ooze, LogPrintfParser, LogItem, LogFormatParser, ParsedLogItem
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


class CustomParser(LogFormatParser):
    def __init__(self):
        LogFormatParser.__init__(self)
        self.set_format(a=int, b=str, c=float)

    def parse(self, content):
        tokens = content.split(" ")
        t = int(tokens[0])
        a = int(tokens[1])
        b = tokens[2]
        c = float(tokens[3])
        item = ParsedLogItem(self, a=a, b=b, c=c)
        item.time = t
        return item


def test_custom_log_parser():
    with tempfile.TemporaryDirectory() as temp:
        file = os.path.join(temp, "test.log")
        with open(file, "w+") as f:
            for i in range(100):
                f.write("{0} {0} {0} {1}\n".format(i, float(i)))
        parser = CustomParser()
        log = Log()
        log.add_file(file, parser)
        o = Ooze()
        o.add_source(log)
    res = o.select(parser.TYPE)
    assert res[42].time == 42
    assert res[42].c == 42.0


if __name__ == "__main__":
    test_custom_log_parser()
