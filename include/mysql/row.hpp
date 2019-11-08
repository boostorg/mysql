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
	std::vector<value> values_;
	const resultset_metadata<Allocator>* metadata_;
public:
	row(): metadata_(nullptr) {};
	row(std::vector<value>&& values, const resultset_metadata<Allocator>& meta):
		values_(std::move(values)), metadata_(&meta) {};

	const std::vector<value>& values() const noexcept { return values_; }
	std::vector<value>& values() noexcept { return values_; }
	const auto& metadata() const noexcept { return *metadata_; }
};

template <typename Allocator>
class owning_row : public row<Allocator>
{
	detail::bytestring<Allocator> buffer_;
public:
	owning_row() = default;
	owning_row(std::vector<value>&& values, const resultset_metadata<Allocator>& meta,
			detail::bytestring<Allocator>&& buffer) :
			row<Allocator>(std::move(values), meta), buffer_(std::move(buffer)) {};
	owning_row(const owning_row&) = delete;
	owning_row(owning_row&&) = default;
	owning_row& operator=(const owning_row&) = delete;
	owning_row& operator=(owning_row&&) = default;
	~owning_row() = default;
};

}



#endif /* INCLUDE_MYSQL_ROW_HPP_ */
