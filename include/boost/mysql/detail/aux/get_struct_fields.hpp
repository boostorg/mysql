#ifndef INCLUDE_BOOST_MYSQL_DETAIL_AUX_GET_STRUCT_FIELDS_HPP_
#define INCLUDE_BOOST_MYSQL_DETAIL_AUX_GET_STRUCT_FIELDS_HPP_

namespace boost {
namespace mysql {
namespace detail {

// To allow a limited way of reflection, structs should
// specialize get_struct_fields with a tuple of pointers to members,
// thus defining which fields should be (de)serialized in the struct
// and in which order
struct not_a_struct_with_fields {}; // Tag indicating a type is not a struct with fields

template <typename T>
struct get_struct_fields
{
	static constexpr not_a_struct_with_fields value {};
};


} // detail
} // mysql
} // boost


#endif /* INCLUDE_BOOST_MYSQL_DETAIL_AUX_GET_STRUCT_FIELDS_HPP_ */
