/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_URI_HPP
#define CAF_IO_URI_HPP

#include <string>
#include <ostream>
#include <utility>

#include "caf/optional.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/detail/comparable.hpp"

namespace caf {
namespace io {

namespace detail {

class uri_private;

void intrusive_ptr_add_ref(uri_private* p);

void intrusive_ptr_release(uri_private* p);

} // namespace detail

namespace {

using str_bounds = std::pair<std::string::iterator, std::string::iterator>;

} // namespace anonymous

///
/// @brief Uniform Resource Identifier (as defined in RFC 3986).
/// @include uri_documentation.txt
/// @note The documentation of the accessors are taken from RFC 3986.
///
class uri : caf::detail::comparable<uri, uri>,
            caf::detail::comparable<uri, const char*>,
            caf::detail::comparable<uri, std::string> {
public:
  // required by util::comparable<uri, uri>
  inline int compare(const uri& what) const {
    return (this == &what) ? 0 : str().compare(what.str());
  }

  // required by util::comparable<uri, std::string>
  inline int compare(const std::string& what) const {
    return str().compare(what);
  }

  // required by util::comparable<uri, const char*>
  inline int compare(const char* what) const {
    // treat a NULL string like an empty string
    return (what) ? str().compare(what) : str().compare("");
  }
  
              
  ///
  /// @brief Create an empty URI.
  ///
  uri();

  ///
  /// @brief Create this object as a copy of @p other.
  /// @param other The original {@link caf::io::uri uri} object.
  /// @note {@link caf::io::uri uri} is implicit shared, thus copy
  ///       operations are very fast and lightweight.
  ///
  uri(const uri& other);
              
  ///
  /// @brief Create an URI from @p uri_str.
  ///
  /// If @p uri_str could not be parsed to a valid URI, then this URI
  /// object will be empty.
  ///
  /// @param uri_str An URI encoded as a string.
  ///
  /// @warning Let <code>a</code> be a string then the assertion
  ///          <code>a.empty() == uri(a).empty()</code> fails, if
  ///          <code>a</code> is not empty, but does not describe a valid URI
  ///          (e.g. "Hello World" is a string, but is an invalid URI).
  ///
  static optional<uri> make(const std::string& uri_str);
  
  ///
  /// @brief Create an URI from @p uri_str_c_str.
  /// @param uri_c_str An URI encoded as a C-string.
  /// @see uri(const std::string&)
  ///
  static optional<uri> make(const char* uri_c_str);

  ///
  /// @brief Get the string describing this URI.
  /// @returns The full string representation of this URI.
  ///
  const std::string& str() const;

  ///
  /// @brief Get the string describing this uri as a C-string.
  /// @returns The full string representation of this URI as C-string.
  /// @note Equal to <code>str().c_str()</code>.
  ///
  inline const char* c_str() const { return str().c_str(); }

  /**
   * @brief Check if this URI is empty.
   * @returns <code>true</code> if this URI is empty;
   *         otherwise <code>false</code>.
   */
  bool empty() const { return str().empty(); }

  ///
  /// @brief Get the host subcomponent of authority.
  ///
  /// The host subcomponent of authority is identified by an IP literal
  /// encapsulated within square brackets, an IPv4 address in dotted-
  /// decimal form, or a registered name.
  ///
  /// @returns The host subcomponent of {@link authority()}.
  ///
  const str_bounds& host() const;

  ///
  /// @brief Check if {@link host()} returns an IPv4 address.
  /// @note The testing is done in the constructor, so this member
  ///       function only checks an internal flag (and has no
  ///       other overhead!).
  /// @returns <code>true</code> if the host subcomponent of {@link authority()}
  ///         returns a string that describes a valid IPv4 address;
  ///         otherwise <code>false</code>.
  ///
  bool host_is_ipv4addr() const;

  ///
  /// @brief Check if {@link host()} returns an IPv6 address.
  /// @note Returns true if host matches the regex
  ///       <code>[a-f0-9:\\.]</code> so {@link host()} might be an
  ///       invalid ipv6 address.
  /// @note The testing is done in the constructor, so this member
  ///       function only checks an internal flag (and has no
  ///       other overhead!).
  /// @returns <code>true</code> if the host subcomponent of {@link authority()}
  ///         returns a string that describes an IPv6 address;
  ///         otherwise <code>false</code>.
  /// @warning This member function does not guarantee, that {@link host()}
  ///          returns a <b>valid</b> IPv6 address.
  ///
  bool host_is_ipv6addr() const;

  ///
  /// @brief Get the port subcomponent of authority.
  ///
  /// Port is either empty or a decimal number (between 0 and 65536).
  /// @returns A string representation of the port
  ///         subcomponent of {@link authority()}.
  ///
  const str_bounds& port() const;

  ///
  /// @brief Get the port subcomponent as integer value.
  ///
  /// This value is always 0 if <code>port().empty() == true</code>.
  /// @returns An integer (16-bit, unsigned) representation of the port
  ///         subcomponent of {@link authority()}.
  ///
  uint16_t port_as_int() const;

  ///
  /// @brief Get the path component of this URI object.
  ///
  /// The path component contains data that serves to identifiy a resource
  /// within the scope of the URI's scheme and naming authority (if any).
  /// @returns The path component.
  ///
  const str_bounds& path() const;

  ///
  /// @brief Get the query component of this URI object.
  ///
  /// The query component contains non-hierarchical data that, along with
  /// data in the path component (Section 3.3), serves to identify a
  /// resource within the scope of the URI's scheme and naming authority
  /// (if any).
  /// @returns The query component.
  ///
  const str_bounds& query() const;

  ///
  /// @brief Get the scheme component of this URI object.
  ///
  /// Each URI begins with a scheme name that refers to a specification for
  /// assigning identifiers within that scheme.
  /// @returns The scheme component.
  ///
  const str_bounds& scheme() const;

  ///
  /// @brief Get the fragment component of this URI object.
  ///
  /// The fragment identifier component of a URI allows indirect
  /// identification of a secondary resource by reference to a primary
  /// resource and additional identifying information.
  /// @returns The fragment component.
  ///
  const str_bounds& fragment() const;

  ///
  /// @brief Get the authority component of this URI object.
  ///
  /// The subcomponents of authority could be queried with
  /// {@link user_information()}, {@link host()} and {@link port()}.
  /// @returns The authority component.
  ///
  const str_bounds& authority() const;


  ///
  /// @brief Get the user information subcomponent of authority.
  ///
  /// The userinfo subcomponent may consist of a user name and, optionally,
  /// scheme-specific information about how to gain authorization to access
  /// the resource.
  /// @returns The user information subcomponent of {@link authority()}.
  ///
  const str_bounds& user_information() const;

  ///
  /// @brief Exchanges the contents of <code>this</code> and @p other.
  /// @param other {@link caf::io::uri uri} object that should exchange its
  ///              content with the content of <code>this</code>.
  ///
  void swap(uri& other);

  ///
  /// @brief Equivalent to <code>uri(other).swap(*this)</code>.
  /// @param other Original {@link caf::io::uri uri} object.
  /// @returns <code>*this</code>.
  ///
  uri& operator=(const uri& other);

  /// @cond private

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, uri& u) {
    return f(meta::type_name("uri"), u.c_str());
  }

  /// @endcond

private:
  uri(detail::uri_private* d);
              
  intrusive_ptr<detail::uri_private> d_;
};

} // namespace io
} // namespace caf

namespace std {

inline ostream& operator<<(ostream& ostr, const caf::io::uri& what) {
  return (ostr << what.str());
}

} // namespace std

#endif // CAF_IO_URI_HPP
