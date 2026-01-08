#pragma once
#include <sstream>

#include "type_traits_ext.h"

namespace xequation
{
namespace value_convert
{

template <typename T, typename = void>
struct has_ostream_operator : std::false_type
{};

template <typename T>
struct has_ostream_operator<
    T, typename std::enable_if<
           !std::is_same<decltype(std::declval<std::ostream &>() << std::declval<T>()), void>::value>::type>
    : std::true_type
{};

template <typename T>
struct is_custom_string_convertible_type
    : std::integral_constant<
          bool, is_string_type<T>::value || is_char_type<T>::value || is_bool_type<T>::value ||
                    is_list_type<T>::value || is_map_type<T>::value || is_set_type<T>::value ||
                    is_complex_type<T>::value>
{};

class StringConverter
{
  private:
    // Private implementation details
    template <typename T, typename = void>
    struct ToStringImpl
    {
        static std::string convert(const T &)
        {
            return "can not convert to string";
        }
    };

    // Specializations as private members
    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_string_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            return "'" + value + "'";
        }
    };

    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_char_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            return "'" + std::string(1, value) + "'";
        }
    };

    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_bool_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            return value ? "true" : "false";
        }
    };

    template <typename T>
    struct ToStringImpl<std::complex<T>>
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
    struct ToStringImpl<T, typename std::enable_if<is_pair_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            return "(" + StringConverter::ToString(value.first) + ", " + StringConverter::ToString(value.second) + ")";
        }
    };

    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_list_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            if(value.empty())
            {
                return "[]";
            }

            std::string result;
            for (auto it = value.begin(); it != value.end(); ++it)
            {
                if (it != value.begin())
                    result += ", ";
                result += StringConverter::ToString(*it);
            }
            return "[" + result + "]";
        }
    };

    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_map_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            if(value.empty())
            {
                return "{}";
            }

            std::string result;
            for (auto it = value.begin(); it != value.end(); ++it)
            {
                if (it != value.begin())
                    result += ", ";
                result += StringConverter::ToString(it->first) + ": " + StringConverter::ToString(it->second);
            }
            return "{" + result + "}";
        }
    };

    template <typename T>
    struct ToStringImpl<T, typename std::enable_if<is_set_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            if(value.empty())
            {
                return "{}";
            }

            std::string result;
            for (auto it = value.begin(); it != value.end(); ++it)
            {
                if (it != value.begin())
                    result += ", ";
                result += StringConverter::ToString(*it);
            }
            return "{" + result + "}";
        }
    };

    template <typename T>
    struct ToStringImpl<
        T,
        typename std::enable_if<has_ostream_operator<T>::value && !is_custom_string_convertible_type<T>::value>::type>
    {
        static std::string convert(const T &value)
        {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    };

  public:
    // Public interface
    template <typename T>
    static std::string ToString(const T &value)
    {
        return ToStringImpl<T>::convert(value);
    }
};

} // namespace value_convert
} // namespace xequation