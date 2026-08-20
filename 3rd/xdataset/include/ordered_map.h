#ifndef XDATASET_ORDERED_MAP_H
#define XDATASET_ORDERED_MAP_H

#include <cstddef>
#include <initializer_list>
#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

// ============================================================================
// ordered_map<K, V> — 纯 STL 实现的插入顺序保持哈希映射
// ============================================================================
//
// 内部用 std::list + std::unordered_map 组合实现，覆盖项目所有用法。
// ============================================================================

template <typename Key, typename T>
class ordered_map
{
public:
    using key_type        = Key;
    using mapped_type     = T;
    using value_type      = std::pair<Key, T>;
    using size_type       = std::size_t;
    using reference       = value_type&;
    using const_reference = const value_type&;

private:
    using list_type = std::list<value_type>;

public:
    using iterator               = typename list_type::iterator;
    using const_iterator         = typename list_type::const_iterator;
    using reverse_iterator       = typename list_type::reverse_iterator;
    using const_reverse_iterator = typename list_type::const_reverse_iterator;

private:
    using index_type = std::unordered_map<Key, iterator>;

    list_type  list_;
    index_type index_;

    void rebuild_index()
    {
        index_.clear();
        for (auto it = list_.begin(); it != list_.end(); ++it)
            index_.emplace(it->first, it);
    }

public:
    ordered_map() = default;

    ordered_map(std::initializer_list<value_type> init)
    {
        for (auto& p : init)
            (*this)[p.first] = p.second;
    }

    ordered_map(const ordered_map& other)
        : list_(other.list_)
    {
        rebuild_index();
    }

    ordered_map(ordered_map&& other) noexcept
        : list_(std::move(other.list_))
        , index_(std::move(other.index_))
    {}

    ordered_map& operator=(const ordered_map& other)
    {
        if (this != &other)
        {
            list_ = other.list_;
            rebuild_index();
        }
        return *this;
    }

    ordered_map& operator=(ordered_map&& other) noexcept
    {
        if (this != &other)
        {
            list_  = std::move(other.list_);
            index_ = std::move(other.index_);
        }
        return *this;
    }

    // ---- 迭代器 ------------------------------------------------------------
    iterator       begin()        noexcept { return list_.begin(); }
    const_iterator begin()  const noexcept { return list_.begin(); }
    const_iterator cbegin() const noexcept { return list_.cbegin(); }

    iterator       end()        noexcept { return list_.end(); }
    const_iterator end()  const noexcept { return list_.end(); }
    const_iterator cend() const noexcept { return list_.cend(); }

    reverse_iterator       rbegin()        noexcept { return list_.rbegin(); }
    const_reverse_iterator rbegin()  const noexcept { return list_.rbegin(); }
    const_reverse_iterator crbegin() const noexcept { return list_.crbegin(); }

    reverse_iterator       rend()        noexcept { return list_.rend(); }
    const_reverse_iterator rend()  const noexcept { return list_.rend(); }
    const_reverse_iterator crend() const noexcept { return list_.crend(); }

    // ---- 容量 --------------------------------------------------------------
    bool      empty() const noexcept { return list_.empty(); }
    size_type size()  const noexcept { return list_.size(); }

    // ---- 元素访问 ----------------------------------------------------------
    mapped_type& at(const Key& key)
    {
        auto it = index_.find(key);
        if (it == index_.end())
            throw std::out_of_range("ordered_map::at: key not found");
        return it->second->second;
    }

    const mapped_type& at(const Key& key) const
    {
        auto it = index_.find(key);
        if (it == index_.end())
            throw std::out_of_range("ordered_map::at: key not found");
        return it->second->second;
    }

    mapped_type& operator[](const Key& key)
    {
        auto it = index_.find(key);
        if (it != index_.end())
            return it->second->second;

        list_.emplace_back(key, mapped_type{});
        auto list_it = std::prev(list_.end());
        index_.emplace(key, list_it);
        return list_it->second;
    }

    mapped_type& operator[](Key&& key)
    {
        auto it = index_.find(key);
        if (it != index_.end())
            return it->second->second;

        list_.emplace_back(std::move(key), mapped_type{});
        auto list_it = std::prev(list_.end());
        index_.emplace(list_it->first, list_it);
        return list_it->second;
    }

    // ---- 查找 --------------------------------------------------------------
    iterator find(const Key& key)
    {
        auto it = index_.find(key);
        return it != index_.end() ? it->second : list_.end();
    }

    const_iterator find(const Key& key) const
    {
        auto it = index_.find(key);
        if (it != index_.end())
            return it->second;   // iterator 隐式转换为 const_iterator
        return list_.end();
    }

    size_type count(const Key& key) const
    {
        return index_.count(key);
    }

    bool contains(const Key& key) const
    {
        return index_.find(key) != index_.end();
    }

    // ---- 首尾元素 (保持插入顺序，故有 front/back 语义) -------------------
    reference       front()        { return list_.front(); }
    const_reference front()  const { return list_.front(); }
    reference       back()         { return list_.back(); }
    const_reference back()   const { return list_.back(); }

    // ---- 修改器 ------------------------------------------------------------
    void clear() noexcept
    {
        list_.clear();
        index_.clear();
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(const Key& key, Args&&... args)
    {
        auto it = index_.find(key);
        if (it != index_.end())
        {
            it->second->second = mapped_type(std::forward<Args>(args)...);
            return {it->second, false};
        }
        list_.emplace_back(key, mapped_type(std::forward<Args>(args)...));
        auto list_it = std::prev(list_.end());
        index_.emplace(key, list_it);
        return {list_it, true};
    }

    template <typename... Args>
    std::pair<iterator, bool> emplace(Key&& key, Args&&... args)
    {
        auto it = index_.find(key);
        if (it != index_.end())
        {
            it->second->second = mapped_type(std::forward<Args>(args)...);
            return {it->second, false};
        }
        list_.emplace_back(std::move(key), mapped_type(std::forward<Args>(args)...));
        auto list_it = std::prev(list_.end());
        index_.emplace(list_it->first, list_it);
        return {list_it, true};
    }

    // ---- insert ------------------------------------------------------------
    std::pair<iterator, bool> insert(const value_type& value)
    {
        return emplace(value.first, value.second);
    }

    std::pair<iterator, bool> insert(value_type&& value)
    {
        return emplace(std::move(value.first), std::move(value.second));
    }

    template <typename P>
    auto insert(P&& value)
        -> decltype(emplace(std::forward<P>(value).first, std::forward<P>(value).second))
    {
        return emplace(std::forward<P>(value).first, std::forward<P>(value).second);
    }

    // ---- try_emplace (不覆写已存在的 key) ---------------------------------
    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const Key& key, Args&&... args)
    {
        auto it = index_.find(key);
        if (it != index_.end())
            return {it->second, false};
        list_.emplace_back(key, mapped_type(std::forward<Args>(args)...));
        auto list_it = std::prev(list_.end());
        index_.emplace(key, list_it);
        return {list_it, true};
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(Key&& key, Args&&... args)
    {
        auto it = index_.find(key);
        if (it != index_.end())
            return {it->second, false};
        list_.emplace_back(std::move(key), mapped_type(std::forward<Args>(args)...));
        auto list_it = std::prev(list_.end());
        index_.emplace(list_it->first, list_it);
        return {list_it, true};
    }

    // ---- insert_or_assign (总是覆写) ---------------------------------------
    template <typename M>
    std::pair<iterator, bool> insert_or_assign(const Key& key, M&& obj)
    {
        auto it = index_.find(key);
        if (it != index_.end())
        {
            it->second->second = std::forward<M>(obj);
            return {it->second, false};
        }
        list_.emplace_back(key, std::forward<M>(obj));
        auto list_it = std::prev(list_.end());
        index_.emplace(key, list_it);
        return {list_it, true};
    }

    template <typename M>
    std::pair<iterator, bool> insert_or_assign(Key&& key, M&& obj)
    {
        auto it = index_.find(key);
        if (it != index_.end())
        {
            it->second->second = std::forward<M>(obj);
            return {it->second, false};
        }
        list_.emplace_back(std::move(key), std::forward<M>(obj));
        auto list_it = std::prev(list_.end());
        index_.emplace(list_it->first, list_it);
        return {list_it, true};
    }

    // ---- erase -------------------------------------------------------------
    iterator erase(const_iterator pos)
    {
        index_.erase(pos->first);
        return list_.erase(pos);
    }

    size_type erase(const Key& key)
    {
        auto it = index_.find(key);
        if (it == index_.end())
            return 0;
        list_.erase(it->second);
        index_.erase(it);
        return 1;
    }

    void swap(ordered_map& other) noexcept
    {
        list_.swap(other.list_);
        index_.swap(other.index_);
    }

    bool operator==(const ordered_map& other) const { return list_ == other.list_; }
    bool operator!=(const ordered_map& other) const { return list_ != other.list_; }
};

template <typename K, typename V>
void swap(ordered_map<K, V>& a, ordered_map<K, V>& b) noexcept
{
    a.swap(b);
}

#endif  // XDATASET_ORDERED_MAP_H
