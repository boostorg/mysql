#ifndef TEST_SERIALIZATION_TEST_COMMON_HPP_
#define TEST_SERIALIZATION_TEST_COMMON_HPP_

#include "mysql/impl/basic_serialization.hpp"
#include "mysql/impl/messages.hpp"
#include "mysql/impl/constants.hpp"
#include <gtest/gtest.h>
#include <string>
#include <boost/type_index.hpp>

namespace mysql
{
namespace detail
{

// Operator == for structs
template <std::size_t index, typename T>
bool equals_struct(const T& lhs, const T& rhs)
{
	if constexpr (index == std::tuple_size<std::decay_t<decltype(T::fields)>>::value)
	{
		return true;
	}
	else
	{
		constexpr auto pmem = std::get<index>(T::fields);
		return (rhs.*pmem == lhs.*pmem) && equals_struct<index+1>(lhs, rhs);
	}
}

template <typename T>
std::enable_if_t<is_struct_with_fields<T>::value, bool>
operator==(const T& lhs, const T& rhs)
{
	return equals_struct<0>(lhs, rhs);
}


class TypeErasedValue
{
public:
	virtual ~TypeErasedValue() {}
	virtual void serialize(SerializationContext& ctx) const = 0;
	virtual std::size_t get_size(const SerializationContext& ctx) const = 0;
	virtual Error deserialize(DeserializationContext& ctx) = 0;
	virtual std::string get_type_name() const = 0;
	virtual std::shared_ptr<TypeErasedValue> default_construct() const = 0;
	virtual bool equals(const TypeErasedValue& rhs) const = 0;

	bool operator==(const TypeErasedValue& rhs) const { return equals(rhs); }
};

template <typename T>
class TypeErasedValueImpl : public TypeErasedValue
{
	T value_;
public:
	TypeErasedValueImpl(const T& v): value_(v) {};
	void serialize(SerializationContext& ctx) const override { ::mysql::detail::serialize(value_, ctx); }
	std::size_t get_size(const SerializationContext& ctx) const override { return ::mysql::detail::get_size(value_, ctx); }
	Error deserialize(DeserializationContext& ctx) override { return ::mysql::detail::deserialize(value_, ctx); }
	std::string get_type_name() const override
	{
		return boost::typeindex::type_id<T>().pretty_name();
	}
	std::shared_ptr<TypeErasedValue> default_construct() const override
	{
		T res; // intentionally not value-initializing it
		return std::make_shared<TypeErasedValueImpl<T>>(res);
	}
	bool equals(const TypeErasedValue& rhs) const
	{
		auto typed_value = dynamic_cast<const TypeErasedValueImpl<T>*>(&rhs);
		return typed_value && (typed_value->value_ == value_);
	}
};

struct SerializeParams
{
	std::shared_ptr<TypeErasedValue> value;
	std::vector<uint8_t> expected_buffer;
	std::string test_name;
	bool is_space_sensitive;

	template <typename T>
	SerializeParams(const T& v, std::vector<uint8_t>&& buff,
			        std::string&& name="default", bool is_space_sensitive=true):
		value(std::make_shared<TypeErasedValueImpl<T>>(v)),
		expected_buffer(move(buff)),
		test_name(move(name)),
		is_space_sensitive(is_space_sensitive)
	{
	}
};

std::ostream& operator<<(std::ostream& os, const SerializeParams& params)
{
	return os << params.value->get_type_name() << " - " << params.test_name;
}

std::vector<uint8_t> concat(std::vector<uint8_t>&& lhs, const std::vector<uint8_t>& rhs)
{
	size_t lhs_size = lhs.size();
	std::vector<uint8_t> res (move(lhs));
	res.resize(lhs_size + rhs.size());
	std::memcpy(res.data() + lhs_size, rhs.data(), rhs.size());
	return res;
}

}
}


#endif /* TEST_SERIALIZATION_TEST_COMMON_HPP_ */
