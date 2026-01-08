#pragma once
#include <complex>
#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace xequation
{
namespace value_convert
{
template <typename T>
struct is_list_type : std::false_type
{};

template <typename T, typename A>
struct is_list_type<std::vector<T, A>> : std::true_type
{};

template <typename T, typename A>
struct is_list_type<std::list<T, A>> : std::true_type
{};

template <typename T>
struct is_map_type : std::false_type
{};

template <typename Key, typename Value, typename Compare, typename Alloc>
struct is_map_type<std::map<Key, Value, Compare, Alloc>> : std::true_type
{};

template <typename Key, typename Value, typename Hash, typename Pred, typename Alloc>
struct is_map_type<std::unordered_map<Key, Value, Hash, Pred, Alloc>> : std::true_type
{};

template <typename T>
struct is_set_type : std::false_type
{};

template <typename Key, typename Compare, typename Alloc>
struct is_set_type<std::set<Key, Compare, Alloc>> : std::true_type
{};

template <typename Key, typename Hash, typename Pred, typename Alloc>
struct is_set_type<std::unordered_set<Key, Hash, Pred, Alloc>> : std::true_type
{};

template <typename T>
struct is_pair_type : std::false_type
{};

template <typename T1, typename T2>
struct is_pair_type<std::pair<T1, T2>> : std::true_type
{};

template <typename T>
struct is_complex_type : std::false_type
{};

template <typename T>
struct is_complex_type<std::complex<T>> : std::true_type
{};

template <typename T>
struct is_char_type : std::false_type
{};
template <>
struct is_char_type<char> : std::true_type
{};

template <typename T>
struct is_bool_type : std::false_type
{};
template <>
struct is_bool_type<bool> : std::true_type
{};

template <typename T>
struct is_string_type : std::false_type
{};
template <>
struct is_string_type<std::string> : std::true_type
{};
} // namespace value_convert
} // namespace xequation