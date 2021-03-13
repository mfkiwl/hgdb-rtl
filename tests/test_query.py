from ooze import Ooze, RTL, VCD, Port, get_value, VCDSignal, GenericQueryObject, QueryArray


def test_join(get_vector_file):
    vcd_file = get_vector_file("test_vcd.vcd")
    vcd = VCD(vcd_file)
    o = Ooze()
    o.add_source(vcd)
    sv_file = get_vector_file("test_vcd.sv")
    rtl = RTL()
    rtl.add_file(sv_file)
    o.add_source(rtl)
    signals = o.select(Port)
    value = get_value(10)
    values = o.select(VCDSignal).map(value)
    assert len(signals) == 3
    res = signals.join(values, "path")
    assert len(res) == 3
    a = res.where(name="a")
    assert a.value == 2


def test_type_conversion():
    o = Ooze()
    obj = {"a": 1, "b": "2"}
    g_o = o.object(obj)
    assert g_o.a == 1
    assert g_o.b == "2"
    array = o.array([g_o])
    assert len(array) == 1
    assert array[0] == g_o


def test_sequence():
    o = Ooze()
    array = []
    for i in range(10):
        array.append(o.object({"a": i, "b": i + 1}))
    array = o.array(array)
    items = array
    array = array.seq(items, lambda pre, after: pre.a == after.b)
    assert len(array) == 9
    array = array.seq(items, lambda pre, after: pre[-1].a == after.b)
    assert len(array) == 8
    # [2, 1, 0]  [3, 2, 1]  [4, 3, 2]
    assert array[2][-1].a == 2


if __name__ == "__main__":
    from conftest import get_vector_file_fn
    test_join(get_vector_file_fn)
