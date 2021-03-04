import os
from ooze import Instance, Ooze, RTL


def setup_source(filename):
    filename = get_vector_file(filename)
    rtl = RTL()
    rtl.add_file(filename)
    o = Ooze()
    o.add_source(rtl)
    return o


def get_vector_file(filename):
    vector_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "vectors")
    f = os.path.join(vector_dir, filename)
    assert os.path.exists(f)
    return f


def test_instance_select():
    o = setup_source("test_instance_select.sv")
    lst = o.select(Instance)
    assert len(lst) == 11
    python_list = list(lst)
    assert str(lst) == str(python_list)


def test_instance_where():
    o = setup_source("test_instance_select.sv")
    result = o.select(Instance)
    result = result.where(lambda x: x.definition == "mod1")
    assert len(result) == 4


if __name__ == "__main__":
    test_instance_where()
