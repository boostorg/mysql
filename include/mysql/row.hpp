#ifndef INCLUDE_MYSQL_ROW_HPP_
#define INCLUDE_MYSQL_ROW_HPP_

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
	const std::vector<field_metadata>* metadata_;
public:
	row(): metadata_(nullptr) {};
	row(std::vector<value>&& values, const std::vector<field_metadata>& meta):
		values_(std::move(values)), metadata_(&meta) {};

	const std::vector<value>& values() const noexcept { return values_; }
	std::vector<value>& values() noexcept { return values_; }
	const std::vector<field_metadata>& metadata() const noexcept { return *metadata_; }
};

class owning_row : public row
{
	detail::bytestring buffer_;
public:
	owning_row() = default;
	owning_row(std::vector<value>&& values, const std::vector<field_metadata>& meta,
			detail::bytestring&& buffer) :
			row(std::move(values), meta), buffer_(std::move(buffer)) {};
	owning_row(const owning_row&) = delete;
	owning_row(owning_row&&) = default;
	owning_row& operator=(const owning_row&) = delete;
	owning_row& operator=(owning_row&&) = default;
	~owning_row() = default;
};

// Note: only values are checked, metadata do not have to match
inline bool operator==(const row& lhs, const row& rhs) { return lhs.values() == rhs.values(); }
inline bool operator!=(const row& lhs, const row& rhs) { return !(lhs == rhs); }

inline bool operator==(const std::vector<owning_row>& lhs, const std::vector<owning_row>& rhs)
{
	return detail::container_equals(lhs, rhs);
}
inline bool operator!=(const std::vector<owning_row>& lhs, const std::vector<owning_row>& rhs)
{
	return !(lhs == rhs);
}

}



#endif /* INCLUDE_MYSQL_ROW_HPP_ */
