#ifndef MYSQL_ASIO_ROW_HPP
#define MYSQL_ASIO_ROW_HPP

#include "mysql/impl/basic_types.hpp"
#include "mysql/value.hpp"
#include "mysql/metadata.hpp"
#include "mysql/impl/container_equals.hpp"
#include <algorithm>

namespace mysql
{

class row
{
	std::vector<value> values_;
public:
	row(std::vector<value>&& values = {}):
		values_(std::move(values)) {};

	const std::vector<value>& values() const noexcept { return values_; }
	std::vector<value>& values() noexcept { return values_; }
};

class owning_row : public row
{
	detail::bytestring buffer_;
public:
	owning_row() = default;
	owning_row(std::vector<value>&& values, detail::bytestring&& buffer) :
			row(std::move(values)), buffer_(std::move(buffer)) {};
	owning_row(const owning_row&) = delete;
	owning_row(owning_row&&) = default;
	owning_row& operator=(const owning_row&) = delete;
	owning_row& operator=(owning_row&&) = default;
	~owning_row() = default;
};

inline bool operator==(const row& lhs, const row& rhs) { return lhs.values() == rhs.values(); }
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }
inline std::ostream& operator<<(std::ostream& os, const row& value)
{
	os << '{';
	const auto& arr = value.values();
	if (!arr.empty())
	{
		os << arr[0];
		for (auto it = std::next(arr.begin()); it != arr.end(); ++it)
		{
			os << ", " << *it;
		}
	}
	return os << '}';
}

// Allow comparisons between vectors of rows and owning rows
template <
	typename RowTypeLeft,
	typename RowTypeRight,
	typename=std::enable_if_t<std::is_base_of_v<row, RowTypeLeft> && std::is_base_of_v<row, RowTypeRight>>
>
inline bool operator==(const std::vector<RowTypeLeft>& lhs, const std::vector<RowTypeRight>& rhs)
{
	return detail::container_equals(lhs, rhs);
}

template <
	typename RowTypeLeft,
	typename RowTypeRight,
	typename=std::enable_if_t<std::is_base_of_v<row, RowTypeLeft> && std::is_base_of_v<row, RowTypeRight>>
>
inline bool operator!=(const std::vector<RowTypeLeft>& lhs, const std::vector<RowTypeRight>& rhs)
{
	return !(lhs == rhs);
}

}



#endif