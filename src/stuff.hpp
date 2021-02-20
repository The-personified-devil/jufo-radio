#ifndef STUFF_HPP
#define STUFF_HPP

// https://gist.github.com/klmr/2775736#file-make_array-hpp
template <typename... T>
constexpr auto make_array(T&&... values) ->
    std::array<
       typename std::decay<
           typename std::common_type<T...>::type>::type,
       sizeof...(T)> {
    return std::array<
        typename std::decay<
            typename std::common_type<T...>::type>::type,
        sizeof...(T)>{std::forward<T>(values)...};
}
#endif