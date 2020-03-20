#ifndef MYSQL_ASIO_ROW_HPP
#define MYSQL_ASIO_ROW_HPP

#include "boost/mysql/detail/aux/bytestr.hpp"
#include "boost/mysql/detail/aux/container_equals.hpp"
#include "boost/mysql/value.hpp"
#include "boost/mysql/metadata.hpp"
#include <algorithm>

namespace boost {
namespace mysql {

/**
 * \brief Represents a row returned from a query.
 * \details Call row::values() to get the actual sequence of mysql::value.
 * There will be the same number of values and in the same order as fields
 * in the SQL query that produced the row. You can get more information
 * about these fields using resultset::fields().
 *
 * If any of the values is a string (mysql::value having string_view
 * as actual type), it will point to an externally owned piece of memory.
 * Thus, the row base class is not owning; this is contrary to owning_row,
 * that actually owns the string memory of its values.
 */
class row
{
	std::vector<value> values_;
public:
	/// Default and initializing constructor.
	row(std::vector<value>&& values = {}):
		values_(std::move(values)) {};

	/// Accessor for the sequence of values.
	const std::vector<value>& values() const noexcept { return values_; }

	/// Accessor for the sequence of values.
	std::vector<value>& values() noexcept { return values_; }
};

/**
 * \brief A row that owns a chunk of memory for its string values.
 * \detail Default constructible and movable, but not copyable.
 */
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

/// Compares two rows.
inline bool operator==(const row& lhs, const row& rhs) { return lhs.values() == rhs.values(); }

/// Compares two rows.
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }

/// Streams a row.
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

} // mysql
} // boost



#endif
