from ooze import Ooze, VCD, VCDSignal, get_value


def test_vcd_read(get_vector_file):
    vcd_file = get_vector_file("test_vcd.vcd")
    vcd = VCD(vcd_file)
    o = Ooze()
    o.add_source(vcd)
    res = o.select(VCDSignal)
    print(res)


if __name__ == "__main__":
    from conftest import get_vector_file_fn
    test_vcd_read(get_vector_file_fn)
