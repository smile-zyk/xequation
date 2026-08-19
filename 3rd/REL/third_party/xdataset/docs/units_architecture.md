# 单位系统架构（替代 llnl-units）

## 1. 概述

用自研的 UnitData（7 SI 维度位域）+ UnitRegistry（可扩展注册表）替代 llnl-units，保持 Unit 公开 API 完全不变。

### 文件清单

```
include/xdataset/
  unit_data.h        ← 新建: UnitData 核心结构 + 运算符
  unit_registry.h     ← 新建: UnitRegistry + ScalePrefix + PredefUnit 注册表
  unit.h              ← 改造: 删除 llnl-units 依赖, 改用 UnitData + double

src/
  unit.cc             ← 改造: parse/to_string/best_display 全部重写
  data_series.cc      ← 改造: 删除 units::convert, 简化为 v *= mult
  measurement.cc      ← 改造: 同上

CMakeLists.txt        ← 删除 find_package(llnl-units) 和 link
vcpkg.json            ← 删除 "llnl-units" 依赖
```

---

## 2. UnitData —— 7 SI 维度的整数指数向量

```cpp
// include/xdataset/unit_data.h

namespace xdataset {

struct UnitData {
    int8_t m   = 0;    // meter
    int8_t kg  = 0;    // kilogram
    int8_t s   = 0;    // second
    int8_t A   = 0;    // ampere
    int8_t K   = 0;    // kelvin       (保留, 当前 REL 未使用)
    int8_t mol = 0;    // mole         (保留)
    int8_t cd  = 0;    // candela      (保留)

    // ── 量纲运算 ──────────────────────────────────
    //
    // 乘法: 各维指数相加. 物理意义: m^a * m^b = m^(a+b)
    //
    UnitData operator*(const UnitData& o) const;

    // 除法: 各维指数相减
    //
    UnitData operator/(const UnitData& o) const;

    // 倒数: m^n → m^(-n). 等价格 {dim} / 1
    //
    UnitData inv() const;

    // 幂: 各维指数 × n
    //
    UnitData pow(int n) const;

    // ── 查询 ──────────────────────────────────────

    bool empty() const;        // 全零 = 无量纲
    bool operator==(const UnitData& o) const;
    bool operator!=(const UnitData& o) const;

    // ── 序列化 ────────────────────────────────────
    //
    // 用于 reverse-mapping 的 key 串.
    // 示例: {0,0,-1,0,0,0,0} → "s^-1"
    //       {2,1,-3,-1,0,0,0} → "kg*m^2*s^-3*A^-1"
    //
    std::string key() const;

    // 标准 hash 支持 (用于 unordered_map)
    //
    struct Hash {
        size_t operator()(const UnitData& d) const {
            // 将 7 个 int8_t 打包为 uint64_t 后 hash
        }
    };
};

} // namespace xdataset
```

**大小**: 7 × 1 byte = 7 bytes (`static_assert(sizeof(UnitData) == 7)`). 对齐后填 1 byte padding = 8 bytes.

**防溢出**: 当前 REL 场景最大指数范围约 [-4, +4], int8_t (-128~127) 远不会溢出. 对于乘除运算链, 可加 debug 断言.

---

## 3. 注册表 —— 分两类

### 3.1 Scale Prefix 表（全局常量）

```
┌─────────┬────────────┐
│ Prefix  │ Factor     │
├─────────┼────────────┤
│ T       │ 1e12       │
│ G       │ 1e9        │
│ M       │ 1e6        │
│ K       │ 1e3        │   ← 注意大小写: K=1e3, 大写
│ k       │ 1e3        │   ← k 也是 1e3, 小写
│ _       │ 1.0        │   ← 显式无量纲标记
│ m       │ 1e-3       │   ← m=milli, 而 meter 不是 prefix
│ u       │ 1e-6       │
│ n       │ 1e-9       │
│ p       │ 1e-12      │
│ f       │ 1e-15      │
│ a       │ 1e-18      │
└─────────┴────────────┘
```

按 name length 降序排列用于贪婪匹配: 优先匹配 `"M"` 再匹配 `"m"`, 避免 `"MHz"` 被 `"m"` 截走.

### 3.2 UnitRegistry —— 两类注册

```cpp
// include/xdataset/unit_registry.h

namespace xdataset {

class UnitRegistry {
public:
    // ─── 类型 A: 可缩放的基本单位 ─────────────────
    //
    // 注册后自动支持所有 scale prefix 的组合.
    //
    //   register_base("Hz", {0,0,-1,0,0,0,0});
    //
    //   自动支持: Hz, kHz, MHz, GHz, mHz, uHz, nHz, ...
    //
    void register_base(const std::string& name, const UnitData& dim);

    // 别名: 同一个量纲的另一个名字
    //
    //   register_base("Ohm", {...});
    //   register_alias("Ohms", "Ohm");
    //
    void register_alias(const std::string& alias,
                        const std::string& base_name);

    // ─── 类型 B: 预定义不可缩放单位 ───────────────
    //
    // 固定乘数, 拒绝任何 scale prefix.
    //
    //   register_predef("ft", 0.3048, {1,0,0,0,0,0,0});
    //
    //   parse("ft")  → Unit{0.3048, meter}
    //   parse("kft") → 抛异常
    //
    void register_predef(const std::string& name,
                         double mult,
                         const UnitData& dim);

    // ─── 查询接口 ─────────────────────────────────

    // 查类型 A (base name → dim)
    const UnitData* lookup_base(const std::string& name) const;

    // 查类型 B (predef name → {mult, dim})
    struct PredefEntry { double mult; UnitData dim; };
    const PredefEntry* lookup_predef(const std::string& name) const;

    // 反向查: dim → base name (优先返回类型 A 的 canonical name)
    //
    // 用于 to_string: canonicalize 后查回人类可读名
    const std::string* reverse_lookup(const UnitData& dim) const;
};

// 全局单例: REL 词汇表
UnitRegistry& rel_registry();

} // namespace xdataset
```

### 3.3 内置 REL 词汇表

```cpp
UnitRegistry& rel_registry() {
    static UnitRegistry r;

    // ═══════════════════════════════════════════════
    //  类型 A: 可缩放
    // ═══════════════════════════════════════════════

    r.register_base("meter", {1,0,0,0,0,0,0});
    r.register_alias("meters", "meter");
    r.register_alias("metre",  "meter");
    r.register_alias("metres", "meter");

    r.register_base("sec", {0,0,1,0,0,0,0});

    r.register_base("Hz",  { 0, 0,-1, 0,0,0,0});
    r.register_base("V",   { 2, 1,-3,-1,0,0,0});
    r.register_base("A",   { 0, 0, 0, 1,0,0,0});
    r.register_base("W",   { 2, 1,-3, 0,0,0,0});
    r.register_base("Ohm", { 2, 1,-3,-2,0,0,0});
    r.register_alias("Ohms", "Ohm");
    r.register_base("S",   {-2,-1, 3, 2,0,0,0});
    r.register_base("F",   {-2,-1, 4, 2,0,0,0});
    r.register_base("H",   { 2, 1,-2,-2,0,0,0});

    // ═══════════════════════════════════════════════
    //  类型 B: 预定义 (不可缩放)
    // ═══════════════════════════════════════════════

    r.register_predef("cm",   1e-2,     {1,0,0,0,0,0,0});
    r.register_predef("mil",  2.54e-5,  {1,0,0,0,0,0,0});
    r.register_predef("mils", 2.54e-5,  {1,0,0,0,0,0,0});
    r.register_predef("in",   2.54e-2,  {1,0,0,0,0,0,0});
    r.register_predef("ft",   0.3048,   {1,0,0,0,0,0,0});
    r.register_predef("mi",   1609.344, {1,0,0,0,0,0,0});
    r.register_predef("nmi",  1852.0,   {1,0,0,0,0,0,0});
    r.register_predef("PHz",  1e15,     {0,0,-1,0,0,0,0});
    r.register_predef("dB",   1.0,      {0,0,0,0,0,0,0});

    return r;
}
```

---

## 4. Unit 类 —— 公开 API

```cpp
// include/xdataset/unit.h (改造后)

namespace xdataset {

struct UnitScale {
    double      scale;   // 显示值 = 原始值 × scale
    std::string name;    // 显示单位名
};

class XDATASET_API Unit {
public:
    // ── 构造 ──────────────────────────────────────
    Unit();                             // 默认无量纲
    static Unit None();                 // 别名, 同默认
    static Unit parse(const std::string& s);  // 解析 REL 字符串

    // ── 查询 ──────────────────────────────────────
    double multiplier()     const;
    bool   is_canonical()   const;  // mult==1.0 && 非仿射
    bool   is_affine()      const;  // 仿射单位标志 (当前始终 false)
    bool   has_dimension()  const;  // 有物理量纲
    bool   is_dimensionless()const; // canonical && !has_dimension
    bool   same_dimension(const Unit& o) const;  // 量纲等价

    // ── 转换 ──────────────────────────────────────
    Unit        canonicalized() const;     // mult=1, 纯量纲
    std::string to_string()     const;     // → "MHz", "meter", "ft" ...
    UnitScale   best_display(double v) const;

    // ── 量纲算术 ──────────────────────────────────
    Unit operator*(const Unit& o) const;   // dim 相乘, mult 忽略
    Unit operator/(const Unit& o) const;

    // ── 比较 ──────────────────────────────────────
    bool operator==(const Unit& o) const;
    bool operator!=(const Unit& o) const;

private:
    double   mult_ = 1.0;    // 乘数因子
    UnitData dim_;            // 量纲向量

    // 内部构造
    explicit Unit(double mult, UnitData dim);
};

} // namespace xdataset
```

**注意**: 与旧 API 相比:
- **删除**: `const units::precise_unit& raw()` — 不再暴露内部类型
- 其他所有公开方法签名不变

---

## 5. Parse 流程

```
┌─────────────────────────────────────────────────┐
│            Unit::parse(input_string)             │
├─────────────────────────────────────────────────┤
│                                                  │
│  ① 空串? → throw                                 │
│                                                  │
│  ② 查类型 B 表 (predef exact match)              │
│    命中 → 返回 Unit{predef.mult, predef.dim}     │
│                                                  │
│  ③ 贪婪剥离 scale prefix                         │
│    对每个 prefix (按 name length 降序):           │
│      if input starts_with(prefix.name):           │
│          remainder = input.drop(prefix.len)       │
│          break                                    │
│                                                  │
│  ④ 用 remainder 查类型 A 表                       │
│    命中 → 返回 Unit{prefix.factor, base.dim}     │
│                                                  │
│  ⑤ 步骤③未找到 prefix:                            │
│     整体直接查类型 A 表                            │
│     命中 → 返回 Unit{1.0, base.dim}              │
│                                                  │
│  ⑥ 全部失败 → throw invalid_argument              │
│                                                  │
└─────────────────────────────────────────────────┘
```

### 示例

| 输入 | 类型B | 剥离prefix | remainder | 类型A | 结果 |
|------|:--:|------|------|:--:|------|
| `"Hz"` | — | 无匹配 | `"Hz"` | ✅ | `{1.0, s⁻¹}` |
| `"MHz"` | — | `"M"`(1e6) | `"Hz"` | ✅ | `{1e6, s⁻¹}` |
| `"GHz"` | — | `"G"`(1e9) | `"Hz"` | ✅ | `{1e9, s⁻¹}` |
| `"kHz"` | — | `"k"`(1e3) | `"Hz"` | ✅ | `{1e3, s⁻¹}` |
| `"mHz"` | — | `"m"`(1e-3) | `"Hz"` | ✅ | `{1e-3, s⁻¹}` |
| `"ft"` | ✅ `{0.3048,m}` | — | — | — | `{0.3048, m}` |
| `"kft"` | ❌ 不在B表 | `"k"`(1e3) | `"ft"` | ❌ 不在A表 → 抛异常 |
| `"MdB"` | ❌ 不在B表 | `"M"`(1e6) | `"dB"` | ❌ 不在A表 → 抛异常 |
| `"M"` | ❌ 不在B表 | `"M"`(1e6) | `""` | — | `{1e6, dimensionless}` |
| `"mV"` | — | `"m"`(1e-3) | `"V"` | ✅ | `{1e-3, V}` |
| `"uF"` | — | `"u"`(1e-6) | `"F"` | ✅ | `{1e-6, F}` |

---

## 6. UnitData::key() —— 降级为基本量纲组合

当 `to_string()` 无法在注册表中找到匹配名时，降级输出原始 SI 量纲组合.
输出格式：

```
规则:
  - 按 m, kg, s, A, K, mol, cd 顺序遍历
  - 指数 == 0 → 跳过
  - 指数 == 1 → 只输出量纲名, 不加 ^1
  - 指数 != 1 → 输出 "名^(exp)"
  - 各分量用 "*" 连接
  - 全零 → 返回 "" (空串, 表示无量纲)

示例:
  {0,0,-1,0,0,0,0}        → "s^-1"
  {2,1,-3,-1,0,0,0}       → "kg*m^2*s^-3*A^-1"
  {1,0,0,0,0,0,0}         → "m"
  {0,0,0,0,0,0,0}         → ""          (无量纲)
  {0,0,0,0,1,0,0}         → "K"
  {0,0,0,1,0,0,0}         → "A"
```

实现:

```cpp
std::string UnitData::key() const {
    struct { const char* name; int8_t exp; } parts[] = {
        {"m", m}, {"kg", kg}, {"s", s}, {"A", A},
        {"K", K}, {"mol", mol}, {"cd", cd},
    };
    std::string r;
    for (const auto& p : parts) {
        if (p.exp == 0) continue;
        if (!r.empty()) r += '*';
        r += p.name;
        if (p.exp != 1) { r += '^'; r += std::to_string(p.exp); }
    }
    return r;
}
```

---

## 7. to_string 流程

```
┌─────────────────────────────────────────────────┐
│              Unit::to_string()                    │
├─────────────────────────────────────────────────┤
│                                                  │
│  ① canonical = canonicalized()                   │
│     (mult=1.0 的纯量纲)                           │
│                                                  │
│  ② pure scale prefix 只有 mult 没有 dim?          │
│     查 kScalePrefixes, 命中 → 返回 prefix 名     │
│     (如 mult=1e6, dim="") → "M")                │
│                                                  │
│  ③ reverse_lookup(canonical.dim)                 │
│     命中 → base_name (如 "Hz", "meter")          │
│     未命中 → dim.key() — 原始 SI 量纲组合         │
│              (如 "kg*m^2*s^-3*A^-1")             │
│                                                  │
│  ④ 如果 original mult == 1.0:                    │
│     直接返回 base_name 或 dim.key()               │
│                                                  │
│  ⑤ 匹配 original mult 到 scale prefix 表:         │
│     命中 → prefix + base_name                    │
│            (如 1e6 + "Hz" → "MHz")              │
│                                                  │
│  ⑥ 查类型 B 反向表:                               │
│     命中 → 返回 predef_name (如 "ft", "cm")      │
│                                                  │
│  ⑦ 最终 fallback:                                │
│     dim.key() 非空 → mult + "*" + dim.key()      │
│                     (如 "2.54e-05*m")            │
│     dim.key() 为空  → std::to_string(mult)       │
│                     (如 "1000")                  │
│                                                  │
└─────────────────────────────────────────────────┘
```

### 示例

| Unit | canonical dim | base_name | mult | 走的步骤 | 输出 |
|------|---------------|-----------|------|:--:|------|
| `{1e6, {s⁻¹}}` | `{s⁻¹}` | `"Hz"` | 1e6 | ⑤ prefix | `"MHz"` |
| `{1e9, {s⁻¹}}` | `{s⁻¹}` | `"Hz"` | 1e9 | ⑤ prefix | `"GHz"` |
| `{1.0, {s⁻¹}}` | `{s⁻¹}` | `"Hz"` | 1.0 | ④ canonical | `"Hz"` |
| `{0.3048, {m¹}}` | `{m¹}` | `"meter"` | 0.3048 | ⑥ B反向 | `"ft"` |
| `{1e-2, {m¹}}` | `{m¹}` | `"meter"` | 1e-2 | ⑥ B反向 | `"cm"` |
| `{2.54e-5, {m¹}}` | `{m¹}` | `"meter"` | 2.54e-5 | ⑥ B反向 | `"mil"` |
| `{1.0, {}}` | `{}` | — | 1.0 | ④ canonical | `""` (无量纲) |
| `{1e6, {}}` | `{}` | — | 1e6 | ② pure scale | `"M"` |

**降级示例** (registry 中无注册名时):

| Unit | dim.key() | mult | 走的步骤 | 输出 |
|------|-----------|------|:--:|------|
| `{1.0, {1,2,-3,0,0,0,0}}` | `"m*kg^2*s^-3"` | 1.0 | ④ fallback | `"m*kg^2*s^-3"` |
| `{1000, {0,1,0,0,0,0,0}}` | `"kg"` | 1000 | ⑦ fallback | `"1000*kg"` |
| `{5.0, {}}` | `""` | 5.0 | ⑦ fallback | `"5"` |
| `{1.0, {0,0,-1,0,0,0,0}}` | —(有注册`"Hz"`) | 1.0 | ④ canonical | `"Hz"` |

---

## 8. Unit::operator* / Unit::operator/ 

只对量纲层做运算, multiplier 始终设为 1.0. 这与现有行为完全一致:

```cpp
Unit Unit::operator*(const Unit& other) const {
    // 现有: return Unit(precise_unit(base_units() * other.base_units()));
    // 新:
    return Unit(1.0, dim_ * other.dim_);
}

Unit Unit::operator/(const Unit& other) const {
    return Unit(1.0, dim_ / other.dim_);
}
```

---

## 9. 调用方适配

### data_series.cc & measurement.cc

所有 `units::convert()` 和 `unit_.raw()` 的使用点:

```cpp
// 旧代码 (共 6 处):
v = affine ? units::convert(v, unit_.raw(), target.raw()) : v * mult;

// 新代码:
v *= mult;
```

因为 REL 词汇表中没有仿射单位（没有 °C、°F）, `is_affine()` 始终返回 `false`, 这个分支永远不会进入. 去掉后不仅简化代码, 还避免了一次函数调用.

### 删除 raw() 的影响

`raw()` 的唯一用途是传给 `units::convert()`. 既然 `units::convert` 被删除, 没有任何调用方需要 `raw()`.

---

## 10. 构建系统变更

### CMakeLists.txt

```diff
- find_package(llnl-units CONFIG REQUIRED)
  # 删除这行

- target_link_libraries(xdataset PUBLIC ... llnl-units::units ...)
+ target_link_libraries(xdataset PUBLIC ...)
  # 删除 llnl-units::units
```

### vcpkg.json

```diff
  "dependencies": [
-     "llnl-units",
      ...
  ],
```

---

## 11. 测试覆盖对照

所有现有 unit_test.cc 中的 60+ 测试在新系统下语义对等:

| 测试分类 | 测试数 | 覆盖的关键路径 |
|----------|:--:|------|
| 构造/解析 | 7 | `Unit()`, `None()`, `parse("Hz")`, `parse("meter")`, 非法串, 空串 |
| Scale factors | 13 | `parse("T")`→1e12, ..., `parse("a")`→1e-18, 纯数字解析 |
| Scale + unit | 6 | `parse("MHz")`→1e6·Hz, `parse("mA")`→1e-3·A, 等 |
| 预定义 | 8 | `parse("mil")`→2.54e-5·m, `parse("dB")`→无量纲, 等 |
| 别名 | 4 | `parse("meters")`==`parse("meter")`, 等 |
| 复合拒绝 | 1 | `parse("MHz/kOhm")` 抛异常 |
| 大小写 | 3 | `parse("MHz")`≠`parse("mHz")`, 等 |
| Canonicalize | 2 | `parse("cm").canonicalized()`→meter, `parse("MHz").canonicalized()`→Hz |
| SameDimension | 3 | `same_dimension(parse("meter"), parse("cm"))`==true |
| has_dimension | 3 | scale factors 无量纲, Hz/V 有量纲 |
| 乘/除 | 2 | `meter*Hz == meter/sec` |
| to_string | 10 | round-trip 7个基本单位, compound, canonical/non-canonical, aliases |
| best_display | 6 | W→mW/MW, Ohm→KOhm, Hz→GHz, MHz→no further scale |
| 相等/不等 | 3 | `==` / `!=` |
| 预定义拒绝scale | 1 | `parse("kin")`/`parse("Mmil")` 抛异常 |

---

## 12. 用户自定义扩展示例

```cpp
// 添加 cm (类型 B):
rel_registry().register_predef("cm", 1e-2, {1,0,0,0,0,0,0});

// 添加 帕斯卡 (类型 A, 可缩放):
rel_registry().register_base("Pa", {-1,1,-2,0,0,0,0});  // kg·m⁻¹·s⁻²
// 自动支持: Pa, kPa, MPa, GPa, mPa, ...

// 添加 摄氏度 (仿射单位, 类型 B):
// {0,0,0,0,1,0,0} = K, 需要额外标记 affine
rel_registry().register_predef("degC", 1.0, {0,0,0,0,1,0,0}, /*affine=*/true);
```

---

## 13. 实施顺序

```
Step 1  include/xdataset/unit_data.h         新建: UnitData 核心结构
Step 2  include/xdataset/unit_registry.h      新建: UnitRegistry + 词汇表
Step 3  include/xdataset/unit.h               改造: 删 llnl, 用新内部存储
Step 4  src/unit.cc                           重写: parse / to_string / best_display
Step 5  src/data_series.cc                    适配: 删除 units::convert
Step 6  src/measurement.cc                    适配: 同上
Step 7  CMakeLists.txt                        删除 llnl-units 依赖
Step 8  vcpkg.json                            删除 "llnl-units"
Step 9  编译 + 运行全部 165 个测试
```

每个 Step 完成后编译验证, Step 9 前不期望所有测试通过.
