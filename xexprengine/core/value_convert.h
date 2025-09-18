#include <complex>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>

namespace xexprengine
{
namespace value_convert
{

template <typename T, typename = void>
struct has_ostream_operator : std::false_type {};

template <typename T>
struct has_ostream_operator<T, 
    typename std::enable_if<
        !std::is_same<
            decltype(std::declval<std::ostream&>() << std::declval<T>()),
            void
        >::value
    >::type> : std::true_type {};

template <typename T>
struct is_list_type : std::false_type {};

template <typename T, typename A>
struct is_list_type<std::vector<T, A>> : std::true_type {};

template <typename T, typename A>
struct is_list_type<std::list<T, A>> : std::true_type {};

template <typename T>
struct is_map_type : std::false_type {};

template <typename Key, typename Value, typename Compare, typename Alloc>
struct is_map_type<std::map<Key, Value, Compare, Alloc>> : std::true_type {};

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
struct is_map_type<std::unordered_map<Key, Value, Hash, Pred, Alloc>> : std::true_type {};

template <typename T>
struct is_set_type : std::false_type {};

template <typename Key, typename Compare, typename Alloc>
struct is_set_type<std::set<Key, Compare, Alloc>> : std::true_type {};

template <typename Key, typename Hash, typename Pred, typename Alloc>
struct is_set_type<std::unordered_set<Key, Hash, Pred, Alloc>> : std::true_type {};

template <typename T> struct is_complex_type : std::false_type {};
template <typename T> struct is_complex_type<std::complex<T>> : std::true_type {};

template <typename T>
struct is_custom_string_convertible_type : std::integral_constant<bool,
    is_list_type<T>::value || is_map_type<T>::value || is_set_type<T>::value || is_complex_type<T>::value> {};

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

template <>
struct ToStringHelper<char>
{
    static std::string convert(const char &value) { return std::string(1, value); }
};

template <>
struct ToStringHelper<bool>
{
    static std::string convert(const bool &value) { return value ? "true" : "false"; }
};

template <typename T>
struct ToStringHelper<std::complex<T>>
{
    static std::string convert(const std::complex<T> &value)
    {
        std::ostringstream oss;
        oss << "(" << value.real();
        if (value.imag() < 0)
            oss << " - " << -value.imag() << "j";
        else
            oss << " + " << value.imag() << "j";
        oss << ")";
        return oss.str();
    }
};

template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_list_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (it != value.begin()) result += ", ";
            result += StringConverter::ToString(*it);
        }
        return "[" + result + "]";
    }
};

template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_map_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (it != value.begin()) result += ", ";
            result += StringConverter::ToString(it->first) + ": " + 
                     StringConverter::ToString(it->second);
        }
        return "{" + result + "}";
    }
};

template <typename T>
struct ToStringHelper<T, typename std::enable_if<is_set_type<T>::value>::type>
{
    static std::string convert(const T &value)
    {
        std::string result;
        for (auto it = value.begin(); it != value.end(); ++it)
        {
            if (it != value.begin()) result += ", ";
            result += StringConverter::ToString(*it);
        }
        return "{" + result + "}";
    }
};

template <typename T>
struct ToStringHelper<T, typename std::enable_if<
    has_ostream_operator<T>::value && 
    !is_custom_string_convertible_type<T>::value
>::type>
{
    static std::string convert(const T &value)
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
};

} // namespace value_convert
} // namespace xexprengine