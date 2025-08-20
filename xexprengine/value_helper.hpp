#include <type_traits>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

template<typename T>
std::string ConvertToString(const T& value);

template<typename T>
struct is_have_to_string {
private:
    template<typename U>
    static auto test(int) -> decltype(std::declval<U>().ToString(), std::true_type());

    template<typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
struct is_list_type : std::false_type {};

template<typename T>
struct is_list_type<std::vector<T>> : std::true_type {};

template<typename T>
struct is_list_type<std::list<T>> : std::true_type {};

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

template<typename T, typename = void>
struct ToStringHelper {
    static std::string convert(const T&) {
        return "can not convert to string";
    }
};

template<>
struct ToStringHelper<std::string> {
    static std::string convert(const std::string& value) {
        return value;
    }
};

template<typename T>
struct ToStringHelper<T, typename std::enable_if<std::is_arithmetic<T>::value>::type> {
    static std::string convert(const T& value) {
        return std::to_string(value);
    }
};

template<typename T>
struct ToStringHelper<T, typename std::enable_if<is_have_to_string<T>::value>::type> {
    static std::string convert(const T& value) {
        return value.ToString();
    }
};

template<typename T>
struct ToStringHelper<T, typename std::enable_if<is_list_type<T>::value>::type> {
    static std::string convert(const T& value) 
    {
        std::string result;
        for(const auto& item : value) 
        {
            if (!result.empty()) {
                result += ", ";
            }
            result += ConvertToString(item);
        }
        return "[" + result + "]";
    }
};

template<typename T>
struct ToStringHelper<T, typename std::enable_if<is_map_type<T>::value>::type> {
    static std::string convert(const T& value) 
    {
        std::string result;
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (!result.empty()) {
                result += ", ";
            }
            result += ConvertToString(it->first) + ": " + ConvertToString(it->second);
        }
        return "{" + result + "}";
    }
};

template<typename T>
struct ToStringHelper<T, typename std::enable_if<is_set_type<T>::value>::type> {
    static std::string convert(const T& value) 
    {
        std::string result;
        for (const auto& item : value) {
            if (!result.empty()) {
                result += ", ";
            }
            result += ConvertToString(item);
        }
        return "{" + result + "}";
    }
};

template<typename T>
std::string ConvertToString(const T& value) {
    return ToStringHelper<T>::convert(value);
}