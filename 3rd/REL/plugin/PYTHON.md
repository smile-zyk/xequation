# REL Python 插件 — 使用指南

> **Status**: Updated
> **Date**: 2026-08-16
> **Scope**: `rel_runtime` 的 Python 嵌入接口,面向**用户**(如何加载 Python 插件、注册函数、读写数据)。

---

## 1. 概览

REL 支持用 Python 扩展功能:在运行时加载 `.py` 插件,插件里用 `rel.register_function(...)`
注册新函数,之后这些函数和内置函数一样,可以在 REL 表达式、REPL、或其它插件里调用。

核心入口只有三个(C++ 侧,`Environment`):

```cpp
rel::Environment::LoadPython("my_plugins.py");   // 加载一个 .py 插件文件
rel::Environment::ExecPython("print(1+1)");      // 执行一段 Python 代码
rel::Environment::IsPythonAvailable();           // 是否编译了 Python 支持
```

> **解释器生命周期由宿主管理**:`rel_runtime` 不再惰性创建解释器。宿主
> (`rel.exe` / 测试)必须在加载插件前通过 `rel_python_env` 静态库初始化
> 嵌入式 Python 环境(见 §2.1),退出前按顺序清理(见 §2.2)。

Python 脚本里 `import rel` 即可访问整个运行时 API。

---

## 2. 构建

Python 支持默认开启(`BUILD_PYTHON=ON`)。需要零外部依赖的 `rel_runtime`(只依赖 xdataset)时,用 `-DBUILD_PYTHON=OFF` 关闭:

```bash
cmake -B build                  # 默认开启 Python
cmake -B build -DBUILD_PYTHON=OFF   # 关闭 Python,rel_runtime 零外部依赖
cmake --build build
```

前置依赖:

- **pybind11** —— `pip install pybind11`(构建脚本会用 `python3 -m pybind11 --cmakedir` 自动定位,无需手动 `pybind11_DIR`)。
- **CPython 开发头文件** —— 随 Python 安装。
- **numpy**(运行时可选)—— 只有用到 `np.asarray(...)` 互操作时才需要。

> numpy 互操作走的是 Python **buffer 协议**,编译期**不需要** numpy 头文件。

---

## 2.1 嵌入式 Python 环境(rel_python_env)

解释器的配置与生命周期独立在静态库 `rel_python_env`(`src/python_env`,命名空间
`xequation::python`,只用 CPython C API,不依赖 pybind11):

```cpp
namespace xequation::python {

struct PyEnvConfig {
    std::string py_home;                     // Python 安装前缀(含标准库)
    std::vector<std::string> lib_path_list;  // sys.path(非空时完全替代默认路径计算)
};

class PyEnvManager {
public:
    static void SetPyEnvConfig(const PyEnvConfig& config);  // 须在 Initialize 前调用
    static void SetDefaultPyEnvConfig();                    // CMake 注入的默认路径
    static const PyEnvConfig& GetPyEnvConfig();
    static void InitializePyEnv();   // 幂等;失败抛 std::runtime_error
    static void ShutdownPyEnv();     // 仅 finalize 本库创建的解释器
    static bool IsInitialized();
    static bool ManagePythonContext();
};

}  // namespace xequation::python
```

语义:

- `InitializePyEnv()` 幂等:若 `Py_IsInitialized()` 已为真(宿主自管理),
  记 `manage_python_context_ = false` 直接返回;否则由本库通过
  `PyConfig_InitPythonConfig` + `Py_InitializeFromConfig` 创建解释器并持有所有权。
- `ShutdownPyEnv()` 只在 `manage_python_context_ == true` 时 `Py_FinalizeEx()`,
  宿主自管理的解释器不受影响。
- `SetDefaultPyEnvConfig()` 使用 CMake 在构建期注入的路径(`sys.base_prefix`、
  stdlib、`lib-dynload`、`site-packages`),保证插件无论工作目录在哪都能
  `import` 标准库与 pip 包。注意:`module_search_paths_set = 1` 会**完全替代**
  CPython 的默认路径计算,所以列表必须完整。
- 配置必须在 `InitializePyEnv()` 之前设置,解释器存活期间再调用 `SetPyEnvConfig`
  会抛异常。

宿主侧典型用法(`rel.exe` 的 `main.cc`):

```cpp
xequation::python::PyEnvManager::SetDefaultPyEnvConfig();
xequation::python::PyEnvManager::InitializePyEnv();
// ... 运行 REPL / 加载插件 ...
rel::Environment::CleanupPythonState();            // 先释放回调注册表(GIL 下)
xequation::python::PyEnvManager::ShutdownPyEnv();  // 再 finalize(仅自管理时)
```

## 2.2 关闭顺序(重要)

`rel_runtime` 的回调注册表持有 `pybind11::function`(Python 对象引用)。
退出时必须先调用 `rel::Environment::CleanupPythonState()`(原 `ShutdownPython`,
已更名——它现在只清理回调注册表,不再 finalize 解释器),再调用
`PyEnvManager::ShutdownPyEnv()`。顺序反了会导致 Python 对象在解释器销毁后
才被析构,进程崩溃。

---

## 3. 快速开始

```python
# my_plugins.py
import numpy as np
import rel

def snr(args):
    signal = np.asarray(args["signal"])   # Value -> ndarray
    noise  = np.asarray(args["noise"])
    return 20.0 * np.log10(signal / noise)   # ndarray -> 自动转 Value

rel.register_function("snr", [
    rel.Param("signal"),
    rel.Param("noise"),
], snr)
```

```cpp
rel::Environment env;
rel::Environment::InitBuiltinFunctions();
env.LoadPython("my_plugins.py");   // 注册了 "snr"

// 之后在 REPL / 表达式 / C++ 里都能用:
rel::Value v = rel::Environment::CallFunction("snr",
    rel::Value::Real(1.0), rel::Value::Real(0.1));
```

REPL 里: `snr(signal, noise)` 直接可用。

---

## 4. 类型系统

`import rel` 下可见的类型:

| 类型 | 说明 |
|------|------|
| `Unit` | 物理单位 |
| `Measurement` | 带单位的标量 / 向量 / 矩阵(值类型) |
| `DataSeries` | 一列 `Measurement`,numpy 互操作的中转站 |
| `DataArray` | 多维数组 + 坐标轴 |
| `Value` | 统一入口:`Measurement` 或 `DataArray` 二选一 |
| `Block` / `BlockCreateInfo` | 数据块(自变量 + 因变量) |
| `Dataset` | 树形数据集 |
| `Param` / `ComputedParam` | 函数参数描述符 |

### 4.1 Unit

```python
u = rel.Unit()                      # 无量纲
u = rel.Unit.parse("GHz")           # 从 REL 词汇表解析
u.multiplier                        # 1e9
u.has_dimension()                   # True
u.same_dimension(rel.Unit())        # False
u3 = u * rel.Unit.parse("Hz")       # 量纲乘法
str(u)                              # "GHz"
```

### 4.2 Measurement

```python
m = rel.Measurement(3.14)                       # real scalar
m = rel.Measurement(42)                         # integer scalar
m = rel.Measurement("hello")                    # string scalar
m = rel.Measurement(True)                       # boolean scalar
m = rel.Measurement(3.14, rel.Unit.parse("GHz"))  # 带单位
m = rel.Measurement(np.array([1.0, 2.0, 3.0]))  # vector
m = rel.Measurement(np.array([[1,2],[3,4]]))    # matrix

# 便捷工厂
rel.Measurement.real(3.14, rel.Unit.parse("GHz"))
rel.Measurement.integer(42)
rel.Measurement.string("hello")
rel.Measurement.boolean(True)

m.data_kind        # "scalar" | "vector" | "matrix"
m.data_type        # "real" | "integer" | "complex" | "string" | "boolean"
m.unit             # Unit
m.element_count    # int
m.element_at(0)    # -> Measurement (保留 unit)
```

### 4.3 DataSeries

```python
ds = rel.DataSeries.from_array(np.array([1.0, 2.0, 3.0]))  # 唯一入口(拷贝)
len(ds)             # 行数
ds.unit / ds.data_type / ds.data_kind
ds[0]               # -> Measurement
ds.measurement_at(5)
ds.iloc(0, 10)      # 切片
```

### 4.4 DataArray

```python
da = block.GetOrCreateDataArray("Vout")
da = dataset.GetDataArray("Vout")     # 名称全局唯一时

da.data_kind        # "dependent" | "independent"
da.indep_names      # ["freq", "power"]
da.rank / da.flat_size

da.data             # -> DataSeries (因变量)
da.indep_data(1)    # 第 1 个自变量(1-based,最内层优先)
da.indep_data("freq")

da.select([0, 0])   # 多维索引(Equal 维度坍缩)
da.at([1])          # 单元格向量/矩阵索引
```

### 4.5 Value — 统一入口

```python
v = rel.Value.real(3.14)
v = rel.Value.integer(42)
v = rel.Value.string("hello")
v = rel.Value.boolean(True)
v = rel.Value.complex(1+2j)
v = rel.Value.array_real([1.0, 2.0, 3.0])
v = rel.Value.array_integer([1, 2, 3])
v = rel.Value.array_string(["a", "b", "c"])

v.is_measurement() / v.is_data_array() / v.is_scalar() / v.is_vector() / v.is_matrix()
v.data_kind / v.data_type / v.unit / v.rows / v.indep_names

m  = v.as_measurement()   # throw if DataArray
da = v.as_data_array()    # throw if Measurement

ds = v.data()             # -> DataSeries
```

---

## 5. numpy 互操作

统一用 `np.asarray(...)` 导出、`DataSeries.from_array(...)` 导入:

```python
import numpy as np
import rel

# 导出(real/int/complex 零拷贝;string 拷贝;bool 标量 -> np.bool_)
arr = np.asarray(measurement)   # scalar -> (), vector -> (W,), matrix -> (R,C)
arr = np.asarray(dataseries)    # scalar -> (N,), vector -> (N,W), matrix -> (N,R,C)
arr = np.asarray(value)         # 等价 np.asarray(value.data())
arr = np.asarray(dataarray)     # flat 视图

# 导入(拷贝)
ds = rel.DataSeries.from_array(np.array([1.0, 2.0, 3.0]))  # 1d -> scalar
ds = rel.DataSeries.from_array(np.zeros((10, 3)))          # 2d -> vector
ds = rel.DataSeries.from_array(np.zeros((10, 2, 2)))       # 3d -> matrix
```

| dtype | 支持 | 说明 |
|-------|:---:|------|
| `float64` / `int32` / `complex128` | ✅ | 零拷贝 |
| `bool` | ✅ | 标量导出为 `np.bool_`;数组按 0/1 int 处理 |
| `str` | ✅ | `__array__` 拷贝为 `<U` 数组 |

> **单位不会跨 numpy 往返**:`np.asarray` 只导出数值、不带单位;`from_array` 返回无量纲。
> 需保留单位时显式走 `Measurement` / `Value`。

---

## 6. 注册函数

### 6.1 register_function

```python
rel.register_function(name, params, fn)

# 示例
rel.register_function("my_add", [
    rel.Param("a"),
    rel.Param("b", default=rel.Value.integer(10)),
], my_python_fn)
```

回调签名:

```python
def my_python_fn(args: dict):
    """args: dict[str, Value] —— 参数名 -> Value"""
    v = args["a"]            # Value
    x = np.asarray(v)        # -> ndarray
    return x * 2             # 自动转 Value
```

返回值自动转换:

| Python 返回 | → Value |
|-------------|---------|
| `rel.Value` / `rel.Measurement` / `rel.DataArray` | 直通 |
| `np.ndarray`(1/2/3-d 数值) | `DataArray` / `Measurement` |
| `float` / `int` / `complex` / `str` / `bool` | 对应标量 |
| `list` / `tuple` | 转 ndarray 再转换 |

### 6.2 Param / ComputedParam

```python
rel.Param("a")                                   # 必需参数
rel.Param("b", default=rel.Value.integer(10))    # 静态默认值
rel.ComputedParam("c", default_fn)               # 计算默认值(只读前置参数)
```

### 6.3 查询与注销

```python
rel.function_names()             # 所有已注册函数名
rel.unregister_function("my_add")
```

---

## 7. 调用内置函数 / 常量

`rel` 模块通过懒加载(`__getattr__`)暴露所有已注册函数(内置、C++ 扩展、Python 插件)与常量:

```python
import rel

rel.sin(rel.Value.real(0.0))       # 内置函数
rel.db(da, 50, 50)                 # 位置参数
rel.db(r=r, z1=50, z2=75)          # 关键字参数
rel.indep(da, "freq")

rel.PI / rel.e / rel.c0 / rel.i    # 内置常量
```

> 每次调用都现查注册表,所以**后注册 / 重新注册的函数立刻生效**,无缓存失效问题。

---

## 8. 程序化构造 Dataset

```python
import numpy as np
import rel

freq = rel.DataSeries.from_array(np.linspace(1e9, 10e9, 100))
power = rel.DataSeries.from_array(np.array([-30, -20, -10, 0, 10]))
vout = rel.DataSeries.from_array(np.random.randn(100, 5))

info = rel.BlockCreateInfo(
    independents=[
        ("freq",  freq,  rel.RegularDim(100)),
        ("power", power, rel.RegularDim(5)),
    ],
    dependents=[("Vout", vout)],
)

ds = rel.Dataset("my_data")
ds.AddBlock("simulation/SP1", info)

da = ds.GetDataArray("simulation/SP1", "Vout")
print(da.rank, da.flat_size)   # 2 500
```

### Dataset 常用方法

```python
ds.GetBlock("simulation/SP1")        # -> Block
ds.GetDataArray("simulation/SP1", "Vout")
ds.GetDataArray("Vout")              # 唯一名快捷访问
ds.GetAllBlockPaths()
ds.RemoveBlock("simulation/SP1")
ds.RemoveGroup("simulation")
```

---

## 9. 错误处理

C++ 异常自动映射到 Python 异常:

| C++ 异常 | Python 异常 |
|----------|-------------|
| `std::out_of_range` | `IndexError` |
| `std::overflow_error` | `OverflowError` |
| `std::invalid_argument` / `std::domain_error` / `std::length_error` | `ValueError` |
| `boost::bad_get` | `TypeError` |
| `std::bad_alloc` | `MemoryError` |
| `std::runtime_error` | `RuntimeError` |

---

## 10. 注意事项

1. **`print` 实时同步**:嵌入解释器已启用 `sys.stdout` 行缓冲,`print(...)` 会立即输出到宿主控制台,无需手动 `flush=True`。
2. **插件隔离**:每个 `LoadPython` / `ExecPython` 用独立的 `globals` dict,脚本之间互不污染;`register_function` 是唯一跨插件通信渠道。
3. **`indep_data(索引)` 是 1-based**(最内层 = 1),与 C++ `DataArray::indep_data` 一致。
4. **`at` vs `select`**:`at` 是单元格向量/矩阵元素索引(标量数据会报错);`select` 是多维权(Equal 维度坍缩)。
5. **多线程**:函数注册表已加锁(`Environment::functions_mutex_`);`datasets_` / 常量表暂未加锁(常量表初始化后只读)。
6. **外部 `import rel` 不可用**:嵌入模式不产 `.pyd`,只能在宿主进程内 `import rel`。
7. **Windows DLL 搜索路径**:Python 3.8+ 加载扩展模块(`.pyd`)时不再搜索 `PATH`。`PyEnvManager::InitializePyEnv()` 会在初始化前用 `AddDllDirectory` 注册 CPython DLL 所在目录(`REL_PYTHON_DLL_DIR`,即 msys2 的 `mingw64/bin`)与所有 `sys.path` 目录,否则 numpy 等带原生依赖的包会报误导性的 "import from source directory" 错误。
