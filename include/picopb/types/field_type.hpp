#pragma once

namespace ppb::detail {

// how a field type is stored and written; specialized once per kind of field type
// stores<V> says whether a member of type V can hold it; write and read convert to and from wire
// values, each returns an Error for a value it rejects, and a read returns false for a field that
// arrived with another wire type
template <class T>
struct FieldTypeTraits;

// a type that can be a field's type, the T in Field<N, &M::member, T>
template <class T>
concept FieldType = requires { sizeof(FieldTypeTraits<T>); };

}  // namespace ppb::detail
