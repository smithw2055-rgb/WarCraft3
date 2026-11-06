#pragma once
#include "scripting/JassVM.h"
#include <tuple>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace jass {

namespace detail {

template <typename T>
struct remove_cvref {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

template <typename T>
inline remove_cvref_t<T> ExtractArg(const std::vector<JassValue>& args, size_t index) {
    using CleanT = remove_cvref_t<T>;
    if (index >= args.size()) {
        return CleanT{};
    }
    const JassValue& v = args[index];
    if constexpr (std::is_same_v<CleanT, JassValue>) {
        return v;
    } else if constexpr (std::is_same_v<CleanT, int>) {
        return v.AsInt();
    } else if constexpr (std::is_same_v<CleanT, float>) {
        return v.AsReal();
    } else if constexpr (std::is_same_v<CleanT, double>) {
        return static_cast<double>(v.AsReal());
    } else if constexpr (std::is_same_v<CleanT, bool>) {
        return v.AsBool();
    } else if constexpr (std::is_same_v<CleanT, std::string>) {
        return v.strVal;
    } else {
        static_assert(sizeof(CleanT) == 0, "Unsupported JassBinder argument type");
    }
}

template <typename R>
inline JassValue PackReturn(R&& val) {
    using CleanR = remove_cvref_t<R>;
    if constexpr (std::is_same_v<CleanR, JassValue>) {
        return std::forward<R>(val);
    } else if constexpr (std::is_same_v<CleanR, int>) {
        return JassValue::Int(val);
    } else if constexpr (std::is_same_v<CleanR, float>) {
        return JassValue::Real(val);
    } else if constexpr (std::is_same_v<CleanR, double>) {
        return JassValue::Real(static_cast<float>(val));
    } else if constexpr (std::is_same_v<CleanR, bool>) {
        return JassValue::Bool(val);
    } else if constexpr (std::is_same_v<CleanR, std::string>) {
        return JassValue::Str(val);
    } else if constexpr (std::is_same_v<CleanR, const char*>) {
        return JassValue::Str(val ? std::string(val) : std::string());
    } else {
        static_assert(sizeof(CleanR) == 0, "Unsupported JassBinder return type");
    }
}

template <typename T>
struct function_traits : function_traits<decltype(&remove_cvref_t<T>::operator())> {};

template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct function_traits<R(Args...)> : function_traits<R(*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> : function_traits<R(*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> : function_traits<R(*)(Args...)> {};

template <typename Callable, typename Tuple, size_t... Is>
JassValue InvokeTyped(Callable& fn, const std::vector<JassValue>& args, std::index_sequence<Is...>) {
    using R = typename function_traits<Callable>::return_type;
    if constexpr (std::is_void_v<R>) {
        fn(ExtractArg<std::tuple_element_t<Is, Tuple>>(args, Is)...);
        return JassValue::Null();
    } else {
        return PackReturn(fn(ExtractArg<std::tuple_element_t<Is, Tuple>>(args, Is)...));
    }
}

} // namespace detail

// Convert any typed C++ callable R(Args...) into a JassVM::NativeFunc.
template <typename Callable>
JassVM::NativeFunc BindNative(Callable&& fn) {
    using Traits = detail::function_traits<Callable>;
    using Tuple = typename Traits::args_tuple;
    return [callable = std::forward<Callable>(fn)](const std::vector<JassValue>& args) mutable -> JassValue {
        return detail::InvokeTyped<std::decay_t<Callable>, Tuple>(
            callable, args, std::make_index_sequence<Traits::arity>{});
    };
}

// Register a typed C++ callable directly onto JassVM without manual JassValue unpacking.
template <typename Callable>
void RegisterNativeAuto(JassVM& vm, const std::string& name, Callable&& fn) {
    vm.RegisterNative(name, BindNative(std::forward<Callable>(fn)));
}

} // namespace jass
