#include <complex>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace xexprengine
{

namespace value_convert
{
template <typename T>
struct has_to_string
{
  private:
    template <typename U>
    static auto test(int) -> decltype(std::declval<U>().ToString(), std::true_type());

    template <typename U>
    static std::false_type test(...);

  public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct has_convert_to_string
{
  private:
    template <typename U>
    static auto test(int) -> decltype(ConvertToString(std::declval<U>()), std::true_type());

    template <typename U>
    static std::false_type test(...);

  public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
struct is_list_type : std::false_type
{
};

template <typename T>
struct is_list_type<std::vector<T>> : std::true_type
{
};

template <typename T>
struct is_list_type<std::list<T>> : std::true_type
{
};

template <typename T>
struct is_map_type : std::false_type
{
};

template <typename Key, typename Value, typename Compare, typename Alloc>
struct is_map_type<std::map<Key, Value, Compare, Alloc>> : std::true_type
{
};

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
struct is_map_type<std::unordered_map<Key, Value, Hash, Pred, Alloc>> : std::true_type
{
};

template <typename T>
struct is_set_type : std::false_type
{
};

template <typename Key, typename Compare, typename Alloc>
struct is_set_type<std::set<Key, Compare, Alloc>> : std::true_type
{
};

template <typename Key, typename Hash, typename Pred, typename Alloc>
struct is_set_type<std::unordered_set<Key, Hash, Pred, Alloc>> : std::true_type
{
};

// default template
template <typename T, typename = void>
struct ToStringHelper
{
    static std::string convert(const T &) { return "can not convert to string"; }
};

// String converter
struct StringConverter
{
    template <typename T>
    static std::string ToString(const T &value)
    {
        return ToStringHelper<T>::convert(value);
    }
};

// Specialization for char
template <>
struct ToStringHelper<char>
{
    static std::string convert(const char &value) { return std::string(1, value); }
};

// Specialization for std::string
template <>
struct ToStringHelper<std::string>
{
    static std::string convert(const std::string &value) { return value; }
};

// Specialization for bool
template <>
struct ToStringHelper<bool>
{
    static std::string convert(const bool &value) { return value ? "true" : "false"; }
};

// Specialization for arithmetic types
template <typename T>
struct ToStringHelper<T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
{
    static std::string convert(const T &value) { return std::to_string(value); }
};

// Specialization for complex
template <typename T>
struct ToStringHelper<std::complex<T>>
{
    static std::string convert(const std::complex<T> &value)
    {
        std::string result = "(" + std::to_string(value.real());
        if (value.imag() < 0)
        {
            result += " - " + std::to_string(-value.imag()) + "j";
        }
        else
        {
            result += " + " + std::to_string(value.imag()) + "j";
        }
        result += ")";
        return result;
    }
};

// Specialization for user-defined types with ToString()
template <typename T>
struct ToStringHelper<T, typename std::enable_if<has_to_string<T>::value>::type>
{
    static std::string convert(const T &value) { return value.ToString(); }
};

// Specialization for user-defined types with ConvertToString()
template <typename T>
struct ToStringHelper<T, typename std::enable_if<has_convert_to_string<T>::value>::type>
{
    static std::string convert(const T &value) { return ConvertToString(value); }
};

// Specialization for list types
template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_list_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (const auto &item : value)
        {
            if (!result.empty())
            {
                result += ", ";
            }
            result += StringConverter::ToString(item);
        }
        return "[" + result + "]";
    }
};

// Specialization for map types
template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_map_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (!result.empty())
            {
                result += ", ";
            }
            result += StringConverter::ToString(it->first) + ": " + StringConverter::ToString(it->second);
        }
        return "{" + result + "}";
    }
};

// Specialization for set types
template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_set_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (const auto &item : value)
        {
            if (!result.empty())
            {
                result += ", ";
            }
            result += StringConverter::ToString(item);
        }
        return "{" + result + "}";
    }
};
} // namespace value_convert
} // namespace xexprengine