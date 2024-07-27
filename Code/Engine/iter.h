#pragma once


template <typename T, typename TIter = decltype( std::begin(std::declval<T>()) ), typename = decltype( std::end(std::declval<T>()) )>
constexpr auto gEnumerate(T&& iterable)
{
    struct iterator
    {
        size_t i;
        TIter iter;
        bool operator != (const iterator& other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };

    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator { 0, std::begin(iterable) }; }
        auto end() { return iterator { 0, std::end(iterable) }; }
    };

    return iterable_wrapper { std::forward<T>(iterable) };
}


template <typename Tpl, typename Fx, size_t... Indices>
void _for_each_tuple_element_impl(Tpl&& Tuple, Fx Func, std::index_sequence<Indices...>)
{
    // call Func() on the Indices elements of Tuple
    int ignored[] = { ( static_cast<void>( Func(std::get<Indices>(std::forward<Tpl>(Tuple))) ), 0 )... };
    (void)ignored;
}

template <typename Tpl, typename Fx>
void gForEachTupleElement(Tpl&& Tuple, Fx Func)
{
    // call Func() on each element in Tuple
    _for_each_tuple_element_impl(
        std::forward<Tpl>(Tuple), Func, std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tpl>>>{});
}

template<typename ...T, typename Fx>
void gForEachType(Fx Func)
{
    gForEachTupleElement(std::tuple<T...>(), Func);
}