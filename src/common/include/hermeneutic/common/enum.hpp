#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <typeinfo>
#include <vector>

namespace hermeneutic::enum_support {

void ParseEnumSpec(const char* spec, std::function<void(const char*)> callback);
[[noreturn]] void Error(const std::string& message);
void Log(const std::string& message);
void nifty_init();

template <typename T>
inline T AddDynamicElement(const std::string& name) {
  T dummy = T(0);
  T value = AddDynamicElement(dummy, name);
#ifdef _DEBUG
  Log("dynamic element for enum :: " + name +
      " @ " + std::to_string(static_cast<long long>(value)));
#endif
  return value;
}

}  // namespace hermeneutic::enum_support

#define HERMENEUTIC_ENUM_DECLARE_NIFTY(counter) static int counter = 0;
#define HERMENEUTIC_ENUM_NIFTY(counter) \
  ::hermeneutic::enum_support::nifty_init(); \
  if ((counter)++ == 0)
#define HERMENEUTIC_ENUM_USE(x) (void)(x)

#define HERMENEUTIC_ENUM(type_name, ...)                                          \
  enum class type_name { __VA_ARGS__ };                                           \
  constexpr const char* type_name##SPECS = #__VA_ARGS__;                          \
  type_name StringTo##type_name(const std::string& s);                            \
  const char* type_name##ToString(type_name v);                                   \
  type_name AddDynamicElement(type_name& dummy, const std::string& s);

#define HERMENEUTIC_ENUM_FORWARD(type_name)                                       \
  enum class type_name;                                                           \
  type_name StringTo##type_name(const std::string& s);                            \
  const char* type_name##ToString(type_name v);                                   \
  type_name AddDynamicElement(type_name& dummy, const std::string& s);

#define HERMENEUTIC_ENUM_INSTANTIATE(type_name)                                   \
  inline void FillReverseDictionary##type_name(                                  \
      std::map<std::string, type_name>& map) {                                    \
    int count = 0;                                                                \
    hermeneutic::enum_support::ParseEnumSpec(                                     \
        type_name##SPECS, [&map, &count](const char* s) { map[s] = type_name(count++); }); \
  }                                                                               \
  inline void FillVector##type_name(std::vector<std::string>& vec) {              \
    hermeneutic::enum_support::ParseEnumSpec(                                     \
        type_name##SPECS, [&vec](const char* s) { vec.push_back(s); });            \
  }                                                                               \
  HERMENEUTIC_ENUM_DECLARE_NIFTY(type_name##_nifty_counter)                       \
  struct type_name##Mapper {                                                      \
    type_name##Mapper() { CheckInit(); }                                          \
    void CheckInit() {                                                            \
      HERMENEUTIC_ENUM_NIFTY(type_name##_nifty_counter) {                         \
        map_ = new std::map<std::string, type_name>();                            \
        vec_ = new std::vector<std::string>();                                    \
        FillReverseDictionary##type_name(*map_);                                  \
        FillVector##type_name(*vec_);                                             \
      }                                                                           \
    }                                                                             \
    std::map<std::string, type_name>* map_{nullptr};                              \
    std::vector<std::string>* vec_{nullptr};                                      \
    ~type_name##Mapper() {                                                        \
      delete map_;                                                                \
      delete vec_;                                                                \
    }                                                                             \
  };                                                                              \
  type_name##Mapper type_name##_mapper_;                                          \
  type_name StringTo##type_name(const std::string& s) {                           \
    auto it = type_name##_mapper_.map_->find(s);                                  \
    if (it == type_name##_mapper_.map_->cend()) {                                 \
      hermeneutic::enum_support::Error("can't find enum for" + s);               \
    }                                                                             \
    return it->second;                                                            \
  }                                                                               \
  const char* type_name##ToString(type_name v) {                                  \
    std::size_t idx = static_cast<std::size_t>(v);                                \
    if (idx >= type_name##_mapper_.vec_->size()) {                                \
      hermeneutic::enum_support::Error(                                           \
          "can't find string for " + std::to_string(static_cast<int>(v)));       \
    }                                                                             \
    return (*type_name##_mapper_.vec_)[idx].c_str();                              \
  }                                                                               \
  type_name AddDynamicElement(type_name& dummy, const std::string& name) {        \
    HERMENEUTIC_ENUM_USE(dummy);                                                  \
    type_name##_mapper_.CheckInit();                                              \
    auto it = type_name##_mapper_.map_->find(name);                               \
    if (it != type_name##_mapper_.map_->cend()) {                                 \
      return it->second;                                                          \
    }                                                                             \
    type_name##_mapper_.vec_->push_back(name);                                    \
    type_name value = static_cast<type_name>(type_name##_mapper_.vec_->size() - 1);\
    (*type_name##_mapper_.map_)[name] = value;                                    \
    return value;                                                                 \
  }

