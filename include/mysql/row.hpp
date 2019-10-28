#ifndef INCLUDE_MYSQL_ROW_HPP_
#define INCLUDE_MYSQL_ROW_HPP_

#include "mysql/impl/basic_types.hpp"
#include "mysql/value.hpp"
#include "mysql/metadata.hpp"

namespace mysql
{

template <typename Allocator>
class row
{
	detail::bytestring<Allocator> buffer_;
	std::vector<value> values_;
	const resultset_metadata<Allocator>* metadata_;
public:
	row(): metadata_(nullptr) {};
	row(detail::bytestring<Allocator>&& buffer, std::vector<value>&& values,
			const resultset_metadata<Allocator>& meta):
		buffer_(std::move(buffer)), values_(std::move(values)), metadata_(&meta) {};
	row(const row&) = delete;
	row(row&&) = default;
	row& operator=(const row&) = delete;
	row& operator=(row&&) = default;
	~row() = default;

	const std::vector<value>& values() const noexcept { return values_; }
	const auto& metadata() const noexcept { return *metadata_; }

	// TODO: can we make these private? accessed by resultset
	auto& buffer() { return buffer_; }
	auto& values() { return values_; }
};

}



#endif /* INCLUDE_MYSQL_ROW_HPP_ */
