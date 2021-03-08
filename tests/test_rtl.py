from ooze import Instance, Ooze, RTL, Variable, Port, inside, like, source, source_of


def setup_source(filename, get_vector_file):
    filename = get_vector_file(filename)
    rtl = RTL()
    rtl.add_file(filename)
    o = Ooze()
    o.add_source(rtl)
    return o


def test_instance_select(get_vector_file):
    o = setup_source("test_instance_select.sv", get_vector_file)
    lst = o.select(Instance)
    assert len(lst) == 11
    python_list = list(lst)
    assert str(lst) == str(python_list)


def test_instance_where(get_vector_file):
    o = setup_source("test_instance_select.sv", get_vector_file)
    result = o.select(Instance).where(lambda x: x.definition == "mod1")
    assert len(result) == 4


def test_instance_select_attr(get_vector_file):
    o = setup_source("test_instance_select.sv", get_vector_file)
    result = o.select(Instance).select("path")
    assert len(result) == 11
    result = [r.path for r in result]
    assert "top.inst6.inst4.inst2" in result


def test_var_select(get_vector_file):
    o = setup_source("test_variable_select.sv", get_vector_file)
    result = o.select(Variable)
    assert len(result) == 7
    result = result.where(lambda x: x.name == "a")
    assert len(result) == 2


def test_type_select(get_vector_file):
    o = setup_source("test_variable_select.sv", get_vector_file)
    result = o.select(Variable, Instance)
    assert len(result) == 2
    result = result.select(Variable)
    assert len(result) == 7


def test_rtl_bind(get_vector_file):
    o = setup_source("test_variable_select.sv", get_vector_file)
    obj = o.object({"path": "top.inst"})
    inst = o.bind(obj, Instance)
    assert inst.definition == "mod1"
    obj = o.object({"path": "top.inst.d"})
    var = o.bind(obj, Variable)
    assert var.name == "d"
    obj = o.object({"path": "top.inst.a"})
    port = o.bind(obj, Port)
    assert port.name == "a"


def test_inside_op(get_vector_file):
    o = setup_source("test_inside_op.sv", get_vector_file)
    leaf = o.select(Instance).where(definition="mod1")
    inst = o.select(Instance).where(path="top.inst")
    res = o.select(Instance).where(inside(inst))
    assert res == leaf


def test_instance_select_pattern(get_vector_file):
    o = setup_source("test_instance_select_pattern.sv", get_vector_file)
    res = o.select(Instance).where(name=like(r"inst\d"))
    assert len(res) == 2
    res = o.select(Instance).where(name=like(r"inst\d{2,}"))
    assert res.path == "top.inst12"


def test_port_source(get_vector_file):
    o = setup_source("test_port_source.sv", get_vector_file)
    res = o.select(Variable).map(source)
    assert len(res) == 2
    res = o.select(Variable).where(path="top.inst1.a").map(source)
    assert res.path == "top.inst2"

    # test source of filter
    a = o.select(Port).where(path="top.inst1.a")
    res = o.select(Instance).where(source_of(a))
    assert res.path == "top.inst2"


if __name__ == "__main__":
    from conftest import get_vector_file_fn
    test_rtl_bind(get_vector_file_fn)

