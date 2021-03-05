import os
from ooze import Instance, Ooze, RTL, Variable, inside


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
    result = o.select(Instance).where(lambda x: x.definition == "mod1")
    assert len(result) == 4


def test_instance_select_attr():
    o = setup_source("test_instance_select.sv")
    result = o.select(Instance).select("path")
    assert len(result) == 11
    assert "top.inst6.inst4.inst2" in result


def test_var_select():
    o = setup_source("test_variable_select.sv")
    result = o.select(Variable)
    assert len(result) == 7
    result = result.where(lambda x: x.name == "a")
    assert len(result) == 2


def test_type_select():
    o = setup_source("test_variable_select.sv")
    result = o.select(Variable, Instance)
    assert len(result) == 2
    result = result.select(Variable)
    assert len(result) == 7


def test_inside_op():
    o = setup_source("test_inside_op.sv")
    top = o.select(Instance).where(definition="top")
    inst = o.select(Instance).where(path="top.inst.inst")
    res = o.select(Instance).where(inside(top))
    print(res)


if __name__ == "__main__":
    test_inside_op()
