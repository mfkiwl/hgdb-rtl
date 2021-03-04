import os
from ooze import Instance, Ooze, RTL


def get_vector_file(filename):
    vector_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "vectors")
    f = os.path.join(vector_dir, filename)
    assert os.path.exists(f)
    return f


def test_instance_select():
    filename = get_vector_file("test_instance_select.sv")
    rtl = RTL()
    rtl.add_file(filename)
    o = Ooze()
    o.add_source(rtl)
    lst = o.select(Instance)
    print(len(lst))
    print(lst)


if __name__ == "__main__":
    test_instance_select()
