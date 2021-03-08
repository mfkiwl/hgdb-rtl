from ooze import Ooze, VCD, VCDSignal, get_value, pre_value


def test_vcd_read(get_vector_file):
    vcd_file = get_vector_file("test_vcd.vcd")
    vcd = VCD(vcd_file)
    o = Ooze()
    o.add_source(vcd)
    res = o.select(VCDSignal)
    assert len(res) == 3 * 2
    value = get_value(10)
    res = res.map(value)
    assert int(res[1]) == 2


def test_vcd_pre_value(get_vector_file):
    vcd_file = get_vector_file("test_vcd.vcd")
    vcd = VCD(vcd_file)
    o = Ooze()
    o.add_source(vcd)
    res = o.select(VCDSignal)
    value = get_value(20)
    res = res.map(value)
    print(res)
    res = res.map(pre_value)
    print(res)


if __name__ == "__main__":
    from conftest import get_vector_file_fn
    test_vcd_pre_value(get_vector_file_fn)
