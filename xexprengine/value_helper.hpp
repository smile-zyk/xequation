#include <type_traits>
#include <string>

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
std::string ConvertToString(const T& value) {
    return ToStringHelper<T>::convert(value);
}