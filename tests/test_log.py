from ooze import Log, Ooze, LogPrintfParser, LogItem, LogFormatParser, ParsedLogItem, Transaction, to_transaction,\
    Transactions
import tempfile
import os


def setup_display_parsing(temp):
    file = os.path.join(temp, "test.log")
    with open(file, "w+") as f:
        for i in range(100):
            f.write("@{0} a.b.c: 0x{0:08X}\n".format(i))
    parser = LogPrintfParser("@%t %m: 0x%08X", ["module", "value"])
    log = Log()
    log.add_file(file, parser)
    o = Ooze()
    o.add_source(log)
    return o, parser


def test_log_parsing():
    with tempfile.TemporaryDirectory() as temp:
        o, parser = setup_display_parsing(temp)
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


def test_transaction():
    with tempfile.TemporaryDirectory() as temp:
        o, parser = setup_display_parsing(temp)
    items = o.select(parser.TYPE)
    seq = items.seq(items, lambda pre, after: (pre.value + 1) == after.value)
    seq = seq.seq(items, lambda pre, after: (pre[-1].value + 1) == after.value)

    class TransactionMapper:
        def __init__(self):
            self.id_count = 0

        def get_transaction(self, obj):
            t = Transaction(self.id_count, obj)
            self.id_count += 1
            return t

    tm = TransactionMapper()

    def get_transaction(obj):
        return tm.get_transaction(obj)

    transactions = seq.map(get_transaction)
    assert transactions[42].id == 42
    assert transactions[42].duration == 2


def test_seq_optimized():
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
    items = Transactions(res.map(to_transaction))
    res = items.seq(items, lambda a, b: (a.a + 1) == b.a, 10)
    res = res.seq(items, lambda a, b: (a[-1].a + 1) == b.a, 10)
    assert res[42][-1].a == 44


if __name__ == "__main__":
    test_seq_optimized()
